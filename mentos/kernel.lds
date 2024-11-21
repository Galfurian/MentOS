OUTPUT_FORMAT("elf32-i386")

ENTRY(kmain)

MEMORY {
	USER_SPACE : ORIGIN = 0x00000000, LENGTH = 3072M
	KERNEL_LOWMEM : ORIGIN = 0xC0000000, LENGTH = 896M
	KERNEL_HIGHMEM : ORIGIN = 0xF8000000, LENGTH = 128M
}

SECTIONS
{
	. = 0xC0000000;

    .text : AT(ORIGIN(KERNEL_LOWMEM)) {
        EXCLUDE_FILE(*boot.*.o) *(.text)
    } > KERNEL_LOWMEM

    .rodata ALIGN(4K) : {
        EXCLUDE_FILE(*boot.*.o) *(.rodata)
    } > KERNEL_LOWMEM

    .data ALIGN(4K) : {
        EXCLUDE_FILE(*boot.*.o) *(.data)
    } > KERNEL_LOWMEM

    .bss ALIGN(4K) : {
        *(.bss*)
    } > KERNEL_LOWMEM

    _kernel_end = .;
}
