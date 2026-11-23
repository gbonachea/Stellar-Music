#!/usr/bin/env bash
set -eu
cd "$(dirname "$0")"
cmake -S . -B out -DCMAKE_BUILD_TYPE=Release
cmake --build out --parallel "${JOBS:-3}"
