#!/usr/bin/env bash
# Every public header must compile as its own translation unit - no reliance
# on transitive includes (round 6; pose.hpp used std::array without <array>
# and aimer.hpp used std::string without <string> for years).
# Usage: check_headers.sh CORE_INCLUDE SENTRY_INCLUDE [EXTRA_INCLUDE ...]
# All arguments are added as -I paths; headers are scanned under the first two.
set -eu
CXX="${CXX:-c++}"
roots=("$1" "$2")
incs=()
for d in "$@"; do incs+=("-I$d"); done
tmpdir=$(mktemp -d)
trap 'rm -rf -- "$tmpdir"' EXIT
for root in "${roots[@]}"; do
    if [[ ! -d "$root" ]]; then
        echo "Missing public include directory: $root" >&2
        exit 1
    fi
done
find "${roots[@]}" -type f -name '*.hpp' -print0 > "$tmpdir/headers"
if [[ ! -s "$tmpdir/headers" ]]; then
    echo "No public headers found" >&2
    exit 1
fi
fail=0
count=0
while IFS= read -r -d '' h; do
    count=$((count + 1))
    if ! "$CXX" -std=c++17 -fsyntax-only "${incs[@]}" "$h" 2>"$tmpdir/errors"; then
        echo "NOT self-contained: $h"
        head -5 "$tmpdir/errors"
        fail=1
    fi
done < "$tmpdir/headers"
echo "checked $count headers, failures: $fail"
exit $fail
