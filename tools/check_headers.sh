#!/usr/bin/env bash
# Every public header must compile as its own translation unit - no reliance
# on transitive includes (round 6; pose.hpp used std::array without <array>
# and aimer.hpp used std::string without <string> for years).
# Usage: check_headers.sh CORE_INCLUDE SENTRY_INCLUDE [EXTRA_INCLUDE ...]
# All arguments are added as -I paths; headers are scanned under the first two.
set -u
CXX="${CXX:-c++}"
roots=("$1" "$2")
incs=()
for d in "$@"; do incs+=("-I$d"); done
fail=0
count=0
while IFS= read -r h; do
    count=$((count + 1))
    if ! "$CXX" -std=c++17 -fsyntax-only "${incs[@]}" "$h" 2>/tmp/check_headers_err.txt; then
        echo "NOT self-contained: $h"
        head -5 /tmp/check_headers_err.txt
        fail=1
    fi
done < <(find "${roots[@]}" -name '*.hpp' | sort)
echo "checked $count headers, failures: $fail"
exit $fail
