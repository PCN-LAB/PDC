#!/usr/bin/env bash
#
# The frequency-scaling A/B on Linux. This is the safe, measurable,
# reversible version of "overclocking": we do not raise any limit, we
# change which limit the governor is aiming at, and we measure the result.
#
#   ./scripts/linux_freq.sh show
#   sudo ./scripts/linux_freq.sh ab ./bin/02_scaling -s 1.0 -t 1
#
# NOTHING here overvolts or exceeds a vendor limit. Real overclocking
# lives in firmware/BIOS, voids warranties, and has no business on a
# shared lab machine.
set -uo pipefail
[[ "$(uname -s)" == "Linux" ]] || { echo "Linux only." >&2; exit 1; }

CPUDIR=/sys/devices/system/cpu

show() {
    echo "== governor and limits ========================================"
    if command -v cpupower >/dev/null; then
        cpupower frequency-info | sed 's/^/  /'
    else
        for f in "$CPUDIR"/cpu0/cpufreq/{scaling_driver,scaling_governor,scaling_available_governors,scaling_min_freq,scaling_max_freq,cpuinfo_max_freq}; do
            [[ -r "$f" ]] && printf "  %-28s %s\n" "$(basename "$f")" "$(cat "$f")"
        done
    fi
    echo
    echo "== boost / turbo ============================================="
    if [[ -r "$CPUDIR/intel_pstate/no_turbo" ]]; then
        echo "  intel_pstate/no_turbo = $(cat "$CPUDIR/intel_pstate/no_turbo")  (1 = turbo OFF)"
    elif [[ -r "$CPUDIR/cpufreq/boost" ]]; then
        echo "  cpufreq/boost = $(cat "$CPUDIR/cpufreq/boost")  (0 = boost OFF)  [AMD / acpi-cpufreq]"
    else
        echo "  no boost control exposed (VM, or driver does not support it)"
    fi
    echo
    echo "== current clocks ============================================"
    grep -m8 "cpu MHz" /proc/cpuinfo | sed 's/^/  /' || true
}

set_gov() {
    for g in "$CPUDIR"/cpu*/cpufreq/scaling_governor; do
        [[ -w "$g" ]] && echo "$1" > "$g"
    done
}

set_boost() {   # 1 = on, 0 = off
    if [[ -w "$CPUDIR/intel_pstate/no_turbo" ]]; then
        echo $((1 - $1)) > "$CPUDIR/intel_pstate/no_turbo"
    elif [[ -w "$CPUDIR/cpufreq/boost" ]]; then
        echo "$1" > "$CPUDIR/cpufreq/boost"
    fi
}

ab() {
    [[ $EUID -eq 0 ]] || { echo "run the 'ab' mode with sudo" >&2; exit 1; }
    [[ $# -ge 1 ]] || { echo "usage: sudo $0 ab <program> [args...]" >&2; exit 1; }

    ORIG_GOV=$(cat "$CPUDIR/cpu0/cpufreq/scaling_governor" 2>/dev/null || echo "")
    restore() {
        [[ -n "$ORIG_GOV" ]] && set_gov "$ORIG_GOV"
        set_boost 1
        echo; echo "restored governor='$ORIG_GOV', boost on"
    }
    trap restore EXIT

    for cfg in "performance:1" "performance:0" "powersave:1"; do
        gov="${cfg%%:*}"; boost="${cfg##*:}"
        set_gov "$gov"; set_boost "$boost"; sleep 2
        mhz=$(awk '/cpu MHz/{print int($4); exit}' /proc/cpuinfo)
        echo "--------------------------------------------------------------"
        echo "governor=$gov  boost=$boost  (cpu0 now ~${mhz} MHz)"
        echo "--------------------------------------------------------------"
        if command -v perf >/dev/null; then
            perf stat -e cycles,ref-cycles,instructions -- "$@" 2>&1 | tail -20
        else
            "$@" | tail -20
        fi
        echo
    done
}

case "${1:-show}" in
    show) show ;;
    ab)   shift; ab "$@" ;;
    *)    echo "usage: $0 [show | ab <program> [args...]]" >&2; exit 1 ;;
esac
