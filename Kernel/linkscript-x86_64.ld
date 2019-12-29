ENTRY(entry)
SECTIONS
{
	KERNEL_VMA = 0xFFFFFFFF80000000;
    . = 0x100000;

    .boot.text : {
       *(.boot.text)
   }

   .boot.data :
   {
        *(.boot.data)
        . = ALIGN(4096);
   }
    . += KERNEL_VMA;

    .text : AT(ADDR(.text) - KERNEL_VMA)
    {
        _code = .;
        *(.text)
        *(.rodata)
        . = ALIGN(4096);
    }

   .data : AT(ADDR(.data) - KERNEL_VMA)
   {
        _data = .;
        *(.data)
    _initrd_start = .;
        *(.initrd)    
    _initrd_end = .;
        . = ALIGN(4096);
   }

   .eh_frame : AT(ADDR(.eh_frame) - KERNEL_VMA)
   {
       _ehframe = .;
       *(.eh_frame)
        . = ALIGN(4096);
   }

   .bss : AT(ADDR(.bss) - KERNEL_VMA)
   {
       _bss = .;
       *(.bss)

        *(COMMON)
       . = ALIGN(4096);
   }

   _end = .;

   /DISCARD/ :
   {
        *(.comment)
   }
}
