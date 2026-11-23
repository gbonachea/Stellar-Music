#!/usr/bin/env bash
# Integration tests use a private sink and temporary XDG data/configuration.
set -euo pipefail
cd "$(dirname "$0")/.."
cmake -S . -B out -DHERO_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build out --parallel "${JOBS:-3}"
sink="hero_test_$$"
module="$(pactl load-module module-null-sink "sink_name=$sink")"
trap 'pactl unload-module "$module"' EXIT
PULSE_SINK="$sink" dbus-run-session -- ./out/hero-check
dbus-run-session -- python3 tests/output.py ./out/StellarMusic
