#!/bin/sh
set -eu

test_root=$(mktemp -d)
trap 'rm -rf "$test_root"' EXIT
test_repo="$test_root/project with spaces"
mkdir -p "$test_repo" "$test_root/bin"
cp "$1/build.sh" "$1/CMakePresets.json" "$test_repo/"

# Exercise the real CMake build command without compiling the application.
cat > "$test_repo/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.25)
project(BuildScriptTest NONE)
add_custom_target(record_parallelism ALL
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_SOURCE_DIR}/record.cmake")
EOF
cat > "$test_repo/record.cmake" <<'EOF'
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/makeflags.txt" "$ENV{MAKEFLAGS}")
EOF
cat > "$test_root/bin/nproc" <<'EOF'
#!/bin/sh
printf '3\n'
EOF
chmod +x "$test_root/bin/nproc"
PATH="$test_root/bin:$PATH"
export PATH
unset MAKEFLAGS MFLAGS

check_jobs() {
    if ! sh "$test_repo/build.sh" > "$test_root/output" 2>&1; then
        cat "$test_root/output"
        exit 1
    fi
    flags=$(cat "$test_repo/build/release/makeflags.txt")
    case " $flags " in
        *" -j$1 "*) ;;
        *) printf 'FAIL expected -j%s, got MAKEFLAGS=%s\n' "$1" "$flags" >&2; exit 1 ;;
    esac
}

CMAKE_BUILD_PARALLEL_LEVEL=2
export CMAKE_BUILD_PARALLEL_LEVEL
check_jobs 2
unset CMAKE_BUILD_PARALLEL_LEVEL
check_jobs 3

CMAKE_BUILD_PARALLEL_LEVEL=
export CMAKE_BUILD_PARALLEL_LEVEL
cmake --build "$test_repo/build/release" > "$test_root/output" 2>&1
native_flags=$(cat "$test_repo/build/release/makeflags.txt")
sh "$test_repo/build.sh" > "$test_root/output" 2>&1
flags=$(cat "$test_repo/build/release/makeflags.txt")
if [ "$flags" != "$native_flags" ]; then
    printf 'FAIL an empty parallelism variable must preserve CMake defaults\n' >&2
    exit 1
fi

printf 'PASS build parallelism overrides and defaults\n'
