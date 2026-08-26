#!/usr/bin/env bash
# What machine am I standing on? Run this first, in class, before anything else.
set -u

echo "==================================================================="
echo " Machine summary"
echo "==================================================================="

if [[ "$(uname -s)" == "Darwin" ]]; then
    echo "  OS              : macOS $(sw_vers -productVersion) ($(uname -m))"
    echo "  Chip            : $(sysctl -n machdep.cpu.brand_string)"
    echo "  Logical CPUs    : $(sysctl -n hw.logicalcpu)"
    echo "  Physical cores  : $(sysctl -n hw.physicalcpu)"
    nlev=$(sysctl -n hw.nperflevels 2>/dev/null || echo 1)
    if [[ "$nlev" -ge 2 ]]; then
        echo "  Core clusters   :"
        for i in $(seq 0 $((nlev-1))); do
            name=$(sysctl -n hw.perflevel$i.name 2>/dev/null || echo "level$i")
            n=$(sysctl -n hw.perflevel$i.logicalcpu 2>/dev/null || echo "?")
            l1i=$(sysctl -n hw.perflevel$i.l1icachesize 2>/dev/null || echo "?")
            l1d=$(sysctl -n hw.perflevel$i.l1dcachesize 2>/dev/null || echo "?")
            l2=$(sysctl -n hw.perflevel$i.l2cachesize  2>/dev/null || echo "?")
            echo "      $name: $n cores  L1i=$l1i  L1d=$l1d  L2=$l2"
        done
    fi
    echo "  Cache line      : $(sysctl -n hw.cachelinesize) bytes"
    echo "  RAM             : $(( $(sysctl -n hw.memsize) / 1073741824 )) GB (unified)"
    echo "  Page size       : $(sysctl -n hw.pagesize) bytes"
    echo
    echo "  Note: hw.cpufrequency is Intel-only. Apple Silicon reports no fixed"
    echo "  clock because there isn't one -- see demo 05."
else
    echo "  OS              : $(uname -srm)"
    if command -v lscpu >/dev/null; then lscpu | sed 's/^/  /'; fi
    echo
    echo "  cpufreq:"
    if command -v cpupower >/dev/null; then
        cpupower frequency-info 2>/dev/null | sed 's/^/    /'
    else
        for f in /sys/devices/system/cpu/cpu0/cpufreq/{scaling_driver,scaling_governor,scaling_min_freq,scaling_max_freq}; do
            [[ -r "$f" ]] && echo "    $(basename "$f") = $(cat "$f")"
        done
    fi
    echo
    echo "  perf_event_paranoid = $(cat /proc/sys/kernel/perf_event_paranoid 2>/dev/null || echo '?')"
    echo "    (<=1 is needed for unprivileged hardware counters;"
    echo "     sudo sysctl -w kernel.perf_event_paranoid=1)"
fi
echo
