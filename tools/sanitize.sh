#!/bin/sh
# sanitize.sh -- the suite again, under address and undefined-behaviour
# sanitizers, in its own build directory (the instrumentation changes the
# object code, so it never shares a tree with the binaries anyone runs).
#
# Leak detection is off: Qt and VTK keep singletons alive to exit by design,
# and their housekeeping would bury anything of ours. The target here is
# memory errors and undefined arithmetic, which ASan and UBSan report exactly.
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build="$here/build-sanitize"
cmake -S "$here" -B "$build" -G Ninja -DTF_SANITIZE=ON >/dev/null
cmake --build "$build"
( cd "$build" && ASAN_OPTIONS=detect_leaks=0 ctest --output-on-failure )
