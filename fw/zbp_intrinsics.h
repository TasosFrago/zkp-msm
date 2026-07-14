#ifndef ZBP_INTRINSICS_H
#define ZBP_INTRINSICS_H

#include <stdint.h>
typedef signed char   i8;
typedef unsigned char u8;

typedef signed short   i16;
typedef unsigned short u16;

typedef signed int   i32;
typedef unsigned int u32;

typedef struct {
	u32 w[8];
} __attribute__((aligned(32))) u256;

typedef volatile i8  vi8;
typedef volatile u8  vu8;
typedef volatile i16 vi16;
typedef volatile u16 vu16;
typedef volatile i32 vi32;
typedef volatile u32 vu32;

typedef struct {
	vu32 w[8];
} vu256;

#define NULL ((void *)0)

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
#define MMIO_MODULUS ((vu256 *)MMIO_MODULUS_ADDR)
#define MMIO_GVAL(i) ((vu256 *)MMIO_GVAL_ADDR(i))
#define MMIO_DONE ((volatile u32 *)MMIO_DONE_ADDR)

#define NUM_THREADS 32

static inline __attribute__((always_inline)) int get_tid(void)
{
	register u32 tid __asm__("x4");
	return tid;
}

#define MODE_256B

#define TESTING_INSTRUCTIONS
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

#endif // TESTING_INSTRUCTIONS

#endif // ZBP_INTRINSICS_H
