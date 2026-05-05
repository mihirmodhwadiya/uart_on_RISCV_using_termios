# boot.S - Entry point for RISC-V M-Mode
.section .text
.global _start

_start:
    # 1. Disable interrupts for safety during boot
    csrw mie, zero

    # 2. Set up the stack pointer (sp)
    # The linker script defines _stack_top
    la sp, _stack_top

    # 3. Jump to the C 'main' function
    call main

    # 4. If main ever returns, spin forever
loop:
    j loop
