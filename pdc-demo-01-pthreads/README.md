# Demo 01 — pthreads, performance counters, affinity, frequency scaling

CS-3006 Parallel and Distributed Computing · FAST-NUCES

The first hands-on demo of the course. Five small C programs and a handful of
scripts that take a class from "threads exist" to "here is what the hardware
did, measured, and here is why it did that".

Builds and runs on **macOS / Apple Silicon** (lecture machine) and on
**x86-64 Linux** (lab machines) from the same sources. The platform layer is
one file per OS; everything else is shared.

---

## Quick start

```bash
make
./scripts/sysinfo.sh          # what machine am I on?
./scripts/run_all.sh quick    # ~1 minute, everything, CSVs into results/
```

For the full lecture-length pass:

```bash
./scripts/run_all.sh          # ~3 minutes
```

---

## The five programs

| | what it shows | key output |
|---|---|---|
| `01_hello` | create/join, shared vs private state, a real data race, the mutex fix | lost updates; CPU/wall ratio |
| `02_scaling` | strong scaling over 1..N threads | speedup, efficiency, IPC, effective GHz |
| `03_false_sharing` | private / packed / padded / atomic / mutex, same work | packed-vs-padded gap; stride sweep finds the cache line |
| `04_affinity` | where threads actually run | P-core vs E-core throughput (macOS); pinning and SMT (Linux) |
| `05_frequency` | DVFS, boost, thermal decay, and what "overclocking" means now | throughput vs time; per-thread throughput vs core count |

Every program takes `--csv` for machine-readable output. Useful flags:

```
02_scaling  -t MAXTHREADS  -s SECONDS(target 1-thread time)  -r REPS
03_false_sharing  -t THREADS  -i ITERS  --sweep
04_affinity  -s SECONDS
05_frequency  -d SECONDS(part A duration)  -t MAXTHREADS
```

`-s` calibrates the work size to the machine, so the demo takes the same wall
time on an M1 as on a lab Xeon.

---

## Reading the hardware

The programs measure themselves. To measure the *chip*, use the scripts.

**macOS / Apple Silicon.** The PMU is not reachable from an unprivileged
process — there is no `perf`, and `pthread_setaffinity_np` does not exist.
Two supported routes:

```bash
sudo ./scripts/mac_powermetrics.sh 30 ./bin/02_scaling -s 1.0
./scripts/mac_xctrace.sh ./bin/02_scaling -s 1.0 -t 8    # needs full Xcode
```

`powermetrics` gives per-cluster frequency, residency and package power —
exactly the DVFS story of demo 05. `xctrace` records the Instruments
**CPU Counters** template: cycles, instructions retired, IPC, branch misses.

**Linux.** Real counters, real pinning, real governor control:

```bash
./scripts/linux_perf.sh ./bin/02_scaling -s 1.0 -t 8
./scripts/linux_freq.sh show
sudo ./scripts/linux_freq.sh ab ./bin/02_scaling -s 1.0 -t 1
```

If `perf` refuses: `sudo sysctl -w kernel.perf_event_paranoid=1`.

---

## Layout

```
src/
  common.{h,c}      timing, the shared kernel, argument parsing
  plat.h            platform interface: topology, placement, counters
  plat_macos.c      sysctl topology, QoS placement, Mach affinity probe
  plat_linux.c      sysfs topology, sched affinity, perf_event_open
  plat_common.c     printing shared by both
  0[1-5]_*.c        the demos
scripts/            sysinfo, run_all, and the per-OS measurement helpers
handout/            the student lab handout
slides/             the lecture deck
results/            written by run_all.sh
```

---

## Two things to know before you run this in front of a class

**False sharing needs threads on distinct physical cores.** If two threads
land on SMT siblings of one core (or on a VM whose vCPUs share an L1), the
packed and padded cases will look identical, because no cache line ever
changes owner. On an 8-core M1 with 8 threads it shows clearly.

**Apple Silicon cache lines are 128 bytes, not 64.** `PDC_CACHE_LINE` in
`plat.h` handles it. A padded struct that hard-codes 64 is a genuine bug on
an M-series Mac, and `03_false_sharing --sweep` proves it in about ten
seconds — one of the better moments in the demo.
