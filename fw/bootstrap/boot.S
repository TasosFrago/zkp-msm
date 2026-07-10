.section .text.init
.global _start

_start:
	# Stack pointer setup for each thread
	la   t0, __stack_top # Load base stack addr
	slli t1, x4, 10      # t1 = TID * 1024
	sub  sp, t0, t1      # sp = __stack_top - (TID * 1024)

	jal  ra, main

	bnez tp, hang

	li   a0, 0xF00000A0
	li   a1, 1
	sw   a1, 0(a0)

hang:
	j hang
