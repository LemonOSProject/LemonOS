ENTRY(entry)
SECTIONS
{
	KERNEL_VMA = 0xFFFFFFFF80000000;

    . += KERNEL_VMA;

    .text : AT(ADDR(.text) - KERNEL_VMA)
    {
        _code = .;
        *(.text)
        . = ALIGN(8);

        _ctors_start = .;
        KEEP(*(.init_array .ctors))
        _ctors_end = .;

        . = ALIGN(4096);
    }
    
    .rodata : AT(ADDR(.rodata) - KERNEL_VMA)
    {
        _rodata = .;
        
        *(.rodata)

        . = ALIGN(4096);
    }

   .data : AT(ADDR(.data) - KERNEL_VMA)
   {
        _data = .;
        *(.data)
        . = ALIGN(4096);
   }

   .bss : AT(ADDR(.bss) - KERNEL_VMA)
   {
       _bss = .;
       *(.bss)

        *(COMMON)
       . = ALIGN(4096);
       _bss_end = .;
   }

   _end = .;

   /DISCARD/ :
   {
        *(.comment)
        *(.note.gnu.build-id)
   }
}
