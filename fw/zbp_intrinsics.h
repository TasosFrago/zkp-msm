#ifndef ZBP_INTRINSICS_H
#define ZBP_INTRINSICS_H

#include <stdint.h>
typedef signed char   i8;
typedef unsigned char u8;

typedef signed short   i16;
typedef unsigned short u16;

typedef signed int   i32;
typedef unsigned int u32;

#ifndef __riscv_xzkp256b
#define __riscv_xzkp256b
#endif

#ifdef __riscv_xzkp256b
	#define MODEMODE_256B
	#define BGN_BYTES 32
	#define BGN_WORDS 8
	#define BGN_SHIFT 5
	typedef u32 bgn __attribute__((vector_size(BGN_BYTES), aligned(4)));
	typedef union {
		u32 w[BGN_WORDS];
		bgn v;
	} ubgn;
#elif defined(__riscv_xzkp128b)
	#define MODEMODE_128B
	#define BGN_BYTES 16
	#define BGN_WORDS 4
	#define BGN_SHIFT 4
	typedef u32 bgn __attribute__((vector_size(BGN_BYTES), aligned(4)));
	typedef union {
		u32 w[BGN_WORDS];
		bgn v;
	} ubgn;
#else
#error "XZkp extention (128b or 256b) not specified in -march"
#endif

typedef volatile i8  vi8;
typedef volatile u8  vu8;
typedef volatile i16 vi16;
typedef volatile u16 vu16;
typedef volatile i32 vi32;
typedef volatile u32 vu32;

#define NULL ((void *)0)

void *memset(void *dest, int val, unsigned int len)
{
	unsigned char *ptr = dest;
	unsigned char fill = (unsigned char)val;

	if (len >= BGN_BYTES) {
		u32 word = ((u32)fill << 24) | ((u32)fill << 16) |
		           ((u32)fill << 8)  |  (u32)fill;
		bgn pattern;
		u32 *pw = (u32 *)&pattern;
		for (int i = 0; i < BGN_WORDS; i++) {
			pw[i] = word;
		}

		bgn *bp = (bgn *)ptr;
		unsigned int chunks = len >> BGN_SHIFT;
		while (chunks--) {
			*bp++ = pattern;
		}
		ptr = (unsigned char *)bp;
		len &= (BGN_BYTES - 1);
	}

	while (len-- > 0) {
		*ptr++ = val;
	}
	return dest;
}

void *memcpy(void *dest, const void *src, unsigned int len)
{
	unsigned char *d = dest;
	const unsigned char *s = src;

	if (len >= BGN_BYTES) {
		bgn *bd = (bgn *)d;
		const bgn *bs = (const bgn *)s;
		unsigned int chunks = len >> BGN_SHIFT;
		while (chunks--) {
			*bd++ = *bs++;
		}
		d = (unsigned char *)bd;
		s = (const unsigned char *)bs;
		len &= (BGN_BYTES - 1);
	}

	while (len-- > 0) {
		*d++ = *s++;
	}
	return dest;
}

#define MMIO_BASE_ADDR 0xF0000000U
#define MMIO_BYTES_PER_SLOT 0x20U

#define MMIO_SLOT_ADDR(n) (MMIO_BASE_ADDR + (n) * MMIO_BYTES_PER_SLOT)

#define MMIO_MODULUS_ADDR      MMIO_SLOT_ADDR(0)
#define MMIO_NPRIME_ADDR       MMIO_SLOT_ADDR(1)
#define MMIO_GVALS_START_ADDR  MMIO_SLOT_ADDR(2)

#define MMIO_GLOBAL_REGS 3

#define MMIO_DONE_ADDR         MMIO_SLOT_ADDR(2 + MMIO_GLOBAL_REGS)

#define MMIO_GVAL_ADDR(i) (MMIO_GVALS_START_ADDR + (i) * MMIO_BYTES_PER_SLOT)

#define MMIO_NPRIME ((volatile u32 *)MMIO_NPRIME_ADDR)
#define MMIO_MODULUS ((volatile bgn *)MMIO_MODULUS_ADDR)
#define MMIO_GVAL(i) ((volatile bgn *)MMIO_GVAL_ADDR(i))
#define MMIO_DONE ((volatile u32 *)MMIO_DONE_ADDR)

#define NUM_THREADS 32

static inline __attribute__((always_inline)) int get_tid(void)
{
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wuninitialized"
	register u32 tid __asm__("x4");
	__asm__ volatile ("" : "=r" (tid));
	return tid;
	#pragma clang diagnostic pop
}

#define MODE_256B

// #define TESTING_INSTRUCTIONS
#ifdef TESTING_INSTRUCTIONS

static inline __attribute__((always_inline)) u32 zbp_vmmul(u32 a, u32 b)
{
	uint32_t res;
	__asm__ volatile (
		".insn r 0x0B, 0x0, 0x01, %0, %1, %2 \t# vmmul v.rd, v.rs1, v.rs2"
		: "=r" (res)
		: "r"  (a),
		  "r"  (b)
	);
	return res;
}

static inline __attribute__((always_inline)) u32 zbp_vmadd(u32 a, u32 b)
{
	uint32_t res;
	__asm__ volatile (
		".insn r 0x0B, 0x0, 0x00, %0, %1, %2"
		: "=r" (res)
		: "r"  (a),
		  "r"  (b)
	);
	return res;
}

static inline __attribute__((always_inline)) u32 zbp_vmsub(u32 a, u32 b)
{
	uint32_t res;
	__asm__ volatile (
		".insn r 0x0B, 0x0, 0x20, %0, %1, %2"
		: "=r" (res)
		: "r"  (a),
		  "r"  (b)
	);
	return res;
}

static inline __attribute__((always_inline)) u32 zbp_lv(const u256 *ptr)
{
	uint32_t v_handle;
	__asm__ volatile (
		".insn i 0x2B, 0x0, %0, %1, 0"
		: "=r" (v_handle)
		: "r"  (ptr)
	);
	return v_handle;
}

static inline __attribute__((always_inline)) void zbp_sv(u256 *ptr, u32 v_handle)
{
	__asm__ volatile (
		".insn s 0x2B, 0x1, %1, 0(%0)"
		:
		: "r" (ptr), "r" (v_handle)
	);
}

#else

#ifdef __riscv_xzkp256b
#define ZKP_BUILTIN(name) __builtin_riscv_zkp_##name
#else
#define ZKP_BUILTIN(name) __builtin_riscv_zkp_##name##128
#endif


static inline __attribute__((always_inline)) bgn zbp_bmadd(bgn a, bgn b)
{
	return ZKP_BUILTIN(bmadd)(a, b);
}

static inline __attribute__((always_inline)) bgn zbp_bmsub(bgn a, bgn b)
{
	return ZKP_BUILTIN(bmsub)(a, b);
}

static inline __attribute__((always_inline)) bgn zbp_reduce(bgn a)
{
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wuninitialized"
	register bgn zero_val __asm__("b0");
	__asm__ volatile ("" : "=r" (zero_val));
	return ZKP_BUILTIN(bmadd)(a, zero_val);
	#pragma clang diagnostic pop
}

static inline __attribute__((always_inline)) bgn zbp_bmmul(bgn a, bgn b)
{
	return zbp_reduce(ZKP_BUILTIN(bmmul)(a, b));
}

static inline __attribute__((always_inline)) bgn zbp_bmmul_nr(bgn a, bgn b)
{
	return ZKP_BUILTIN(bmmul)(a, b);
}

static inline __attribute__((always_inline)) void zbp_sync_barrier(void)
{
	__builtin_riscv_zkp_sync_barrier(0);
}

static inline __attribute__((always_inline)) u32 zbp_bext_w(bgn val, i32 window_idx, const u32 window_width)
{
	return ZKP_BUILTIN(bext_w)(val, window_idx, window_width);
}

static inline __attribute__((always_inline)) i32 zbp_ctz(u32 mask)
{
	return __builtin_ctz(mask);
}

#endif // TESTING_INSTRUCTIONS

#endif // ZBP_INTRINSICS_H
