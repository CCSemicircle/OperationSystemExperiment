OUTPUT_FORMAT("pei-i386")
ENTRY(_entry)
SECTIONS {
  . = 0xC0000000 +0x100000;
  .text : {
    *(.text)
  }
  . = ALIGN(4);
  .rodata : {
    *(.rodata)
  }
  .rdata : {
    *(.rdata)
  }
  .data : {
    *(.data)
  }
  . = ALIGN(4);
  _edata = .;
  .bss : {
    *(.bss)
    *(COMMON)
  }
  . = ALIGN(4);
  _end = .;
}
