#!/bin/sh
# Compression work against input size, original program against the
# final one (figures "Зависимость числа команд ..." of the report).
# Builds both from their commits, compresses 1..20 MB prefixes of the
# generated corpus files on one pinned core and writes scaling-raw.txt
# next to this script; extract.py turns it into scaling-*.dat.
#
#   archive/report-2026-10/data/scaling.sh     CPU=3 by default
#
# Needs perf (kernel.perf_event_paranoid <= 2) and 'make corpus'.
set -e
data=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$data/../../.." && pwd)
cpu=${CPU:-3}
work=$(mktemp -d "${TMPDIR:-/tmp}/mz77-scaling.XXXXXX")
trap 'rm -rf "$work"' EXIT

for spec in "base 1f70c33" "final 5d53dc4 -flto=auto"; do
  set -- $spec
  name=$1 commit=$2 extra=$3
  mkdir "$work/src-$name"
  git -C "$repo" archive "$commit" src | tar -x -C "$work/src-$name"
  gcc -O3 -DNDEBUG -fhardened $extra -I"$repo" -I"$work/src-$name/src" \
    "$work/src-$name/src/lz77.c" "$work/src-$name/src/bitio.c" \
    "$repo/bench/bench.c" -o "$work/bin-$name"
done

for file in text random zeros; do
  for mb in 1 2 4 8 16 20; do
    head -c $((mb * 1000000)) "$repo/bench/corpus/generated/$file.dat" \
      > "$work/$file-$mb"
  done
done

raw=$data/scaling-raw.txt
{
  echo "# compression, $(uname -n), $(date -u +%F)"
  echo "# base = 1f70c33 (original), final = 5d53dc4 (-flto=auto, no PGO); -O3 -DNDEBUG -fhardened"
  echo "# taskset -c $cpu perf stat -x, -e cycles:u,instructions:u BIN --mode=compress --repeat=1 PREFIX"
  echo "file size_mb binary round cycles instructions"
} > "$raw"
for round in 1 2 3; do
  for file in text random zeros; do
    for mb in 1 2 4 8 16 20; do
      for binary in base final; do
        counts=$(taskset -c "$cpu" perf stat -x, -e cycles:u,instructions:u \
          "$work/bin-$binary" --mode=compress --repeat=1 "$work/$file-$mb" \
          2>&1 > /dev/null | grep -E 'cycles|instructions' | cut -d, -f1 |
          tr '\n' ' ')
        echo "$file $mb $binary $round $counts" >> "$raw"
      done
    done
  done
done
python3 -I "$data/extract.py"
