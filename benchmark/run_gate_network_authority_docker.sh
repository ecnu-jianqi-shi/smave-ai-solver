#!/bin/sh
set -eu

repository_root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
architecture=${1:-arm64}
case "$architecture" in
    arm64|amd64) ;;
    *) printf 'usage: %s [arm64|amd64]\n' "$0" >&2; exit 2 ;;
esac

platform="linux/$architecture"
image="smave-linux-ci-$architecture"
suffix="$$"
network="smave-gate-authority-$architecture-$suffix"
server="smave-gate-authority-server-$architecture-$suffix"
evidence_directory="$repository_root/build/network-authority/linux-$architecture"
build_directory="$repository_root/build/network-authority/docker-build-$architecture"
evidence_path="$evidence_directory/evidence.txt"
state_path="$evidence_directory/transaction-state.txt"
mirror_path="$evidence_directory/mirror/transaction-state.txt"
key_path="$evidence_directory/authority.key"
rotated_key_path="$evidence_directory/rotated-authority.key"
wrong_key_path="$evidence_directory/wrong-authority.key"
witness_path="$evidence_directory/monotonic-witness.txt"
witness_key_path="$evidence_directory/witness.key"
rotated_witness_key_path="$evidence_directory/rotated-witness.key"
wrong_witness_key_path="$evidence_directory/wrong-witness.key"
mkdir -p "$evidence_directory"
rm -rf "$build_directory"
mkdir -p "$build_directory"
rm -rf "$evidence_directory/mirror" \
    "$evidence_directory/wrong-key-failure" \
    "$evidence_directory/all-generation-failure" \
    "$evidence_directory/fork-failure" \
    "$evidence_directory/rollback-failure" \
    "$evidence_directory/wrong-witness-key-failure" \
    "$evidence_directory/witness-lag-crash" \
    "$evidence_directory/key-rotation-before" \
    "$evidence_directory/key-rotation-after" \
    "$evidence_directory/revoked-key-failure" \
    "$evidence_directory/key-epoch-collision-failure" \
    "$evidence_directory/key-domain-collision-failure" \
    "$evidence_directory/key-symlink-failure" \
    "$evidence_directory/key-permission-failure" \
    "$evidence_directory/short-key-failure"
rm -f "$state_path" "$state_path.previous" "$evidence_path" \
    "$key_path" "$rotated_key_path" "$wrong_key_path" \
    "$witness_path" "$witness_path.previous" \
    "$witness_key_path" "$rotated_witness_key_path" "$wrong_witness_key_path" \
    "$evidence_directory/witness-lag-recovery.txt" \
    "$evidence_directory/key-rotation-recovery.txt"
printf '%s\n' 'smave-authority-fixture-key-2026-07-25-0001' > "$key_path"
printf '%s\n' 'smave-authority-rotated-fixture-key-2026-07-25-0005' > "$rotated_key_path"
printf '%s\n' 'smave-authority-wrong-key-2026-07-25-0000002' > "$wrong_key_path"
printf '%s\n' 'smave-authority-monotonic-witness-key-2026-07-25-0003' > "$witness_key_path"
printf '%s\n' 'smave-authority-rotated-witness-key-2026-07-25-0006' > "$rotated_witness_key_path"
printf '%s\n' 'smave-authority-wrong-witness-key-2026-07-25-0004' > "$wrong_witness_key_path"
chmod 0600 "$key_path" "$rotated_key_path" "$wrong_key_path" \
    "$witness_key_path" "$rotated_witness_key_path" "$wrong_witness_key_path"

cleanup() {
    docker rm -f "$server" >/dev/null 2>&1 || true
    docker network rm "$network" >/dev/null 2>&1 || true
}
trap cleanup EXIT INT TERM

docker build --platform "$platform" \
    --file "$repository_root/benchmark/Dockerfile.linux-ci" \
    --tag "$image" "$repository_root/benchmark" >/dev/null
docker run --rm --platform "$platform" \
    --volume "$repository_root:/src:ro" \
    --volume "$build_directory:/build" \
    "$image" bash -lc '
        set -e
        cmake -S /src -B /build -G Ninja -DCMAKE_BUILD_TYPE=Release >/tmp/configure.log
        cmake --build /build --target smave_gate_network_authority_evidence -j2 >/tmp/build.log
    '
docker network create "$network" >/dev/null

start_server() {
    server_state=${1:-/state/transaction-state.txt}
    server_mirror=${2:-/state/mirror/transaction-state.txt}
    server_key=${3:-/state/authority.key}
    server_witness=${4:-/state/monotonic-witness.txt}
    server_witness_key=${5:-/state/witness.key}
    server_previous_key=${6:--}
    server_previous_witness_key=${7:--}
    docker run --detach --platform "$platform" --name "$server" \
        --network "$network" \
        --volume "$repository_root:/src:ro" \
        --volume "$build_directory:/build:ro" \
        --volume "$evidence_directory:/state" \
        --env STATE_PATH="$server_state" \
        --env MIRROR_PATH="$server_mirror" \
        --env KEY_PATH="$server_key" \
        --env PREVIOUS_KEY_PATH="$server_previous_key" \
        --env WITNESS_PATH="$server_witness" \
        --env WITNESS_KEY_PATH="$server_witness_key" \
        --env PREVIOUS_WITNESS_KEY_PATH="$server_previous_witness_key" \
        "$image" bash -lc '
            set -e
            exec /build/smave_gate_network_authority_evidence server \
                /src/examples/OperatorPoissonGrid10.mo \
                /src/examples/CubicCoupled.mo 38191 \
                "$STATE_PATH" "$MIRROR_PATH" "$KEY_PATH" \
                "$PREVIOUS_KEY_PATH" "$WITNESS_PATH" "$WITNESS_KEY_PATH" \
                "$PREVIOUS_WITNESS_KEY_PATH"
        ' >/dev/null
}

run_client_phase() {
    phase=$1
    docker run --rm --platform "$platform" --network "$network" \
        --volume "$repository_root:/src:ro" \
        --volume "$build_directory:/build:ro" \
        --volume "$evidence_directory:/evidence" \
        "$image" bash -lc "
            set -e
            /build/smave_gate_network_authority_evidence client \
                $phase $server 38191 \
                /src/examples/OperatorPoissonGrid10.mo \
                /src/examples/operator-scenarios \
                /src/examples/CubicCoupled.mo \
                /evidence/evidence.txt docker-bridge-two-container 1
        "
}

wait_server() {
    expected=$1
    label=$2
    status=$(docker wait "$server")
    if [ "$status" -ne "$expected" ]; then
        docker logs "$server" >&2 || true
        printf 'expected %s exit %s, observed %s\n' "$label" "$expected" "$status" >&2
        exit 2
    fi
    docker rm "$server" >/dev/null
}

corrupt_checksums() {
    python3 - "$@" <<'PY'
import pathlib
import sys
for argument in sys.argv[1:]:
    path = pathlib.Path(argument)
    contents = path.read_text()
    marker = "state_sha256="
    start = contents.index(marker) + len(marker)
    replacement = "0" if contents[start] != "0" else "1"
    path.write_text(contents[:start] + replacement + contents[start + 1:])
PY
}

forge_plain_checksums() {
    python3 - "$@" <<'PY'
import hashlib
import pathlib
import sys
for argument in sys.argv[1:]:
    path = pathlib.Path(argument)
    contents = path.read_text()
    marker = "complete_requests="
    start = contents.index(marker) + len(marker)
    end = contents.index("\n", start)
    contents = contents[:start] + str(int(contents[start:end]) + 1000) + contents[end:]
    header = "SMAVE_GATE_TXN_STATE 6\n"
    checksum_marker = "state_sha256="
    checksum_position = contents.index(checksum_marker)
    checksum = hashlib.sha256(contents[len(header):checksum_position].encode()).hexdigest()
    checksum_start = checksum_position + len(checksum_marker)
    checksum_end = contents.index("\n", checksum_start)
    path.write_text(contents[:checksum_start] + checksum + contents[checksum_end:])
PY
}

forge_authenticated_fork() {
    python3 - "$1" "$2" <<'PY'
import hashlib
import hmac
import pathlib
import sys
path = pathlib.Path(sys.argv[1])
key = sys.argv[2].encode()
contents = path.read_text()
header = "SMAVE_GATE_TXN_STATE 6\n"
checksum_marker = "state_sha256="
checksum_position = contents.index(checksum_marker)
body = contents[len(header):checksum_position]
marker = "complete_requests="
start = body.index(marker) + len(marker)
end = body.index("\n", start)
body = body[:start] + str(int(body[start:end]) + 1000) + body[end:]
path.write_text(
    header + body
    + checksum_marker + hashlib.sha256(body.encode()).hexdigest() + "\n"
    + "key_id=" + hashlib.sha256(key).hexdigest()[:16] + "\n"
    + "state_hmac_sha256=" + hmac.new(key, body.encode(), hashlib.sha256).hexdigest()
    + "\nEND\n"
)
PY
}

start_server
run_client_phase before-restart
wait_server 86 commit-crash
start_server
run_client_phase after-durable-restart
wait_server 87 prepublication-crash
start_server
run_client_phase after-torn-restart
wait_server 0 torn-recovery

start_server
run_client_phase before-witness-lag-restart
wait_server 89 witness-lag-crash

witness_lag_directory="$evidence_directory/witness-lag-crash"
mkdir -p "$witness_lag_directory/mirror"
cp "$state_path" "$witness_lag_directory/transaction-state.txt"
cp "$state_path.previous" "$witness_lag_directory/transaction-state.txt.previous"
cp "$mirror_path" "$witness_lag_directory/mirror/transaction-state.txt"
cp "$mirror_path.previous" "$witness_lag_directory/mirror/transaction-state.txt.previous"
cp "$witness_path" "$witness_lag_directory/monotonic-witness.txt"
cp "$witness_path.previous" "$witness_lag_directory/monotonic-witness.txt.previous"
set -- "$evidence_directory"/monotonic-witness.txt.tmp.*
test "$#" -eq 1
test -f "$1"
cp "$1" "$witness_lag_directory/prepared-witness.txt"

start_server
run_client_phase after-witness-lag-restart
wait_server 0 witness-lag-recovery
test -f "$evidence_directory/witness-lag-recovery.txt"

corrupt_checksums "$state_path"
start_server
run_client_phase after-corruption-restart
wait_server 0 checksum-recovery

stale_highest="$evidence_directory/stale-highest.snapshot"
stale_current="$evidence_directory/stale-current.snapshot"
cp "$mirror_path" "$stale_highest"
cp "$mirror_path.previous" "$stale_current"
cp "$stale_current" "$state_path"
cp "$stale_highest" "$state_path.previous"
cp "$stale_current" "$mirror_path"
cp "$stale_highest" "$mirror_path.previous"
rm -f "$stale_highest" "$stale_current"
start_server
run_client_phase after-stale-restart
wait_server 0 stale-recovery

forge_plain_checksums "$state_path" "$state_path.previous"
start_server
run_client_phase after-mirror-recovery
wait_server 0 authenticated-mirror-recovery

key_rotation_before="$evidence_directory/key-rotation-before"
mkdir -p "$key_rotation_before/mirror"
cp "$state_path" "$key_rotation_before/transaction-state.txt"
cp "$state_path.previous" "$key_rotation_before/transaction-state.txt.previous"
cp "$mirror_path" "$key_rotation_before/mirror/transaction-state.txt"
cp "$mirror_path.previous" "$key_rotation_before/mirror/transaction-state.txt.previous"
cp "$witness_path" "$key_rotation_before/monotonic-witness.txt"
cp "$witness_path.previous" "$key_rotation_before/monotonic-witness.txt.previous"

start_server /state/transaction-state.txt /state/mirror/transaction-state.txt \
    /state/rotated-authority.key /state/monotonic-witness.txt \
    /state/rotated-witness.key /state/authority.key /state/witness.key
attempt=0
while [ ! -f "$evidence_directory/key-rotation-recovery.txt" ]; do
    attempt=$((attempt + 1))
    if [ "$attempt" -ge 600 ]; then
        printf 'key rotation recovery record is missing\n' >&2
        exit 2
    fi
    running=$(docker inspect --format '{{.State.Running}}' "$server" 2>/dev/null || true)
    if [ "$running" != true ]; then
        docker logs "$server" >&2 || true
        printf 'key rotation server exited before record\n' >&2
        exit 2
    fi
    sleep 0.1
done
key_rotation_after="$evidence_directory/key-rotation-after"
mkdir -p "$key_rotation_after/mirror"
cp "$state_path" "$key_rotation_after/transaction-state.txt"
cp "$state_path.previous" "$key_rotation_after/transaction-state.txt.previous"
cp "$mirror_path" "$key_rotation_after/mirror/transaction-state.txt"
cp "$mirror_path.previous" "$key_rotation_after/mirror/transaction-state.txt.previous"
cp "$witness_path" "$key_rotation_after/monotonic-witness.txt"
cp "$witness_path.previous" "$key_rotation_after/monotonic-witness.txt.previous"
run_client_phase after-key-rotation
wait_server 0 fixture-key-rotation

start_server /state/transaction-state.txt /state/mirror/transaction-state.txt \
    /state/rotated-authority.key /state/monotonic-witness.txt \
    /state/rotated-witness.key
run_client_phase after-key-revocation
wait_server 0 new-key-only-restart

revoked_directory="$evidence_directory/revoked-key-failure"
mkdir -p "$revoked_directory/mirror"
cp "$key_rotation_before/transaction-state.txt" \
    "$revoked_directory/transaction-state.txt"
cp "$key_rotation_before/transaction-state.txt.previous" \
    "$revoked_directory/transaction-state.txt.previous"
cp "$key_rotation_before/mirror/transaction-state.txt" \
    "$revoked_directory/mirror/transaction-state.txt"
cp "$key_rotation_before/mirror/transaction-state.txt.previous" \
    "$revoked_directory/mirror/transaction-state.txt.previous"
cp "$key_rotation_before/monotonic-witness.txt" \
    "$revoked_directory/monotonic-witness.txt"
start_server /state/revoked-key-failure/transaction-state.txt \
    /state/revoked-key-failure/mirror/transaction-state.txt \
    /state/rotated-authority.key \
    /state/revoked-key-failure/monotonic-witness.txt \
    /state/rotated-witness.key
wait_server 88 revoked-key-recovery

start_server /state/key-epoch-collision-failure/transaction-state.txt \
    /state/key-epoch-collision-failure/mirror/transaction-state.txt \
    /state/rotated-authority.key \
    /state/key-epoch-collision-failure/monotonic-witness.txt \
    /state/rotated-witness.key \
    /state/rotated-authority.key
wait_server 88 key-epoch-collision

start_server /state/key-domain-collision-failure/transaction-state.txt \
    /state/key-domain-collision-failure/mirror/transaction-state.txt \
    /state/rotated-authority.key \
    /state/key-domain-collision-failure/monotonic-witness.txt \
    /state/rotated-authority.key
wait_server 88 key-domain-collision

key_symlink_directory="$evidence_directory/key-symlink-failure"
mkdir -p "$key_symlink_directory"
ln -s ../rotated-authority.key "$key_symlink_directory/authority.key"
start_server /state/key-symlink-failure/transaction-state.txt \
    /state/key-symlink-failure/mirror/transaction-state.txt \
    /state/key-symlink-failure/authority.key \
    /state/key-symlink-failure/monotonic-witness.txt \
    /state/rotated-witness.key
wait_server 88 key-symlink-policy

key_permission_directory="$evidence_directory/key-permission-failure"
mkdir -p "$key_permission_directory"
printf '%s\n' 'smave-authority-rotated-fixture-key-2026-07-25-0005' \
    > "$key_permission_directory/authority.key"
chmod 0644 "$key_permission_directory/authority.key"
start_server /state/key-permission-failure/transaction-state.txt \
    /state/key-permission-failure/mirror/transaction-state.txt \
    /state/key-permission-failure/authority.key \
    /state/key-permission-failure/monotonic-witness.txt \
    /state/rotated-witness.key
wait_server 88 key-permission-policy

short_key_directory="$evidence_directory/short-key-failure"
mkdir -p "$short_key_directory"
printf '%s\n' 'short-fixture-key' > "$short_key_directory/authority.key"
chmod 0600 "$short_key_directory/authority.key"
start_server /state/short-key-failure/transaction-state.txt \
    /state/short-key-failure/mirror/transaction-state.txt \
    /state/short-key-failure/authority.key \
    /state/short-key-failure/monotonic-witness.txt \
    /state/rotated-witness.key
wait_server 88 short-key-policy

wrong_directory="$evidence_directory/wrong-key-failure"
wrong_state="$wrong_directory/transaction-state.txt"
wrong_mirror="$wrong_directory/mirror/transaction-state.txt"
mkdir -p "$wrong_directory/mirror"
cp "$state_path" "$wrong_state"
cp "$state_path.previous" "$wrong_state.previous"
cp "$mirror_path" "$wrong_mirror"
cp "$mirror_path.previous" "$wrong_mirror.previous"
cp "$witness_path" "$wrong_directory/monotonic-witness.txt"
start_server /state/wrong-key-failure/transaction-state.txt \
    /state/wrong-key-failure/mirror/transaction-state.txt \
    /state/wrong-authority.key \
    /state/wrong-key-failure/monotonic-witness.txt \
    /state/rotated-witness.key
wait_server 88 wrong-key-recovery

failure_directory="$evidence_directory/all-generation-failure"
failure_state="$failure_directory/transaction-state.txt"
failure_mirror="$failure_directory/mirror/transaction-state.txt"
mkdir -p "$failure_directory/mirror"
cp "$state_path" "$failure_state"
cp "$state_path.previous" "$failure_state.previous"
cp "$mirror_path" "$failure_mirror"
cp "$mirror_path.previous" "$failure_mirror.previous"
cp "$witness_path" "$failure_directory/monotonic-witness.txt"
corrupt_checksums "$failure_state" "$failure_state.previous" \
    "$failure_mirror" "$failure_mirror.previous"
start_server /state/all-generation-failure/transaction-state.txt \
    /state/all-generation-failure/mirror/transaction-state.txt \
    /state/rotated-authority.key \
    /state/all-generation-failure/monotonic-witness.txt \
    /state/rotated-witness.key
wait_server 88 all-generation-recovery

fork_directory="$evidence_directory/fork-failure"
fork_state="$fork_directory/transaction-state.txt"
fork_mirror="$fork_directory/mirror/transaction-state.txt"
mkdir -p "$fork_directory/mirror"
cp "$state_path" "$fork_state"
cp "$state_path.previous" "$fork_state.previous"
cp "$mirror_path" "$fork_mirror"
cp "$mirror_path.previous" "$fork_mirror.previous"
cp "$witness_path" "$fork_directory/monotonic-witness.txt"
forge_authenticated_fork "$fork_mirror" \
    'smave-authority-rotated-fixture-key-2026-07-25-0005'
start_server /state/fork-failure/transaction-state.txt \
    /state/fork-failure/mirror/transaction-state.txt \
    /state/rotated-authority.key \
    /state/fork-failure/monotonic-witness.txt \
    /state/rotated-witness.key
wait_server 88 authenticated-fork-recovery

rollback_directory="$evidence_directory/rollback-failure"
rollback_state="$rollback_directory/transaction-state.txt"
rollback_mirror="$rollback_directory/mirror/transaction-state.txt"
mkdir -p "$rollback_directory/mirror"
cp "$state_path.previous" "$rollback_state"
cp "$state_path.previous" "$rollback_state.previous"
cp "$state_path.previous" "$rollback_mirror"
cp "$state_path.previous" "$rollback_mirror.previous"
cp "$witness_path" "$rollback_directory/monotonic-witness.txt"
start_server /state/rollback-failure/transaction-state.txt \
    /state/rollback-failure/mirror/transaction-state.txt \
    /state/rotated-authority.key \
    /state/rollback-failure/monotonic-witness.txt \
    /state/rotated-witness.key
wait_server 88 monotonic-rollback-recovery

wrong_witness_directory="$evidence_directory/wrong-witness-key-failure"
wrong_witness_state="$wrong_witness_directory/transaction-state.txt"
wrong_witness_mirror="$wrong_witness_directory/mirror/transaction-state.txt"
mkdir -p "$wrong_witness_directory/mirror"
cp "$state_path" "$wrong_witness_state"
cp "$state_path.previous" "$wrong_witness_state.previous"
cp "$mirror_path" "$wrong_witness_mirror"
cp "$mirror_path.previous" "$wrong_witness_mirror.previous"
cp "$witness_path" "$wrong_witness_directory/monotonic-witness.txt"
start_server /state/wrong-witness-key-failure/transaction-state.txt \
    /state/wrong-witness-key-failure/mirror/transaction-state.txt \
    /state/rotated-authority.key \
    /state/wrong-witness-key-failure/monotonic-witness.txt \
    /state/wrong-witness.key
wait_server 88 wrong-witness-key-recovery

python3 "$repository_root/tests/verify_gate_network_authority.py" \
    --evidence "$evidence_path" --expect-docker-bridge
printf 'network_authority_container_platform=%s\n' "$platform"
printf 'performance_evidence=0\n'
cat "$evidence_path"
