#!/bin/sh
# Unit tests and static analysis for the ZionOS applications.
# Runs inside the builder container (use: scripts/test.sh unit).
#   ZIONOS_GUI=OFF   build only the core library and its tests
#   QUICK=1          skip cppcheck
set -eu
SRC=/src/packages/zionos-apps
B=/src/work/build-unit
mkdir -p "$B"
cmake -S "$SRC" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DZIONOS_SANITIZE=ON \
      -DZIONOS_WERROR=ON -DZIONOS_BUILD_GUI="${ZIONOS_GUI:-ON}" \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON > "$B/cmake.log" 2>&1 || { tail -40 "$B/cmake.log"; exit 1; }
if ! cmake --build "$B" > "$B/build.log" 2>&1; then
    grep -E "error|FAILED:" -A8 "$B/build.log" | grep -v "^/usr/bin/c++" | head -150
    exit 1
fi
grep -c "warning:" "$B/build.log" | sed 's/^/compiler warnings: /'
export ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
(cd "$B" && ctest -LE live --output-on-failure --timeout 180) || exit 1
if [ -z "${QUICK:-}" ]; then
    echo "== cppcheck =="
    cppcheck --quiet --std=c++20 --enable=warning,performance,portability --inline-suppr \
        --error-exitcode=1 --suppress=missingIncludeSystem --suppress=unknownMacro --suppress=normalCheckLevelMaxBranches \
        -I "$SRC" "$SRC/core" $( [ "${ZIONOS_GUI:-ON}" = ON ] && echo "$SRC/ui $SRC/welcome $SRC/settings $SRC/assistant" )
    echo "cppcheck: clean"
fi
