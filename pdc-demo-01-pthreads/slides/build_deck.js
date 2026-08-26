// Builds 03_Pthreads_and_Performance_Measurement.pptx
// CS-3006 Parallel and Distributed Computing -- Fall 2026
const pptxgen = require("pptxgenjs");

const P = {
  ink:    "16222E",   // deep slate  -- dominant dark
  ink2:   "2E4057",   // supporting slate
  paper:  "FFFFFF",
  mist:   "EEF1F4",   // light panel
  amber:  "F2A65A",   // accent: power / heat / clock
  teal:   "2EC4B6",   // accent: cores / parallelism
  rust:   "C1544A",   // accent: the thing that goes wrong
  muted:  "6B7A88",
};
const F = { head: "Cambria", body: "Calibri", code: "Courier New" };

const pres = new pptxgen();
pres.layout = "LAYOUT_WIDE";               // 13.333 x 7.5
pres.author = "CS-3006 PDC";
pres.title  = "Pthreads and Performance Measurement";

const W = 13.333, H = 7.5, M = 0.7;

/* ---------------------------------------------------------------- */
function darkSlide() {
  const s = pres.addSlide();
  s.background = { color: P.ink };
  return s;
}
function lightSlide(title, kicker) {
  const s = pres.addSlide();
  s.background = { color: P.paper };
  if (kicker)
    s.addText(kicker.toUpperCase(), {
      x: M, y: 0.42, w: 8, h: 0.28, fontFace: F.body, fontSize: 11,
      bold: true, color: P.teal, charSpacing: 2, margin: 0,
    });
  s.addText(title, {
    x: M, y: kicker ? 0.72 : 0.55, w: W - 2 * M, h: 0.75,
    fontFace: F.head, fontSize: 34, bold: true, color: P.ink, margin: 0,
  });
  return s;
}
function codeBox(s, lines, o) {
  s.addShape(pres.ShapeType.roundRect, {
    x: o.x, y: o.y, w: o.w, h: o.h, fill: { color: P.ink },
    rectRadius: 0.06, line: { color: P.ink },
  });
  s.addText(lines.join("\n"), {
    x: o.x + 0.18, y: o.y + 0.14, w: o.w - 0.36, h: o.h - 0.28,
    fontFace: F.code, fontSize: o.size || 12, color: "D8E0E8",
    lineSpacing: (o.size || 12) * 1.45, margin: 0, valign: "top",
  });
}
function card(s, o) {
  s.addShape(pres.ShapeType.roundRect, {
    x: o.x, y: o.y, w: o.w, h: o.h, fill: { color: o.fill || P.mist },
    rectRadius: 0.08, line: { color: o.fill || P.mist },
  });
}
function numDot(s, n, x, y, color) {
  s.addShape(pres.ShapeType.ellipse, {
    x, y, w: 0.42, h: 0.42, fill: { color: color || P.teal },
    line: { color: color || P.teal },
  });
  s.addText(String(n), {
    x, y, w: 0.42, h: 0.42, align: "center", valign: "middle",
    fontFace: F.body, fontSize: 15, bold: true, color: P.ink, margin: 0,
  });
}
function note(s, t) { s.addNotes(t); }

/* ================================================================ 1 */
{
  const s = darkSlide();
  s.addText("03", {
    x: M, y: 1.05, w: 2, h: 1.0, fontFace: F.head, fontSize: 64,
    bold: true, color: P.ink2, margin: 0,
  });
  s.addText("Pthreads and\nPerformance Measurement", {
    x: M, y: 2.0, w: 10.5, h: 1.9, fontFace: F.head, fontSize: 46,
    bold: true, color: P.paper, lineSpacing: 52, margin: 0,
  });
  s.addText("Counting what the hardware actually did — threads, counters, affinity, and the clock that will not sit still", {
    x: M, y: 4.0, w: 10.2, h: 0.9, fontFace: F.body, fontSize: 17,
    color: P.amber, margin: 0,
  });
  s.addText("CS-3006  Parallel and Distributed Computing   ·   Fall 2026", {
    x: M, y: 6.4, w: 9, h: 0.35, fontFace: F.body, fontSize: 13,
    color: P.muted, margin: 0,
  });
  note(s, "Demo 01 of the course. Everything on these slides is run live from the pdc-demo-01-pthreads package.");
}

/* ================================================================ 2 */
{
  const s = lightSlide("Three questions this lecture answers", "where we are");
  const qs = [
    ["Did it actually run in parallel?",
     "Wall time alone cannot tell you. CPU-time / wall-time can, and hardware counters can say more."],
    ["Why did it stop getting faster?",
     "Amdahl is only the first suspect. Cache coherence, core heterogeneity and clock throttling are the other three."],
    ["What can I control, and what merely happens to me?",
     "Affinity, scheduling and frequency are all partly out of your hands — and differently so on Linux and on Apple Silicon."],
  ];
  qs.forEach(([h, b], i) => {
    const y = 1.85 + i * 1.55;
    numDot(s, i + 1, M, y + 0.12, [P.teal, P.amber, P.rust][i]);
    s.addText(h, { x: M + 0.68, y: y, w: 11.2, h: 0.42, fontFace: F.head,
      fontSize: 22, bold: true, color: P.ink, margin: 0 });
    s.addText(b, { x: M + 0.68, y: y + 0.46, w: 11.0, h: 0.7, fontFace: F.body,
      fontSize: 15, color: P.muted, margin: 0 });
  });
  note(s, "Frame the whole demo. Come back to these three at the end.");
}

/* ================================================================ 3 */
{
  const s = lightSlide("The machine is not what the textbook drew", "the target");
  s.addText("An 8-core Apple M1 is two different CPUs on one die.", {
    x: M, y: 1.7, w: 11.9, h: 0.4, fontFace: F.body, fontSize: 17,
    color: P.ink2, margin: 0 });

  card(s, { x: M, y: 2.3, w: 5.6, h: 2.35, fill: P.ink });
  s.addText("4 × Firestorm", { x: M + 0.35, y: 2.5, w: 5, h: 0.4,
    fontFace: F.head, fontSize: 22, bold: true, color: P.teal, margin: 0 });
  s.addText([
    { text: "performance cores", options: { breakLine: true } },
    { text: "wide out-of-order, large caches", options: { breakLine: true } },
    { text: "high clock, high power", options: { breakLine: true } },
    { text: "hw.perflevel0.logicalcpu", options: { fontFace: F.code, fontSize: 12 } },
  ], { x: M + 0.35, y: 2.95, w: 5, h: 1.5, fontFace: F.body, fontSize: 14,
       color: "C6D2DC", lineSpacing: 22, margin: 0 });

  card(s, { x: M + 6.0, y: 2.3, w: 5.6, h: 2.35, fill: P.ink2 });
  s.addText("4 × Icestorm", { x: M + 6.35, y: 2.5, w: 5, h: 0.4,
    fontFace: F.head, fontSize: 22, bold: true, color: P.amber, margin: 0 });
  s.addText([
    { text: "efficiency cores", options: { breakLine: true } },
    { text: "narrow, small caches, low clock", options: { breakLine: true } },
    { text: "a fraction of the throughput", options: { breakLine: true } },
    { text: "hw.perflevel1.logicalcpu", options: { fontFace: F.code, fontSize: 12 } },
  ], { x: M + 6.35, y: 2.95, w: 5, h: 1.5, fontFace: F.body, fontSize: 14,
       color: "C6D2DC", lineSpacing: 22, margin: 0 });

  s.addText("Consequence: your eighth thread does not add what your second thread added. That is not overhead — it is a different core.", {
    x: M, y: 4.95, w: 11.9, h: 0.6, fontFace: F.body, fontSize: 16,
    color: P.rust, bold: true, margin: 0 });
  s.addText("A lab x86 box is uniform, but has SMT: two logical CPUs sharing one core's execution units. Same lesson, different mechanism — \"logical CPU\" is a promise of scheduling, not of throughput.", {
    x: M, y: 5.6, w: 11.9, h: 0.8, fontFace: F.body, fontSize: 14,
    color: P.muted, margin: 0 });
  note(s, "Run ./scripts/sysinfo.sh live here. On a lab machine show lscpu and point at Thread(s) per core.");
}

/* ================================================================ 4 */
{
  const s = lightSlide("Pthreads is five function calls", "the API");
  codeBox(s, [
    "#include <pthread.h>",
    "",
    "void *worker(void *arg) {",
    "    task_t *t = (task_t *)arg;",
    "    t->result = compute(t->iters);",
    "    return NULL;            // or pthread_exit(...)",
    "}",
    "",
    "pthread_t th[N];",
    "for (int i = 0; i < N; i++)",
    "    pthread_create(&th[i], NULL, worker, &task[i]);",
    "",
    "for (int i = 0; i < N; i++)",
    "    pthread_join(th[i], NULL);   // blocks till it ends",
  ], { x: M, y: 1.75, w: 7.3, h: 4.3, size: 13 });

  const pts = [
    ["Own stack", "Locals are private. Everything reached through a pointer is not."],
    ["Shared heap & globals", "One address space. This is the whole point, and the whole danger."],
    ["Argument passing", "One void*. Give each thread its own struct — never a pointer to a loop variable."],
    ["Join before reading", "Results are only guaranteed visible after pthread_join."],
  ];
  pts.forEach(([h, b], i) => {
    const y = 1.85 + i * 1.12;
    s.addText(h, { x: 8.4, y, w: 4.4, h: 0.35, fontFace: F.head, fontSize: 17,
      bold: true, color: P.ink, margin: 0 });
    s.addText(b, { x: 8.4, y: y + 0.34, w: 4.4, h: 0.72, fontFace: F.body,
      fontSize: 13, color: P.muted, margin: 0 });
  });
  note(s, "Live: ./bin/01_hello -t 4");
}

/* ================================================================ 5 */
{
  const s = lightSlide("Shared memory, shared bugs", "demo 01");
  s.addText("Four threads. Each adds 1 to the same variable, 200 000 times.", {
    x: M, y: 1.7, w: 11.9, h: 0.4, fontFace: F.body, fontSize: 17,
    color: P.ink2, margin: 0 });

  card(s, { x: M, y: 2.25, w: 3.75, h: 1.75 });
  s.addText("800,000", { x: M, y: 2.45, w: 3.75, h: 0.75, align: "center",
    fontFace: F.head, fontSize: 40, bold: true, color: P.ink, margin: 0 });
  s.addText("expected", { x: M, y: 3.25, w: 3.75, h: 0.35, align: "center",
    fontFace: F.body, fontSize: 14, color: P.muted, margin: 0 });

  card(s, { x: M + 4.1, y: 2.25, w: 3.75, h: 1.75, fill: "F7E4E2" });
  s.addText("≈ 300,000", { x: M + 4.1, y: 2.45, w: 3.75, h: 0.75, align: "center",
    fontFace: F.head, fontSize: 40, bold: true, color: P.rust, margin: 0 });
  s.addText("actually counted", { x: M + 4.1, y: 3.25, w: 3.75, h: 0.35,
    align: "center", fontFace: F.body, fontSize: 14, color: P.muted, margin: 0 });

  card(s, { x: M + 8.2, y: 2.25, w: 3.75, h: 1.75 });
  s.addText("800,000", { x: M + 8.2, y: 2.45, w: 3.75, h: 0.75, align: "center",
    fontFace: F.head, fontSize: 40, bold: true, color: P.teal, margin: 0 });
  s.addText("with a mutex", { x: M + 8.2, y: 3.25, w: 3.75, h: 0.35,
    align: "center", fontFace: F.body, fontSize: 14, color: P.muted, margin: 0 });

  codeBox(s, [
    "counter++      is three instructions, not one:",
    "        ldr  x0, [counter]      // both cores read 7",
    "        add  x0, x0, #1         // both compute 8",
    "        str  x0, [counter]      // both write 8  -> one update lost",
  ], { x: M, y: 4.35, w: 11.9, h: 1.35, size: 13 });
  s.addText("The mutex restores correctness and destroys scalability. Both halves of that sentence get measured in demo 03.", {
    x: M, y: 5.9, w: 11.9, h: 0.5, fontFace: F.body, fontSize: 15,
    color: P.ink2, italic: true, margin: 0 });
  note(s, "Exact loss varies run to run — that non-determinism is itself the lesson.");
}

/* ================================================================ 6 */
{
  const s = lightSlide("Wall time tells you that. Counters tell you what.", "measurement");
  const rows = [
    ["cycles", "hardware time actually spent", P.teal],
    ["instructions", "work that actually completed", P.teal],
    ["IPC = instructions / cycles", "how well the pipeline was fed — the best single health metric", P.amber],
    ["ref-cycles", "time at the invariant base clock; cycles/ref-cycles = the real turbo ratio", P.amber],
    ["cache-misses", "the currency of false sharing and bad access patterns", P.rust],
    ["cpu-migrations", "threads bouncing between cores; pinning should drive this to ~0", P.rust],
    ["task-clock", "CPU-milliseconds; ÷ elapsed = average cores kept busy", P.muted],
  ];
  rows.forEach(([k, v, c], i) => {
    const y = 1.72 + i * 0.66;
    s.addShape(pres.ShapeType.ellipse, { x: M, y: y + 0.09, w: 0.16, h: 0.16,
      fill: { color: c }, line: { color: c } });
    s.addText(k, { x: M + 0.34, y, w: 3.5, h: 0.35, fontFace: F.code,
      fontSize: 13, bold: true, color: P.ink, margin: 0 });
    s.addText(v, { x: M + 4.0, y, w: 7.9, h: 0.42, fontFace: F.body,
      fontSize: 14, color: P.muted, margin: 0 });
  });
  s.addText("Low IPC + high cache-misses = memory.   Low IPC + high context-switches = contention.   Healthy IPC + falling clock = power and heat.", {
    x: M, y: 6.45, w: 11.9, h: 0.5, fontFace: F.body, fontSize: 14,
    bold: true, color: P.ink2, margin: 0 });
  note(s, "This slide is the vocabulary the rest of the course leans on.");
}

/* ================================================================ 7 */
{
  const s = lightSlide("Reading the counters: same physics, different rules", "tooling");
  card(s, { x: M, y: 1.75, w: 5.8, h: 4.4, fill: P.mist });
  s.addText("Linux  (your lab machines)", { x: M + 0.35, y: 2.0, w: 5.1, h: 0.4,
    fontFace: F.head, fontSize: 20, bold: true, color: P.ink, margin: 0 });
  s.addText([
    { text: "The PMU is open to user code.", options: { breakLine: true, bold: true } },
    { text: "perf_event_open() from inside the program, or perf stat around it.", options: { breakLine: true } },
    { text: "Per-process, per-thread, exact.", options: {} },
  ], { x: M + 0.35, y: 2.45, w: 5.1, h: 1.2, fontFace: F.body, fontSize: 14,
       color: P.ink2, lineSpacing: 20, margin: 0 });
  codeBox(s, [
    "perf stat -e cycles,instructions, \\",
    "  ref-cycles,cache-misses,\\",
    "  cpu-migrations ./bin/02_scaling",
    "",
    "# if it refuses:",
    "sudo sysctl -w \\",
    "  kernel.perf_event_paranoid=1",
  ], { x: M + 0.35, y: 3.75, w: 5.1, h: 2.15, size: 11 });

  card(s, { x: M + 6.1, y: 1.75, w: 5.8, h: 4.4, fill: P.mist });
  s.addText("macOS / Apple Silicon", { x: M + 6.45, y: 2.0, w: 5.1, h: 0.4,
    fontFace: F.head, fontSize: 20, bold: true, color: P.ink, margin: 0 });
  s.addText([
    { text: "The PMU is not exposed to user processes.", options: { breakLine: true, bold: true } },
    { text: "No perf. Counters come from Instruments; SoC telemetry from powermetrics.", options: {} },
  ], { x: M + 6.45, y: 2.45, w: 5.1, h: 1.2, fontFace: F.body, fontSize: 14,
       color: P.ink2, lineSpacing: 20, margin: 0 });
  codeBox(s, [
    "# cycles, instructions, IPC",
    "xctrace record \\",
    "  --template 'CPU Counters' \\",
    "  --launch -- ./bin/02_scaling",
    "",
    "# cluster GHz, residency, watts",
    "sudo powermetrics \\",
    "  --samplers cpu_power -i 1000",
  ], { x: M + 6.45, y: 3.75, w: 5.1, h: 2.15, size: 11 });

  s.addText("Same physics, different permission model. Wrapped for you in scripts/linux_perf.sh, scripts/mac_xctrace.sh and scripts/mac_powermetrics.sh.", {
    x: M, y: 6.35, w: 11.9, h: 0.5, fontFace: F.body, fontSize: 14,
    color: P.muted, margin: 0 });
  note(s, "Be explicit that the Mac restriction is a permission decision, not a hardware limitation.");
}

/* ================================================================ 8 */
{
  const s = lightSlide("Strong scaling, and the ceiling", "demo 02");
  s.addChart(pres.ChartType.line, [
    { name: "Ideal (linear)", labels: ["1","2","3","4","5","6","7","8"],
      values: [1,2,3,4,5,6,7,8] },
    { name: "Measured (4P + 4E)", labels: ["1","2","3","4","5","6","7","8"],
      values: [1,1.97,2.9,3.8,4.2,4.5,4.75,4.95] },
  ], {
    x: M, y: 1.8, w: 7.4, h: 4.2,
    showTitle: true, title: "Speedup vs threads — expected shape on a base M1",
    titleFontFace: F.body, titleFontSize: 13, titleColor: P.muted,
    chartColors: [P.muted, P.teal], lineDataSymbol: "circle", lineSize: 3,
    showLegend: true, legendPos: "b", legendFontFace: F.body, legendFontSize: 12,
    catAxisLabelColor: P.muted, valAxisLabelColor: P.muted,
    catAxisLabelFontFace: F.body, valAxisLabelFontFace: F.body,
    catAxisLabelFontSize: 11, valAxisLabelFontSize: 11,
    valGridLine: { color: "DDE3E8", size: 1 }, catGridLine: { style: "none" },
    valAxisMinVal: 0, valAxisMaxVal: 8,
  });
  s.addText("Illustrative shape, not measured data — run it on your own machine.", {
    x: M, y: 6.1, w: 7.4, h: 0.3, fontFace: F.body, fontSize: 11,
    italic: true, color: P.muted, margin: 0 });

  s.addText("Amdahl's law", { x: 8.5, y: 1.9, w: 4.3, h: 0.4,
    fontFace: F.head, fontSize: 21, bold: true, color: P.ink, margin: 0 });
  codeBox(s, ["S(n) ≤  1 / ( (1-p) + p/n )"], { x: 8.5, y: 2.4, w: 4.3, h: 0.6, size: 12 });
  s.addText([
    { text: "p = 0.95  →  ceiling of 20×, no matter how many cores.", options: { breakLine: true } },
    { text: "", options: { breakLine: true } },
    { text: "But the bend at 4 threads here is not Amdahl at all — it is where the P-cores run out and the E-cores take over.", options: { breakLine: true } },
    { text: "", options: { breakLine: true } },
    { text: "Fit Amdahl to a heterogeneous machine and you will \"measure\" a serial fraction that does not exist.", options: { bold: true } },
  ], { x: 8.5, y: 3.2, w: 4.3, h: 2.9, fontFace: F.body, fontSize: 14,
       color: P.ink2, lineSpacing: 19, margin: 0 });
  note(s, "Live: ./bin/02_scaling -s 1.0. Ask the class to predict the bend before it prints.");
}

/* ================================================================ 9 */
{
  const s = lightSlide("Same instructions. Same arithmetic. Different address.", "demo 03");
  s.addText("Four threads incrementing four counters. The only thing that changes is where those counters sit.", {
    x: M, y: 1.68, w: 11.9, h: 0.4, fontFace: F.body, fontSize: 16,
    color: P.ink2, margin: 0 });

  // packed
  s.addText("packed — one cache line", { x: M, y: 2.25, w: 5.6, h: 0.32,
    fontFace: F.body, fontSize: 14, bold: true, color: P.rust, margin: 0 });
  s.addShape(pres.ShapeType.rect, { x: M, y: 2.65, w: 2.62, h: 0.62,
    fill: { color: "F7E4E2" }, line: { color: P.rust, width: 2 } });
  for (let i = 0; i < 4; i++) {
    s.addText("c" + i, { x: M + 0.06 + i * 0.62, y: 2.72, w: 0.55, h: 0.48,
      align: "center", valign: "middle", fontFace: F.code, fontSize: 12,
      bold: true, color: P.ink, margin: 0,
      fill: { color: "FFFFFF" } });
  }
  s.addText("128 bytes — every store invalidates the line in all four L1s", {
    x: M, y: 3.35, w: 5.4, h: 0.35, fontFace: F.body, fontSize: 12,
    color: P.muted, margin: 0 });

  // padded
  s.addText("padded — one line each", { x: M + 6.3, y: 2.25, w: 5.6, h: 0.32,
    fontFace: F.body, fontSize: 14, bold: true, color: P.teal, margin: 0 });
  for (let i = 0; i < 4; i++) {
    s.addShape(pres.ShapeType.rect, { x: M + 6.3 + i * 1.4, y: 2.65, w: 1.25, h: 0.62,
      fill: { color: "E4F5F2" }, line: { color: P.teal, width: 2 } });
    s.addText("c" + i, { x: M + 6.3 + i * 1.4, y: 2.65, w: 1.25, h: 0.62,
      align: "center", valign: "middle", fontFace: F.code, fontSize: 12,
      bold: true, color: P.ink, margin: 0 });
  }
  s.addText("no line is ever shared — no coherence traffic at all", {
    x: M + 6.3, y: 3.35, w: 5.6, h: 0.35, fontFace: F.body, fontSize: 12,
    color: P.muted, margin: 0 });

  card(s, { x: M, y: 3.95, w: 11.9, h: 2.15, fill: P.mist });
  s.addText("False sharing", { x: M + 0.35, y: 4.15, w: 11.2, h: 0.4,
    fontFace: F.head, fontSize: 20, bold: true, color: P.ink, margin: 0 });
  s.addText("The hardware's unit of sharing is a cache line, not a variable. Two cores writing to different variables in the same line still fight for exclusive ownership of it — tens of cycles per store, for data neither core shares.", {
    x: M + 0.35, y: 4.6, w: 11.2, h: 0.8, fontFace: F.body, fontSize: 15,
    color: P.ink2, margin: 0 });
  s.addText("atomic and mutex are TRUE sharing: the algorithm really does serialise. Padding cannot help — only per-thread partials with one reduction at the end.", {
    x: M + 0.35, y: 5.4, w: 11.2, h: 0.6, fontFace: F.body, fontSize: 14,
    color: P.muted, margin: 0 });
  note(s, "Live: ./bin/03_false_sharing. Needs threads on distinct physical cores or the effect vanishes.");
}

/* =============================================================== 10 */
{
  const s = darkSlide();
  s.addText("128", { x: M, y: 1.5, w: 5, h: 2.0, fontFace: F.head,
    fontSize: 110, bold: true, color: P.amber, margin: 0 });
  s.addText("bytes per cache line on Apple Silicon", {
    x: M, y: 3.5, w: 6.6, h: 0.9, fontFace: F.head, fontSize: 26,
    bold: true, color: P.paper, margin: 0 });
  s.addText("Most x86-64 parts use 64. A struct padded with alignas(64) is correct on the lab machines and silently broken on an M-series Mac — two counters still share a line, and no unit test will ever notice.", {
    x: M, y: 4.6, w: 6.6, h: 1.4, fontFace: F.body, fontSize: 16,
    color: "C6D2DC", margin: 0 });
  codeBox(s, [
    "$ ./bin/03_false_sharing --sweep",
    "",
    " stride    wall(s)    Mupd/s",
    " -----------------------------",
    " 8          2.041      39.2",
    " 16         1.988      40.2",
    " 32         1.502      53.3",
    " 64         0.883      90.6",
    " 128        0.121     661.2  <--",
    " 256        0.119     672.3",
    "",
    " ^ the line size, measured",
  ], { x: 7.9, y: 1.6, w: 4.75, h: 4.6, size: 11 });
  s.addText("Don't hard-code it. Measure it.", { x: M, y: 6.25, w: 6.6, h: 0.45,
    fontFace: F.head, fontSize: 20, bold: true, color: P.teal, margin: 0 });
  note(s, "The stride sweep takes about ten seconds and is the most convincing thing in the demo.");
}

/* =============================================================== 11 */
{
  const s = lightSlide("Processor affinity: two different philosophies", "demo 04");
  card(s, { x: M, y: 1.8, w: 5.8, h: 4.1, fill: P.mist });
  s.addText("Linux — control", { x: M + 0.35, y: 2.05, w: 5.1, h: 0.4,
    fontFace: F.head, fontSize: 20, bold: true, color: P.teal, margin: 0 });
  codeBox(s, [
    "cpu_set_t set;",
    "CPU_ZERO(&set);",
    "CPU_SET(3, &set);",
    "pthread_attr_setaffinity_np(",
    "     &attr, sizeof(set), &set);",
  ], { x: M + 0.35, y: 2.55, w: 5.1, h: 1.65, size: 11 });
  s.addText([
    { text: "Hard pinning. That thread runs on CPU 3 and nowhere else.", options: { breakLine: true } },
    { text: "Migrations → 0, caches stay warm, variance drops.", options: { breakLine: true } },
    { text: "You can also get it badly wrong.", options: { bold: true } },
  ], { x: M + 0.35, y: 4.35, w: 5.1, h: 1.4, fontFace: F.body, fontSize: 14,
       color: P.ink2, lineSpacing: 20, margin: 0 });

  card(s, { x: M + 6.1, y: 1.8, w: 5.8, h: 4.1, fill: P.mist });
  s.addText("Apple Silicon — influence", { x: M + 6.45, y: 2.05, w: 5.1, h: 0.4,
    fontFace: F.head, fontSize: 20, bold: true, color: P.amber, margin: 0 });
  codeBox(s, [
    "// no pthread_setaffinity_np here",
    "pthread_attr_set_qos_class_np(",
    "   &attr, QOS_CLASS_USER_INITIATED, 0);",
    "   // -> P cluster",
    "   //    QOS_CLASS_BACKGROUND -> E only",
  ], { x: M + 6.45, y: 2.55, w: 5.1, h: 1.65, size: 10 });
  s.addText([
    { text: "THREAD_AFFINITY_POLICY still compiles and is refused by the kernel.", options: { breakLine: true } },
    { text: "You state intent; the scheduler decides.", options: { breakLine: true } },
    { text: "You cannot get it badly wrong either.", options: { bold: true } },
  ], { x: M + 6.45, y: 4.35, w: 5.1, h: 1.4, fontFace: F.body, fontSize: 14,
       color: P.ink2, lineSpacing: 20, margin: 0 });

  s.addText("Portable code must not depend on either. Ask for what you need; measure what you got.", {
    x: M, y: 6.15, w: 11.9, h: 0.45, fontFace: F.body, fontSize: 15,
    bold: true, color: P.ink2, margin: 0 });
  note(s, "Live: ./bin/04_affinity -s 1.0. The kern_return_t refusal prints first.");
}

/* =============================================================== 12 */
{
  const s = lightSlide("A core is not a core", "demo 04");
  s.addChart(pres.ChartType.bar, [
    { name: "per-thread throughput (Miter/s)",
      labels: ["1 thr, P cluster", "1 thr, E cluster", "4 thr on P", "4 thr on E", "8 thr default"],
      values: [100, 34, 96, 33, 62] },
  ], {
    x: M, y: 1.85, w: 7.5, h: 4.0, barDir: "col",
    showTitle: true, title: "Relative per-thread throughput — expected shape",
    titleFontFace: F.body, titleFontSize: 13, titleColor: P.muted,
    chartColors: [P.teal, P.amber, P.teal, P.amber, P.ink2],
    varyColors: true, showLegend: false,
    showValue: true, dataLabelPosition: "outEnd", dataLabelFontFace: F.body,
    dataLabelFontSize: 11, dataLabelColor: P.ink2,
    catAxisLabelColor: P.muted, valAxisLabelColor: P.muted,
    catAxisLabelFontFace: F.body, catAxisLabelFontSize: 10,
    valAxisLabelFontFace: F.body, valAxisLabelFontSize: 10,
    valGridLine: { color: "DDE3E8", size: 1 }, catGridLine: { style: "none" },
    valAxisMinVal: 0, valAxisMaxVal: 110,
  });
  s.addText("Illustrative shape, not measured data.", { x: M, y: 5.95, w: 7.5, h: 0.3,
    fontFace: F.body, fontSize: 11, italic: true, color: P.muted, margin: 0 });

  const pts = [
    ["The P/E ratio is roughly 3×.", "A hot thread that lands on an efficiency core costs you that — and nothing in your code is told."],
    ["Default QoS lands in between.", "The scheduler spreads across both clusters, so the average thread is neither fast nor slow."],
    ["Oversubscription adds nothing.", "For a compute-bound kernel, 16 threads on 8 cores halves every thread and moves the total not at all."],
  ];
  pts.forEach(([h, b], i) => {
    const y = 1.95 + i * 1.4;
    s.addText(h, { x: 8.6, y, w: 4.2, h: 0.6, fontFace: F.head, fontSize: 16,
      bold: true, color: P.ink, margin: 0 });
    s.addText(b, { x: 8.6, y: y + 0.55, w: 4.2, h: 0.85, fontFace: F.body,
      fontSize: 13, color: P.muted, margin: 0 });
  });
  note(s, "Ask: which of these would a naive OpenMP num_threads(8) give you?");
}

/* =============================================================== 13 */
{
  const s = lightSlide("The clock will not sit still", "demo 05");
  s.addChart(pres.ChartType.line, [
    { name: "single-thread throughput",
      labels: ["1","4","8","12","16","20","24","28","32","36","40"],
      values: [100, 100.4, 99.8, 98.1, 95.2, 92.6, 90.9, 89.8, 89.3, 89.1, 89.0] },
  ], {
    x: M, y: 1.8, w: 7.5, h: 3.6,
    showTitle: true, title: "Sustained load: throughput (%) vs seconds — expected shape",
    titleFontFace: F.body, titleFontSize: 13, titleColor: P.muted,
    chartColors: [P.amber], lineSize: 3, lineDataSymbol: "none",
    showLegend: false,
    catAxisLabelColor: P.muted, valAxisLabelColor: P.muted,
    catAxisLabelFontFace: F.body, valAxisLabelFontFace: F.body,
    catAxisLabelFontSize: 11, valAxisLabelFontSize: 11,
    valGridLine: { color: "DDE3E8", size: 1 }, catGridLine: { style: "none" },
    valAxisMinVal: 80, valAxisMaxVal: 105,
  });
  s.addText("Illustrative shape, not measured data. A fanless MacBook Air shows this far more sharply than a MacBook Pro.", {
    x: M, y: 5.5, w: 7.5, h: 0.5, fontFace: F.body, fontSize: 11,
    italic: true, color: P.muted, margin: 0 });

  s.addText("Two independent effects", { x: 8.6, y: 1.9, w: 4.2, h: 0.4,
    fontFace: F.head, fontSize: 19, bold: true, color: P.ink, margin: 0 });
  s.addText([
    { text: "Over time — ", options: { bold: true } },
    { text: "the boost budget is spent and the die heats up. Same thread, same code, slower.", options: { breakLine: true } },
    { text: "", options: { breakLine: true } },
    { text: "Over cores — ", options: { bold: true } },
    { text: "waking more cores lowers the frequency every core can hold. Your 8-thread run is not 8 copies of your 1-thread run.", options: { breakLine: true } },
    { text: "", options: { breakLine: true } },
    { text: "Both look like \"my parallel code scaled badly\" in wall time alone.", options: { bold: true, color: P.rust } },
  ], { x: 8.6, y: 2.4, w: 4.2, h: 3.4, fontFace: F.body, fontSize: 13.5,
       color: P.ink2, lineSpacing: 18, margin: 0 });
  note(s, "Run ./bin/05_frequency -d 30 with sudo ./scripts/mac_powermetrics.sh in a second window.");
}

/* =============================================================== 14 */
{
  const s = lightSlide("\"Overclocking\", then and now", "demo 05");
  card(s, { x: M, y: 1.85, w: 5.8, h: 3.9, fill: P.mist });
  s.addText("Then", { x: M + 0.35, y: 2.1, w: 5.1, h: 0.4, fontFace: F.head,
    fontSize: 22, bold: true, color: P.muted, margin: 0 });
  s.addText([
    { text: "A fixed clock with headroom left on the table.", options: { breakLine: true } },
    { text: "Raise the multiplier, add voltage, add cooling, gain 20–30%.", options: { breakLine: true } },
    { text: "The chip did exactly what the multiplier said.", options: { breakLine: true } },
    { text: "The risk was yours: instability, heat, a dead part.", options: {} },
  ], { x: M + 0.35, y: 2.6, w: 5.1, h: 3.0, fontFace: F.body, fontSize: 15,
       color: P.ink2, lineSpacing: 22, margin: 0 });

  card(s, { x: M + 6.1, y: 1.85, w: 5.8, h: 3.9, fill: P.ink });
  s.addText("Now", { x: M + 6.45, y: 2.1, w: 5.1, h: 0.4, fontFace: F.head,
    fontSize: 22, bold: true, color: P.amber, margin: 0 });
  s.addText([
    { text: "A closed control loop over power, temperature and active-core count.", options: { breakLine: true } },
    { text: "The part already runs as fast as those limits allow, millisecond by millisecond.", options: { breakLine: true } },
    { text: "x86 desktop: firmware still exposes multipliers; the OS can toggle turbo.", options: { breakLine: true } },
    { text: "Apple Silicon: no multiplier, no voltage offset, no BIOS. Nothing to raise.", options: { bold: true } },
  ], { x: M + 6.45, y: 2.6, w: 5.1, h: 3.0, fontFace: F.body, fontSize: 15,
       color: "C6D2DC", lineSpacing: 22, margin: 0 });

  s.addText("So the question changed: not \"how fast can I make it go\" but \"what did it actually run at, and what made it choose that\" — and that one you can answer, on any machine, with counters.", {
    x: M, y: 6.0, w: 11.9, h: 0.7, fontFace: F.body, fontSize: 16,
    bold: true, color: P.ink2, margin: 0 });
  note(s, "The safe A/B on Linux is scripts/linux_freq.sh: change the governor's target, measure, restore. It never raises a vendor limit.");
}

/* =============================================================== 15 */
{
  const s = lightSlide("The safe experiment on a lab machine", "hands on");
  codeBox(s, [
    "$ ./scripts/linux_freq.sh show          # governor, driver, limits, turbo state",
    "",
    "$ sudo ./scripts/linux_freq.sh ab ./bin/02_scaling -s 1.0 -t 1",
    "",
    "   governor=performance  boost=1     -> cycles/ref-cycles = 1.42",
    "   governor=performance  boost=0     -> cycles/ref-cycles = 1.00",
    "   governor=powersave    boost=1     -> cycles/ref-cycles = 0.61",
    "",
    "   (restores your original governor on exit)",
  ], { x: M, y: 1.8, w: 11.9, h: 2.9, size: 12.5 });

  const pts = [
    ["We change the target, never the limit.", P.teal],
    ["cycles / ref-cycles is the real multiplier — read it, don't assume it.", P.amber],
    ["Firmware-level overclocking voids warranties and has no place on a shared machine.", P.rust],
  ];
  pts.forEach(([t, c], i) => {
    const y = 5.05 + i * 0.55;
    s.addShape(pres.ShapeType.ellipse, { x: M, y: y + 0.09, w: 0.16, h: 0.16,
      fill: { color: c }, line: { color: c } });
    s.addText(t, { x: M + 0.34, y, w: 11.4, h: 0.4, fontFace: F.body,
      fontSize: 15, color: P.ink2, margin: 0 });
  });
  note(s, "On the Mac the equivalent A/B is Low Power Mode on/off, plus battery vs mains.");
}

/* =============================================================== 16 */
{
  const s = lightSlide("Run it yourself", "the lab");
  codeBox(s, [
    "$ make",
    "$ ./scripts/sysinfo.sh          # what machine am I on?",
    "$ ./scripts/run_all.sh          # every demo, CSVs into results/",
  ], { x: M, y: 1.75, w: 11.9, h: 1.35, size: 14 });

  const demos = [
    ["01_hello", "create/join, a real race, the mutex fix"],
    ["02_scaling", "speedup, efficiency, IPC, effective clock"],
    ["03_false_sharing", "packed vs padded, and a stride sweep that finds your cache line"],
    ["04_affinity", "P vs E cores on macOS; pinning and SMT on Linux"],
    ["05_frequency", "sustained load, DVFS, and what you may actually change"],
  ];
  demos.forEach(([n, d], i) => {
    const y = 3.35 + i * 0.62;
    s.addText(n, { x: M, y, w: 2.6, h: 0.4, fontFace: F.code, fontSize: 13,
      bold: true, color: P.teal, margin: 0 });
    s.addText(d, { x: M + 2.8, y, w: 9.1, h: 0.4, fontFace: F.body,
      fontSize: 14, color: P.ink2, margin: 0 });
  });
  s.addText("Deliverable: filled-in tables, answers to Q1–Q9, one speedup plot, and two paragraphs on the largest loss you measured. Compare with someone on a different machine — the shapes should agree even where the numbers don't.", {
    x: M, y: 6.45, w: 11.9, h: 0.7, fontFace: F.body, fontSize: 13.5,
    color: P.muted, margin: 0 });
  note(s, "Handout: handout/PDC-Lab01-Handout.md");
}

/* =============================================================== 17 */
{
  const s = darkSlide();
  s.addText("What to carry forward", { x: M, y: 0.9, w: 10, h: 0.7,
    fontFace: F.head, fontSize: 34, bold: true, color: P.paper, margin: 0 });
  const takeaways = [
    ["Measure, don't assume.", "Wall time says something is slow. Counters say what kind of slow."],
    ["The unit of sharing is a cache line.", "Not a variable. Padding is a correctness concern for performance."],
    ["A logical CPU is a scheduling promise, not a throughput promise.", "P-cores, E-cores and SMT siblings all count as \"a CPU\"."],
    ["Frequency is an output, not an input.", "The chip picks it from power, heat and how many cores are awake."],
    ["Portability is about what you ask for, not what you get.", "State intent, then verify — on both operating systems."],
  ];
  takeaways.forEach(([h, b], i) => {
    const y = 1.85 + i * 1.02;
    numDot(s, i + 1, M, y + 0.02, [P.teal, P.amber, P.teal, P.amber, P.teal][i]);
    s.addText(h, { x: M + 0.68, y: y - 0.06, w: 11.2, h: 0.4, fontFace: F.head,
      fontSize: 18, bold: true, color: P.paper, margin: 0 });
    s.addText(b, { x: M + 0.68, y: y + 0.34, w: 11.2, h: 0.4, fontFace: F.body,
      fontSize: 13.5, color: "9FB0BF", margin: 0 });
  });
  s.addText("Next: OpenMP — the same measurements, a fraction of the code.", {
    x: M, y: 6.88, w: 11.2, h: 0.4, fontFace: F.body, fontSize: 15,
    italic: true, color: P.amber, margin: 0 });
  note(s, "Close by returning to the three questions from slide 2.");
}

pres.writeFile({ fileName: "03_Pthreads_and_Performance_Measurement.pptx" })
    .then(f => console.log("wrote", f));
