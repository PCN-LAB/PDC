/*
 * Demo 04 -- processor affinity on a heterogeneous machine.
 *
 * Every thread does exactly the SAME amount of work, so per-thread
 * throughput is a direct measurement of the core it landed on.
 *
 * macOS / Apple Silicon
 *   There is no pthread_setaffinity_np, and the old Mach affinity-tag API
 *   is refused by the kernel. Placement is a *hint* expressed as a QoS
 *   class: QOS_CLASS_USER_INITIATED runs on the P cluster, QOS_CLASS_
 *   BACKGROUND is confined to the E cluster. The demo proves both:
 *   it tries the affinity tag and prints the kernel's refusal, then shows
 *   the P/E throughput ratio you get from QoS.
 *
 * Linux
 *   Real hard pinning with sched_setaffinity. The demo pins one thread per
 *   logical CPU, then two threads onto SMT siblings of the SAME physical
 *   core, then oversubscribes one CPU -- three very different outcomes.
 *
 * Run:  ./bin/04_affinity [-s SECONDS] [--csv]
 */
#include "common.h"
#include "plat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <inttypes.h>

#ifdef PDC_MACOS
extern int pdc_macos_try_affinity_tag(int tag);
#endif

typedef struct {
    uint64_t    iters;
    uint64_t    seed;
    pdc_place_t place;
    int         cpu;
    int         placed_ok;
    double      wall;
    double      cpu_sec;
    double      result;
} targ_t;

static void *worker(void *p)
{
    targ_t *a = (targ_t *)p;
    /* Re-assert placement from inside the thread as well: on macOS the
     * attribute already did it, on Linux this covers PLACE_PIN when the
     * attribute path was not used. */
    if (a->place != PLACE_DEFAULT) {
        int r = pdc_place_self(a->place, a->cpu);
        if (r == 0) a->placed_ok = 1;
    }
    double t0 = pdc_wall_sec();
    a->result  = pdc_kernel(a->iters, a->seed);
    a->wall    = pdc_wall_sec() - t0;
    a->cpu_sec = pdc_thread_cpu_sec();
    return NULL;
}

typedef struct {
    double agg_mitr;      /* aggregate throughput, million iters/s        */
    double per_min, per_max, per_avg;   /* per-thread throughput          */
    double wall;
    int    n;
    int    placed_ok;
} group_t;

/* cpus may be NULL (not pinning); otherwise cpus[i] is the CPU for thread i */
static group_t run_group(int n, pdc_place_t place, const int *cpus,
                         uint64_t iters_per_thread)
{
    pthread_t *th = calloc((size_t)n, sizeof(pthread_t));
    targ_t    *ta = calloc((size_t)n, sizeof(targ_t));
    group_t    g;
    memset(&g, 0, sizeof(g));
    g.n = n;

    double t0 = pdc_wall_sec();
    for (int i = 0; i < n; i++) {
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        ta[i].iters = iters_per_thread;
        ta[i].seed  = 0x9E3779B97F4A7C15ULL * (uint64_t)(i + 1);
        ta[i].place = place;
        ta[i].cpu   = cpus ? cpus[i] : 0;
        pdc_place_attr(&attr, place, ta[i].cpu);
        pthread_create(&th[i], &attr, worker, &ta[i]);
        pthread_attr_destroy(&attr);
    }
    for (int i = 0; i < n; i++) pthread_join(th[i], NULL);
    g.wall = pdc_wall_sec() - t0;

    volatile double sink = 0.0;
    g.per_min = 1e30; g.per_max = 0.0;
    for (int i = 0; i < n; i++) {
        sink += ta[i].result;
        double m = (double)iters_per_thread / ta[i].wall / 1e6;
        if (m < g.per_min) g.per_min = m;
        if (m > g.per_max) g.per_max = m;
        g.per_avg += m;
        g.placed_ok += ta[i].placed_ok;
    }
    (void)sink;
    g.per_avg /= (double)n;
    g.agg_mitr = (double)iters_per_thread * (double)n / g.wall / 1e6;

    free(th); free(ta);
    return g;
}

static void row(const char *label, group_t g, int csv)
{
    if (csv)
        printf("\"%s\",%d,%.6f,%.3f,%.3f,%.3f,%.3f\n", label, g.n, g.wall,
               g.agg_mitr, g.per_avg, g.per_min, g.per_max);
    else
        printf("  %-38s %3d %9.3f %10.1f %10.1f  %6.1f/%-6.1f\n",
               label, g.n, g.wall, g.agg_mitr, g.per_avg, g.per_min, g.per_max);
}

#ifdef PDC_LINUX
/* first two entries of cpu0's SMT sibling list, e.g. "0,8" or "0-1" */
static int smt_siblings(int *a, int *b)
{
    FILE *f = fopen("/sys/devices/system/cpu/cpu0/topology/thread_siblings_list", "r");
    if (!f) return -1;
    char buf[128] = {0};
    if (!fgets(buf, sizeof(buf), f)) { fclose(f); return -1; }
    fclose(f);
    int x = -1, y = -1;
    if (sscanf(buf, "%d,%d", &x, &y) == 2 || sscanf(buf, "%d-%d", &x, &y) == 2) {
        if (x != y) { *a = x; *b = y; return 0; }
    }
    return -1;
}
#endif

int main(int argc, char **argv)
{
    pdc_bind_stdout();
    pdc_topo_t topo;
    pdc_topology(&topo);
    double target = pdc_arg_dbl(argc, argv, "-s", 1.0);
    int    csv    = pdc_arg_flag(argc, argv, "--csv");

    if (!csv) {
        printf("\n=== Demo 04: processor affinity ==================================\n\n");
        pdc_topology_print(&topo);
        printf("\n  %s\n\n", pdc_place_note());
    }

    uint64_t per = pdc_calibrate(target);
    if (!csv)
        printf("  work per thread  : %" PRIu64 " kernel iterations\n\n", per);

    if (csv) printf("scenario,threads,wall_s,agg_mitr,per_thread_avg,min,max\n");
    else {
        printf("  %-38s %3s %9s %10s %10s %13s\n",
               "scenario", "n", "wall(s)", "agg Mitr/s", "per-thr", "min/max");
        printf("  "); pdc_rule(88);
    }

#ifdef PDC_MACOS
    /* 1. Show the kernel refusing the classic affinity API. */
    int kr = pdc_macos_try_affinity_tag(1);
    if (!csv) {
        printf("  thread_policy_set(THREAD_AFFINITY_POLICY) -> kern_return_t %d %s\n",
               kr, kr == 0 ? "(accepted)" : "(REFUSED -- expected on Apple Silicon)");
        printf("  "); pdc_rule(88);
    }

    int np = topo.perf_cores > 0 ? topo.perf_cores : 1;
    int ne = topo.eff_cores  > 0 ? topo.eff_cores  : 1;

    row("1 thread, default QoS",      run_group(1,  PLACE_DEFAULT, NULL, per), csv);
    row("1 thread, P-cluster QoS",    run_group(1,  PLACE_PERF,    NULL, per), csv);
    row("1 thread, E-cluster QoS",    run_group(1,  PLACE_EFF,     NULL, per), csv);
    row("all P-cores, P-cluster QoS", run_group(np, PLACE_PERF,    NULL, per), csv);
    row("all E-cores, E-cluster QoS", run_group(ne, PLACE_EFF,     NULL, per), csv);
    row("all cores, default QoS",     run_group(topo.total_logical,
                                                PLACE_DEFAULT, NULL, per), csv);
    row("oversubscribed 2x, P QoS",   run_group(np * 2, PLACE_PERF, NULL, per), csv);

    if (!csv) {
        printf("\n  Talking points:\n");
        printf("    - The P/E single-thread ratio is the size of the heterogeneity\n");
        printf("      penalty. A scheduler that puts your hot thread on an E core\n");
        printf("      costs you that factor, and your code never sees why.\n");
        printf("    - 'all cores, default' is usually NOT the sum of the two\n");
        printf("      cluster rows: the OS also has to run everything else.\n");
        printf("    - Oversubscription buys nothing for a compute-bound kernel --\n");
        printf("      aggregate throughput stays flat while every thread halves.\n");
        printf("    - macOS gives you influence, not control. Linux gives control.\n");
        printf("      Design portable code that does not depend on either.\n\n");
    }
#endif

#ifdef PDC_LINUX
    int ncpu = topo.total_logical;
    int cpus[512];
    for (int i = 0; i < ncpu && i < 512; i++) cpus[i] = i;

    row("1 thread, unpinned", run_group(1, PLACE_DEFAULT, NULL, per), csv);
    { int c[1] = {0};
      row("1 thread, pinned to cpu0", run_group(1, PLACE_PIN, c, per), csv); }

    int a = -1, b = -1;
    if (smt_siblings(&a, &b) == 0) {
        int same[2] = {a, b};
        row("2 threads, SAME physical core (SMT)",
            run_group(2, PLACE_PIN, same, per), csv);
    }
    if (ncpu >= 2) {
        /* pick the lowest CPU that is not an SMT sibling of cpu0 */
        int other = 1;
        while (other < ncpu && (other == a || other == b)) other++;
        if (other >= ncpu) other = 1;
        int diff[2] = {0, other};
        row("2 threads, DIFFERENT physical cores",
            run_group(2, PLACE_PIN, diff, per), csv);
    }
    { int same[8]; for (int i = 0; i < 8; i++) same[i] = 0;
      int k = ncpu < 4 ? ncpu : 4;
      row("N threads, all pinned to cpu0",
          run_group(k, PLACE_PIN, same, per), csv); }
    row("one thread per logical CPU (pinned)",
        run_group(ncpu, PLACE_PIN, cpus, per), csv);
    row("one thread per logical CPU (unpinned)",
        run_group(ncpu, PLACE_DEFAULT, NULL, per), csv);

    if (!csv) {
        printf("\n  Talking points:\n");
        printf("    - Two threads on SMT siblings share one core's execution units.\n");
        printf("      Per-thread throughput roughly halves. 'Logical CPU' is not a core.\n");
        printf("    - Pinning removes migration and keeps caches warm; the win is\n");
        printf("      usually small but the VARIANCE drops a lot.\n");
        printf("    - Pinning everything to cpu0 is the control experiment: the OS\n");
        printf("      time-slices and aggregate throughput collapses to one core.\n\n");
    }
#endif
    return 0;
}
