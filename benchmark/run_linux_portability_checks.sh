#!/bin/sh
set -eu

repository_root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
architecture=${1:-arm64}

case "$architecture" in
    arm64|amd64) ;;
    *)
        printf 'usage: %s [arm64|amd64]\n' "$0" >&2
        exit 2
        ;;
esac

platform="linux/$architecture"
image="smave-linux-ci-$architecture"
evidence_directory="$repository_root/build/portability/linux-$architecture"
evidence_path="$evidence_directory/evidence.txt"
mkdir -p "$evidence_directory"

docker build \
    --platform "$platform" \
    --file "$repository_root/benchmark/Dockerfile.linux-ci" \
    --tag "$image" \
    "$repository_root/benchmark" >/dev/null

{
    printf 'SMAVE_LINUX_PORTABILITY_CHECK 1\n'
    printf 'requested_platform=%s\n' "$platform"
    printf 'performance_evidence=0\n'
    docker run --rm --platform "$platform" \
        --volume "$repository_root:/src:ro" \
        "$image" bash -lc '
            set -e
            printf "reported_machine=%s\n" "$(uname -m)"
            printf "operating_system=%s\n" "$(. /etc/os-release; printf "%s %s" "$NAME" "$VERSION_ID")"
            printf "c_compiler=%s\n" "$(cc --version | head -n 1)"
            printf "cxx_compiler=%s\n" "$(c++ --version | head -n 1)"
            printf "cmake=%s\n" "$(cmake --version | head -n 1)"
            cmake -S /src -B /tmp/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
                >/tmp/configure.log
            cmake --build /tmp/build -j2 >/tmp/build.log
            ctest --test-dir /tmp/build --output-on-failure -j2
            printf "process_isolation_harness=passed\n"
        '
    printf 'END\n'
} | tee "$evidence_path"
