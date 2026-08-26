/*
 * Demo 03 -- what sharing actually costs.
 *
 * Same number of increments in every mode. The only thing that changes is
 * *where in memory* the counters live and how they are updated:
 *
 *   private    thread-local variable                   (no sharing)
 *   packed     counters[i], 8 bytes apart              (FALSE sharing)
 *   padded     counters[i * cacheline]                 (no sharing again)
 *   atomic     one shared counter, lock-free add       (true sharing)
 *   mutex      one shared counter behind a pthread lock
 *
 * The packed/padded pair is the money shot: identical code, identical
 * instruction count, and a several-fold difference in run time caused
 * purely by cache-line ownership ping-ponging between cores.
 *
 * Run:  ./bin/03_false_sharing [-t THREADS] [-i ITERS] [--sweep] [--csv]
 */
#include "common.h"
#include "plat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <inttypes.h>

typedef enum { M_PRIVATE, M_PACKED, M_PADDED, M_ATOMIC, M_MUTEX, M_COUNT } mode_t_;

static const char *mode_name[M_COUNT] = {
    "private", "packed(false sharing)", "padded", "atomic(true sharing)", "mutex"
};

typedef struct {
    int              mode;
    long             iters;
    volatile long   *slot;        /* packed / padded target */
    long            *shared;      /* atomic / mutex target  */
    pthread_mutex_t *lock;
    long             priv;
} warg_t;

static void *worker(void *p)
{
    warg_t *a = (warg_t *)p;
    switch (a->mode) {
    case M_PRIVATE: {
        /* volatile so the compiler really emits load-add-store, exactly
         * like the shared modes below. Without it the loop vanishes at -O2
         * and the baseline is meaningless. */
        volatile long local = 0;
        for (long i = 0; i < a->iters; i++) local++;
        a->priv = local;
        break;
    }
    case M_PACKED:
    case M_PADDED:
        for (long i = 0; i < a->iters; i++) (*a->slot)++;
        break;
    case M_ATOMIC:
        for (long i = 0; i < a->iters; i++)
            __atomic_fetch_add(a->shared, 1, __ATOMIC_RELAXED);
        break;
    case M_MUTEX:
        for (long i = 0; i < a->iters; i++) {
            pthread_mutex_lock(a->lock);
            (*a->shared)++;
            pthread_mutex_unlock(a->lock);
        }
        break;
    }
    return NULL;
}

/* stride_bytes is only used by the packed/padded modes */
static double run_mode(int mode, int n, long iters, size_t stride_bytes)
{
    void *raw = NULL;
    size_t bytes = stride_bytes * (size_t)n + 4096;
    if (posix_memalign(&raw, 4096, bytes) != 0) { perror("posix_memalign"); exit(1); }
    memset(raw, 0, bytes);

    long shared = 0;
    pthread_mutex_t lock;
    pthread_mutex_init(&lock, NULL);

    pthread_t *th = calloc((size_t)n, sizeof(pthread_t));
    warg_t    *ar = calloc((size_t)n, sizeof(warg_t));

    for (int i = 0; i < n; i++) {
        ar[i].mode   = mode;
        ar[i].iters  = iters;
        ar[i].slot   = (volatile long *)((char *)raw + stride_bytes * (size_t)i);
        ar[i].shared = &shared;
        ar[i].lock   = &lock;
    }

    double t0 = pdc_wall_sec();
    for (int i = 0; i < n; i++) pthread_create(&th[i], NULL, worker, &ar[i]);
    for (int i = 0; i < n; i++) pthread_join(th[i], NULL);
    double wall = pdc_wall_sec() - t0;

    pthread_mutex_destroy(&lock);
    free(th); free(ar); free(raw);
    return wall;
}

int main(int argc, char **argv)
{
    pdc_bind_stdout();
    pdc_topo_t topo;
    pdc_topology(&topo);

    int  n     = pdc_arg_int(argc, argv, "-t", topo.total_logical > 1
                                               ? topo.total_logical : 2);
    long iters = (long)pdc_arg_dbl(argc, argv, "-i", 20e6);
    int  sweep = pdc_arg_flag(argc, argv, "--sweep");
    int  csv   = pdc_arg_flag(argc, argv, "--csv");
    if (n < 2) n = 2;

    if (!csv) {
        printf("\n=== Demo 03: false sharing and contention ========================\n\n");
        pdc_topology_print(&topo);
        printf("\n  threads          : %d\n", n);
        printf("  increments/thread: %ld  (identical in every mode)\n", iters);
        printf("  assumed line size: %d bytes\n\n", PDC_CACHE_LINE);
    }

    if (sweep) {
        if (csv) printf("stride_bytes,wall_s,mupd_per_s\n");
        else {
            printf("  Stride sweep -- packed layout, stride grows from 8 bytes to 1 KiB.\n");
            printf("  The step change tells you the cache line size of THIS machine.\n\n");
            printf("  %-14s %10s %12s\n", "stride(bytes)", "wall(s)", "Mupd/s");
            printf("  "); pdc_rule(40);
        }
        for (size_t s = 8; s <= 1024; s *= 2) {
            double w = run_mode(M_PACKED, n, iters, s);
            double mups = (double)n * (double)iters / w / 1e6;
            if (csv) printf("%zu,%.6f,%.2f\n", s, w, mups);
            else {
                printf("  %-14zu %10.3f %12.1f", s, w, mups);
                if (s == (size_t)PDC_CACHE_LINE) printf("   <-- assumed line size");
                printf("\n");
            }
        }
        if (!csv)
            printf("\n  Where the time stops improving is where one cache line ends.\n"
                   "  Apple Silicon: 128 bytes. Most x86-64: 64 bytes. Hard-coding 64\n"
                   "  in a padded structure is therefore a real bug on an M-series Mac.\n\n");
        return 0;
    }

    double base = 0.0;
    if (csv) printf("mode,wall_s,relative,mupd_per_s\n");
    else {
        printf("  %-24s %10s %10s %12s\n", "mode", "wall(s)", "vs private", "Mupd/s");
        printf("  "); pdc_rule(60);
    }

    for (int m = 0; m < M_COUNT; m++) {
        size_t stride = (m == M_PADDED) ? (size_t)PDC_CACHE_LINE : sizeof(long);
        double w = run_mode(m, n, iters, stride);
        if (m == 0) base = w;
        double mups = (double)n * (double)iters / w / 1e6;
        if (csv) printf("%s,%.6f,%.2f,%.2f\n", mode_name[m], w, w / base, mups);
        else printf("  %-24s %10.3f %9.1fx %12.1f\n",
                    mode_name[m], w, w / base, mups);
    }

    if (!csv) {
        printf("\n  Talking points:\n");
        printf("    - packed vs padded run the SAME instructions. The gap is pure\n");
        printf("      cache-coherence traffic: every store invalidates the line in\n");
        printf("      every other core's L1, so the line ping-pongs between cores.\n");
        printf("    - This is why performance counters matter. Wall time says\n");
        printf("      \"slow\"; the counters say \"stalled on memory, not compute\".\n");
        printf("    - atomic and mutex are TRUE sharing: the algorithm really does\n");
        printf("      serialise. No amount of padding fixes that -- you need a\n");
        printf("      different algorithm (per-thread partials, reduce at the end).\n");
        printf("    - Re-run with --sweep to measure the cache line size yourself.\n\n");
    }
    return 0;
}
