#!/bin/sh
set -e
. "$(dirname "$0")/common.sh"
bench=${MZ77_BENCH:-$benchdir/mz77-bench}
"$bench" --generate="$corpus/generated" >/dev/null
describe_machine
rm -f "$results/bench.csv"
corpus_files | xargs "$bench" --repeat="${REPEAT:-5}" --label=configured \
  --csv="$results/bench.csv" | tee "$results/bench.txt"
echo "results in $results"
