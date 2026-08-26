# Lab 01 — Measuring Parallelism for Real

**CS-3006 Parallel and Distributed Computing**
Threads · performance counters · processor affinity · frequency scaling

---

## Why this lab exists

You can write a parallel program in an afternoon. Knowing whether it actually
went faster — and *why* — is the skill this course is about.

By the end of this lab you will have measured, on real silicon:

1. that threads share memory, and that sharing has a correctness cost,
2. how far a workload actually scales, and where the ceiling comes from,
3. that two identical programs can differ several-fold purely from **where
   in memory** their counters live,
4. that "a core" is not one thing — some cores are much faster than others,
5. that clock speed is a variable the hardware chooses, not a constant you set.

---

## Before you start

```bash
make
./scripts/sysinfo.sh
```

Write down, from the output:

| | your machine |
|---|---|
| CPU / chip | |
| logical CPUs | |
| physical cores | |
| P-cores / E-cores (Apple Silicon) | |
| cache line size | |
| max clock reported by the OS | |

If the OS reports **no** maximum clock, that is not a bug. Hold that thought
until Part 5.

---

## Part 1 — Threads share everything except the stack

```bash
./bin/01_hello -t 4 -i 200000
```

Four threads each add 1 to the same variable, 200 000 times. Expected total:
800 000.

**Record:** unsynchronised total = ______  lost updates = ______ %

**Why.** `counter++` is three machine operations: load, add, store. Two cores
can load the same value, both add one, and both store the same result. One
increment vanishes. Nothing in pthreads prevents this; nothing is supposed to.
The mutex version is correct — and, as Part 3 will measure, spectacularly slow.

> **Q1.** Re-run with `-t 2` and `-t 8`. Does the *percentage* of lost updates
> rise, fall, or stay flat? Explain in terms of how often two threads are
> inside the load-add-store window at the same time.

> **Q2.** `CPU / wall` is printed at the bottom. What value would you expect
> for a purely sequential program? What does a value of 3.8 tell you?

---

## Part 2 — Strong scaling, and its ceiling

Fixed total work, split across more and more threads.

```bash
./bin/02_scaling -s 1.0 -r 3
```

**Record** the speedup and efficiency columns.

| threads | wall (s) | speedup | efficiency % |
|---|---|---|---|
| 1 | | 1.00 | 100 |
| 2 | | | |
| 4 | | | |
| 8 | | | |

**Definitions you are expected to know.**

- Speedup `S(n) = T(1) / T(n)`. Ideal is `n`.
- Efficiency `E(n) = S(n) / n`. Ideal is 1.
- **Amdahl's law**: if a fraction *p* of the work is parallel,
  `S(n) ≤ 1 / ((1-p) + p/n)`. Even *p* = 0.95 caps you at 20×, forever.

> **Q3.** From your `S(8)`, solve Amdahl's law for the implied serial fraction
> `1-p`. Is that number plausible for this kernel, which does no I/O and
> allocates nothing inside the loop? If not, what *else* is eating the
> difference? (Parts 3, 4 and 5 are the three usual suspects.)

**On an Apple Silicon Mac** you will probably see efficiency fall off a cliff
somewhere past the P-core count — on a base M1, past 4 threads. That bend is
not overhead. It is Part 4.

---

## Part 3 — False sharing: the same code, several times slower

```bash
./bin/03_false_sharing
```

Five modes, identical instruction counts:

| mode | what it does |
|---|---|
| `private` | each thread increments its own local variable |
| `packed` | `counter[i]`, 8 bytes apart — all in **one cache line** |
| `padded` | `counter[i * cacheline]` — one line each |
| `atomic` | one shared counter, lock-free add |
| `mutex` | one shared counter behind a lock |

**Record** the `vs private` column for each mode.

**The point.** `packed` and `padded` compile to the same instructions and do
the same arithmetic. The only difference is the address. When two cores write
to the same cache line, the coherence protocol (MESI) must move exclusive
ownership of that line between them on *every store* — dozens of cycles each
time, for data neither core actually shares. This is **false sharing**: the
hardware's unit of sharing is a cache line, not a variable.

`atomic` and `mutex` are **true sharing**: the algorithm genuinely serialises.
Padding cannot help. Only a different algorithm can — per-thread partial
results, combined once at the end.

### Measure your own cache line

```bash
./bin/03_false_sharing --sweep
```

The stride at which the time stops improving *is* the cache line size.

**Record:** measured line size = ______ bytes

> **Q4.** Apple Silicon uses 128-byte lines; most x86-64 parts use 64. A
> popular C++ codebase pads its per-thread structures with
> `alignas(64)`. What happens to that code on an M1, and would any test
> catch it?

> **Q5.** Sketch the memory layout of the `packed` case for 4 threads. Mark
> the cache line boundary. Now do it for `padded`.

*Note:* if `packed` and `padded` come out the same, check whether your threads
are landing on distinct physical cores. Two SMT siblings share an L1 cache, so
no line ever changes owner and the effect disappears.

---

## Part 4 — Where does a thread actually run?

```bash
./bin/04_affinity -s 1.0
```

### On Apple Silicon

The program first tries the classic Mach affinity API and prints the kernel's
refusal. **There is no processor affinity on Apple Silicon.** You cannot pin a
thread to a core. What you can do is state *intent* through a QoS class:

```c
pthread_attr_set_qos_class_np(&attr, QOS_CLASS_USER_INITIATED, 0); // P-cores
pthread_attr_set_qos_class_np(&attr, QOS_CLASS_BACKGROUND,     0); // E-cores
```

**Record:**

| scenario | per-thread throughput |
|---|---|
| 1 thread, P-cluster | |
| 1 thread, E-cluster | |
| **P/E ratio** | |

That ratio is the cost of your hot thread landing on the wrong cluster — and
your code never gets told which one it got.

### On Linux

You have hard affinity:

```c
cpu_set_t set; CPU_ZERO(&set); CPU_SET(3, &set);
pthread_attr_setaffinity_np(&attr, sizeof(set), &set);
sched_setaffinity(0, sizeof(set), &set);   // or for the running thread
```

**Record** the three interesting rows: two threads on SMT siblings of one
core, two threads on different cores, and everything pinned to `cpu0`.

> **Q6.** Two threads pinned to SMT siblings of a single core give roughly
> half the per-thread throughput of two threads on separate cores, yet the
> OS calls both "2 CPUs". What does a logical CPU actually guarantee you?

> **Q7.** macOS gives you influence; Linux gives you control. Write down one
> concrete advantage of each design. Which would you want in a datacentre
> scheduler, and which in a laptop?

---

## Part 5 — Clock speed is not a constant

```bash
./bin/05_frequency -d 30
```

**Part A** runs one thread for 30 seconds and samples its throughput.
**Part B** raises the thread count and watches *per-thread* throughput fall.

**Record:**

- throughput at t = 1 s: ______   at t = 30 s: ______  → change ______ %
- per-thread throughput at 1 thread: ______  at max threads: ______ %

Then watch the hardware side while it runs:

```bash
# macOS
sudo ./scripts/mac_powermetrics.sh 30 ./bin/05_frequency -d 25
# Linux
./scripts/linux_perf.sh ./bin/02_scaling -s 1.0 -t 8
sudo ./scripts/linux_freq.sh ab ./bin/02_scaling -s 1.0 -t 1
```

### What "overclocking" means in 2026

The classic idea — raise the multiplier, add voltage, get more GHz — assumed
a chip that ran at a fixed clock with headroom left on the table. Modern parts
do not leave headroom. They run a closed control loop over power draw, die
temperature, and how many cores are awake, and they already sit at the highest
frequency those constraints allow, millisecond by millisecond.

- **Apple Silicon**: no multiplier, no voltage offset, no BIOS. The only
  user-visible knobs are Low Power Mode / High Power Mode and whether the
  laptop is on mains. Frequency is chosen in hardware.
- **x86 desktop**: firmware still exposes multipliers and voltage offsets, and
  turbo/boost can be toggled from the OS. `scripts/linux_freq.sh` does the
  safe half of that: it changes which limit the governor aims at, and
  measures the result. It never raises a vendor limit.

The measurable question is no longer "how fast can I make it go" but **"what
did it actually run at, and what made it choose that?"** — which is exactly
what Parts A and B answered.

> **Q8.** Your single-thread throughput drops 12% after 20 seconds of load.
> Name two physically different causes, and describe a measurement that
> distinguishes them.

> **Q9.** `cycles / ref-cycles` from `perf stat` is the real multiplier over
> the base clock. Why is wall-clock time alone unable to tell you whether a
> slowdown came from a lower clock or from more stalls per cycle?

---

## The counters, and what each one is for

| counter | question it answers |
|---|---|
| `cycles` | how much hardware time was spent |
| `instructions` | how much work completed |
| **IPC** = instr/cycles | how well the pipeline was fed — the single best health metric |
| `ref-cycles` | time at the invariant base clock; `cycles/ref-cycles` = real turbo ratio |
| `cache-misses` | the currency of false sharing and bad access patterns |
| `branch-misses` | control-flow cost; usually flat, so a jump is a real signal |
| `cpu-migrations` | threads bouncing between cores — pinning should drive this to ~0 |
| `context-switches` | oversubscription and lock convoys |
| `task-clock` | CPU-milliseconds; `task-clock / elapsed` = average cores kept busy |

**Rule of thumb.** Wall time tells you *that* something is slow. Counters tell
you *what kind* of slow: low IPC with high cache-misses is memory; low IPC
with high context-switches is contention; healthy IPC with a falling effective
clock is the power and thermal envelope, not your code.

---

## Deliverable

A short report (2–3 pages) containing:

1. Your filled-in tables from Parts 1–5, with the machine you used.
2. Answers to Q1–Q9.
3. One plot: speedup vs thread count from `02_scaling --csv`, with the ideal
   linear line drawn alongside.
4. Two paragraphs: the single largest source of lost performance you measured
   on your machine, and what you would change in the code to recover it.

Compare your numbers with a classmate on a different machine — an M-series
Mac against an x86 laptop is the most instructive pairing. The *shapes* of
the curves should agree even where the absolute numbers do not. Where they
disagree in shape, that is the interesting part; say why.
