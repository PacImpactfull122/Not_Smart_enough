#!/bin/sh
set -eu

CXX=${CXX:-clang++}
STD=-std=c++20
WARN="-Wall -Wextra -Werror"
INCLUDE="-Isrc"
TMPDIR=${TMPDIR:-/tmp}/opencode-build-check

mkdir -p "$TMPDIR"

for header in src/core/arena.hpp src/core/schema.hpp; do
  name=$(basename "$header" .hpp)
  printf '#include "core/%s.hpp"\nint main() { return 0; }\n' "$name" > "$TMPDIR/$name.cpp"
  "$CXX" $STD $WARN $INCLUDE "$TMPDIR/$name.cpp" -o "$TMPDIR/$name"
  echo "ok: $header"
done
