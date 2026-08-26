/*
 * common.h -- timing, the shared compute kernel, and table printing.
 * CS-3006 Parallel and Distributed Computing.
 */
#ifndef PDC_COMMON_H
#define PDC_COMMON_H

#include <stdint.h>
#include <stddef.h>

/* ---- time ---------------------------------------------------------- */
double pdc_wall_sec(void);          /* monotonic wall clock             */
double pdc_proc_cpu_sec(void);      /* user+sys CPU time, whole process */
double pdc_thread_cpu_sec(void);    /* CPU time of the calling thread   */

/* ---- the kernel every demo shares ---------------------------------- */
/*
 * A compute-bound, cache-resident mix of integer shifts and double math.
 * It is deliberately register-only: no memory traffic, nothing the
 * compiler can hoist out, and the result is returned so it cannot be
 * dead-code eliminated. That makes it a clean probe for *clock speed and
 * core count* -- demo 03 adds the memory traffic on purpose.
 */
double pdc_kernel(uint64_t iters, uint64_t seed);

/* Calibrate: iterations that take roughly `target_sec` on this machine. */
uint64_t pdc_calibrate(double target_sec);

/* ---- small helpers -------------------------------------------------- */
void   pdc_bind_stdout(void);                   /* line-buffered output  */
double pdc_median(double *v, int n);
void   pdc_rule(int width);
int    pdc_arg_int(int argc, char **argv, const char *flag, int dflt);
double pdc_arg_dbl(int argc, char **argv, const char *flag, double dflt);
int    pdc_arg_flag(int argc, char **argv, const char *flag);

/* ---- result record shared by the demos ------------------------------ */
typedef struct {
    int      threads;
    double   wall;
    double   cpu;
    double   throughput;     /* million kernel iterations / second      */
    uint64_t cycles;
    uint64_t instructions;
    uint64_t cache_misses;
    uint64_t branch_misses;
    int      have_ctrs;
} pdc_result_t;

#endif /* PDC_COMMON_H */
