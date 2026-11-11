#!/bin/sh
# run-tests.sh -- the whole suite, in one command.
#
# This is TenForge's `npm test`: "run the tests" is one thing a person, a
# hook or CI can do without knowing which build directory is current or
# which tests need a display.
#
# There is one half only. TenForge builds against no other project's
# checkout (SurView, OpenCorr and TenForge are independent kits), so there
# is no pinned engine suite to run beside it.
#
# Usage: tools/run-tests.sh [-q]
#   -q  quieter: only failures and the summary.
#
# Exits 0 only if every test passes.

set -eu

here=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build="$here/build-ninja"

quiet=0
[ "${1:-}" = "-q" ] && quiet=1
say() { [ "$quiet" -eq 1 ] || printf '%s\n' "$*"; }

# No display handling here on purpose: the walkthrough tests carry their own
# private X server (tests/CMakeLists.txt), so the suite is headless however
# it is started.

say "=== No em-dashes ==="
"$here/tools/check-no-em-dashes.sh"

say "=== Building TenForge and its tests ==="
cmake -S "$here" -B "$build" -G Ninja >/dev/null
cmake --build "$build"

say ""
say "=== TenForge test suite ==="
status=0
( cd "$build" && ctest --output-on-failure ) || status=$?

echo ""
if [ "$status" -eq 0 ]; then
    echo "ALL TESTS PASSED"
    exit 0
fi
echo "FAILED: TenForge test suite"
exit 1
