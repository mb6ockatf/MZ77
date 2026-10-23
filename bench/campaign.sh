#!/bin/sh
# Measure one step of the optimisation work and keep every raw log in
# bench/log/MACHINE/STEP/.  See bench/log/README.
#
#   bench/campaign.sh STEP          e.g. 00-baseline, 01-block-io
#
# Environment:
#   MACHINE       directory name for this machine (default: short hostname)
#   REPEAT        runs per file for bench and bench-compare (default 9)
#   CPU           core to pin the measurements to (default: the last one)
#   SKIP_COMPARE  set to 1 to leave out bench-compare
#   SKIP_PROFILE  set to 1 to leave out gprof, perf and valgrind
#   MZ77_BUILDDIR build tree, configured with --enable-release (default:
#                 the source tree)
set -e
step=${1:?usage: $0 STEP}
here=$(cd "$(dirname "$0")" && pwd)
srcdir=$(cd "$here/.." && pwd)
builddir=${MZ77_BUILDDIR:-$srcdir}
machine=${MACHINE:-$(hostname -s)}
repeat=${REPEAT:-9}
cpu=${CPU:-$(($(nproc) - 1))}
out=$srcdir/bench/log/$machine/$step
raw=$out/raw

if ! "$builddir/config.status" --config | grep -q enable-release; then
  echo "$0: $builddir is not configured with --enable-release" >&2
  exit 1
fi
if [ -e "$out" ]; then
  echo "$0: $out exists; pick another STEP or remove it" >&2
  exit 1
fi
mkdir -p "$raw"

# Progress lines also go to raw/campaign.log; each stage logs to its own file.
log () { echo "[$(date +%H:%M:%S)] $*" | tee -a "$raw/campaign.log"; }
run () {
  name=$1; shift
  log "start $name: $*"
  if "$@" > "$raw/$name.log" 2>&1; then
    log "done  $name"
  else
    status=$?
    log "FAILED $name (exit $status), see raw/$name.log"
    exit $status
  fi
}

base=$(git -C "$srcdir" rev-parse HEAD)
git -C "$srcdir" diff HEAD -- . ':!bench/log' > "$raw/uncommitted.patch"
git -C "$srcdir" status --short -- . ':!bench/log' > "$raw/git-status.txt"
{
  echo "step:       $step"
  echo "machine:    $machine"
  echo "started:    $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "base:       $base"
  if [ -s "$raw/uncommitted.patch" ]; then
    echo "tree:       base plus uncommitted.patch" \
      "(sha256 $(sha256sum < "$raw/uncommitted.patch" | cut -c1-16))"
  else
    echo "tree:       clean"
  fi
  echo "cpu:        $(sed -n 's/^model name[[:space:]]*: //p' /proc/cpuinfo | head -1)"
  echo "threads:    $(nproc)"
  echo "pinned to:  CPU $cpu"
  echo "governor:   $(cat /sys/devices/system/cpu/cpu"$cpu"/cpufreq/scaling_governor 2>/dev/null || echo unknown)" \
    "(energy preference: $(cat /sys/devices/system/cpu/cpu"$cpu"/cpufreq/energy_performance_preference 2>/dev/null || echo unknown))"
  echo "power:      $(cat /sys/class/power_supply/A*/online /sys/class/power_supply/AC*/online 2>/dev/null | head -1 | sed 's/1/on AC/;s/0/on battery/')"
  echo "kernel:     $(uname -r)"
  echo "compiler:   $(${CC:-gcc} --version | head -1)"
  echo "configure:  $("$builddir/config.status" --config)"
  echo "repeat:     $repeat"
  echo "MZ77_MATCH: ${MZ77_MATCH:-unset (automatic choice)}"
  echo "load:       $(cut -d' ' -f1-3 /proc/loadavg)"
  echo "memory:     $(free -m | awk '/^Mem/ { print $7 " MiB available of " $2 }')"
} > "$raw/conditions.txt"
cat "$raw/conditions.txt"

run build make -C "$builddir" -j"$(nproc)"
run check make -C "$builddir" check
run build-bench make -C "$builddir/bench" mz77-bench
export MZ77_RESULTS="$raw"

pin="taskset -c $cpu"
command -v taskset > /dev/null || pin=
run bench $pin make -C "$builddir/bench" bench REPEAT="$repeat"

# Archive checksums: an optimisation that must not change the format has
# to give byte-identical archives to the previous step.
log "start archives"
work=$(mktemp -d /dev/shm/mz77-campaign.XXXXXX)
for file in $(ls "$builddir"/bench/corpus/generated/*.dat; find "$builddir/bench/corpus/silesia" -type f | sort); do
  "$builddir/bench/mz77-bench" --mode=compress --repeat=1 \
    --keep="$work/archive" "$file" > /dev/null
  echo "$(sha256sum < "$work/archive" | cut -d' ' -f1)  $(basename "$file")"
done > "$raw/archives.sha256"
rm -rf "$work"
log "done  archives"

if [ "${SKIP_COMPARE:-0}" != 1 ]; then
  run bench-compare $pin make -C "$builddir/bench" bench-compare REPEAT="$repeat"
fi
if [ "${SKIP_PROFILE:-0}" != 1 ]; then
  run profile $pin make -C "$builddir/bench" profile
fi

echo "finished:   $(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$raw/conditions.txt"
"$here/summarize.sh" "$srcdir/bench/log/$machine" > "$srcdir/bench/log/$machine/SUMMARY.md"
if [ ! -f "$out/EXPLANATION.md" ]; then
  {
    echo "# $step on $machine"
    echo
    echo "## What was measured"
    echo
    echo "## Conditions"
    echo
    sed 's/^/    /' "$raw/conditions.txt"
    echo
    echo "## Results"
    echo
    echo "## Interpretation"
  } > "$out/EXPLANATION.md"
fi
log "results in $out"
