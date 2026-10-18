#!/bin/sh
# Build the codec with several compiler settings and time each one on the
# whole corpus: the machine-independent (-O levels, LTO) and
# machine-dependent (-march=native, PGO) comparison.  Writes
# $results/variants.csv and a per-variant summary in $results/variants.txt.
set -e
. "$(dirname "$0")/common.sh"
repeat=${REPEAT:-5}
describe_machine
build O2 -O2
"$benchdir/bin/O2" --generate="$corpus/generated" >/dev/null
variants='O0|-O0
O1|-O1
O2|-O2
O3|-O3
O3-native|-O3 -march=native
O3-native-lto|-O3 -march=native -flto
O3-native-lto-pgo|-O3 -march=native -flto PGO'
rm -f "$results/variants.csv"
echo "$variants" | while IFS='|' read -r name flags; do
  case $flags in
    *PGO)
      flags=${flags% PGO}
      pgo_dir=$benchdir/pgo-data
      rm -rf "$pgo_dir"
      build "$name" $flags -fprofile-generate -fprofile-dir="$pgo_dir"
      "$benchdir/bin/$name" --repeat=1 "$srcdir"/test/input/* >/dev/null
      "$benchdir/bin/$name" --generate="$benchdir/pgo-train" --size=4000000 >/dev/null
      "$benchdir/bin/$name" --repeat=1 "$benchdir"/pgo-train/*.dat >/dev/null
      build "$name" $flags -fprofile-use -fprofile-dir="$pgo_dir" -Wno-missing-profile
      rm -rf "$benchdir/pgo-train"
      ;;
    *)
      build "$name" $flags
      ;;
  esac
  echo "== $name: $flags"
  corpus_files | xargs "$benchdir/bin/$name" --repeat="$repeat" \
    --label="$name" --csv="$results/variants.csv"
done

awk -F, 'NR > 1 {
    size[$1] += $3; arch[$1] += $4; comp[$1] += $7; dec[$1] += $8
    if (!($1 in seen)) { seen[$1] = 1; order[++count] = $1 }
  }
  END {
    printf "%-20s %8s %10s %10s %10s %10s\n", "variant", "ratio",
      "comp_s", "decomp_s", "comp_MB/s", "dec_MB/s"
    for (i = 1; i <= count; i++) {
      name = order[i]
      printf "%-20s %8.4f %10.3f %10.3f %10.2f %10.2f\n", name,
        arch[name] / size[name], comp[name], dec[name],
        size[name] / 1e6 / comp[name], size[name] / 1e6 / dec[name]
    }
  }' "$results/variants.csv" | tee "$results/variants.txt"
echo "results in $results"
