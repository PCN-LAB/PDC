/*
 * Demo 02 -- strong scaling, measured properly.
 *
 * Fixed total work, split across 1..T threads. Reports wall time, speedup,
 * parallel efficiency, and -- where the OS lets us read the PMU -- cycles,
 * instructions, IPC and the effective clock the cores actually ran at.
 *
 * Run:  ./bin/02_scaling [-t MAXTHREADS] [-s SECONDS] [-r REPS] [--csv]
 *
 *   -s  target single-thread run time in seconds (default 1.0); the work
 *       size is calibrated to this machine so the demo takes the same
 *       wall time on an M1 as on a lab Xeon.
 */
#include "common.h"
#include "plat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <inttypes.h>

typedef struct {
    uint64_t iters;
    uint64_t seed;
    double   result;
    double   cpu_sec;
    double   wall_sec;
} task_t;

static void *worker(void *p)
{
    task_t *t = (task_t *)p;
    double t0 = pdc_wall_sec();
    t->result   = pdc_kernel(t->iters, t->seed);
    t->wall_sec = pdc_wall_sec() - t0;
    t->cpu_sec  = pdc_thread_cpu_sec();
    return NULL;
}

/* One measured run at `n` threads over `total` kernel iterations. */
static void run_once(int n, uint64_t total, pdc_result_t *out, int use_ctrs)
{
    pthread_t *th = calloc((size_t)n, sizeof(pthread_t));
    task_t    *ta = calloc((size_t)n, sizeof(task_t));
    uint64_t   per = total / (uint64_t)n;

    pdc_ctrs_t c;
    int have = 0;
    if (use_ctrs) have = (pdc_ctrs_open(&c) == 0);

    double cpu_before = pdc_proc_cpu_sec();
    if (have) pdc_ctrs_start(&c);
    double t0 = pdc_wall_sec();

    for (int i = 0; i < n; i++) {
        ta[i].iters = (i == n - 1) ? total - per * (uint64_t)(n - 1) : per;
        ta[i].seed  = 0x9E3779B97F4A7C15ULL * (uint64_t)(i + 1);
        pthread_create(&th[i], NULL, worker, &ta[i]);
    }
    for (int i = 0; i < n; i++) pthread_join(th[i], NULL);

    double wall = pdc_wall_sec() - t0;
    if (have) pdc_ctrs_stop(&c);
    double cpu = pdc_proc_cpu_sec() - cpu_before;

    volatile double sink = 0.0;
    for (int i = 0; i < n; i++) sink += ta[i].result;
    (void)sink;

    memset(out, 0, sizeof(*out));
    out->threads    = n;
    out->wall       = wall;
    out->cpu        = cpu;
    out->throughput = (double)total / wall / 1e6;
    out->have_ctrs  = have;
    if (have) {
        out->cycles        = c.cycles;
        out->instructions  = c.instructions;
        out->cache_misses  = c.cache_misses;
        out->branch_misses = c.branch_misses;
        pdc_ctrs_close(&c);
    }
    free(th); free(ta);
}

int main(int argc, char **argv)
{
    pdc_bind_stdout();
    pdc_topo_t topo;
    pdc_topology(&topo);

    int    maxt   = pdc_arg_int(argc, argv, "-t", topo.total_logical);
    double target = pdc_arg_dbl(argc, argv, "-s", 1.0);
    int    reps   = pdc_arg_int(argc, argv, "-r", 3);
    int    csv    = pdc_arg_flag(argc, argv, "--csv");
    if (maxt < 1) maxt = 1;
    if (reps < 1) reps = 1;

    /* probe once so we know whether the counter columns mean anything */
    pdc_ctrs_t probe;
    int ctrs_ok = (pdc_ctrs_open(&probe) == 0);
    const char *ctrs_why = probe.why;
    if (ctrs_ok) pdc_ctrs_close(&probe);

    if (!csv) {
        printf("\n=== Demo 02: strong scaling ======================================\n\n");
        pdc_topology_print(&topo);
    }

    uint64_t total = pdc_calibrate(target);
    if (!csv) {
        printf("\n  work size        : %" PRIu64 " kernel iterations "
               "(~%.1f s single-threaded)\n", total, target);
        printf("  repetitions      : %d (median reported)\n", reps);
        printf("  HW counters      : %s\n", ctrs_ok ? "available (perf_event_open)"
                                                    : "not available");
        if (!ctrs_ok) printf("                     %s\n", ctrs_why ? ctrs_why : "");
        printf("\n");
    }

    double base = 0.0;
    if (csv)
        printf("threads,wall_s,cpu_s,speedup,efficiency,mitr_per_s,"
               "cycles,instructions,ipc,eff_ghz\n");

    if (!csv) {
        printf("  %-8s %9s %9s %9s %7s %10s", "threads", "wall(s)", "cpu(s)",
               "speedup", "eff%", "Mitr/s");
        if (ctrs_ok) printf(" %8s %9s", "IPC", "GHz(eff)");
        printf("\n  ");
        pdc_rule(ctrs_ok ? 80 : 60);
    }

    for (int n = 1; n <= maxt; n++) {
        double  walls[32];
        pdc_result_t best;
        int r = reps > 32 ? 32 : reps;
        pdc_result_t keep;
        memset(&keep, 0, sizeof(keep));
        for (int k = 0; k < r; k++) {
            run_once(n, total, &best, ctrs_ok);
            walls[k] = best.wall;
            if (k == 0 || best.wall < keep.wall) keep = best;
        }
        double wall = pdc_median(walls, r);
        /* report the median wall time with the counters of the fastest run */
        double speedup = (n == 1) ? 1.0 : base / wall;
        if (n == 1) { base = wall; speedup = 1.0; }
        double eff = 100.0 * speedup / (double)n;
        double mitr = (double)total / wall / 1e6;

        double ipc = 0.0, ghz = 0.0;
        if (keep.have_ctrs && keep.cycles > 0) {
            ipc = (double)keep.instructions / (double)keep.cycles;
            ghz = (double)keep.cycles / keep.cpu / 1e9;
        }

        if (csv) {
            printf("%d,%.6f,%.6f,%.4f,%.2f,%.3f,%" PRIu64 ",%" PRIu64
                   ",%.4f,%.4f\n",
                   n, wall, keep.cpu, speedup, eff, mitr,
                   keep.cycles, keep.instructions, ipc, ghz);
        } else {
            printf("  %-8d %9.3f %9.3f %9.2f %7.1f %10.1f", n, wall, keep.cpu,
                   speedup, eff, mitr);
            if (ctrs_ok) printf(" %8.2f %9.2f", ipc, ghz);
            printf("\n");
        }
    }

    if (!csv) {
        printf("\n  How to read this table:\n");
        printf("    speedup    = T(1)/T(n).  Ideal is n. Amdahl's law is the ceiling.\n");
        printf("    eff%%       = speedup/n.  Below ~70%% something is being wasted.\n");
        if (ctrs_ok) {
            printf("    IPC        = instructions per cycle. Stays flat while scaling is\n");
            printf("                 healthy; a drop means stalls (memory, contention).\n");
            printf("    GHz(eff)   = cycles / CPU-seconds -- the clock the cores REALLY\n");
            printf("                 ran at. Watch it fall as thread count rises: that is\n");
            printf("                 the power/thermal wall, not a software problem.\n");
        } else if (topo.heterogeneous) {
            printf("    Mitr/s     = throughput. Because %d of the %d cores are %s cores,\n",
                   topo.eff_cores, topo.total_logical, topo.eff_name);
            printf("                 the last threads each add far less than the first ones.\n");
            printf("                 That bend in the curve is heterogeneity, not overhead.\n");
        }
        printf("\n  Run scripts/mac_powermetrics.sh in a second terminal to watch\n");
        printf("  cluster frequency and power while this table is being produced.\n\n");
    }
    return 0;
}
