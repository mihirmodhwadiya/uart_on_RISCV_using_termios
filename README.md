RISC-V M-Mode Hardware Validation Environment
Project Overview

This project implements a complete hardware validation pipeline for the RISC-V architecture. It consists of a bare-metal firmware executing in Machine Mode (M-mode) on a simulated RISC-V core and a Linux-based host utility that monitors and reports execution results via a virtualized UART interface.
System Architecture

The validation environment bridges the gap between low-level hardware execution and high-level result reporting through the following components:

    QEMU (virt machine): Emulates a RISC-V 64-bit system with an integrated NS16550A UART peripheral mapped at 0x10000000.

    M-Mode Firmware: A low-level binary that initializes the hardware, performs architectural checks, and transmits status strings.

    Linux Host Utility: A C program using the termios API to interface with the virtualized serial device (PTY) for real-time logging.

Technical Implementation
1. Bare-Metal Firmware

The firmware is designed for minimal overhead and direct hardware interaction:

    Bootstrapping: The boot.S assembly entry point configures the stack pointer and clears the mie (Machine Interrupt Enable) register to ensure a stable execution environment.

    ISA Validation: Implements ACT-style (Architecture Compliance Test) checks, including misa CSR parsing to verify the Presence of the Atomic (A) and Compressed (C) extensions.

    UART Driver: A custom driver for the NS16550A peripheral manages transmitter holding register (THR) and line status register (LSR) states to enable string output.

2. Cross-Compilation Workflow

The build process utilizes the RISC-V GNU Toolchain. Key technical hurdles addressed include:

    Memory Model: Utilized -mcmodel=medany to resolve R_RISCV_HI20 relocation truncations, allowing the firmware to be linked across a 64-bit address space.

    ISA Extensions: Explicitly enabled the zicsr extension via -march=rv64gc_zicsr to support CSR instructions in modern GCC versions.

3. Host Communication

The host utility, uart.c, establishes a serial link at 115200 baud. It provides reliable PASS/FAIL detection by parsing the incoming byte stream from the emulated hardware.
Build and Execution
Prerequisites

    RISC-V GNU Toolchain (riscv64-unknown-elf- or riscv-none-elf-)

    QEMU System Emulator (qemu-system-riscv64)

Compilation
Bash

# Compile and link the firmware
make firmware

# Compile the host utility
gcc host/uart_test.c -o uart_test

Running the Validation

    Launch QEMU with a PTY-backed serial port:
    Bash

    qemu-system-riscv64 -machine virt -nographic -bios firmware.elf -serial pty

    Note the redirected char device (e.g., /dev/pts/X) and execute the host utility:
    Bash

    ./uart_test /dev/pts/X

Acknowledgments

This implementation and the associated technical documentation were developed with architectural guidance from:

    Inspired-by: Gemini (AI Technical Collaborator)

    Inspired-by: Claude (AI Technical Collaborator)
