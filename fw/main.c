#include "zbp_intrinsics.h"

#define WINDOW_BITS 4
#define WINDOWS (256 / WINDOW_BITS)
#define BUCKETS (1 << WINDOW_BITS)

typedef struct {
	bgn x;
	bgn y;
	bgn zz;
	bgn zzz;
} xyzz_t;

typedef struct {
	u32 size;
	xyzz_t points[];
} point_arr_t;

typedef struct {
	u32 size;
	bgn scalars[];
} scalar_arr_t;

static volatile xyzz_t RES[NUM_THREADS];

static const volatile ubgn M = { .w = { 0xFFFFFF43, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
		       0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF } };
static const volatile ubgn R2  = { .w = { 0x00008B89, 0, 0, 0, 0, 0, 0, 0 } };
static vu32  N_P  = 0xea53fa95;

static const ubgn One = { .w = { 1, 0, 0, 0, 0, 0, 0, 0 } };
static volatile ubgn A   = { .w = { 5, 0, 0, 0, 0, 0, 0, 0 } };
static volatile ubgn B   = { .w = { 6, 0, 0, 0, 0, 0, 0, 0 } };


void ecc_padd(const xyzz_t *P, const xyzz_t *Q, xyzz_t *R);
void ecc_pdbl(const xyzz_t *Qin, xyzz_t *Qout);

bgn mont_init(const bgn *v);
bgn mont_trans_back(const bgn *v);

void mont_init_points(const point_arr_t *P, point_arr_t *Pm);
void mont_trans_back_points(const point_arr_t *P, point_arr_t *Pm);

void mont_init_points_mt(const point_arr_t *P, point_arr_t *Pm);
void mont_trans_back_points_mt(const point_arr_t *P, point_arr_t *Pm);

void msm(const point_arr_t *points, const scalar_arr_t *scalars, xyzz_t *out, i32 *out_valid);
void msm_singleThreaded(const point_arr_t *points, const scalar_arr_t *scalars);

static volatile xyzz_t partial_res[NUM_THREADS];
static volatile u32 partial_valid[NUM_THREADS];
static volatile u32 partial_done[NUM_THREADS];

extern char __inputs_base[];

int main()
{
	*MMIO_MODULUS = M.v;
	*MMIO_NPRIME = N_P;

	bgn Av = A.v;
	bgn Bv = B.v;
	*MMIO_GVAL(0) = mont_init(&Av);
	*MMIO_GVAL(1) = mont_init(&Bv);

	point_arr_t *pt_arr = (point_arr_t *)__inputs_base;
	scalar_arr_t *sc_arr = (scalar_arr_t *)(__inputs_base + sizeof(u32) + 64 * sizeof(xyzz_t));

	mont_init_points_mt(pt_arr, pt_arr);

	zbp_sync_barrier();

	xyzz_t partial;
	i32 partial_ok;

	msm(pt_arr, sc_arr, &partial, &partial_ok);

	partial_res[get_tid()] = partial;
	partial_valid[get_tid()] = (u32)partial_ok;
	partial_done[get_tid()] = 1;

	zbp_sync_barrier();

	i32 Q_valid = 0;
	xyzz_t Q;

	if(get_tid() == 0) {
		for(u32 t = 0; t < NUM_THREADS; t++) {
			if(!partial_valid[t]) {
				continue;
			}
			xyzz_t tmp = (xyzz_t)partial_res[t];
			if(!Q_valid) {
				Q = tmp;
				Q_valid = 1;
			} else {
				ecc_padd(&Q, &tmp, &Q);
			}
		}
	}
	RES[0] = (Q_valid) ? Q : (xyzz_t){0};

	RES[0].x   = mont_trans_back((const bgn *)&RES[0].x);
	RES[0].y   = mont_trans_back((const bgn *)&RES[0].y);
	RES[0].zz  = mont_trans_back((const bgn *)&RES[0].zz);
	RES[0].zzz = mont_trans_back((const bgn *)&RES[0].zzz);

	return 0;
}

static void get_thread_range(u32 total, u32 tid, u32 *start, u32 *count)
{
	u32 base = total / NUM_THREADS;
	u32 rem = total % NUM_THREADS;

	if(tid < rem) {
		*count = base + 1;
		*start = tid * (base + 1);
	} else {
		*count = base;
		*start = rem * (base + 1) + (tid - rem) * base;
	}
}

static inline u32 get_window(const bgn *scalar, i32 window_idx)
{
	// u32 windows_per_word = 32 / WINDOW_BITS;
	// u32 word_idx = (u32)window_idx / windows_per_word;
	// u32 bit_off = ((u32)window_idx % windows_per_word) * WINDOW_BITS;
	u32 word_idx = (u32)window_idx >> 3;
	u32 bit_off = ((u32)window_idx & 7) * WINDOW_BITS;
	return ((*scalar)[word_idx] >> bit_off) & (BUCKETS - 1);
}

void msm(const point_arr_t *points, const scalar_arr_t *scalars, xyzz_t *out, i32 *out_valid)
{
	u32 tid = get_tid();

	u32 start, count;
	get_thread_range(points->size, tid, &start, &count);

	static xyzz_t buckets[NUM_THREADS][BUCKETS];
	static u32 bucket_valid[NUM_THREADS][BUCKETS];

	xyzz_t Q;
	i32 Q_valid = 0;

	for(i32 w = WINDOWS - 1; w >= 0; w--) {

		for(i32 b = 1; b < BUCKETS; b++) {
			bucket_valid[tid][b] = 0;
		}

		for(u32 i = start; i < start + count; i++) {
			u32 digit = get_window(&scalars->scalars[i], w);

			if(digit == 0) {
				continue;
			}

			if(!bucket_valid[tid][digit]) {
				buckets[tid][digit] = points->points[i];
				bucket_valid[tid][digit] = 1;
			} else {
				ecc_padd(&buckets[tid][digit], &points->points[i], &buckets[tid][digit]);
			}
		}

		xyzz_t running_sum, window_total;
		i32 running_valid = 0;
		i32 total_valid = 0;
		i32 window_is_exact_cpy = 0;

		for(i32 b = BUCKETS - 1; b >= 1; b--) {
			if(bucket_valid[tid][b]) {
				if(!running_valid) {
					running_sum = buckets[tid][b];
					running_valid = 1;
				} else {
					ecc_padd(&running_sum, &buckets[tid][b], &running_sum);
					window_is_exact_cpy = 0;
				}
			}

			if(running_valid) {
				if(!total_valid) {
					window_total = running_sum;
					total_valid = 1;
					window_is_exact_cpy = 1;
				} else {
					if(window_is_exact_cpy) {
						ecc_pdbl(&window_total, &window_total);
						window_is_exact_cpy = 0;
					} else {
						ecc_padd(&window_total, &running_sum, &window_total);
					}
				}
			}
		}

		if(Q_valid) {
			for(i32 d = 0; d < WINDOW_BITS; d++) {
				ecc_pdbl(&Q, &Q);
			}
		}
		if(total_valid) {
			if(!Q_valid) {
				Q = window_total;
				Q_valid = 1;
			} else {
				ecc_padd(&Q, &window_total, &Q);
			}
		}
	}

	*out = (Q_valid) ? Q : (xyzz_t){0};
	*out_valid = Q_valid;
}

void msm_singleThreaded(const point_arr_t *points, const scalar_arr_t *scalars)
{
	u32 tid = get_tid();

	static xyzz_t buckets[NUM_THREADS][BUCKETS];
	static u32 bucket_valid[NUM_THREADS][BUCKETS];

	xyzz_t Q;
	i32 Q_valid = 0;

	for(i32 w = WINDOWS - 1; w >= 0; w--) {

		for(i32 b = 1; b < BUCKETS; b++) {
			bucket_valid[tid][b] = 0;
		}

		for(i32 i = 0; i < points->size; i++) {
			u32 digit = get_window(&scalars->scalars[i], w);

			if(digit == 0) {
				continue;
			}

			if(!bucket_valid[tid][digit]) {
				buckets[tid][digit] = points->points[i];
				bucket_valid[tid][digit] = 1;
			} else {
				ecc_padd(&buckets[tid][digit], &points->points[i], &buckets[tid][digit]);
			}
		}

		xyzz_t running_sum, window_total;
		i32 running_valid = 0;
		i32 total_valid = 0;
		i32 window_is_exact_cpy = 0;

		for(i32 b = BUCKETS - 1; b >= 1; b--) {
			if(bucket_valid[tid][b]) {
				if(!running_valid) {
					running_sum = buckets[tid][b];
					running_valid = 1;
				} else {
					ecc_padd(&running_sum, &buckets[tid][b], &running_sum);
					window_is_exact_cpy = 0;
				}
			}

			if(running_valid) {
				if(!total_valid) {
					window_total = running_sum;
					total_valid = 1;
					window_is_exact_cpy = 1;
				} else {
					if(window_is_exact_cpy) {
						ecc_pdbl(&window_total, &window_total);
						window_is_exact_cpy = 0;
					} else {
						ecc_padd(&window_total, &running_sum, &window_total);
					}
				}
			}

			/*
			if(!bucket_valid[tid][b]) {
				continue;
			}

			if(!running_valid) {
				running_sum = buckets[tid][b];
				running_valid = 1;
			} else {
				ecc_padd(&running_sum, &buckets[tid][b], &running_sum);
			}

			if(!total_valid) {
				window_total = running_sum;
				total_valid = 1;
			} else {
				ecc_padd(&window_total, &running_sum, &window_total);
			}
			*/
		}

		if(Q_valid) {
			for(i32 d = 0; d < WINDOW_BITS; d++) {
				ecc_pdbl(&Q, &Q);
			}
		}

		if(total_valid) {
			if(!Q_valid) {
				Q = window_total;
				Q_valid = 1;
			} else {
				ecc_padd(&Q, &window_total, &Q);
			}
		}
	}
	RES[tid] = (Q_valid) ? Q : (xyzz_t){0};
}

bgn mont_init(const bgn *v)
{
	return zbp_bmmul(*v, (bgn)R2.v);
}

bgn mont_trans_back(const bgn *v)
{
	return zbp_bmmul(*v, One.v);
}

void mont_init_points(const point_arr_t *P, point_arr_t *Pm)
{
	Pm->size = P->size;
	for(i32 i = 0; i < P->size; i++) {
		Pm->points[i].x   = mont_init(&P->points[i].x);
		Pm->points[i].y   = mont_init(&P->points[i].y);
		Pm->points[i].zz  = mont_init(&P->points[i].zz);
		Pm->points[i].zzz = mont_init(&P->points[i].zzz);
	}
}

void mont_trans_back_points(const point_arr_t *P, point_arr_t *Pm)
{
	Pm->size = P->size;
	for(i32 i = 0; i < P->size; i++) {
		Pm->points[i].x   = mont_trans_back(&P->points[i].x);
		Pm->points[i].y   = mont_trans_back(&P->points[i].y);
		Pm->points[i].zz  = mont_trans_back(&P->points[i].zz);
		Pm->points[i].zzz = mont_trans_back(&P->points[i].zzz);
	}
}

void mont_init_points_mt(const point_arr_t *P, point_arr_t *Pm)
{
	u32 start, count;
	get_thread_range(P->size, get_tid(), &start, &count);

	Pm->size = P->size;

	for(i32 i = start; i < start + count; i++) {
		Pm->points[i].x   = mont_init(&P->points[i].x);
		Pm->points[i].y   = mont_init(&P->points[i].y);
		Pm->points[i].zz  = mont_init(&P->points[i].zz);
		Pm->points[i].zzz = mont_init(&P->points[i].zzz);
	}
}

void mont_trans_back_points_mt(const point_arr_t *P, point_arr_t *Pm)
{
	u32 start, count;
	get_thread_range(P->size, get_tid(), &start, &count);

	Pm->size = P->size;
	for(i32 i = start; i < start + count; i++) {
		Pm->points[i].x   = mont_trans_back(&P->points[i].x);
		Pm->points[i].y   = mont_trans_back(&P->points[i].y);
		Pm->points[i].zz  = mont_trans_back(&P->points[i].zz);
		Pm->points[i].zzz = mont_trans_back(&P->points[i].zzz);
	}
}

void ecc_pdbl(const xyzz_t *Qin, xyzz_t *Qout)
{
	bgn vX   = Qin->x;
	bgn vY   = Qin->y;
	bgn vZZ  = Qin->zz;
	bgn vZZZ = Qin->zzz;

	bgn vU = zbp_bmadd(vY, vY);
	bgn vV = zbp_bmmul(vU, vU);
	bgn vW = zbp_bmmul(vU, vV);
	bgn vS = zbp_bmmul(vX, vV);

	bgn vX1_2 = zbp_bmmul(vX, vX);
	bgn vZZ1_2 = zbp_bmmul(vZZ, vZZ);

	bgn vX1_2_t = zbp_bmadd(vX1_2, vX1_2);
	bgn vX1_2_3 = zbp_bmadd(vX1_2_t, vX1_2);

	bgn vZZ1_2a = zbp_bmmul(*MMIO_GVAL(0), vZZ1_2);

	bgn vM = zbp_bmadd(vX1_2_3, vZZ1_2a);
	bgn vM2 = zbp_bmmul(vM, vM);

	bgn v2S = zbp_bmadd(vS, vS);
	bgn vX3 = zbp_bmsub(vM2, v2S);

	bgn vSX3 = zbp_bmsub(vS, vX3);
	bgn vWY1 = zbp_bmmul(vW, vY);
	bgn vMSX3 = zbp_bmmul(vM, vSX3);

	bgn vY3 = zbp_bmsub(vMSX3, vWY1);
	bgn vZZ3 = zbp_bmmul(vV, vZZ);
	bgn vZZZ3 = zbp_bmmul(vW, vZZZ);

	Qout->x = vX3;
	Qout->y = vY3;
	Qout->zz = vZZ3;
	Qout->zzz = vZZZ3;
	return;
}

void ecc_padd(const xyzz_t *P, const xyzz_t *Q, xyzz_t *R)
{
	bgn vX1  = P->x;
	bgn vY1  = P->y;
	bgn vZZ1 = P->zz;
	bgn vZZZ1 = P->zzz;

	bgn vX2  = Q->x;
	bgn vY2  = Q->y;
	bgn vZZ2 = Q->zz;
	bgn vZZZ2 = Q->zzz;

	bgn vU1 = zbp_bmmul(vX1, vZZ2);
	bgn vU2 = zbp_bmmul(vX2, vZZ1);

	bgn vS1 = zbp_bmmul(vY1, vZZZ2);
	bgn vS2 = zbp_bmmul(vY2, vZZZ1);

	bgn vP = zbp_bmsub(vU2, vU1);
	bgn vR = zbp_bmsub(vS2, vS1);

	bgn vPP = zbp_bmmul(vP, vP);
	bgn vPPP = zbp_bmmul(vPP, vP);

	bgn vQ = zbp_bmmul(vU1, vPP);
	bgn v2Q = zbp_bmadd(vQ, vQ);

	bgn vR2 = zbp_bmmul(vR, vR);

	bgn vX3s1 = zbp_bmsub(vR2, vPPP);
	bgn vX3 = zbp_bmsub(vX3s1, v2Q);

	bgn vQsX3 = zbp_bmsub(vQ, vX3);

	bgn vRQsX3 = zbp_bmmul(vR, vQsX3);
	bgn vS1sPPP = zbp_bmmul(vS1, vPPP);

	bgn vY3 = zbp_bmsub(vRQsX3, vS1sPPP);

	bgn vZZ = zbp_bmmul(vZZ1, vZZ2);
	bgn vZZ3 = zbp_bmmul(vZZ, vPP);
	bgn vZZZ = zbp_bmmul(vZZZ1, vZZZ2);
	bgn vZZZ3 = zbp_bmmul(vZZZ, vPPP);

	R->x = vX3;
	R->y = vY3;
	R->zz = vZZ3;
	R->zzz = vZZZ3;
	return;
}
