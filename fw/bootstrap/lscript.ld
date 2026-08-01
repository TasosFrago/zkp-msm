OUTPUT_ARCH( "riscv" )
ENTRY( _start )

MEMORY {
	IMEM (rx) : ORIGIN = 0x00000000, LENGTH = 0x4000
	DMEM (rw) : ORIGIN = 0x00000000, LENGTH = 0x1000000
	MMIO (rw) : ORIGIN = 0xF0000000, LENGTH = 0xA0
}

SECTIONS {
	.text : {
		*(.text.init)
		*(.text*)
	} > IMEM

	.data : {
		*(.rodata*)
		*(.sdata*)
		*(.data*)
	} > DMEM

	.bss  : {
		*(.bss*)
	}  > DMEM

	. = ALIGN(16);
	. += 0x20000; /* 128KB stack = 4KB stack/thread */
	__stack_top = .;

	. = ALIGN(4096);
	__inputs_base = .;
	
	__dmem_total_size = ORIGIN(DMEM) + LENGTH(DMEM);

	.mmio_section : { *(.mmio) } > MMIO

}
