#include "svdpi.h"
#include <capstone/capstone.h>
#include <cstdint>
#include <stdio.h>
#include <stdint.h>


const char *ABI_NAMES[] = {
    "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
    "s0",   "s1", "a0", "a1", "a2", "a3", "a4", "a5",
    "a6",   "a7", "s2", "s3", "s4", "s5", "s6", "s7",
    "s8",   "s9", "s10","s11","t3", "t4", "t5", "t6"
};

extern "C" const char *decode_riscv(int instr_in)
{
	uint32_t instr = (uint32_t)instr_in;

	static char buffer[128];

	uint8_t code[4];

	uint32_t opcode = instr & 0x7F;
	uint32_t rd     = (instr >> 7) & 0x1F;
	uint32_t funct3 = (instr >> 12) & 0x7;
	uint32_t rs1    = (instr >> 15) & 0x1F;
	uint32_t rs2    = (instr >> 20) & 0x1F;
	uint32_t funct7 = (instr >> 25) & 0x7F;

	if(opcode == 0x0B) {
		if(funct7 == 0x01) {
			snprintf(buffer, sizeof(buffer), "vmmul v%u, v%u, v%u", rd, rs1, rs2);
			return buffer;
		} else if (funct7 == 0x00) {
			snprintf(buffer, sizeof(buffer), "vmadd v%u, v%u, v%u", rd, rs1, rs2);
			return buffer;
		} else if (funct7 == 0x20) {
			snprintf(buffer, sizeof(buffer), "vmsub v%u, v%u, v%u", rd, rs1, rs2);
			return buffer;
		} else {
			snprintf(buffer, sizeof(buffer), "INVALID opcode: 0x%x, funct7: 0x%x", opcode, funct7);
			return buffer;
		}
	}
	if(opcode == 0x2B) {
		if (funct3 == 0x0) {
			int32_t imm = instr >> 20;
			if(imm & 0x800) imm -= 4096;

			snprintf(buffer, sizeof(buffer), "lv v%u, %d(%s)", rd, imm, ABI_NAMES[rs1]);
			return buffer;

		} else if(funct3 == 0x1) {
			int32_t imm = (funct7 << 5) | rd;
			if(imm & 0x800) imm -= 4096;

			snprintf(buffer, sizeof(buffer), "sv v%u, %d(%s)", rs2, imm, ABI_NAMES[rs1]);
			return buffer;

		} else {
			snprintf(buffer, sizeof(buffer), "INVALID opcode: 0x%x, funct3: 0x%x", opcode, funct3);
			return buffer;
		}
	}

	static csh handle = 0;
	static cs_insn *insn = NULL;
	static bool initialized = false;

	if(!initialized) {
		if(cs_open(CS_ARCH_RISCV, CS_MODE_RISCV32, &handle) != CS_ERR_OK) {
			return "?? (Capstone Init Failed)";
		}
		insn = cs_malloc(handle);
		initialized = true;
	}

	code[0] = (instr >> 0) & 0xFF;
	code[1] = (instr >> 8) & 0xFF;
	code[2] = (instr >> 16) & 0xFF;
	code[3] = (instr >> 24) & 0xFF;

	const uint8_t *code_ptr = code;
	size_t code_size = 4;
	uint64_t address = 0;

	if(cs_disasm_iter(handle, &code_ptr, &code_size, &address, insn)) {
		snprintf(buffer, sizeof(buffer), "%s %s", insn->mnemonic, insn->op_str);
	} else {
		snprintf(buffer, sizeof(buffer), "??");
	}

	return buffer;
}
