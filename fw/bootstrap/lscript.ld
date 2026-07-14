OUTPUT_ARCH( "riscv" )
ENTRY( _start )

MEMORY {
	/* 16KB Instruction Memory, 64KB Data Memory */
	IMEM (rx) : ORIGIN = 0x00000000, LENGTH = 0x4000
	DMEM (rw) : ORIGIN = 0x00000000, LENGTH = 0x10000
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
	.bss  : { *(.bss*) }  > DMEM

	.mmio_section : { *(.mmio) } > MMIO

	__stack_top = ORIGIN(DMEM) + LENGTH(DMEM);
}
