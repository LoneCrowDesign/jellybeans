#!/usr/bin/env bash
# Validate the registry, verify the generated artifacts match it, and run the
# host and manifest tests.
#
# Firmware carries a generated copy of roost_registry.h and the pipeline a
# generated manifest.schema.json; both must match the registry they were
# generated from. Regenerating and diffing here is what catches a hand edit to
# either. Run it before any commit that touches registry/.
set -euo pipefail

cd "$(dirname "$0")/.."
BUILD="${TMPDIR:-/tmp}/roost_registry_test"
BUILD_ALIGN="${TMPDIR:-/tmp}/roost_row_alignment_test"
BUILD_DERIV="${TMPDIR:-/tmp}/roost_runtime_derivations_test"

echo "== registry validation and drift check"
python3 tools/gen_registry.py --check \
    --out generated/roost_registry.h \
    --schema-out generated/manifest.schema.json

echo
echo "== host test (generated header)"
g++ -std=c++17 -Wall -Wextra -Werror -Igenerated \
    tests/test_registry_header.cpp -o "$BUILD"
"$BUILD"

echo
echo "== row alignment (every record, every emptiness pattern)"
g++ -std=c++17 -Wall -Wextra -Werror -Igenerated \
    tests/test_row_alignment.cpp -o "$BUILD_ALIGN"
"$BUILD_ALIGN"

echo
echo "== runtime derivations (shared computations, tested here not per device)"
g++ -std=c++17 -Wall -Wextra -Werror -Igenerated -Iruntime \
    tests/test_runtime_derivations.cpp -o "$BUILD_DERIV"
"$BUILD_DERIV"

echo
echo "== board profiles (capability masks derived from compile-time macros)"
./tests/board_profiles.sh

echo
echo "== manifest test (generated schema)"
python3 tests/test_manifest.py

echo
echo "== rendered manifest (the shared renderer, against the generated schema)"
./tests/render_manifest.sh
