#!/usr/bin/env bash
#
# Real PMU counters on Apple Silicon, via Instruments' command line.
# This is the macOS answer to `perf stat`.
#
#   ./scripts/mac_xctrace.sh ./bin/02_scaling -s 1.0 -t 8
#
# Requires Xcode (not just the Command Line Tools):
#   xcode-select -p        # should print .../Xcode.app/...
#   sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
#
# The recording is written to results/<name>.trace. Open it with:
#   open results/<name>.trace
# and read the "CPU Counters" track: cycles, instructions retired, IPC,
# branch misses, and per-core-cluster attribution.
set -euo pipefail

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "macOS only. On Linux use scripts/linux_perf.sh." >&2; exit 1
fi
if ! command -v xctrace >/dev/null; then
    echo "xctrace not found. Install Xcode and run:" >&2
    echo "  sudo xcode-select -s /Applications/Xcode.app/Contents/Developer" >&2
    exit 1
fi
if [[ $# -lt 1 ]]; then
    echo "usage: $0 <program> [args...]" >&2; exit 1
fi

mkdir -p results
STAMP=$(date +%Y%m%d-%H%M%S)
TRACE="results/counters-$STAMP.trace"

echo "Recording CPU Counters into $TRACE"
echo "Command: $*"
echo

xctrace record \
    --template 'CPU Counters' \
    --output "$TRACE" \
    --launch -- "$@"

echo
echo "Done. Open it with:   open \"$TRACE\""
echo
echo "In Instruments, the numbers to point at:"
echo "  Cycles                  how long the work took in hardware time"
echo "  Instructions Retired    how much work actually completed"
echo "  IPC = Instructions/Cycles"
echo "     ~4-6 on a Firestorm P-core running this kernel well"
echo "     dropping sharply => stalls: cache misses, coherence, contention"
echo "  Effective clock = Cycles / elapsed-CPU-time"
echo "     compare the 1-thread and 8-thread runs and watch it fall"
