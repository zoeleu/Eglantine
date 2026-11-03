#!/usr/bin/env bash
# Local clang PGO + ThinLTO + -march=native build, in three steps:
#   tools/pgo.sh gen    build an instrumented binary in build-pgo-gen
#   (run build-pgo-gen/bin/eglantine, play, then quit the game and the emulator cleanly;
#    the profile is only written on a normal exit)
#   tools/pgo.sh merge  merge the raw profiles into eglantine.profdata
#   tools/pgo.sh use    build the optimized binary in build-pgo-use
# The profile is tied to the exact source and to this clang version. Regenerate it after
# larger code changes.
set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
profraw_dir="$root/build-pgo-gen/profraw"
profdata="$root/eglantine.profdata"
jobs=${JOBS:-$(nproc)}

export CCACHE_BASEDIR=${CCACHE_BASEDIR:-$(dirname "$root")}

common=(
    -G Ninja -S "$root"
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
    -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
    -DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld
    -DCMAKE_SHARED_LINKER_FLAGS=-fuse-ld=lld
    -DYUZU_TESTS=OFF
    -DENABLE_WERROR=OFF
    -DENABLE_UPDATE_CHECKER=OFF
    -DUSE_DISCORD_PRESENCE=OFF
)
native="-march=native -mtune=native -flto=thin"

case "${1:-}" in
gen)
    rm -rf "$profraw_dir"
    flags="$native -fprofile-generate=$profraw_dir"
    cmake -B "$root/build-pgo-gen" "${common[@]}" \
        "-DCMAKE_C_FLAGS=$flags" "-DCMAKE_CXX_FLAGS=$flags"
    ninja -C "$root/build-pgo-gen" -j"$jobs" eglantine eden-cli
    echo "Run: LLVM_PROFILE_FILE='$profraw_dir/%p-%m.profraw' $root/build-pgo-gen/bin/eglantine"
    ;;
merge)
    shopt -s nullglob
    raws=("$profraw_dir"/*.profraw)
    ((${#raws[@]})) || { echo "no .profraw files in $profraw_dir" >&2; exit 1; }
    llvm-profdata merge -output="$profdata" "${raws[@]}"
    echo "wrote $profdata"
    ;;
use)
    [[ -s $profdata ]] || { echo "missing $profdata, run merge first" >&2; exit 1; }
    flags="$native -fprofile-use=$profdata -Wno-backend-plugin \
-Wno-profile-instr-unprofiled -Wno-profile-instr-out-of-date"
    cmake -B "$root/build-pgo-use" "${common[@]}" \
        "-DCMAKE_C_FLAGS=$flags" "-DCMAKE_CXX_FLAGS=$flags"
    ninja -C "$root/build-pgo-use" -j"$jobs" eglantine eden-cli
    ;;
*)
    echo "usage: $0 gen|merge|use" >&2
    exit 1
    ;;
esac
