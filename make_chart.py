import re
import matplotlib.pyplot as plt

levels = {"NS-Green": 4, "NS-Yellow": 3, "All-Red": 2, "EW-Green": 1, "EW-Yellow": 0}
pat = re.compile(r"t=\s*(\d+) ms \| \[SignalController\] (?:start )?-> (\S+)")

times, states = [], []
with open("stress.log") as f:
    for line in f:
        m = pat.search(line)
        if m and m.group(2) in levels:
            times.append(int(m.group(1)))
            states.append(m.group(2))

t0 = times[0]
x = [(t - t0) / 1000 for t in times]
y = [levels[s] for s in states]

plt.figure(figsize=(10, 4))
plt.step(x, y, where="post", color="tab:blue")
plt.yticks(list(levels.values()), list(levels.keys()))
plt.xlabel("Time (s)")
plt.title("Signal phase timeline under 300 ms CPU load (0 deadline misses)")
plt.grid(alpha=0.3)
plt.tight_layout()
plt.savefig("signal_timeline.png", dpi=200)
print("Saved signal_timeline.png with", len(x), "phase changes")