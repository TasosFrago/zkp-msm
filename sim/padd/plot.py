# /// script
# requires-python = ">=3.12"
# dependencies = [
#     "matplotlib>=3.10.8",
#     "pyqt5>=5.15.11",
# ]
# ///
import csv
import argparse
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
from matplotlib.patches import Patch
from dataclasses import dataclass

@dataclass
class GantData:
    start: int
    duration: int
    op_type: str
    produces: str
    threadID: int


parser = argparse.ArgumentParser(description="Run the simulator with custom configurations.")

parser.add_argument(
    "--max-threads",
    type=int,
    default=20,
    help="Maximum number of threads to use (default: 20)"
)
parser.add_argument(
    "--async-load-cycles",
    type=int,
    default=3,
    help="Number of async load cycles (default: 3)"
)

args = parser.parse_args()
MAX_THREADS = args.max_threads
ASYNC_LOAD_CYCLES = args.async_load_cycles

print(f"MAX_THREADS: {MAX_THREADS}")

gant_schedule = []
max_cycle = 0

# 1. Read the Testbench Data
with open("sim/padd/schedule.csv", 'r') as f:
    reader = csv.DictReader(f)
    for row in reader:
        start = int(row['start_cycle'])
        duration = int(row['duration'])
        threadID = int(row['threadID'])

        max_cycle = max(max_cycle, start + duration)
        gant_schedule.append(GantData(
            start=start,
            duration=duration,
            op_type=row['op_type'],
            produces=row['produces'],
            threadID=threadID
        ))

mul_lat = max(task.duration for task in gant_schedule if task.op_type == 'M')
add_lat = max(task.duration for task in gant_schedule if task.op_type == 'A')
load_lat = max(task.duration for task in gant_schedule if task.op_type == 'L')
print(f"Latencies: M: {mul_lat}, A: {add_lat}, L: {load_lat}")

total_height = max(10, len(gant_schedule) * 0.15)
total_width = max(16, max_cycle * 0.08)
fig, ax = plt.subplots(figsize=(total_width, total_height))

sorted_tasks = sorted(gant_schedule, key=lambda task: task.start, reverse=True)
base = plt.get_cmap('tab20')
cmap_extra = plt.get_cmap('Set1')
colors = [base(i) if i < 20 else cmap_extra((i-20) % 3) for i in range(MAX_THREADS)]

y_ticks, y_labels = [], []

for i, task in enumerate(sorted_tasks):
    color = colors[task.threadID]
    hatch = '///' if task.op_type == 'A' else '\\\\' if task.op_type == 'L' else ''

    ax.barh(i, task.duration,
            left=task.start, color=color, edgecolor='black',
            hatch=hatch, height=1.0, linewidth=0.5)

    display_name = f"L{task.produces}" if task.op_type == 'L' else f"{task.produces}"
    text_color = 'black' if task.op_type == 'L' else 'white'

    position = (task.start + task.duration // 2) if task.duration > 3 else (task.start + 1)
    ha = 'center' if task.duration > 3 else 'left'

    ax.text(position, i, display_name, ha=ha, va='center', 
            color=text_color, fontweight='bold', fontsize=7)

    y_ticks.append(i)
    y_labels.append(f"T{task.threadID}")

ax.set_yticks(y_ticks)
ax.set_yticklabels(y_labels, fontsize=6)
ax.set_xlabel('Clock Cycles', fontsize=12, fontweight='bold')
ax.set_title(f"Hardware Trace Gantt ({MAX_THREADS} Threads, {max_cycle} cycles)", fontsize=11, fontweight='bold')
ax.xaxis.set_major_locator(ticker.MultipleLocator(5))
ax.xaxis.set_minor_locator(ticker.MultipleLocator(1))
ax.grid(axis='x', which='both', color='gray', linestyle='--', linewidth=0.5, alpha=0.3)

from matplotlib.patches import Patch

legend_elements = [
    Patch(facecolor='gray', edgecolor='black', label=f"MUL ({mul_lat} cyc)"),
    Patch(facecolor='gray', edgecolor='black', hatch='///', label=f"ADD ({add_lat} cyc)"),
    Patch(facecolor='gray', edgecolor='black', hatch='\\\\', label=f"LOAD ({ASYNC_LOAD_CYCLES} cyc)")
]

for i in range(MAX_THREADS):
    legend_elements.append(Patch(facecolor=colors[i], edgecolor='black', label=f"Thread Slot {i}"))
ax.legend(handles=legend_elements, loc='upper right', fontsize=8)

plt.tight_layout()
plt.savefig('hardware_plot.svg', format='svg', bbox_inches='tight')
print("Saved hardware trace to hardware_plot.svg")
