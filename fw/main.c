#include "zbp_intrinsics.h"

#define ITERATIONS 5000

static vu32 results[NUM_THREADS];
static vu32 pass_flags[NUM_THREADS];
static vu32 scratch[NUM_THREADS][4];

int main()
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

	/*
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
	*/

	return 0;
}
