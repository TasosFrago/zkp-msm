#include "zbp_intrinsics.h"

#define ITERATIONS 64

static vu32 results[NUM_THREADS];
static vu32 pass_flags[NUM_THREADS];
static vu32 scratch[NUM_THREADS][4];

int main()
{
	i32 tid = get_tid();

	u32 acc = (u32)tid;

	for(i32 i = 0; i < ITERATIONS; i++) {
		*MMIO_NPRIME = (u32)(tid + 1);
		u32 np_read = *MMIO_NPRIME;
		
		(void)np_read;

		scratch[tid][i & 3] = acc;
		u32 back = scratch[tid][i & 3];

		acc = acc * 3U + back + 1U;

		if(tid == 0) {
			u32 x = acc;
			for(i32 j = 0; j < 8; j++) {
				x = x * 3U + 1U;
			}
			acc ^= x;
		}

		u32 np2 = *MMIO_NPRIME;
		scratch[tid][(i + 1) & 3] = np2 ^ acc;
	}

	results[tid] = acc;
	pass_flags[tid] = 1;

	return 0;
}
