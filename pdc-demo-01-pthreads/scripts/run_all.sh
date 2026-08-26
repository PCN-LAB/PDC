#!/usr/bin/env bash
#
# One command for the whole lab. Builds, runs every demo, writes CSVs into
# results/ and prints a short summary you can screenshot for the class.
#
#   ./scripts/run_all.sh              # ~3 minutes
#   ./scripts/run_all.sh quick        # ~1 minute
set -uo pipefail
cd "$(dirname "$0")/.."

MODE="${1:-full}"
if [[ "$MODE" == "quick" ]]; then S=0.4; D=10; else S=1.0; D=30; fi

mkdir -p results
STAMP=$(date +%Y%m%d-%H%M%S)
HOSTTAG=$(uname -s)-$(uname -m)

echo "Building..."
make -s clean >/dev/null 2>&1 || true
make -s || { echo "build failed" >&2; exit 1; }

echo
./scripts/sysinfo.sh | tee "results/sysinfo-$HOSTTAG-$STAMP.txt"

run_nocsv() {  # demos with no --csv mode
    local name="$1"; shift
    echo
    echo "################################################################"
    echo "# $name"
    echo "################################################################"
    "$@" | tee "results/$name-$HOSTTAG-$STAMP.txt"
}

run() {   # run <name> <binary> [args...]
    local name="$1"; shift
    echo
    echo "################################################################"
    echo "# $name"
    echo "################################################################"
    "$@" | tee "results/$name-$HOSTTAG-$STAMP.txt"
    "$@" --csv > "results/$name-$HOSTTAG-$STAMP.csv" 2>/dev/null || true
}

run_nocsv 01_hello   ./bin/01_hello -t 4
run 02_scaling      ./bin/02_scaling -s "$S" -r 3
run 03_false_share  ./bin/03_false_sharing
run 03_stride_sweep ./bin/03_false_sharing --sweep
run 04_affinity     ./bin/04_affinity -s "$S"
run 05_frequency    ./bin/05_frequency -d "$D"

echo
echo "################################################################"
echo "# Results written to results/*-$STAMP.*"
echo "################################################################"
ls -1 results/*"$STAMP"* | sed 's/^/  /'

cat <<EOF

Next, the part the programs cannot do for themselves -- read the hardware:

  macOS :  sudo ./scripts/mac_powermetrics.sh 30 ./bin/02_scaling -s 1.0
           ./scripts/mac_xctrace.sh ./bin/02_scaling -s 1.0 -t 8
  Linux :  ./scripts/linux_perf.sh ./bin/02_scaling -s 1.0 -t 8
           ./scripts/linux_freq.sh show
           sudo ./scripts/linux_freq.sh ab ./bin/02_scaling -s 1.0 -t 1
EOF
