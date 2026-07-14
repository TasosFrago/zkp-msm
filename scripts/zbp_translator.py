#!/usr/bin/python3
# example: decode_riscv.py (concept — you may need to install capstone and adapt modes)
import surfer
from capstone import Cs, CS_ARCH_RISCV, CS_MODE_RISCV32

import traceback
import sys

ABI_NAMES = [
    "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
    "s0",   "s1", "a0", "a1", "a2", "a3", "a4", "a5",
    "a6",   "a7", "s2", "s3", "s4", "s5", "s6", "s7",
    "s8",   "s9", "s10","s11","t3", "t4", "t5", "t6"
]

cs = Cs(CS_ARCH_RISCV, CS_MODE_RISCV32)

class ZBPDecoder:
    name = "ZBP Custom RV32 Decoder"

    def __init__(self):
        pass

    def variable_info(self, variable):
        """Tells Surfer what type of data to display (String)"""
        return surfer.ValueKind.Custom

    def basic_translate(self, value):
        """Called by Surfer for every value change on the waveform."""
        try:
            numeric_val = 0
            if isinstance(value, str):
                v_lower = value.lower()
                if "x" in v_lower or "z" in v_lower:
                    return "?? (undefined)", surfer.ValueKind.Warn()

                if v_lower.startswith("0x"):
                    numeric_val = int(value, 16)
                elif v_lower.startswith("0b"):
                    numeric_val = int(value, 2)
                elif all(c in '01' for c in value) and len(value) >= 16:
                    # Only assume binary if it's a long, raw bit-string from the waveform
                    numeric_val = int(value, 2)
                else:
                    try:
                        # Default to Decimal
                        numeric_val = int(value)
                    except ValueError:
                        # Fallback just in case Surfer passes raw Hex without the "0x" prefix
                        numeric_val = int(value, 16)

            opcode = numeric_val & 0x7F
            rd     = (numeric_val >> 7) & 0x1F
            funct3 = (numeric_val >> 12) & 0x7
            rs1    = (numeric_val >> 15) & 0x1F
            rs2    = (numeric_val >> 20) & 0x1F
            funct7 = (numeric_val >> 25) & 0x7F

            # ==========================================
            # INTERCEPT CUSTOM VECTOR MATH (Opcode 0x0B)
            # ==========================================
            if opcode == 0x0B:
                if funct7 == 0x01: return f"vmmul v{rd}, v{rs1}, v{rs2}", surfer.ValueKind.Normal()
                if funct7 == 0x00: return f"vmadd v{rd}, v{rs1}, v{rs2}", surfer.ValueKind.Normal()
                if funct7 == 0x20: return f"vmsub v{rd}, v{rs1}, v{rs2}", surfer.ValueKind.Normal()
                return f"v_math_unk v{rd}, v{rs1}, v{rs2} (f7={hex(funct7)})", surfer.ValueKind.Warn()

            # ==========================================
            # INTERCEPT CUSTOM VECTOR MEMORY (Opcode 0x2B)
            # ==========================================
            if opcode == 0x2B:
                if funct3 == 0x0:
                    imm = numeric_val >> 20
                    if imm & 0x800: imm -= 4096
                    return f"lv v{rd}, {imm}({ABI_NAMES[rs1]})", surfer.ValueKind.Normal()

                if funct3 == 0x1:
                    imm = (funct7 << 5) | rd 
                    if imm & 0x800: imm -= 4096
                    return f"sv v{rs2}, {imm}({ABI_NAMES[rs1]})", surfer.ValueKind.Normal()

                return f"v_mem_unk (f3={hex(funct3)})", surfer.ValueKind.Warn()

            # ==========================================
            # FALLBACK TO STANDARD RV32 (Capstone)
            # ==========================================
            b = numeric_val.to_bytes(4, byteorder='little')
            for insn in cs.disasm(b, 0):
                return f"{insn.mnemonic} {insn.op_str}", surfer.ValueKind.Normal()

            return "?? (invalid opcode)", surfer.ValueKind.Warn()

        except Exception as e:
            err_msg = traceback.format_exc()
            print(f"DEBUG: Error in ZBPDecoder: {err_msg}", file=sys.stderr)
            print(f"DEBUG: Input Value was: {repr(value)}", file=sys.stderr)
            return f"?? (Error: {str(e)})", surfer.ValueKind.Warn()

translators = ["ZBPDecoder"]
