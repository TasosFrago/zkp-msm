.section .text
.global main

main:
    # Let's do some basic math to test the SALU!
    addi x5, x0, 10           # x5 = 10
    addi x6, x4, 5            # x6 = TID + 5
    add  x7, x5, x6           # x7 = x5 + x6

    # Test a branch
    bne  x4, x0, not_thread_0 # If TID != 0, branch away

    # Only Thread 0 does this
    addi x8, x0, 99

not_thread_0:
    # All threads end up here
    ret
