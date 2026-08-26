/*
 * plat.h -- thin platform layer for the PDC pthreads demo.
 *
 * Two backends:
 *   plat_macos.c  Apple Silicon (M1/M2/M3): QoS-based core-cluster steering,
 *                 sysctl topology.  The PMU is NOT exposed to user code, so
 *                 hardware counters come from powermetrics / Instruments.
 *   plat_linux.c  x86-64 or arm64 Linux: sched affinity pinning and real
 *                 hardware counters through perf_event_open().
 *
 * CS-3006 Parallel and Distributed Computing.
 */
#ifndef PDC_PLAT_H
#define PDC_PLAT_H

#include <pthread.h>
#include <stdint.h>

#if defined(__APPLE__)
#  define PDC_MACOS 1
#elif defined(__linux__)
#  define PDC_LINUX 1
#endif

/* Apple Silicon uses 128-byte cache lines; x86-64 uses 64. Getting this
 * wrong is exactly the bug demo 03 is designed to expose. */
#if defined(__APPLE__) && defined(__aarch64__)
#  define PDC_CACHE_LINE 128
#else
#  define PDC_CACHE_LINE 64
#endif

/* ------------------------------------------------------------------ */
/* Topology                                                            */
/* ------------------------------------------------------------------ */
typedef struct {
    int  total_logical;     /* logical CPUs visible to the scheduler   */
    int  physical_cores;    /* physical cores (== total when unknown)  */
    int  perf_cores;        /* P-cores; 0 if the machine is uniform    */
    int  eff_cores;         /* E-cores; 0 if the machine is uniform    */
    int  heterogeneous;     /* 1 when P and E clusters both exist      */
    double freq_hint_ghz;   /* nominal/base clock if the OS reports it */
    char brand[192];
    char perf_name[48];
    char eff_name[48];
    char os[48];
} pdc_topo_t;

void pdc_topology(pdc_topo_t *t);
void pdc_topology_print(const pdc_topo_t *t);

/* ------------------------------------------------------------------ */
/* Thread placement                                                    */
/* ------------------------------------------------------------------ */
typedef enum {
    PLACE_DEFAULT = 0,  /* let the OS scheduler decide                 */
    PLACE_PERF,         /* prefer performance cores (macOS QoS)        */
    PLACE_EFF,          /* prefer efficiency cores  (macOS QoS)        */
    PLACE_PIN           /* hard-pin to one logical CPU (Linux)         */
} pdc_place_t;

const char *pdc_place_name(pdc_place_t p);

/* Configure a pthread_attr_t before pthread_create().
 * Returns 0 on success, -1 if this platform cannot honour the request
 * (the caller should carry on -- an unsupported placement is a teaching
 * point, not a fatal error). `cpu` is only used by PLACE_PIN. */
int pdc_place_attr(pthread_attr_t *attr, pdc_place_t p, int cpu);

/* Same request, applied to the calling thread after it has started. */
int pdc_place_self(pdc_place_t p, int cpu);

/* One-line explanation of what placement really does here. Used by the
 * demos so students see why the Mac and the Linux lab differ. */
const char *pdc_place_note(void);

/* ------------------------------------------------------------------ */
/* Hardware performance counters                                       */
/* ------------------------------------------------------------------ */
typedef struct {
    int      available;      /* 0 => only derived metrics are possible */
    int      fd[4];
    uint64_t cycles;
    uint64_t instructions;
    uint64_t cache_misses;
    uint64_t branch_misses;
    const char *why;         /* why counters are unavailable, if so    */
} pdc_ctrs_t;

int  pdc_ctrs_open(pdc_ctrs_t *c);   /* 0 = counters live, -1 = derived only */
void pdc_ctrs_start(pdc_ctrs_t *c);
void pdc_ctrs_stop(pdc_ctrs_t *c);
void pdc_ctrs_close(pdc_ctrs_t *c);

#endif /* PDC_PLAT_H */
