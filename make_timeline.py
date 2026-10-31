import re
import sys
import matplotlib.pyplot as plt
from matplotlib.patches import Patch

logfile = sys.argv[1] if len(sys.argv) > 1 else "stress.log"

sig_re = re.compile(r"t=\s*(\d+) ms \| \[SignalController\] (.*)")
states = {  # state -> (NS light, EW light)
    "NS-Green":  ("G", "R"),
    "NS-Yellow": ("Y", "R"),
    "All-Red":   ("R", "R"),
    "EW-Green":  ("R", "G"),
    "EW-Yellow": ("R", "Y"),
}
colors = {"G": "#2ca02c", "Y": "#f1c40f", "R": "#d62728", "E": "#8e44ad"}

events = []
with open(logfile) as f:
    for line in f:
        m = sig_re.search(line)
        if m:
            events.append((int(m.group(1)), m.group(2)))

# Phase changes: "start -> NS-Green" or "-> EW-Green"
phases = []
for t, msg in events:
    name = msg.replace("start -> ", "").replace("-> ", "").strip()
    if name in states:
        phases.append((t, name))

# Emergency intervals: from "EMERGENCY override approach=N" to "Emergency cleared"
emerg = []
open_start, open_app = None, None
for t, msg in events:
    if msg.startswith("EMERGENCY override approach="):
        open_start = t
        open_app = msg.split("approach=")[1][0]
    elif "Emergency cleared" in msg and open_start is not None:
        emerg.append((open_start, t, open_app))
        open_start = None
if open_start is not None:
    emerg.append((open_start, events[-1][0], open_app))

t0 = phases[0][0]
end_t = events[-1][0]
sec = lambda t: (t - t0) / 1000

fig, ax = plt.subplots(figsize=(12, 3.5))
lanes = {"NS signal": 2, "EW signal": 1, "Emergency": 0}

for i, (t, name) in enumerate(phases):
    nxt = phases[i + 1][0] if i + 1 < len(phases) else end_t
    ns, ew = states[name]
    for lane, c in (("NS signal", ns), ("EW signal", ew)):
        ax.broken_barh([(sec(t), sec(nxt) - sec(t))],
                       (lanes[lane] - 0.35, 0.7), facecolors=colors[c])

for start, stop, app in emerg:
    ax.broken_barh([(sec(start), sec(stop) - sec(start))],
                   (lanes["Emergency"] - 0.35, 0.7), facecolors=colors["E"])
    ax.text(sec(start) + 0.1, lanes["Emergency"], f"approach {app}",
            va="center", color="white", fontsize=8)

ax.set_yticks(list(lanes.values()))
ax.set_yticklabels(list(lanes.keys()))
ax.set_xlabel("Time (s)")
ax.set_xlim(0, sec(end_t) + 1)
ax.set_title(f"Signal and emergency timeline ({logfile})")
ax.legend(handles=[Patch(color=colors["G"], label="Green"),
                   Patch(color=colors["Y"], label="Yellow"),
                   Patch(color=colors["R"], label="Red"),
                   Patch(color=colors["E"], label="Emergency override")],
          loc="upper right", ncol=4, fontsize=8)
plt.tight_layout()
plt.savefig("timeline_lanes.png", dpi=200)
print(f"Saved timeline_lanes.png: {len(phases)} phase changes, {len(emerg)} emergency intervals")