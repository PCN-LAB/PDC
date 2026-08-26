#include "plat.h"
#include <stdio.h>

const char *pdc_place_name(pdc_place_t p)
{
    switch (p) {
    case PLACE_DEFAULT: return "default";
    case PLACE_PERF:    return "P-cores";
    case PLACE_EFF:     return "E-cores";
    case PLACE_PIN:     return "pinned";
    }
    return "?";
}

void pdc_topology_print(const pdc_topo_t *t)
{
    printf("  CPU              : %s\n", t->brand);
    printf("  OS               : %s\n", t->os);
    printf("  logical CPUs     : %d\n", t->total_logical);
    printf("  physical cores   : %d\n", t->physical_cores);
    if (t->heterogeneous)
        printf("  clusters         : %d x %s + %d x %s  (heterogeneous)\n",
               t->perf_cores, t->perf_name, t->eff_cores, t->eff_name);
    else
        printf("  clusters         : uniform SMP\n");
    if (t->freq_hint_ghz > 0.0)
        printf("  max clock (OS)   : %.2f GHz\n", t->freq_hint_ghz);
    else
        printf("  max clock (OS)   : not reported by the OS\n");
    printf("  cache line       : %d bytes (compile-time)\n", PDC_CACHE_LINE);
}
