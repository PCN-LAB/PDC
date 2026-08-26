#include "plat.h"

#ifdef PDC_MACOS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <pthread/qos.h>
#include <mach/mach.h>
#include <mach/thread_policy.h>

/* ------------------------------------------------------------------ */
/* Topology via sysctl                                                 */
/* ------------------------------------------------------------------ */
static int sysctl_int(const char *name, int dflt)
{
    int64_t v = 0;
    size_t len = sizeof(v);
    if (sysctlbyname(name, &v, &len, NULL, 0) != 0) return dflt;
    return (int)v;
}

static void sysctl_str(const char *name, char *out, size_t n, const char *dflt)
{
    size_t len = n;
    if (sysctlbyname(name, out, &len, NULL, 0) != 0 || len == 0)
        snprintf(out, n, "%s", dflt);
    out[n - 1] = 0;
}

void pdc_topology(pdc_topo_t *t)
{
    memset(t, 0, sizeof(*t));

    char rel[64];
    sysctl_str("kern.osproductversion", rel, sizeof(rel), "?");
    snprintf(t->os, sizeof(t->os), "macOS %s", rel);

    t->total_logical  = sysctl_int("hw.logicalcpu", 1);
    t->physical_cores = sysctl_int("hw.physicalcpu", t->total_logical);
    sysctl_str("machdep.cpu.brand_string", t->brand, sizeof(t->brand),
               "Apple Silicon");

    /* Apple Silicon exposes one "perflevel" per core cluster.
     * perflevel0 == fastest cluster (P), perflevel1 == E cluster. */
    int nlevels = sysctl_int("hw.nperflevels", 1);
    if (nlevels >= 2) {
        t->heterogeneous = 1;
        t->perf_cores = sysctl_int("hw.perflevel0.logicalcpu", 0);
        t->eff_cores  = sysctl_int("hw.perflevel1.logicalcpu", 0);
        sysctl_str("hw.perflevel0.name", t->perf_name, sizeof(t->perf_name),
                   "Performance");
        sysctl_str("hw.perflevel1.name", t->eff_name, sizeof(t->eff_name),
                   "Efficiency");
    } else {
        snprintf(t->perf_name, sizeof(t->perf_name), "n/a");
        snprintf(t->eff_name,  sizeof(t->eff_name),  "n/a");
    }

    /* hw.cpufrequency is Intel-only; on Apple Silicon there is no fixed
     * nominal clock to report, which is itself part of the lesson. */
    int hz = sysctl_int("hw.cpufrequency", 0);
    t->freq_hint_ghz = hz > 0 ? (double)hz / 1e9 : 0.0;
}

/* ------------------------------------------------------------------ */
/* Placement: QoS classes steer threads between the P and E clusters   */
/* ------------------------------------------------------------------ */
const char *pdc_place_note(void)
{
    return "macOS/Apple Silicon: there is no pthread_setaffinity_np and "
           "THREAD_AFFINITY_POLICY returns KERN_NOT_SUPPORTED. Placement is "
           "expressed as *intent* through QoS classes: USER_INITIATED runs on "
           "the P cluster, BACKGROUND is confined to the E cluster.";
}

int pdc_place_attr(pthread_attr_t *attr, pdc_place_t p, int cpu)
{
    (void)cpu;
    switch (p) {
    case PLACE_PERF:
        return pthread_attr_set_qos_class_np(attr, QOS_CLASS_USER_INITIATED, 0)
               == 0 ? 0 : -1;
    case PLACE_EFF:
        return pthread_attr_set_qos_class_np(attr, QOS_CLASS_BACKGROUND, 0)
               == 0 ? 0 : -1;
    case PLACE_PIN:
        return -1;              /* genuinely impossible here */
    default:
        return -1;
    }
}

int pdc_place_self(pdc_place_t p, int cpu)
{
    (void)cpu;
    switch (p) {
    case PLACE_PERF:
        return pthread_set_qos_class_self_np(QOS_CLASS_USER_INITIATED, 0)
               == 0 ? 0 : -1;
    case PLACE_EFF:
        return pthread_set_qos_class_self_np(QOS_CLASS_BACKGROUND, 0)
               == 0 ? 0 : -1;
    case PLACE_PIN:
        return -1;
    default:
        return -1;
    }
}

/*
 * Used by demo 04 to *show* the failure rather than hide it: the classic
 * Mach affinity-tag API still compiles on Apple Silicon but the kernel
 * refuses it. Returns the kern_return_t so the demo can print it.
 */
int pdc_macos_try_affinity_tag(int tag)
{
    thread_affinity_policy_data_t pol = { tag };
    mach_port_t th = pthread_mach_thread_np(pthread_self());
    return (int)thread_policy_set(th, THREAD_AFFINITY_POLICY,
                                  (thread_policy_t)&pol,
                                  THREAD_AFFINITY_POLICY_COUNT);
}

/* ------------------------------------------------------------------ */
/* Counters: the PMU is not reachable from an unprivileged process     */
/* ------------------------------------------------------------------ */
int pdc_ctrs_open(pdc_ctrs_t *c)
{
    memset(c, 0, sizeof(*c));
    for (int i = 0; i < 4; i++) c->fd[i] = -1;
    c->available = 0;
    c->why = "macOS does not expose the PMU to user processes. Use "
             "scripts/mac_powermetrics.sh (cluster frequency, residency, power) "
             "or scripts/mac_xctrace.sh (cycles/instructions via Instruments).";
    return -1;
}

void pdc_ctrs_start(pdc_ctrs_t *c) { (void)c; }
void pdc_ctrs_stop(pdc_ctrs_t *c)  { (void)c; }
void pdc_ctrs_close(pdc_ctrs_t *c) { (void)c; }

#endif /* PDC_MACOS */
