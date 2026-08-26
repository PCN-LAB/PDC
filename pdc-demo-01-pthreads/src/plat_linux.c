#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "plat.h"

#ifdef PDC_LINUX

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sched.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>

/* ------------------------------------------------------------------ */
/* Topology                                                            */
/* ------------------------------------------------------------------ */
static int read_first_line(const char *path, char *buf, size_t n)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    if (!fgets(buf, (int)n, f)) { fclose(f); return -1; }
    fclose(f);
    buf[strcspn(buf, "\n")] = 0;
    return 0;
}

static int count_siblings(const char *list)
{
    int n = 1;
    for (const char *p = list; *p; p++) if (*p == ',' || *p == '-') n++;
    return n;
}

void pdc_topology(pdc_topo_t *t)
{
    memset(t, 0, sizeof(*t));
    snprintf(t->os, sizeof(t->os), "Linux");
    t->total_logical = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (t->total_logical < 1) t->total_logical = 1;

    char buf[256];
    if (read_first_line("/sys/devices/system/cpu/cpu0/topology/thread_siblings_list",
                        buf, sizeof(buf)) == 0) {
        int smt = count_siblings(buf);
        t->physical_cores = smt > 0 ? t->total_logical / smt : t->total_logical;
    } else {
        t->physical_cores = t->total_logical;
    }
    if (t->physical_cores < 1) t->physical_cores = t->total_logical;

    /* CPU brand string */
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (f) {
        while (fgets(buf, sizeof(buf), f)) {
            if (strncmp(buf, "model name", 10) == 0) {
                char *c = strchr(buf, ':');
                if (c) {
                    c++; while (*c == ' ') c++;
                    c[strcspn(c, "\n")] = 0;
                    snprintf(t->brand, sizeof(t->brand), "%s", c);
                }
                break;
            }
        }
        fclose(f);
    }
    if (!t->brand[0]) snprintf(t->brand, sizeof(t->brand), "unknown CPU");

    if (read_first_line("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq",
                        buf, sizeof(buf)) == 0)
        t->freq_hint_ghz = atof(buf) / 1e6;   /* kHz -> GHz */

    /* Uniform SMP is the normal case in the lab; big.LITTLE arm64 Linux
     * boxes would need cpu_capacity parsing, which we skip here. */
    t->perf_cores = 0;
    t->eff_cores  = 0;
    t->heterogeneous = 0;
    snprintf(t->perf_name, sizeof(t->perf_name), "n/a");
    snprintf(t->eff_name,  sizeof(t->eff_name),  "n/a");
}

/* ------------------------------------------------------------------ */
/* Placement: real hard affinity                                       */
/* ------------------------------------------------------------------ */
const char *pdc_place_note(void)
{
    return "Linux: PLACE_PIN uses sched_setaffinity/pthread_attr_setaffinity_np "
           "(true hard pinning). PLACE_PERF/PLACE_EFF have no meaning on a "
           "uniform SMP machine and are ignored.";
}

int pdc_place_attr(pthread_attr_t *attr, pdc_place_t p, int cpu)
{
    if (p != PLACE_PIN) return -1;
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET((size_t)(cpu % (int)sysconf(_SC_NPROCESSORS_ONLN)), &set);
    return pthread_attr_setaffinity_np(attr, sizeof(set), &set) == 0 ? 0 : -1;
}

int pdc_place_self(pdc_place_t p, int cpu)
{
    if (p != PLACE_PIN) return -1;
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET((size_t)(cpu % (int)sysconf(_SC_NPROCESSORS_ONLN)), &set);
    return sched_setaffinity(0, sizeof(set), &set) == 0 ? 0 : -1;
}

/* ------------------------------------------------------------------ */
/* Counters: perf_event_open, self-monitoring, inherited by threads    */
/* ------------------------------------------------------------------ */
static long perf_open(struct perf_event_attr *a, pid_t pid, int cpu,
                      int grp, unsigned long flags)
{
    return syscall(__NR_perf_event_open, a, pid, cpu, grp, flags);
}

static int open_one(uint32_t type, uint64_t config)
{
    struct perf_event_attr a;
    memset(&a, 0, sizeof(a));
    a.type           = type;
    a.size           = sizeof(a);
    a.config         = config;
    a.disabled       = 1;
    a.inherit        = 1;      /* threads created later are counted too */
    a.exclude_kernel = 1;
    a.exclude_hv     = 1;
    return (int)perf_open(&a, 0, -1, -1, 0);
}

int pdc_ctrs_open(pdc_ctrs_t *c)
{
    memset(c, 0, sizeof(*c));
    for (int i = 0; i < 4; i++) c->fd[i] = -1;

    c->fd[0] = open_one(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES);
    if (c->fd[0] < 0) {
        c->available = 0;
        c->why = (errno == EACCES || errno == EPERM)
            ? "perf_event_open denied: run 'sudo sysctl -w kernel.perf_event_paranoid=1'"
            : "perf_event_open unavailable on this host (container or VM?)";
        return -1;
    }
    c->fd[1] = open_one(PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);
    c->fd[2] = open_one(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES);
    c->fd[3] = open_one(PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES);
    c->available = 1;
    c->why = NULL;
    return 0;
}

void pdc_ctrs_start(pdc_ctrs_t *c)
{
    if (!c->available) return;
    for (int i = 0; i < 4; i++) {
        if (c->fd[i] < 0) continue;
        ioctl(c->fd[i], PERF_EVENT_IOC_RESET, 0);
        ioctl(c->fd[i], PERF_EVENT_IOC_ENABLE, 0);
    }
}

void pdc_ctrs_stop(pdc_ctrs_t *c)
{
    if (!c->available) return;
    uint64_t v[4] = {0, 0, 0, 0};
    for (int i = 0; i < 4; i++) {
        if (c->fd[i] < 0) continue;
        ioctl(c->fd[i], PERF_EVENT_IOC_DISABLE, 0);
        if (read(c->fd[i], &v[i], sizeof(uint64_t)) != (ssize_t)sizeof(uint64_t))
            v[i] = 0;
    }
    c->cycles        = v[0];
    c->instructions  = v[1];
    c->cache_misses  = v[2];
    c->branch_misses = v[3];
}

void pdc_ctrs_close(pdc_ctrs_t *c)
{
    for (int i = 0; i < 4; i++)
        if (c->fd[i] >= 0) { close(c->fd[i]); c->fd[i] = -1; }
    c->available = 0;
}

#endif /* PDC_LINUX */
