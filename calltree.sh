#!/bin/sh
# Call tree of functions, hiding GTK, GLib and libc calls
#   calltree.sh                      text tree from main
#   calltree.sh --main=lz77_compress text tree from one function
#   calltree.sh --dot > calls.dot    graph for Graphviz
# ./calltree.sh                                              # whole program, from main
# ./calltree.sh --main=lz77_compress --main=lz77_decompress  # just the codec
# ./calltree.sh -d 3                                         # only 3 levels deep
# ./calltree.sh --dot | dot -Tpdf -Grankdir=LR -o calls.pdf  # picture for the report
# ./calltree.sh --dot --main=lz77_compress | dot -Tpdf -Grankdir=LR -o codec.pdf
tmp=$(mktemp -d) || exit 1
trap 'rm -rf "$tmp"' EXIT
mkdir "$tmp/src"
for file in src/*.c; do
  sed -E 's/for \(([A-Za-z_][A-Za-z0-9_]* +)+([A-Za-z_][A-Za-z0-9_]* *=)/for (\2/' \
    "$file" > "$tmp/$file"
done
if [ "$1" = --dot ]
then
  shift
  (cd "$tmp" && cflow --format=dot "$@" src/*.c) > "$tmp/raw.dot"
  awk 'NR == FNR { if (/label="[^"]*\(\)"\]$/) external[$1] = 1; next }
       ($1 in external) || ($3 in external) { next }
       $2 ~ /^\[label=/ { print "    " $1 " [label=\"" $1; next }
       { sub(/^src\//, ""); print }' "$tmp/raw.dot" "$tmp/raw.dot"
else
  (cd "$tmp" && cflow --tree --brief "$@" src/*.c) |
    grep -v '<>$' |
    sed -E 's/: .*<src\/([^ ]+) ([0-9]+)>/  (\1:\2)/'
fi

