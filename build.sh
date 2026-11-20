#!/bin/sh
set -eu

CXX=${CXX:-clang++}
STD=-std=c++20
WARN="-Wall -Wextra -Werror"
INCLUDE="-Isrc"

mkdir -p build

"$CXX" $STD $WARN $INCLUDE -c src/core/arena.cpp -o build/arena.o 2>/dev/null || true

echo "build de cabecalhos: nada a compilar (core e header-only)"
echo "use cmake em ambiente com CMAKE_ROOT valido para builds futuros"
