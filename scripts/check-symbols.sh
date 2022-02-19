#!/bin/sh
# Check that no internal symbols are being leaked from the library
# $1: path to the .so
# $2: path to the lib/bemenu.h

hash grep nm sort awk comm printf cat touch

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

grep '^BM_PUBLIC' "$2" | grep -o '[a-z_]*(' | grep -o '[a-z_]*' | awk 'NF' | sort > "$tmp/hdr.txt"

nm --extern-only "$1" |\
   awk '/T/{if (substr($3,1,1) == "_") print substr($3, 2); else print $3}' |\
   grep -o '[a-z_]*' | awk 'NF' | sort > "$tmp/lib.txt"

comm -13 "$tmp/hdr.txt" "$tmp/lib.txt" > "$tmp/leaks.txt"
comm -23 "$tmp/hdr.txt" "$tmp/lib.txt" > "$tmp/missing.txt"

if [ -s "$tmp/leaks.txt" ]; then
   printf 'SYMBOL LEAKAGE: following symbols should not be marked BM_PUBLIC:\n'
   cat "$tmp/leaks.txt" | awk '$0="> "$0'
   touch "$tmp/failure"
fi

if [ -s "$tmp/missing.txt" ]; then
   printf 'SYMBOL MISSING: following BM_PUBLIC symbols were not found from the binary:\n'
   cat "$tmp/missing.txt" | awk '$0="> "$0'
   touch "$tmp/failure"
fi

shift 2
for renderer in "$@"; do
   nm --extern-only "$renderer" | awk '/T/{print $3}' | grep -v register_renderer | awk 'NF' > "$tmp/${renderer}_leaks.txt"
   if [ -s "$tmp/${renderer}_leaks.txt" ]; then
      printf 'SYMBOL LEAKAGE: %s should only have a register_renderer symbol visible\n' "$renderer"
      cat "$tmp/${renderer}_leaks.txt" | awk '$0="> "$0'
      touch "$tmp/failure"
   fi
done

test -f "$tmp/failure" && exit 1 || exit 0
