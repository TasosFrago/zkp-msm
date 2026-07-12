#include "svdpi.h"
#include <capstone/capstone.h>
#include <cstdint>
#include <stdio.h>
#include <stdint.h>

extern "C" const char *decode_riscv(int instr_in)
{
	uint32_t instr = (uint32_t)instr_in;

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

	static char buffer[128];

	uint8_t code[4];
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
