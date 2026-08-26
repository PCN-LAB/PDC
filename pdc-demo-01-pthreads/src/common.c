#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "common.h"
#include "plat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>

#ifdef PDC_MACOS
#  include <mach/mach.h>
#endif

/* ------------------------------------------------------------------ */
double pdc_wall_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

double pdc_proc_cpu_sec(void)
{
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return (double)ru.ru_utime.tv_sec + (double)ru.ru_utime.tv_usec * 1e-6
         + (double)ru.ru_stime.tv_sec + (double)ru.ru_stime.tv_usec * 1e-6;
}

double pdc_thread_cpu_sec(void)
{
#ifdef PDC_MACOS
    mach_port_t self = pthread_mach_thread_np(pthread_self());
    thread_basic_info_data_t info;
    mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;
    if (thread_info(self, THREAD_BASIC_INFO, (thread_info_t)&info, &count)
            != KERN_SUCCESS)
        return 0.0;
    return (double)info.user_time.seconds
         + (double)info.user_time.microseconds * 1e-6
         + (double)info.system_time.seconds
         + (double)info.system_time.microseconds * 1e-6;
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) != 0) return 0.0;
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
#endif
}

/* ------------------------------------------------------------------ */
/* The kernel. Kept in its own translation unit and NOT inlined into
 * the demos so that every demo measures byte-identical work.          */
double pdc_kernel(uint64_t iters, uint64_t seed)
{
    uint64_t x = seed | 1ULL;
    double acc = 0.0;
    for (uint64_t i = 0; i < iters; i++) {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        double v = (double)(x & 0xFFFFFFULL) * (1.0 / 16777216.0);
        acc += v * v - 0.5 * v;
    }
    return acc;
}

uint64_t pdc_calibrate(double target_sec)
{
    uint64_t n = 1000000ULL;
    double t = 0.0;
    volatile double sink = 0.0;
    for (int tries = 0; tries < 24; tries++) {
        double t0 = pdc_wall_sec();
        sink += pdc_kernel(n, 12345);
        t = pdc_wall_sec() - t0;
        if (t > 0.08) break;
        n *= 2;
    }
    (void)sink;
    if (t <= 0.0) return n;
    double scaled = (double)n * (target_sec / t);
    if (scaled < 1e5) scaled = 1e5;
    if (scaled > 4e10) scaled = 4e10;
    /* round to a whole million so printed numbers stay tidy */
    uint64_t out = (uint64_t)(scaled / 1e6 + 0.5) * 1000000ULL;
    return out ? out : 1000000ULL;
}

/* ------------------------------------------------------------------ */
void pdc_bind_stdout(void) { setvbuf(stdout, NULL, _IOLBF, 0); }

static int cmp_dbl(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

double pdc_median(double *v, int n)
{
    if (n <= 0) return 0.0;
    double *tmp = (double *)malloc((size_t)n * sizeof(double));
    if (!tmp) return v[0];
    memcpy(tmp, v, (size_t)n * sizeof(double));
    qsort(tmp, (size_t)n, sizeof(double), cmp_dbl);
    double m = (n & 1) ? tmp[n / 2] : 0.5 * (tmp[n / 2 - 1] + tmp[n / 2]);
    free(tmp);
    return m;
}

void pdc_rule(int width)
{
    for (int i = 0; i < width; i++) putchar('-');
    putchar('\n');
}

int pdc_arg_int(int argc, char **argv, const char *flag, int dflt)
{
    for (int i = 1; i < argc - 1; i++)
        if (strcmp(argv[i], flag) == 0) return atoi(argv[i + 1]);
    return dflt;
}

double pdc_arg_dbl(int argc, char **argv, const char *flag, double dflt)
{
    for (int i = 1; i < argc - 1; i++)
        if (strcmp(argv[i], flag) == 0) return atof(argv[i + 1]);
    return dflt;
}

int pdc_arg_flag(int argc, char **argv, const char *flag)
{
    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], flag) == 0) return 1;
    return 0;
}
