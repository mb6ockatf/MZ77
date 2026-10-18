#!/bin/sh
# Fetch the Silesia corpus (Deorowicz, 2003) into bench/corpus/silesia and
# generate the synthetic corpus into bench/corpus/generated.
set -e
. "$(dirname "$0")/common.sh"
url=https://sun.aei.polsl.pl//~sdeor/corpus/silesia.zip
mkdir -p "$corpus"
if [ ! -f "$corpus/silesia/dickens" ]; then
  if [ ! -f "$corpus/silesia.zip" ]; then
    echo "fetching $url"
    curl -fL --retry 3 -o "$corpus/silesia.zip.part" "$url"
    mv "$corpus/silesia.zip.part" "$corpus/silesia.zip"
  fi
  mkdir -p "$corpus/silesia"
  unzip -oq "$corpus/silesia.zip" -d "$corpus/silesia"
fi
echo "silesia: $(ls "$corpus/silesia" | wc -l) files, $(du -sh "$corpus/silesia" | cut -f1)"
