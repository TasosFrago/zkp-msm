import re
import sys
import os
import bisect
from dataclasses import dataclass, field
from typing import List, Dict, Optional

NUM_THREADS = 32

@dataclass
class FunctionSymbol:
    """Represents a compiled function parsed from the symbol table."""
    pc: int
    name: str

@dataclass
class FunctionStats:
    """Tracks execution statistics for a single function."""
    name: str
    instr_count: int = 0
    total_cycles: int = 0

    @property
    def ipc(self) -> float:
        """Calculates Instructions Per Cycle (IPC)."""
        return self.instr_count / self.total_cycles if self.total_cycles > 0 else 0.0

@dataclass
class ThreadProfile:
    """Aggregates execution statistics for a single hardware thread."""
    tid: int
    func_stats: Dict[str, FunctionStats] = field(default_factory=dict)
    total_instr: int = 0
    total_cycles: int = 0

    def record_instr(self, func_name: str) -> None:
        if func_name not in self.func_stats:
            self.func_stats[func_name] = FunctionStats(func_name)
        self.func_stats[func_name].instr_count += 1
        self.total_instr += 1

    def record_cycles(self, func_name: str, cycles: int) -> None:
        if cycles <= 0:
            return
        if func_name not in self.func_stats:
            self.func_stats[func_name] = FunctionStats(func_name)
        self.func_stats[func_name].total_cycles += cycles
        self.total_cycles += cycles

def parse_symbols(sym_file: str) -> List[FunctionSymbol]:
    """Parses a symbol table to map start addresses to function names."""
    functions: List[FunctionSymbol] = []

    try:
        with open(sym_file, 'r') as f:
            for line in f:
                parts = line.strip().split()
                if len(parts) == 3:
                    addr_str, sym_type, name = parts
                    if sym_type.lower() == 't':
                        functions.append(FunctionSymbol(pc=int(addr_str, 16), name=name))
    except FileNotFoundError:
        print(f"Error: Could not find symbol file '{sym_file}'")
        sys.exit(1)

    functions.sort(key=lambda f: f.pc)
    print(f"Found following functions:")
    for function in functions:
        print(f"\t<{function.name}> {function.pc}")
    return functions

def process_trace(trace_file: str, functions: List[FunctionSymbol], tid: int) -> ThreadProfile:
    """Scans a trace file and builds a cycle-accurate perf profile for that thread."""
    profile = ThreadProfile(tid=tid)
    func_pcs = [f.pc for f in functions]

    # Matches lines like:
    # 0x00000004 | 0x1e028293 | addi t0, t0, 480 | // @[410000] [40 c]
    # 0x00000010  <--- [REDIRECT to 0x00000118] | ... | // @[1370000] [136 c]
    trace_regex = re.compile(r'^\s*0x([0-9a-fA-F]+).*?//\s*@\[\d+\]\s*\[(\d+)\s*c\]')

    if not os.path.exists(trace_file):
        return profile

    last_func: Optional[str] = None
    last_cycle: int = 0

    with open(trace_file, 'r') as f:
        for line in f:
            match = trace_regex.match(line)
            if match:
                pc = int(match.group(1), 16)
                current_cycle = int(match.group(2))

                idx = bisect.bisect_right(func_pcs, pc) - 1
                current_func = functions[idx].name if idx >= 0 else "UNKNOWN_OR_BOOT"

                profile.record_instr(current_func)

                if last_func is not None:
                    delta = current_cycle - last_cycle
                    profile.record_cycles(last_func, delta)

                last_func = current_func
                last_cycle = current_cycle

    return profile

def print_perf_table(title: str, profile: ThreadProfile, is_avg: bool = False) -> None:
    """Formats and prints the profiling report."""
    print(f"\n{'-'*85}")
    print(f" {title}")
    print(f"{'-'*85}")

    if profile.total_instr == 0:
        print("  No valid instructions recorded (Thread Idle/Parked).")
        print(f"{'-'*85}")
        return

    metric_prefix = "Avg " if is_avg else ""
    print(f" {metric_prefix}Total Instr: {profile.total_instr:,.0f} | {metric_prefix}Total Cycles: {profile.total_cycles:,.0f}")
    print(f"{'-'*85}")
    print(f" Time Overhead | {metric_prefix}Cycles    | {metric_prefix}Instr Cnt | IPC  | Function Name")
    print(f"{'-'*85}")

    # Sort functions by highest cycle count (Time)
    sorted_stats = sorted(profile.func_stats.values(), key=lambda s: s.total_cycles, reverse=True)

    for stat in sorted_stats:
        overhead = (stat.total_cycles / profile.total_cycles) * 100 if profile.total_cycles > 0 else 0.0
        print(f"        {overhead:>5.2f}% | {stat.total_cycles:>11,.0f} | {stat.instr_count:>11,.0f} | {stat.ipc:.2f} | {stat.name}")
    print(f"{'-'*85}")

def generate_average_profile(profiles: List[ThreadProfile]) -> ThreadProfile:
    """Merges a list of thread profiles into a single average profile."""
    avg_profile = ThreadProfile(tid=-1)

    for p in profiles:
        for name, stat in p.func_stats.items():
            if name not in avg_profile.func_stats:
                avg_profile.func_stats[name] = FunctionStats(name)

            avg_profile.func_stats[name].instr_count += stat.instr_count
            avg_profile.func_stats[name].total_cycles += stat.total_cycles

        avg_profile.total_instr += p.total_instr
        avg_profile.total_cycles += p.total_cycles

    for stat in avg_profile.func_stats.values():
        stat.instr_count //= NUM_THREADS
        stat.total_cycles //= NUM_THREADS

    avg_profile.total_instr //= NUM_THREADS
    avg_profile.total_cycles //= NUM_THREADS

    return avg_profile

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python3 zbp_perf.py <symbols.txt> <trace_directory>")
        sys.exit(1)

    sym_path = sys.argv[1]
    trace_dir = sys.argv[2]

    print("Parsing symbol table...")
    funcs = parse_symbols(sym_path)
    print(f"Found {len(funcs)} executable functions.")

    active_profiles: List[ThreadProfile] = []

    for tid in range(NUM_THREADS):
        trace_file = os.path.join(trace_dir, f"trace_tid_{tid}.txt")
        profile = process_trace(trace_file, funcs, tid)

        if profile.total_instr > 0:
            active_profiles.append(profile)
            print_perf_table(f"PERF REPORT: THREAD {tid}", profile)

    if active_profiles:
        avg_prof = generate_average_profile(active_profiles)
        print_perf_table(f"CPU AVERAGE REPORT (Across all {NUM_THREADS} Hardware Threads)", avg_prof, is_avg=True)
    else:
        print("\nNo instruction traces were found across any threads.")
