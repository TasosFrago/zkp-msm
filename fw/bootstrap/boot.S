.section .text.init
.global _start

_start:
	# Stack pointer setup for each thread
	la   t0, __stack_top # Load base stack addr
	slli t1, tp, 12      # t1 = TID * 4096
	sub  sp, t0, t1      # sp = __stack_top - (TID * 4096)

	jal  ra, main

	bnez tp, hang

	li   a0, 0xF00000A0
	li   a1, 1
	sw   a1, 0(a0)

hang:
	j hang
