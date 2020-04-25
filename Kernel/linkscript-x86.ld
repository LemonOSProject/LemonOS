ENTRY(entry)
OUTPUT_FORMAT(elf32-i386)

KERNEL_VIRTUAL_BASE = 0xC0000000;

SECTIONS {
    . = 100000;

    .multiboot.hdr : {
       *(.multiboot.hdr)
   }

    .multiboot.data : {
       *(.multiboot.data)
   }

   .multiboot.text : {
       *(.multiboot.text)
   }

   . += KERNEL_VIRTUAL_BASE;

   .text ALIGN(4096) : AT(ADDR(.text) - KERNEL_VIRTUAL_BASE) {
       *(.text)
   }

   .data ALIGN (4096) : AT(ADDR(.data) - KERNEL_VIRTUAL_BASE) {
       *(.data)
       *(.rodata*)
   }

   .bss ALIGN (4096) : AT(ADDR(.bss) - KERNEL_VIRTUAL_BASE) {
       _sbss = .;
       *(COMMON)
       *(.bss)
       _ebss = .;
   }

   /DISCARD/ : {
       *(.eh_frame);
       *(.comment*);
   }

   kernel_end = .;
}


/*SECTIONS
{
	. = 100000;
	
    .multiboot : { *(.multiboot); }
	.text : { *(.text); }
	.data : {
		*(.data) ;
		*(.rodata*);
	}
	.bss  : { *(.bss);}

	/DISCARD/ : {
       *(.eh_frame);
       *(.comment*);
   }
    
    kernel_end = .; 
}*/