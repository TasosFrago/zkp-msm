#!/usr/bin/python3
import surfer
import sys
import ctypes
import os

# ==========================================
# 1. GLOBAL LOAD (Strictly runs ONCE)
# ==========================================
script_dir = os.path.dirname(os.path.abspath(__file__))
libpath = os.path.join(script_dir, "build/sim/zbp/libdisasm.so")

try:
    disasm_lib = ctypes.CDLL(libpath)
    disasm_lib.decode_riscv.argtypes = [ctypes.c_int]
    disasm_lib.decode_riscv.restype = ctypes.c_char_p
    LIB_LOADED = True
except Exception as e:
    print(f"[ZBP Translator] Failed to load libdisasm.so: {e}", file=sys.stderr)
    LIB_LOADED = False


class ZBPDecoder:
    name = "ZBP Custom RV32 Decoder"

    def variable_info(self, variable):
        return surfer.ValueKind.Custom

    def basic_translate(self, value):
        if not LIB_LOADED:
            return "?? (lib missing)", surfer.ValueKind.Warn()

        try:
            if type(value) is int:
                numeric_val = value
            else:
                if "x" in value or "z" in value or "X" in value or "Z" in value:
                    return "??", surfer.ValueKind.Warn()

                if value.startswith("0x"):
                    numeric_val = int(value, 16)
                elif value.startswith("0b"):
                    numeric_val = int(value, 2)
                else:
                    numeric_val = int(value)

            decoded_str = disasm_lib.decode_riscv(numeric_val).decode('utf-8').strip().replace("\t", " ")

            if "INVALID" in decoded_str or "??" in decoded_str:
                return decoded_str, surfer.ValueKind.Warn()

            return decoded_str, surfer.ValueKind.Normal()

        except Exception as e:
            return f"?? (Err: {e})", surfer.ValueKind.Warn()

translators = ["ZBPDecoder"]
