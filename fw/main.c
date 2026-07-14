#include "zbp_intrinsics.h"

#define ITERATIONS 5000

// static vu32 results[NUM_THREADS];
// static vu32 pass_flags[NUM_THREADS];
// static vu32 scratch[NUM_THREADS][4];
static u256 Result[NUM_THREADS];

static const u256 M = { .w = { 0xFFFFFF43, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
                               0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF } };
static const u256 R2  = { .w = { 0x00008B89, 0, 0, 0, 0, 0, 0, 0 } };
static vu32  N_P  = 0xea53fa95;

static const u256 One = { .w = { 1, 0, 0, 0, 0, 0, 0, 0 } };
static const u256 A   = { .w = { 5, 0, 0, 0, 0, 0, 0, 0 } };
static const u256 B   = { .w = { 6, 0, 0, 0, 0, 0, 0, 0 } };

typedef struct {
	u256 x;
	u256 y;
	u256 zz;
	u256 zzz;
} xyzz_t;

static xyzz_t P1;
static xyzz_t P2;
static xyzz_t RES[NUM_THREADS];

void ecc_padd(const xyzz_t *P, const xyzz_t *Q, xyzz_t *R);
void mont_init(u256 *v);
void mont_trans_back(u256 *v);

int main()
{
	u32 v_mod = zbp_lv(&M);
	zbp_sv((u256 *)MMIO_MODULUS, v_mod);

	*MMIO_NPRIME = N_P;

	mont_init(&P1.x);
	mont_init(&P1.y);
	mont_init(&P1.zz);
	mont_init(&P1.zzz);

	mont_init(&P2.x);
	mont_init(&P2.y);
	mont_init(&P2.zz);
	mont_init(&P2.zzz);

	ecc_padd(&P1, &P2, &RES[get_tid()]);

	mont_trans_back(&RES[get_tid()].x);
	mont_trans_back(&RES[get_tid()].y);
	mont_trans_back(&RES[get_tid()].zz);
	mont_trans_back(&RES[get_tid()].zzz);

	return 0;
}

void mont_init(u256 *v)
{
	u32 v_R2 = zbp_lv(&R2);
	u32 v_val = zbp_lv(v);

	u32 v_res = zbp_vmmul(v_val, v_R2);

	zbp_sv(v, v_res);
}

void mont_trans_back(u256 *v)
{
	u32 v_One = zbp_lv(&One);
	u32 v_val = zbp_lv(v);

	u32 v_res = zbp_vmmul(v_val, v_One);

	zbp_sv(v, v_res);
}

void ecc_padd(const xyzz_t *P, const xyzz_t *Q, xyzz_t *R)
{
	u32 vX1  = zbp_lv(&P->x);
	u32 vY1  = zbp_lv(&P->y);
	u32 vZZ1 = zbp_lv(&P->zz);
	u32 vZZZ1 = zbp_lv(&P->zzz);

	u32 vX2  = zbp_lv(&Q->x);
	u32 vY2  = zbp_lv(&Q->y);
	u32 vZZ2 = zbp_lv(&Q->zz);
	u32 vZZZ2 = zbp_lv(&Q->zzz);

	u32 vU1 = zbp_vmmul(vX1, vZZ2);
	u32 vU2 = zbp_vmmul(vX2, vZZ1);

	u32 vS1 = zbp_vmmul(vY1, vZZZ2);
	u32 vS2 = zbp_vmmul(vY2, vZZZ1);

	u32 vP = zbp_vmsub(vU2, vU1);
	u32 vR = zbp_vmsub(vS2, vS1);

	u32 vPP = zbp_vmmul(vP, vP);
	u32 vPPP = zbp_vmmul(vPP, vP);

	u32 vQ = zbp_vmmul(vU1, vPP);
	u32 v2Q = zbp_vmadd(vQ, vQ);

	u32 vR2 = zbp_vmmul(vR, vR);

	u32 vX3s1 = zbp_vmsub(vR2, vPPP);
	u32 vX3 = zbp_vmsub(vX3s1, v2Q);

	u32 vQsX3 = zbp_vmsub(vQ, vX3);

	u32 vRQsX3 = zbp_vmmul(vR, vQsX3);
	u32 vS1sPPP = zbp_vmmul(vS1, vPPP);

	u32 vY3 = zbp_vmsub(vRQsX3, vS1sPPP);

	u32 vZZ = zbp_vmmul(vZZ1, vZZ2);
	u32 vZZ3 = zbp_vmmul(vZZ, vPP);
	u32 vZZZ = zbp_vmmul(vZZZ1, vZZZ2);
	u32 vZZZ3 = zbp_vmmul(vZZZ, vPPP);

	zbp_sv(&R->x, vX3);
	zbp_sv(&R->y, vY3);
	zbp_sv(&R->zz, vZZ3);
	zbp_sv(&R->zzz, vZZZ3);

	return;
}

/*
void test_function()
{
	i32 tid = get_tid();

	u32 state = 0x811C9DC5U ^ ((u32)tid * 0x01000193U);
	u32 acc = 0;

	scratch[tid][0] = (u32)tid;
	scratch[tid][1] = (u32)(tid + 1);
	scratch[tid][2] = (u32)(tid + 2);
	scratch[tid][3] = (u32)(tid + 3);

	for(i32 i = 0; i < ITERATIONS; i++) {
		state ^= state << 13;
		state ^= state >> 17;
		state ^= state << 5;

		state = state * 1664525U + 1013904223U;

		u32 read_idx = (i + 2) & 3;
		u32 write_idx = i & 3;

		u32 mix = scratch[tid][read_idx];
		scratch[tid][write_idx] = state ^ mix;

		acc += (state ^ scratch[tid][write_idx]);

		*MMIO_NPRIME = acc;
	}
	results[tid] = acc;
	pass_flags[tid] = 1;
}
*/
