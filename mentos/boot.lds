OUTPUT_FORMAT("elf32-i386")

ENTRY(boot_entry)

MEMORY {
	BOOTLOADER_MEM : ORIGIN = 0x00100000, LENGTH = 128M
}

SECTIONS
{
    . = ORIGIN(BOOTLOADER_MEM);
	
    # Put a symbol end here, it tells us where all the kernel code/data starts.
    _bootloader_start = .;

    .multiboot_header ALIGN(4) : AT(ORIGIN(BOOTLOADER_MEM)) {
        _multiboot_header_start = .;
        *(.multiboot_header)
        _multiboot_header_end = .;
    } > BOOTLOADER_MEM

    .text ALIGN(4) : {
        *(.text)
    } > BOOTLOADER_MEM

    .rodata ALIGN(4) : {
        *(.rodata)
        *(.rodata.*)
    } > BOOTLOADER_MEM

    .data ALIGN(4) : {
        *(.data)
    } > BOOTLOADER_MEM

    .bss ALIGN(16) : {
        *(.bss)
    } > BOOTLOADER_MEM

    # Put a symbol end here, it tells us where all the kernel code/data ends.
    _bootloader_end = .;
}
