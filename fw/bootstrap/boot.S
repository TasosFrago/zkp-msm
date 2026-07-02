.section .text.init
.global _start

_start:
	# Stack pointer setup for each thread
	la   t0, __stack_top # Load base stack addr
	slli t1, x4, 10      # t1 = TID * 1024
	sub  sp, t0, t1      # sp = __stack_top - (TID * 1024)

	jal  ra, main

hang:
	j hang
