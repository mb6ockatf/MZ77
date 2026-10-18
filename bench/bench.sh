#!/bin/sh
# Time the codec as configured (the flags ./configure chose) on the whole
# corpus.  Writes $results/bench.csv and $results/bench.txt.
set -e
. "$(dirname "$0")/common.sh"
bench=${MZ77_BENCH:-$benchdir/mz77-bench}
"$bench" --generate="$corpus/generated" >/dev/null
describe_machine
rm -f "$results/bench.csv"
corpus_files | xargs "$bench" --repeat="${REPEAT:-5}" --label=configured \
  --csv="$results/bench.csv" | tee "$results/bench.txt"
echo "results in $results"
