/*
 * Demo 05 -- clock speed is not a constant.
 *
 * Part A  Sustained single-thread load, sampled over time. You see the
 *         boost clock at the start and, on a fanless or loaded machine,
 *         the slow decay as the power/thermal budget runs out.
 * Part B  Per-thread throughput as thread count rises. Adding cores costs
 *         clock: the same core runs slower when its neighbours are busy.
 * Part C  What the OS will and will not let you change.
 *
 * This is the honest version of "overclocking". On a modern chip the
 * frequency is a closed-loop function of power, temperature and how many
 * cores are active. On Apple Silicon there is no multiplier to raise --
 * so the measurable, teachable question is not "how fast can I make it
 * go" but "what is it actually running at, and what made it choose that".
 *
 * Run:  ./bin/05_frequency [-d SECONDS] [-t MAXTHREADS] [--csv]
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
    double   wall;
    double   result;
} farg_t;

static void *worker(void *p)
{
    farg_t *a = (farg_t *)p;
    double t0 = pdc_wall_sec();
    a->result = pdc_kernel(a->iters, a->seed);
    a->wall   = pdc_wall_sec() - t0;
    return NULL;
}

static double run_n(int n, uint64_t per, double *per_thread_avg)
{
    pthread_t *th = calloc((size_t)n, sizeof(pthread_t));
    farg_t    *fa = calloc((size_t)n, sizeof(farg_t));
    double t0 = pdc_wall_sec();
    for (int i = 0; i < n; i++) {
        fa[i].iters = per;
        fa[i].seed  = 0x9E3779B97F4A7C15ULL * (uint64_t)(i + 1);
        pthread_create(&th[i], NULL, worker, &fa[i]);
    }
    for (int i = 0; i < n; i++) pthread_join(th[i], NULL);
    double wall = pdc_wall_sec() - t0;
    volatile double sink = 0.0;
    double acc = 0.0;
    for (int i = 0; i < n; i++) {
        sink += fa[i].result;
        acc  += (double)per / fa[i].wall / 1e6;
    }
    (void)sink;
    if (per_thread_avg) *per_thread_avg = acc / (double)n;
    free(th); free(fa);
    return wall;
}

#ifdef PDC_LINUX
static double cur_mhz(void)
{
    FILE *f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", "r");
    if (f) { double khz = 0; if (fscanf(f, "%lf", &khz) == 1) { fclose(f); return khz / 1000.0; } fclose(f); }
    f = fopen("/proc/cpuinfo", "r");
    if (!f) return 0.0;
    char buf[256];
    while (fgets(buf, sizeof(buf), f)) {
        if (strncmp(buf, "cpu MHz", 7) == 0) {
            char *c = strchr(buf, ':');
            if (c) { double v = atof(c + 1); fclose(f); return v; }
        }
    }
    fclose(f);
    return 0.0;
}
#endif

int main(int argc, char **argv)
{
    pdc_bind_stdout();
    pdc_topo_t topo;
    pdc_topology(&topo);
    double dur  = pdc_arg_dbl(argc, argv, "-d", 20.0);
    int    maxt = pdc_arg_int(argc, argv, "-t", topo.total_logical);
    int    csv  = pdc_arg_flag(argc, argv, "--csv");
    if (maxt < 1) maxt = 1;

    if (!csv) {
        printf("\n=== Demo 05: frequency, DVFS and the truth about overclocking ====\n\n");
        pdc_topology_print(&topo);
    }

    /* one table row per ~1 second of load */
    uint64_t chunk = pdc_calibrate(1.0);

    /* ---------------- Part A: sustained load over time ---------------- */
    if (csv) printf("part,x,v1,v2,v3\n");  /* A: t_s,Mitr/s,rel,MHz  B: threads,agg,per-thread,rel */
    else {
        printf("\n  Part A -- one thread, %.0f s of sustained load\n\n", dur);
        printf("  %8s %12s %10s", "t (s)", "Mitr/s", "vs start");
#ifdef PDC_LINUX
        printf(" %10s", "cpu0 MHz");
#endif
        printf("\n  "); pdc_rule(46);
    }
    double t_start = pdc_wall_sec(), first = 0.0;
    while (pdc_wall_sec() - t_start < dur) {
        double t0 = pdc_wall_sec();
        volatile double s = pdc_kernel(chunk, 99);
        (void)s;
        double w = pdc_wall_sec() - t0;
        double m = (double)chunk / w / 1e6;
        if (first == 0.0) first = m;
        double mhz = 0.0;
#ifdef PDC_LINUX
        mhz = cur_mhz();
#endif
        if (csv) printf("A,%.2f,%.3f,%.4f,%.1f\n",
                        pdc_wall_sec() - t_start, m, m / first, mhz);
        else {
            printf("  %8.1f %12.1f %9.1f%%",
                   pdc_wall_sec() - t_start, m, 100.0 * m / first);
#ifdef PDC_LINUX
            printf(" %10.0f", mhz);
#endif
            printf("\n");
        }
    }

    /* ---------------- Part B: clock vs active core count --------------- */
    uint64_t per = pdc_calibrate(0.6);
    if (!csv) {
        printf("\n  Part B -- per-thread throughput as more cores wake up\n\n");
        printf("  %-8s %12s %14s %10s\n", "threads", "agg Mitr/s",
               "per-thread", "vs 1 thr");
        printf("  "); pdc_rule(50);
    }
    double solo = 0.0;
    for (int n = 1; n <= maxt; n++) {
        double pt = 0.0;
        double wall = run_n(n, per, &pt);
        double agg = (double)per * (double)n / wall / 1e6;
        if (n == 1) solo = pt;
        if (csv) printf("B,%d,%.3f,%.3f,%.4f\n", n, agg, pt, pt / solo);
        else printf("  %-8d %12.1f %14.1f %9.1f%%\n",
                    n, agg, pt, 100.0 * pt / solo);
    }

    if (csv) return 0;

    /* ---------------- Part C: what you can actually change ------------- */
    printf("\n  Part C -- what the OS lets you change\n\n");
#ifdef PDC_MACOS
    printf("    Apple Silicon has NO user-accessible frequency control:\n");
    printf("      * no multiplier, no voltage offset, no BIOS, no XTU/Ryzen Master\n");
    printf("      * the only OS-level knob is Low Power Mode / High Power Mode\n");
    printf("        (System Settings > Battery), and on laptops the AC/battery state\n");
    printf("      * frequency is chosen by hardware from power, temperature and the\n");
    printf("        number of active cores -- exactly what Part A and B just measured\n\n");
    printf("    To watch the hardware side of the same experiment:\n");
    printf("      sudo powermetrics --samplers cpu_power -i 1000 -n 20\n");
    printf("      (or ./scripts/mac_powermetrics.sh, which tabulates it for you)\n\n");
    printf("    For real cycle/instruction counters, use Instruments:\n");
    printf("      ./scripts/mac_xctrace.sh ./bin/02_scaling -t 8\n\n");
#endif
#ifdef PDC_LINUX
    printf("    On this Linux box you DO have knobs (root required):\n");
    printf("      cpupower frequency-info                  # governor, min/max, driver\n");
    printf("      sudo cpupower frequency-set -g performance\n");
    printf("      sudo cpupower frequency-set -g powersave\n");
    printf("      # Intel turbo off/on:\n");
    printf("      echo 1 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo\n");
    printf("      # AMD / acpi-cpufreq:\n");
    printf("      echo 0 | sudo tee /sys/devices/system/cpu/cpufreq/boost\n");
    printf("      See scripts/linux_freq.sh -- it runs the A/B for you.\n\n");
    printf("    Measure the effect, do not trust the label:\n");
    printf("      perf stat -e cycles,ref-cycles ./bin/02_scaling -t 1\n");
    printf("      cycles/ref-cycles is the real multiplier over the base clock.\n\n");
#endif
    printf("    The lesson: 'overclocking' as a manual multiplier is a 2005 idea.\n");
    printf("    Modern parts already run as fast as power and heat allow, and back\n");
    printf("    off automatically. The engineering question moved from 'raise the\n");
    printf("    clock' to 'do more work per cycle' -- which is what IPC, cache\n");
    printf("    behaviour and parallelism are for.\n\n");
    return 0;
}
