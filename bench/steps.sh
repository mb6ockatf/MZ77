#!/bin/sh
# Rebuild the codec as it was at every step of the optimisation work and
# count the CPU cycles each build needs for the whole corpus, back to back
# on one core.  Wall time on a laptop moves with the clock frequency;
# cycles do not, so this is the comparison to quote between steps.
#
#   bench/steps.sh                 all steps, ROUNDS=3
#   ROUNDS=5 CPU=3 bench/steps.sh
#   MZ77_MATCH=avx2 bench/steps.sh force a match routine (#16 and later)
#
# Needs perf (kernel.perf_event_paranoid <= 2), git history with the step
# commits, and the corpus (make corpus).  Writes $results/steps-raw.txt
# (every measurement) and $results/steps.txt (medians and speed-ups).
set -e
. "$(dirname "$0")/common.sh"
rounds=${ROUNDS:-3}
cpu=${CPU:-$(($(nproc) - 1))}
command -v perf > /dev/null || { echo "$0: perf not found" >&2; exit 1; }
steps='00-baseline 1f70c33
01-block-io cc7a482
02-inline-bitio 08b2ec5
03-no-memmove 08b2ec5 bench/log/hppavillion/03-no-memmove/raw/uncommitted.patch
04-compare-8-bytes c1c1f4a
05-avx512bw 02312cd
06-avx2 55149a1
07-lto 5d53dc4 - lto
07-lto-pgo 5d53dc4 - pgo'
work=$(mktemp -d "${TMPDIR:-/tmp}/mz77-steps.XXXXXX")
trap 'rm -rf "$work"' EXIT
release="-O3 -DNDEBUG -fhardened"

build_step () {
  name=$1 commit=$2 patch=$3 kind=$4
  tree=$work/$name
  mkdir -p "$tree"
  git -C "$srcdir" archive "$commit" src | tar -x -C "$tree"
  if [ -n "$patch" ] && [ "$patch" != - ]; then
    patch -s -p1 -d "$tree" < "$srcdir/$patch"
  fi
  set -- $release
  case $kind in
    lto|pgo) set -- "$@" -flto=auto ;;
  esac
  sources="$tree/src/lz77.c $tree/src/bitio.c $srcdir/bench/bench.c"
  if [ "$kind" = pgo ]; then
    "$CC" "$@" -fprofile-generate -fprofile-dir="$tree/pgo" \
      -I"$builddir" -I"$tree/src" $sources -o "$tree/bench"
    "$tree/bench" --generate="$tree/train-data" --size=4000000 > /dev/null
    for match in auto avx2 word; do
      MZ77_MATCH=$match "$tree/bench" --repeat=1 \
        "$srcdir"/test/input/* "$tree"/train-data/*.dat > /dev/null
    done
    set -- "$@" -fprofile-use -fprofile-partial-training \
      -fprofile-dir="$tree/pgo" -Wno-missing-profile
  fi
  "$CC" "$@" -I"$builddir" -I"$tree/src" $sources -o "$tree/bench"
  mv "$tree/bench" "$work/bin-$name"
}

echo "$steps" | while read -r name commit patch kind
do
  echo "building $name"
  build_step "$name" "$commit" "$patch" "$kind"
done
generated=$(ls "$corpus"/generated/*.dat)
silesia=$(find "$corpus/silesia" -type f | sort)
mkdir -p "$work/archives"
for file in $generated $silesia
do
  "$work/bin-00-baseline" --mode=compress --repeat=1 \
    --keep="$work/archives/$(basename "$file")" "$file" > /dev/null
done
archives=$(ls "$work"/archives/*)

cycles () {
  taskset -c "$cpu" perf stat -x, -e cycles:u "$@" 2>&1 > /dev/null |
    grep cycles | cut -d, -f1
}

describe_machine
raw=$results/steps-raw.txt
{
  cat "$results/machine.txt"
  echo "pinned to: CPU $cpu, rounds: $rounds, MZ77_MATCH: ${MZ77_MATCH:-unset}"
  echo "# round step set cycles"
} > "$raw"
round=1
while [ $round -le "$rounds" ]
do
  echo "round $round"
  echo "$steps" | while read -r name rest
do
    bin=$work/bin-$name
    echo "$round $name compress-generated $(cycles "$bin" --mode=compress --repeat=1 $generated)"
    echo "$round $name compress-silesia $(cycles "$bin" --mode=compress --repeat=1 $silesia)"
    echo "$round $name decompress-all $(cycles "$bin" --archives --repeat=1 $archives)"
  done >> "$raw"
  round=$((round + 1))
done

awk '
  /^[0-9]+ / {
    key = $2 SUBSEP $3
    values[key, ++count[key]] = $4
    if (!($2 in seen)) { seen[$2] = 1; order[++steps] = $2 }
  }
  function median(key,   n, i, j, tmp, list) {
    n = count[key]
    for (i = 1; i <= n; i++) list[i] = values[key, i]
    for (i = 1; i <= n; i++)
      for (j = i + 1; j <= n; j++)
        if (list[j] < list[i]) { tmp = list[i]; list[i] = list[j]; list[j] = tmp }
    return n % 2 ? list[(n + 1) / 2] : (list[n / 2] + list[n / 2 + 1]) / 2
  }
  END {
    print "Median cycles (G) over the rounds; speed-up = previous / this,"
    print "total = 00-baseline / this.  compress = generated + silesia."
    print ""
    printf "%-20s %9s %9s %9s %9s %9s %9s %9s %9s\n", "step", "compress",
      "vs prev", "total", "silesia", "vs prev", "decomp", "vs prev", "total"
    for (s = 1; s <= steps; s++) {
      name = order[s]
      gen = median(name SUBSEP "compress-generated") / 1e9
      sil = median(name SUBSEP "compress-silesia") / 1e9
      dec = median(name SUBSEP "decompress-all") / 1e9
      comp = gen + sil
      if (s == 1) { base_comp = comp; base_dec = dec; prev_comp = comp; prev_sil = sil; prev_dec = dec }
      printf "%-20s %9.2f %8.2fx %8.2fx %9.2f %8.2fx %9.2f %8.2fx %8.2fx\n",
        name, comp, prev_comp / comp, base_comp / comp, sil, prev_sil / sil,
        dec, prev_dec / dec, base_dec / dec
      prev_comp = comp; prev_sil = sil; prev_dec = dec
    }
  }' "$raw" | tee "$results/steps.txt"
echo "results in $results"
