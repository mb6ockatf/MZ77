#!/bin/sh
# Profile the codec on one input with gprof, perf and valgrind.
# Input: $PROFILE_INPUT, else Silesia's dickens, else the synthetic text.
# Everything lands in $results/profile/; summary.txt has the headline numbers.
set -e
. "$(dirname "$0")/common.sh"
input=$(profile_input)
out=$results/profile
mkdir -p "$out"
describe_machine
cp "$results/machine.txt" "$out/"
echo "profiling on $input" | tee "$out/input.txt"
[ -f "$input" ] || { echo "$0: $input missing; run 'make corpus'" >&2; exit 1; }

# Optimised code, as shipped, but with frame pointers for call stacks.
# -fno-inline keeps byte_at, hash3 and ht_insert visible as functions in
# gprof; perf and valgrind use the normal inlined build.
build gprof -O2 -g -pg -fno-inline -fno-omit-frame-pointer
build prof -O2 -g -fno-omit-frame-pointer

bench_args="--repeat=1 $input"
archive=$out/input.lz77
"$benchdir/bin/prof" --mode=compress --repeat=1 --keep="$archive" "$input" >/dev/null

# gprof samples every 10 ms, so run 5 times for enough samples.  Its call
# counts are exact.  Time spent inside libc (memmove, fputc, fgetc) is not
# instrumented and shows up under _init.
echo "== gprof"
(cd "$out" && rm -f gmon.out &&
  "$benchdir/bin/gprof" --repeat=5 "$input" >/dev/null &&
  gprof -b "$benchdir/bin/gprof" gmon.out > gprof.txt && rm -f gmon.out)

echo "== perf stat"
events=cycles:u,instructions:u,branches:u,branch-misses:u
events=$events,cache-references:u,cache-misses:u,L1-dcache-loads:u,L1-dcache-load-misses:u
perf stat -r 5 -e "$events" -o "$out/perf-stat-compress.txt" -- \
  "$benchdir/bin/prof" --mode=compress $bench_args >/dev/null
perf stat -r 5 -e "$events" -o "$out/perf-stat-decompress.txt" -- \
  "$benchdir/bin/prof" --archives --repeat=1 "$archive" >/dev/null

echo "== perf record"
perf record -q -e cycles:u -F 4999 -g -o "$out/perf.data" -- \
  "$benchdir/bin/prof" $bench_args >/dev/null 2>&1
perf report -q -i "$out/perf.data" --stdio --no-children --percent-limit=0.5 \
  > "$out/perf-report.txt" 2>/dev/null
perf report -q -i "$out/perf.data" --stdio --no-children --sort=srcline \
  --percent-limit=0.5 -g none > "$out/perf-lines.txt" 2>/dev/null
# perf 6.12 crashes when it interleaves source with the disassembly, so
# annotate shows instructions only; perf-lines.txt maps samples to lines.
for func in lz77_compress lz77_decompress br_read bw_write; do
  (perf annotate -q -i "$out/perf.data" --stdio --no-source -s "$func" \
    > "$out/perf-annotate-$func.txt") 2>/dev/null || true
  [ -s "$out/perf-annotate-$func.txt" ] || rm -f "$out/perf-annotate-$func.txt"
done

echo "== callgrind"
valgrind -q --tool=callgrind --dump-instr=yes \
  --callgrind-out-file="$out/callgrind.out" \
  "$benchdir/bin/prof" $bench_args >/dev/null
callgrind_annotate --inclusive=yes "$out/callgrind.out" > "$out/callgrind.txt"
callgrind_annotate --auto=yes "$out/callgrind.out" "$srcdir/src/lz77.c" \
  "$srcdir/src/bitio.c" > "$out/callgrind-lines.txt" 2>/dev/null || true

echo "== cachegrind"
valgrind -q --tool=cachegrind --cache-sim=yes --branch-sim=yes \
  --cachegrind-out-file="$out/cachegrind.out" \
  "$benchdir/bin/prof" $bench_args >/dev/null
cg_annotate "$out/cachegrind.out" > "$out/cachegrind.txt"

# Headline numbers for the report.
{
  echo "input: $input ($(wc -c < "$input") bytes)"
  echo
  echo "gprof flat profile (top 10):"
  sed -n '/^  %/,/^$/p' "$out/gprof.txt" | head -12
  for mode in compress decompress; do
    echo "perf stat, $mode only (mean of 5 runs):"
    awk '/cycles|instructions|branch|cache|elapsed/ { sub(/^ +/, "  "); print }' \
      "$out/perf-stat-$mode.txt"
    echo
  done
  echo "perf hottest source lines:"
  head -15 "$out/perf-lines.txt"
  echo
  echo "cachegrind totals (I = instructions, D1/LL = L1/last-level data"
  echo "cache misses, Bc/Bcm = conditional branches and mispredictions):"
  awk '/^Ir/ { for (i = 1; i <= NF; i++) { name[i] = $i; gsub(/_+$/, "", name[i]) } }
       /PROGRAM TOTALS/ { n = 0
         for (i = 1; i <= NF; i++) if ($i !~ /^\(/ && $i != "PROGRAM" && $i != "TOTALS")
           printf "  %-6s %s\n", name[++n], $i }' "$out/cachegrind.txt"
} > "$out/summary.txt"
cat "$out/summary.txt"
echo "results in $out"
