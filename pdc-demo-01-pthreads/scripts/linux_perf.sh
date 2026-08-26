#!/usr/bin/env bash
#
# Hardware counters on the Linux lab machines -- the students' path.
#
#   ./scripts/linux_perf.sh ./bin/02_scaling -s 1.0 -t 8
#
# If perf refuses, lower the paranoia level (per boot):
#   sudo sysctl -w kernel.perf_event_paranoid=1
set -uo pipefail

if [[ "$(uname -s)" != "Linux" ]]; then
    echo "Linux only. On macOS use scripts/mac_xctrace.sh." >&2; exit 1
fi
if ! command -v perf >/dev/null; then
    echo "perf not installed. Try:" >&2
    echo "  sudo apt install linux-tools-common linux-tools-\$(uname -r)   # Debian/Ubuntu" >&2
    echo "  sudo dnf install perf                                          # Fedora/RHEL" >&2
    exit 1
fi
if [[ $# -lt 1 ]]; then echo "usage: $0 <program> [args...]" >&2; exit 1; fi

EVENTS=cycles,instructions,ref-cycles,cache-references,cache-misses,branch-misses,context-switches,cpu-migrations,task-clock

echo "== perf stat =================================================="
perf stat -e "$EVENTS" -- "$@"

cat <<'EOF'

== how to read it =============================================
  instructions/cycle (IPC)  work done per unit of hardware time.
                            Flat across thread counts = healthy scaling.
  cycles / ref-cycles       the real turbo multiplier over the base clock.
                            Falls as more cores wake up: that is the
                            power/thermal budget, not your code.
  cache-misses              the currency of false sharing. Compare
                            ./bin/03_false_sharing packed vs padded.
  cpu-migrations            threads bouncing between cores. Pinning
                            (demo 04) should drive this to ~0.
  task-clock                CPU-milliseconds; task-clock/elapsed is the
                            average number of cores you kept busy.
EOF
