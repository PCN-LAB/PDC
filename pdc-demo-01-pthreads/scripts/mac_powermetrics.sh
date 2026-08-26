#!/usr/bin/env bash
#
# Watch what the hardware is actually doing while a demo runs.
# macOS only. Needs sudo -- powermetrics reads SoC telemetry.
#
#   ./scripts/mac_powermetrics.sh                 # 20 samples, 1 s apart
#   ./scripts/mac_powermetrics.sh 60              # 60 samples
#   ./scripts/mac_powermetrics.sh 30 ./bin/02_scaling -s 1.0
#
# With a command, the command is launched and sampled until it exits.
# Columns: per-cluster active frequency (MHz) and active residency (%),
# plus package power. This is the closest thing macOS gives you to
# `perf stat -e cycles` -- it is telemetry, not a per-process counter,
# so keep the machine otherwise idle.
set -uo pipefail

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "This script is macOS-only. On Linux use scripts/linux_perf.sh." >&2
    exit 1
fi

N="${1:-20}"; shift || true
OUT="$(mktemp -t pdc_pm)"
trap 'rm -f "$OUT"' EXIT

echo "Sampling SoC telemetry: $N samples, 1 s apart. sudo password may be required."
echo

if [[ $# -gt 0 ]]; then
    echo "Launching: $*"
    "$@" >/dev/null 2>&1 &
    WORKLOAD=$!
    sudo powermetrics --samplers cpu_power -i 1000 -n "$N" > "$OUT" 2>/dev/null
    wait "$WORKLOAD" 2>/dev/null || true
else
    sudo powermetrics --samplers cpu_power -i 1000 -n "$N" > "$OUT" 2>/dev/null
fi

awk '
/Cluster HW active frequency/ {
    split($0, a, ":"); name=$1;
    gsub(/[^0-9.]/, "", a[2]); freq[name]=a[2];
}
/Cluster HW active residency/ {
    split($0, a, ":"); name=$1;
    match(a[2], /[0-9.]+/); res[name]=substr(a[2], RSTART, RLENGTH);
}
/^CPU Power/       { split($0,a,":"); gsub(/[^0-9.]/,"",a[2]); cpu=a[2]; }
/^Combined Power/  { split($0,a,":"); gsub(/[^0-9.]/,"",a[2]); tot=a[2];
    n++;
    line = sprintf("%4d", n);
    for (k in freq) line = line sprintf("  %s %6.0f MHz %5.1f%%", k, freq[k], res[k]);
    printf "%s   CPU %6.0f mW   pkg %6.0f mW\n", line, cpu, tot;
}
END {
    if (n == 0) print "No samples parsed -- powermetrics output format may differ on this macOS version.";
    print "";
    print "Read it like this:";
    print "  P-Cluster frequency climbing to its ceiling  = boost / DVFS ramp";
    print "  frequency sagging while residency stays high = power or thermal limit";
    print "  E-Cluster busy while P-Cluster idles         = your threads landed on";
    print "                                                the efficiency cores";
}' "$OUT"
