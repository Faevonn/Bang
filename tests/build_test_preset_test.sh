#!/bin/sh
set -eu

test_root=$(mktemp -d)
trap 'rm -rf "$test_root"' EXIT
test_repo="$test_root/project with spaces"
mkdir -p "$test_repo"
cp "$1/build.sh" "$1/CMakePresets.json" "$test_repo/"
CMAKE_BUILD_PARALLEL_LEVEL=2
export CMAKE_BUILD_PARALLEL_LEVEL

cat > "$test_repo/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.25)
project(BuildTestPreset NONE)
include(CTest)
EOF

if sh "$test_repo/build.sh" test > "$test_root/output" 2>&1; then
    printf 'FAIL build.sh test reported success with no tests\n' >&2
    exit 1
fi
case "$(cat "$test_root/output")" in
    *'No tests were found'*) ;;
    *) cat "$test_root/output"; exit 1 ;;
esac

cat >> "$test_repo/CMakeLists.txt" <<'EOF'
add_test(NAME passing COMMAND "${CMAKE_COMMAND}" -E true)
EOF
if ! sh "$test_repo/build.sh" test > "$test_root/output" 2>&1; then
    cat "$test_root/output"
    exit 1
fi

cat >> "$test_repo/CMakeLists.txt" <<'EOF'
add_test(NAME intentional-failure COMMAND "${CMAKE_COMMAND}" -E false)
add_test(NAME after-failure
    COMMAND "${CMAKE_COMMAND}" -E touch "${CMAKE_BINARY_DIR}/ran-after-failure")
EOF
if sh "$test_repo/build.sh" test > "$test_root/output" 2>&1; then
    printf 'FAIL build.sh test ignored a failing test\n' >&2
    exit 1
fi
case "$(cat "$test_root/output")" in
    *'intentional-failure'*) ;;
    *) cat "$test_root/output"; exit 1 ;;
esac
if [ -e "$test_repo/build/debug/ran-after-failure" ]; then
    printf 'FAIL build.sh test ignored the preset stopOnFailure setting\n' >&2
    exit 1
fi
printf 'PASS test preset failure and success handling\n'
