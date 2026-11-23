#!/usr/bin/env bash
set -eu
cd "$(dirname "$0")"
./build.sh
exec ./out/StellarMusic "$@"
