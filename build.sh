#!/bin/sh
set -eu

CXX=${CXX:-clang++}
STD=-std=c++20
WARN="-Wall -Wextra -Werror"
INCLUDE="-Isrc"
OUT=build
TMPDIR=${TMPDIR:-/tmp}/opencode-build-check

mkdir -p "$OUT" "$TMPDIR"

for header in src/core/arena.hpp src/core/schema.hpp src/core/sql.hpp src/core/session_store.hpp; do
  name=$(basename "$header" .hpp)
  printf '#include "core/%s.hpp"\nint main() { return 0; }\n' "$name" > "$TMPDIR/$name.cpp"
  "$CXX" $STD $WARN $INCLUDE "$TMPDIR/$name.cpp" -o "$TMPDIR/$name"
  echo "ok: $header"
done

"$CXX" $STD $WARN $INCLUDE -c src/core/session_store.cpp -o "$OUT/session_store.o" $(pkg-config --cflags libpq)
echo "ok: src/core/session_store.cpp"
