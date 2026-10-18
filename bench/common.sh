# Shared settings for the benchmark and profiling scripts.  Sourced, not run.
# The Makefile passes MZ77_SRCDIR, MZ77_BUILDDIR and CC; running a script
# by hand from the source tree works too.
here=$(cd "$(dirname "$0")" && pwd)
srcdir=${MZ77_SRCDIR:-$(cd "$here/.." && pwd)}
builddir=${MZ77_BUILDDIR:-$srcdir}
benchdir=$builddir/bench
CC=${CC:-gcc}
corpus=$benchdir/corpus
if [ ! -f "$builddir/config.h" ]; then
  echo "$0: $builddir/config.h missing; run ./configure first" >&2
  exit 1
fi
rev=$(git -C "$srcdir" rev-parse --short HEAD 2>/dev/null || echo norev)
if ! git -C "$srcdir" diff --quiet HEAD -- src bench 2>/dev/null; then
  rev=$rev-dirty
fi
results=${MZ77_RESULTS:-$benchdir/results/$rev}
mkdir -p "$results"

build () {
  name=$1; shift
  mkdir -p "$benchdir/bin"
  "$CC" "$@" -I"$builddir" -I"$srcdir/src" \
    "$srcdir/src/lz77.c" "$srcdir/src/bitio.c" "$srcdir/bench/bench.c" \
    ${link_flags:-} -o "$benchdir/bin/$name"
}

corpus_files () {
  ls "$corpus"/generated/*.dat 2>/dev/null
  if [ -d "$corpus/silesia" ]; then
    find "$corpus/silesia" -type f | sort
  fi
}

profile_input () {
  if [ -n "${PROFILE_INPUT:-}" ]; then
    echo "$PROFILE_INPUT"
  elif [ -f "$corpus/silesia/dickens" ]; then
    echo "$corpus/silesia/dickens"
  else
    echo "$corpus/generated/text.dat"
  fi
}

describe_machine () {
  {
    echo "date:     $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "commit:   $rev"
    echo "cpu:      $(sed -n 's/^model name[[:space:]]*: //p' /proc/cpuinfo | head -1)"
    echo "threads:  $(nproc)"
    echo "governor: $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || echo unknown)"
    echo "kernel:   $(uname -r)"
    echo "compiler: $("$CC" --version | head -1)"
  } > "$results/machine.txt"
}
