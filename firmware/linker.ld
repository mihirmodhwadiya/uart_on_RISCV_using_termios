OUTPUT_ARCH( "riscv" )
ENTRY( _start )

SECTIONS
{
  /* QEMU virt RAM starts at 0x80000000 */
  . = 0x80000000;

  .text : {
    *(.text .text.*)
  }

  .data : {
    *(.data .data.*)
  }

  .bss : {
    *(.bss .bss.*)
    *(COMMON)
  }

  /* Define stack at the end of the first 64KB */
  . = ALIGN(16);
  . += 0x10000; /* 64KB stack */
  _stack_top = .;
}
