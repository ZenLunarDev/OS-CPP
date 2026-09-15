OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)

SECTIONS
{
    . = 0x40000000;

    .text : {
        *(.text*)
        *(.rodata*)
        *(.data*)
    }

    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
