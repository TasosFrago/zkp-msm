#ifdef VERILATOR
#include "svdpi.h"
#endif
#include <cstdint>
#include <stdio.h>
#include <string>

// LLVM Headers
#include "llvm/Support/TargetSelect.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDisassembler/MCDisassembler.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCInstPrinter.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCObjectFileInfo.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TargetParser/Triple.h"

using namespace llvm;

// Static wrapper to hold LLVM context and objects so we only initialize once
struct LLVMDisassembler {
	const Target* TheTarget = nullptr;
	std::unique_ptr<MCRegisterInfo> MRI;
	std::unique_ptr<MCAsmInfo> MAI;
	std::unique_ptr<MCInstrInfo> MII;
	std::unique_ptr<MCSubtargetInfo> STI;
	std::unique_ptr<MCContext> Ctx;
	std::unique_ptr<MCDisassembler> DisAsm;
	std::unique_ptr<MCInstPrinter> IP;

	bool init() {
		InitializeAllTargetInfos();
		InitializeAllTargetMCs();
		InitializeAllDisassemblers();

		std::string Error;

		Triple TheTriple("riscv32");

		TheTarget = TargetRegistry::lookupTarget(TheTriple, Error);
		if (!TheTarget) return false;

		// Create standard MC objects
		MRI.reset(TheTarget->createMCRegInfo(TheTriple));
		MAI.reset(TheTarget->createMCAsmInfo(*MRI, TheTriple, MCTargetOptions()));

		MII.reset(TheTarget->createMCInstrInfo());

		STI.reset(TheTarget->createMCSubtargetInfo(TheTriple, "generic", "+m,+zbb,+xzkp256b"));

		Ctx = std::make_unique<MCContext>(TheTriple, *MAI, *MRI, *STI);

		DisAsm.reset(TheTarget->createMCDisassembler(*STI, *Ctx));
		if (!DisAsm) return false;

		// 0 is the default assembly flavor
		IP.reset(TheTarget->createMCInstPrinter(TheTriple, 0, *MAI, *MII, *MRI));
		if (!IP) return false;

		return true;
	}
};

extern "C" const char *decode_riscv(int instr_in)
{
	thread_local static char buffer[128];
	thread_local static LLVMDisassembler llvm_disasm;
	thread_local static bool initialized = false;

	if (!initialized) {
		if (!llvm_disasm.init()) return "?? (LLVM Init Failed)";
		initialized = true;
	}

	uint32_t instr = (uint32_t)instr_in;
	uint8_t code[4];
	code[0] = (instr >> 0) & 0xFF;
	code[1] = (instr >> 8) & 0xFF;
	code[2] = (instr >> 16) & 0xFF;
	code[3] = (instr >> 24) & 0xFF;
	ArrayRef<uint8_t> Bytes(code, 4);

	MCInst Inst;
	uint64_t Size = 0;
	MCDisassembler::DecodeStatus Status =
	llvm_disasm.DisAsm->getInstruction(Inst, Size, Bytes, 0, nulls());

	if (Status == MCDisassembler::Success) {
		std::string s;
		raw_string_ostream os(s);

		llvm_disasm.IP->printInst(&Inst, 0, "", *llvm_disasm.STI, os);

		std::string decoded = os.str();
		decoded.erase(0, decoded.find_first_not_of(" \t\r\n"));

		snprintf(buffer, sizeof(buffer), "%s", decoded.c_str());
	} else {
		snprintf(buffer, sizeof(buffer), "INVALID (0x%08X)", instr);
	}

	return buffer;
}
/*
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

	if(opcode == 0x0B && funct3 == 0) {
		if(funct7 == 0x01) {
			snprintf(buffer, sizeof(buffer), "bmmul b%u, b%u, b%u", rd, rs1, rs2);
			return buffer;
		} else if (funct7 == 0x00) {
			snprintf(buffer, sizeof(buffer), "bmadd b%u, b%u, b%u", rd, rs1, rs2);
			return buffer;
		} else if (funct7 == 0x20) {
			snprintf(buffer, sizeof(buffer), "bmsub b%u, b%u, b%u", rd, rs1, rs2);
			return buffer;
		} else if (funct7 == 0x03) {
			snprintf(buffer, sizeof(buffer), "mv.bn b%u, b%u, b%u", rd, rs1, rs2);
			return buffer;
		} else if (funct7 == 0x13) {
			snprintf(buffer, sizeof(buffer), "shfl.bn b%u, b%u, b%u", rd, rs1, rs2);
			return buffer;
		} else if (funct7 == 0x14) {
			snprintf(buffer, sizeof(buffer), "shfli.bn b%u, b%u, b%u", rd, rs1, rs2);
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

			snprintf(buffer, sizeof(buffer), "lbn b%u, %d(%s)", rd, imm, ABI_NAMES[rs1]);
			return buffer;

		} else if(funct3 == 0x1) {
			int32_t imm = (funct7 << 5) | rd;
			if(imm & 0x800) imm -= 4096;

			snprintf(buffer, sizeof(buffer), "sbn b%u, %d(%s)", rs2, imm, ABI_NAMES[rs1]);
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
*/
