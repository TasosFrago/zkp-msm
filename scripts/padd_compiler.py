from dataclasses import dataclass
from typing import Literal
from collections import defaultdict

Op_t = Literal['A', 'S', 'M', 'NOOP']

@dataclass
class Operand_t:
    is_init: bool # 1 bit
    bank: int     # 2 bits
    idx: int      # 3 bits

    wb_en: bool = True
    val_type: int = 0

    def src_to_int(self) -> int:
        val = (int(self.is_init) & 0x1) << 5
        val |= (self.bank & 0x3) << 3
        val |= (self.idx & 0x7)
        return val

    def dest_to_int(self) -> int:
        val = (int(self.wb_en) & 0x1) << 8
        val |= (self.val_type & 0x7) << 5
        val |= (self.bank & 0x3) << 3
        val |= (self.idx & 0x7)
        return val

@dataclass
class Instr_t:
    op_type: Op_t
    dest:  str
    src_a: str
    src_b: str

    def __str__(self):
        op_sym = {'M': '*', 'A': '+', 'S': '-'}.get(self.op_type, '')
        return f"{self.dest} = {self.src_a} {op_sym} {self.src_b}" if op_sym else "NOOP"

    __repr__ = __str__


CHUNKS = 8
LATENCY = {
    'M': 3 * CHUNKS,
    'A': (CHUNKS + 1),
    'S': (CHUNKS + 1),
    'NOOP': 1
}


class Compiler:
    def __init__(self, initial_mapping: dict[str, Operand_t], output_types: dict[str, int], instructions: list[Instr_t]):
        self.initial_mapping = initial_mapping
        self.initial_vals = list(initial_mapping.keys())
        self.output_vals = output_types
        self.instructions = instructions

    def __ddg(self) -> tuple[dict[int, list[int]], dict[int, int]]:
        """Build data dependency graph"""
        var_to_producer = {instr.dest: i for i, instr in enumerate(self.instructions)}
        adj_list = defaultdict(list)
        in_degree = {i: 0 for i in range(len(self.instructions))}

        for i, instr in enumerate(self.instructions):
            for src in (instr.src_a, instr.src_b):
                if src not in self.initial_vals and src in var_to_producer:
                    producer_idx = var_to_producer[src]
                    if i not in adj_list[producer_idx]:
                        adj_list[producer_idx].append(i)
                        in_degree[i] += 1

        return adj_list, in_degree

    def __compute_priorities(self, adj_list: dict) -> dict[int, int]:
        memo = {}
        def get_critical_path(node_idx):
            if node_idx in memo:
                return memo[node_idx]
            max_child_path = 0
            for child in adj_list[node_idx]:
                max_child_path = max(max_child_path, get_critical_path(child))
            op_latency = LATENCY[self.instructions[node_idx].op_type]
            memo[node_idx] = op_latency + max_child_path
            return memo[node_idx]

        return {i: get_critical_path(i) for i in range(len(self.instructions))}

    def list_schedule(self) -> list[Instr_t]:
        adj_list, in_degree = self.__ddg()
        priorities = self.__compute_priorities(adj_list)
        var_ready_cycle = {v: 0 for v in self.initial_vals}

        sorted_instructions = []
        current_cycle = 0
        candidate_pool: list[int] = [i for i in range(len(self.instructions)) if in_degree[i] == 0]

        while len(sorted_instructions) < len(self.instructions):
            ready_list: list[int] = []

            for i in candidate_pool:
                instr = self.instructions[i]
                ready_a = var_ready_cycle.get(instr.src_a, 0)
                ready_b = var_ready_cycle.get(instr.src_b, 0)

                if ready_a <= current_cycle and ready_b <= current_cycle:
                    ready_list.append(i)

            if not ready_list:
                next_cycles = [max(var_ready_cycle.get(self.instructions[i].src_a, 0),
                                   var_ready_cycle.get(self.instructions[i].src_b, 0)) 
                               for i in candidate_pool]
                current_cycle = min(next_cycles)
                continue

            ready_list.sort(key=lambda idx: priorities[idx], reverse=True)
            best_idx = ready_list[0]
            candidate_pool.remove(best_idx)
            instr = self.instructions[best_idx]

            sorted_instructions.append(instr)
            var_ready_cycle[instr.dest] = current_cycle + LATENCY[instr.op_type]

            for child in adj_list[best_idx]:
                in_degree[child] -= 1
                if in_degree[child] == 0:
                    candidate_pool.append(child)

            current_cycle += 1

        return sorted_instructions

    def register_assignment(self, instructions: list[Instr_t]) -> dict[str, Operand_t]:
        born_at = {v: 0 for v in self.initial_vals}
        dies_at = {v: -1 for v in self.initial_vals}
        wb_cycle = {}

        for i, instr in enumerate(instructions):
            born_at[instr.dest] = i
            wb_cycle[instr.dest] = i + LATENCY[instr.op_type]
            dies_at[instr.src_a] = max(dies_at.get(instr.src_a, -1), i)
            if instr.src_b != instr.src_a:
                dies_at[instr.src_b] = max(dies_at.get(instr.src_b, -1), i)

        dests = [instr.dest for instr in instructions]
        wb_en_map = {}
        stored_dests = []

        for var in dests:
            if dies_at.get(var, -1) >= born_at[var]:
                wb_en_map[var] = True
                stored_dests.append(var)
            else:
                wb_en_map[var] = False

        interfere = defaultdict(set)
        def add_edge(u, v):
            if u != v:
                interfere[u].add(v)
                interfere[v].add(u)

        for instr in instructions:
            add_edge(instr.src_a, instr.src_b)

        for i in range(len(stored_dests)):
            for j in range(i + 1, len(stored_dests)):
                v1, v2 = stored_dests[i], stored_dests[j]
                if wb_cycle[v1] == wb_cycle[v2]:
                    add_edge(v1, v2)

        colors = {var: op.bank for var, op in self.initial_mapping.items()}

        all_vars = self.initial_vals + stored_dests
        for var in all_vars:
            if var in colors:
                continue

            used_colors = {colors[n] for n in interfere[var] if n in colors}
            for c in range(4):
                if c not in used_colors:
                    colors[var] = c
                    break
            else:
                raise ValueError(f"Spill required! Could not fit '{var}' into 4 banks.")

        final_mapping: dict[str, Operand_t] = self.initial_mapping.copy()
        bank_vars = {0: [], 1: [], 2: [], 3: []}

        for var in stored_dests:
            bank_vars[colors[var]].append(var)

        for bank_id in range(4):
            vars_in_banks = sorted(bank_vars[bank_id], key=lambda v: born_at[v])
            active = []

            for var in vars_in_banks:
                birth = born_at[var]
                active = [(v, idx) for v, idx in active if dies_at[v] > birth]

                used_indices = {idx for v, idx in active}
                free_idx = 0
                while free_idx in used_indices:
                    free_idx += 1

                if free_idx >= 8:
                    raise ValueError(f"Bank {bank_id} overflowed MAX_DEPTH of 8 slots!")

                active.append((var, free_idx))
                val_t = self.output_vals.get(var, 0)
                final_mapping[var] = Operand_t(False, bank_id, free_idx, wb_en=True, val_type=val_t)

        for var in dests:
            if not wb_en_map[var]:
                val_t = self.output_vals.get(var, 0)
                final_mapping[var] = Operand_t(False, 0, 0, wb_en=False, val_type=val_t)

        return final_mapping

    def generate_assembly(self, scheduled_instrs: list[Instr_t], mapping: dict[str, Operand_t]):
        print(f"--- Final Re-Mapped Assembly (NUM_OPS: {len(scheduled_instrs)}) ---")
        for instr in scheduled_instrs:
            d_reg = mapping[instr.dest]
            a_reg = mapping[instr.src_a]
            b_reg = mapping[instr.src_b]

            a_str = f"{'I' if a_reg.is_init else ''}B{a_reg.bank}[{a_reg.idx}]"
            b_str = f"{'I' if b_reg.is_init else ''}B{b_reg.bank}[{b_reg.idx}]"

            val_t_str = f" [Tag:{d_reg.val_type}]" if d_reg.val_type != 0 else ""
            d_str = f"STREAM_OUT{val_t_str}" if not d_reg.wb_en else f"B{d_reg.bank}[{d_reg.idx}]{val_t_str}"

            sym = {'M': '*', 'A': '+', 'S': '-'}.get(instr.op_type, '')

            # Print side-by-side comparison
            og = f"{instr.dest:>6} = {instr.src_a:>6} {sym} {instr.src_b:>6}"
            new = f"{d_str:>7} = {a_str:>7} {sym} {b_str:>7}"
            print(f"{og:<25} | {new}")

    def print_statistics(self, mapping: dict[str, Operand_t], number_size_bits: int = 256, max_threads: int = 27):
            print("\n--- Hardware Utilization Statistics ---")

            bank_slots = {0: 0, 1: 0, 2: 0, 3: 0}
            for op in mapping.values():
                bank_slots[op.bank] = max(bank_slots[op.bank], op.idx + 1)

            active_banks = sum(1 for slots in bank_slots.values() if slots > 0)
            total_slots = sum(bank_slots.values())

            print(f"Number of Active Banks: {active_banks} / 4")
            print(f"Total Registers (Slots) Used: {total_slots}")
            print("-" * 39)

            # Print per-bank breakdown
            for bank_id in range(4):
                slots = bank_slots[bank_id]
                mem_bits = slots * number_size_bits
                mem_bytes = mem_bits // 8
                print(f"Bank {bank_id} Size: {slots:>2} slots | {mem_bits:>4} bits | {mem_bytes:>3} bytes")

            # Print total footprint
            total_mem_bits = (total_slots * number_size_bits) * max_threads
            total_mem_bytes = total_mem_bits // 8
            print("-" * 39)
            print(f"Total Memory:          | {total_mem_bits:>4} bits | {total_mem_bytes:>3} bytes")

    def generate_hex(self, sched_instr: list[Instr_t], mapping: dict[str, Operand_t], filename: str):
        with open(filename, 'w') as f:
            for i, instr in enumerate(sched_instr):
                dest_reg = mapping[instr.dest]
                src_a_reg = mapping[instr.src_a]
                src_b_reg = mapping[instr.src_b]

                op_val = {'A': 0, 'S': 1, 'M': 2, 'NOOP': 3}.get(instr.op_type, 3)

                val = (op_val & 0x3) << 21
                val |= (src_a_reg.src_to_int() & 0x3F) << 15
                val |= (src_b_reg.src_to_int() & 0x3F) << 9
                val |= (dest_reg.dest_to_int() & 0x1FF)

                f.write(f"{val:06x}\n")
                print(f"{val:06x}\n", end="")

    def generate_tb_op_seq(self, sched_instr: list[Instr_t]):
        print("const OpType OP_SEQ[] = {")
        print("\t", end="")
        for i, instr in enumerate(sched_instr):
            ending = ", " if i < len(sched_instr)-1 else ''
            print(f"""{{ "{instr.dest}", '{instr.op_type}' }}{ending}""", end="")
        print("\n};")

    def generate_gtkwave_filter(self, mapping: dict[str, Operand_t], filename: str):
        src_filename = filename.replace('.txt', '_src.txt')
        dest_filename = filename.replace('.txt', '_dest.txt')

        sources = []
        destinations = []

        for var, op in mapping.items():
            if op.is_init:
                sources.append((f"{op.src_to_int():02X}", f"{var} (Init)"))
            elif not op.wb_en:
                destinations.append((f"{op.dest_to_int():03X}", f"{var} (Stream Out)"))
            else:
                sources.append((f"{op.src_to_int():02X}", f"{var}"))
                destinations.append((f"{op.dest_to_int():03X}", f"{var}"))

        sources.sort(key=lambda x: x[0])
        destinations.sort(key=lambda x: x[0])

        with open(src_filename, 'w') as f:
            f.write("# --- 6-BIT SOURCE TAGS (Apply to instr.src_a, instr.src_b, etc.) ---\n")
            for hex_val, label in sources:
                f.write(f"{hex_val} {label}\n")

        with open(dest_filename, 'w') as f:
            f.write("# --- 9-BIT DESTINATION TAGS (Apply to alu.dest, add_tag_out.dest, etc.) ---\n")
            for hex_val, label in destinations:
                f.write(f"{hex_val} {label}\n")


# ==========================================
# External Configuration
# ==========================================
initial_mapping = {
    'X1':   Operand_t(True, 0, 0),
    'Y1':   Operand_t(True, 0, 1),
    'ZZ1':  Operand_t(True, 0, 2),
    'ZZZ1': Operand_t(True, 0, 3),
    'X2':   Operand_t(True, 0, 4),
    'Y2':   Operand_t(True, 0, 5)
}

output_vals = {'X3': 1, 'Y3': 2, 'ZZ3': 3, 'ZZZ3': 4}

_instructions = [
    Instr_t('M', 'U2', 'X2', 'ZZ1'),
    Instr_t('M', 'S2', 'Y2', 'ZZZ1'),
    Instr_t('S', 'P', 'U2', 'X1'),
    Instr_t('S', 'R', 'S2', 'Y1'),
    Instr_t('M', 'PP', 'P', 'P'),
    Instr_t('M', 'PPP', 'PP', 'P'),
    Instr_t('M', 'Q', 'X1', 'PP'),
    Instr_t('M', 'RR', 'R', 'R'),
    Instr_t('S', 'X3t1', 'RR', 'PPP'),
    Instr_t('A', 'Q2', 'Q', 'Q'),
    Instr_t('S', 'X3', 'X3t1', 'Q2'),
    Instr_t('M', 'Y1P', 'Y1', 'PPP'),
    Instr_t('S', 'QsX3', 'Q', 'X3'),
    Instr_t('M', 'RT', 'R', 'QsX3'),
    Instr_t('S', 'Y3', 'RT', 'Y1P'),
    Instr_t('M', 'ZZ3', 'ZZ1', 'PP'),
    Instr_t('M', 'ZZZ3', 'ZZZ1', 'PPP'),
]

dbl_initial_mapping = {
    'X1':   Operand_t(True, 0, 0),
    'Y1':   Operand_t(True, 0, 1),
    'ZZ1':  Operand_t(True, 0, 2),
    'ZZZ1': Operand_t(True, 0, 3),
    'a':    Operand_t(True, 0, 4)
}

dbl_output_vals = {'X3': 1, 'Y3': 2, 'ZZ3': 3, 'ZZZ3': 4}

instructions_double = [
    Instr_t('A', 'U',      'Y1',     'Y1'),
    Instr_t('M', 'V',      'U',      'U'),
    Instr_t('M', 'W',      'U',      'V'),
    Instr_t('M', 'S',      'X1',     'V'),
    Instr_t('M', 'X1_2',   'X1',     'X1'),
    Instr_t('M', 'ZZ1_2',  'ZZ1',    'ZZ1'),
    Instr_t('A', 'X1_2_t', 'X1_2',   'X1_2'),
    Instr_t('A', 'X1_2_3', 'X1_2_t', 'X1_2'),
    Instr_t('M', 'ZZ1_2a', 'a',      'ZZ1_2'),
    Instr_t('A', 'M',      'X1_2_3', 'ZZ1_2a'),
    Instr_t('M', 'M2',     'M',      'M'),
    Instr_t('A', '2S',     'S',      'S'),
    Instr_t('S', 'X3',     'M2',     '2S'),
    Instr_t('S', 'SX3',    'S',      'X3'),
    Instr_t('M', 'WY1',    'W',      'Y1'),
    Instr_t('M', 'MSX3',   'M',      'SX3'),
    Instr_t('S', 'Y3',     'MSX3',   'WY1'),
    Instr_t('M', 'ZZ3',    'V',      'ZZ1'),
    Instr_t('M', 'ZZZ3',   'W',      'ZZZ1'),
]

complete_initial_mapping = {
    'X1':   Operand_t(True, 0, 0),
    'Y1':   Operand_t(True, 0, 1),
    'ZZ1':  Operand_t(True, 0, 2),
    'ZZZ1': Operand_t(True, 0, 3),
    'X2':   Operand_t(True, 0, 4),
    'Y2':   Operand_t(True, 0, 5),
    'ZZ2':  Operand_t(True, 0, 6),
    'ZZZ2': Operand_t(True, 0, 7),
}

output_vals = {'X3': 1, 'Y3': 2, 'ZZ3': 3, 'ZZZ3': 4}

complete_padd_instr = [
    Instr_t('M', 'U1', 'X1', 'ZZ2'),
    Instr_t('M', 'U2', 'X2', 'ZZ1'),
    Instr_t('M', 'S1', 'Y1', 'ZZZ2'),
    Instr_t('M', 'S2', 'Y2', 'ZZZ1'),
    Instr_t('S', 'P', 'U2', 'U1'),
    Instr_t('S', 'R', 'S2', 'S1'),
    Instr_t('M', 'PP', 'P', 'P'),
    Instr_t('M', 'PPP', 'PP', 'P'),
    Instr_t('M', 'Q', 'U1', 'PP'),
    Instr_t('A', 'Q2', 'Q', 'Q'),
    Instr_t('M', 'R2', 'R', 'R'),
    Instr_t('S', 'X3s1', 'R2', 'PPP'),
    Instr_t('S', 'X3', 'X3s1', 'Q2'),
    Instr_t('S', 'QsX3', 'Q', 'X3'),
    Instr_t('M', 'RQsX3', 'R', 'QsX3'),
    Instr_t('M', 'S1sPPP', 'S1', 'PPP'),
    Instr_t('S', 'Y3', 'RQsX3', 'S1sPPP'),
    Instr_t('M', 'ZZ', 'ZZ1', 'ZZ2'),
    Instr_t('M', 'ZZ3', 'ZZ', 'PP'),
    Instr_t('M', 'ZZZ', 'ZZZ1', 'ZZZ2'),
    Instr_t('M', 'ZZZ3', 'ZZZ', 'PPP'),
]

if __name__ == '__main__':
    compiler = Compiler(initial_mapping, output_vals, _instructions)
    scheduled_instrs = compiler.list_schedule()
    mapping = compiler.register_assignment(scheduled_instrs)
    compiler.generate_assembly(scheduled_instrs, mapping)
    compiler.print_statistics(mapping)
    compiler.generate_hex(scheduled_instrs, mapping, "./rtl/padd/instructions/padd_instructions.mem")
    compiler.generate_gtkwave_filter(mapping, "./sim/padd/operand_filter.txt")
    compiler.generate_tb_op_seq(scheduled_instrs);
    del compiler, scheduled_instrs, mapping

    compiler = Compiler(dbl_initial_mapping, dbl_output_vals, instructions_double)
    scheduled_instrs = compiler.list_schedule()
    mapping = compiler.register_assignment(scheduled_instrs)
    compiler.generate_assembly(scheduled_instrs, mapping)
    compiler.print_statistics(mapping)
    compiler.generate_hex(scheduled_instrs, mapping, "./rtl/padd/instructions/pdbl_instructions.mem")
    compiler.generate_tb_op_seq(scheduled_instrs);
    del compiler, scheduled_instrs, mapping

    compiler = Compiler(complete_initial_mapping, output_vals, complete_padd_instr)
    scheduled_instrs = compiler.list_schedule()
    mapping = compiler.register_assignment(scheduled_instrs)
    compiler.generate_assembly(scheduled_instrs, mapping)
    compiler.print_statistics(mapping)
    compiler.generate_hex(scheduled_instrs, mapping, "./rtl/padd/instructions/pcadd_instructions.mem")
    compiler.generate_gtkwave_filter(mapping, "./sim/padd/operand_filter.txt")
    compiler.generate_tb_op_seq(scheduled_instrs);
