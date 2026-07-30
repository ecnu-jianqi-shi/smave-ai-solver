#!/usr/bin/env python3

import argparse
import hashlib
import hmac
import shutil
import subprocess
import sys
import time
from pathlib import Path
from typing import Optional


AUTHENTICATION_KEY = "smave-authority-fixture-key-2026-07-25-0001"
ROTATED_AUTHENTICATION_KEY = "smave-authority-rotated-fixture-key-2026-07-25-0005"
WRONG_AUTHENTICATION_KEY = "smave-authority-wrong-key-2026-07-25-0000002"
WITNESS_KEY = "smave-authority-monotonic-witness-key-2026-07-25-0003"
ROTATED_WITNESS_KEY = "smave-authority-rotated-witness-key-2026-07-25-0006"
WRONG_WITNESS_KEY = "smave-authority-wrong-witness-key-2026-07-25-0004"


def run_phase(arguments: argparse.Namespace, phase: str) -> int:
    return subprocess.run(
        [
            arguments.executable,
            "client",
            phase,
            "127.0.0.1",
            arguments.port,
            arguments.linear,
            arguments.scenarios,
            arguments.nonlinear,
            arguments.output,
            "loopback-two-endpoint",
            "0",
        ],
        check=False,
    ).returncode


def corrupt_checksum(path: Path) -> None:
    contents = path.read_text()
    marker = "state_sha256="
    checksum_start = contents.index(marker) + len(marker)
    replacement = "0" if contents[checksum_start] != "0" else "1"
    path.write_text(
        contents[:checksum_start] + replacement + contents[checksum_start + 1 :]
    )


def forge_plain_checksum(path: Path) -> None:
    contents = path.read_text()
    marker = "complete_requests="
    value_start = contents.index(marker) + len(marker)
    value_end = contents.index("\n", value_start)
    value = str(int(contents[value_start:value_end]) + 1000)
    contents = contents[:value_start] + value + contents[value_end:]
    header = "SMAVE_GATE_TXN_STATE 6\n"
    checksum_marker = "state_sha256="
    body_start = len(header)
    checksum_position = contents.index(checksum_marker)
    checksum = hashlib.sha256(
        contents[body_start:checksum_position].encode()
    ).hexdigest()
    checksum_start = checksum_position + len(checksum_marker)
    checksum_end = contents.index("\n", checksum_start)
    path.write_text(contents[:checksum_start] + checksum + contents[checksum_end:])


def forge_authenticated_fork(path: Path, key: str) -> None:
    contents = path.read_text()
    header = "SMAVE_GATE_TXN_STATE 6\n"
    checksum_marker = "state_sha256="
    key_marker = "key_id="
    hmac_marker = "state_hmac_sha256="
    checksum_position = contents.index(checksum_marker)
    body = contents[len(header):checksum_position]
    marker = "complete_requests="
    value_start = body.index(marker) + len(marker)
    value_end = body.index("\n", value_start)
    body = body[:value_start] + str(int(body[value_start:value_end]) + 1000) + body[value_end:]
    key_bytes = key.encode()
    envelope = (
        header
        + body
        + checksum_marker
        + hashlib.sha256(body.encode()).hexdigest()
        + "\n"
        + key_marker
        + hashlib.sha256(key_bytes).hexdigest()[:16]
        + "\n"
        + hmac_marker
        + hmac.new(key_bytes, body.encode(), hashlib.sha256).hexdigest()
        + "\nEND\n"
    )
    path.write_text(envelope)


def stage_stale_current(state: Path, mirror: Path) -> None:
    mirror_previous = Path(f"{mirror}.previous")
    highest = mirror.read_bytes()
    stale = mirror_previous.read_bytes()
    state.write_bytes(stale)
    Path(f"{state}.previous").write_bytes(highest)
    mirror.write_bytes(stale)
    mirror_previous.write_bytes(highest)


def wait_for_status(process: subprocess.Popen, expected: int, label: str) -> bool:
    status = process.wait(timeout=10)
    if status == expected:
        return True
    print(f"expected {label} exit {expected}, observed {status}", file=sys.stderr)
    return False


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", required=True)
    parser.add_argument("--linear", required=True)
    parser.add_argument("--scenarios", required=True)
    parser.add_argument("--nonlinear", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--port", default="38191")
    arguments = parser.parse_args()

    subprocess.run(
        [arguments.executable, "crypto-self-test"],
        check=True,
    )

    output = Path(arguments.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    state = output.parent / "transaction-state.txt"
    mirror = output.parent / "mirror" / "transaction-state.txt"
    key = output.parent / "authority.key"
    rotated_key = output.parent / "rotated-authority.key"
    wrong_key = output.parent / "wrong-authority.key"
    witness = output.parent / "monotonic-witness.txt"
    witness_key = output.parent / "witness.key"
    rotated_witness_key = output.parent / "rotated-witness.key"
    wrong_witness_key = output.parent / "wrong-witness.key"
    all_failure_directory = output.parent / "all-generation-failure"
    wrong_key_directory = output.parent / "wrong-key-failure"
    fork_failure_directory = output.parent / "fork-failure"
    rollback_failure_directory = output.parent / "rollback-failure"
    wrong_witness_directory = output.parent / "wrong-witness-key-failure"
    witness_lag_directory = output.parent / "witness-lag-crash"
    key_rotation_directory = output.parent / "key-rotation-before"
    key_rotation_after_directory = output.parent / "key-rotation-after"
    revoked_key_directory = output.parent / "revoked-key-failure"
    key_epoch_collision_directory = output.parent / "key-epoch-collision-failure"
    key_domain_collision_directory = output.parent / "key-domain-collision-failure"
    key_symlink_failure_directory = output.parent / "key-symlink-failure"
    key_permission_failure_directory = output.parent / "key-permission-failure"
    short_key_failure_directory = output.parent / "short-key-failure"
    for directory in [
        mirror.parent,
        all_failure_directory,
        wrong_key_directory,
        fork_failure_directory,
        rollback_failure_directory,
        wrong_witness_directory,
        witness_lag_directory,
        key_rotation_directory,
        key_rotation_after_directory,
        revoked_key_directory,
        key_epoch_collision_directory,
        key_domain_collision_directory,
        key_symlink_failure_directory,
        key_permission_failure_directory,
        short_key_failure_directory,
    ]:
        shutil.rmtree(directory, ignore_errors=True)
    for path in [
        state, Path(f"{state}.previous"), output, key, rotated_key, wrong_key, witness,
        Path(f"{witness}.previous"),
        witness_key, rotated_witness_key, wrong_witness_key,
        output.parent / "witness-lag-recovery.txt",
        output.parent / "key-rotation-recovery.txt",
    ]:
        path.unlink(missing_ok=True)
    for pattern in ("transaction-state.txt*.tmp.*", "monotonic-witness.txt*.tmp.*"):
        for temporary in output.parent.rglob(pattern):
            temporary.unlink()
    key.write_text(AUTHENTICATION_KEY + "\n")
    rotated_key.write_text(ROTATED_AUTHENTICATION_KEY + "\n")
    wrong_key.write_text(WRONG_AUTHENTICATION_KEY + "\n")
    witness_key.write_text(WITNESS_KEY + "\n")
    rotated_witness_key.write_text(ROTATED_WITNESS_KEY + "\n")
    wrong_witness_key.write_text(WRONG_WITNESS_KEY + "\n")
    for key_path in [
        key,
        rotated_key,
        wrong_key,
        witness_key,
        rotated_witness_key,
        wrong_witness_key,
    ]:
        key_path.chmod(0o600)

    def server_command_for(
        server_state: Path = state,
        server_mirror: Path = mirror,
        current_key: Path = key,
        previous_key: Optional[Path] = None,
        server_witness: Path = witness,
        current_witness_key: Path = witness_key,
        previous_witness_key: Optional[Path] = None,
    ) -> list[str]:
        return [
            arguments.executable,
            "server",
            arguments.linear,
            arguments.nonlinear,
            arguments.port,
            str(server_state),
            str(server_mirror),
            str(current_key),
            str(previous_key) if previous_key is not None else "-",
            str(server_witness),
            str(current_witness_key),
            str(previous_witness_key) if previous_witness_key is not None else "-",
        ]

    server_command = server_command_for()
    server = subprocess.Popen(server_command)
    try:
        if run_phase(arguments, "before-restart") != 0:
            return 2
        if not wait_for_status(server, 86, "commit-crash"):
            return 2

        server = subprocess.Popen(server_command)
        if run_phase(arguments, "after-durable-restart") != 0:
            return 2
        if not wait_for_status(server, 87, "prepublication-crash"):
            return 2

        server = subprocess.Popen(server_command)
        if run_phase(arguments, "after-torn-restart") != 0:
            return 2
        if not wait_for_status(server, 0, "torn-recovery"):
            return 2

        server = subprocess.Popen(server_command)
        if run_phase(arguments, "before-witness-lag-restart") != 0:
            return 2
        if not wait_for_status(server, 89, "witness-lag crash"):
            return 2
        witness_lag_directory.mkdir()
        witness_lag_mirror = witness_lag_directory / "mirror" / "transaction-state.txt"
        witness_lag_mirror.parent.mkdir()
        for source, destination in zip(
            [state, Path(f"{state}.previous"), mirror, Path(f"{mirror}.previous")],
            [
                witness_lag_directory / "transaction-state.txt",
                witness_lag_directory / "transaction-state.txt.previous",
                witness_lag_mirror,
                Path(f"{witness_lag_mirror}.previous"),
            ],
        ):
            shutil.copy2(source, destination)
        shutil.copy2(witness, witness_lag_directory / "monotonic-witness.txt")
        shutil.copy2(
            Path(f"{witness}.previous"),
            witness_lag_directory / "monotonic-witness.txt.previous",
        )
        prepared_witnesses = list(
            output.parent.glob("monotonic-witness.txt.tmp.*")
        )
        if len(prepared_witnesses) != 1:
            print("one prepared witness is required after lag crash", file=sys.stderr)
            return 2
        shutil.copy2(
            prepared_witnesses[0], witness_lag_directory / "prepared-witness.txt"
        )

        server = subprocess.Popen(server_command)
        if run_phase(arguments, "after-witness-lag-restart") != 0:
            return 2
        if not wait_for_status(server, 0, "witness-lag recovery"):
            return 2
        if not (output.parent / "witness-lag-recovery.txt").exists():
            print("witness-lag recovery record is missing", file=sys.stderr)
            return 2

        corrupt_checksum(state)
        server = subprocess.Popen(server_command)
        if run_phase(arguments, "after-corruption-restart") != 0:
            return 2
        if not wait_for_status(server, 0, "checksum-recovery"):
            return 2

        stage_stale_current(state, mirror)
        server = subprocess.Popen(server_command)
        if run_phase(arguments, "after-stale-restart") != 0:
            return 2
        if not wait_for_status(server, 0, "stale-recovery"):
            return 2

        forge_plain_checksum(state)
        forge_plain_checksum(Path(f"{state}.previous"))
        server = subprocess.Popen(server_command)
        if run_phase(arguments, "after-mirror-recovery") != 0:
            return 2
        if not wait_for_status(server, 0, "authenticated-mirror-recovery"):
            return 2

        key_rotation_directory.mkdir()
        key_rotation_mirror = (
            key_rotation_directory / "mirror" / "transaction-state.txt"
        )
        key_rotation_mirror.parent.mkdir()
        pre_rotation_snapshots = [
            key_rotation_directory / "transaction-state.txt",
            key_rotation_directory / "transaction-state.txt.previous",
            key_rotation_mirror,
            Path(f"{key_rotation_mirror}.previous"),
        ]
        for source, destination in zip(
            [state, Path(f"{state}.previous"), mirror, Path(f"{mirror}.previous")],
            pre_rotation_snapshots,
        ):
            shutil.copy2(source, destination)
        shutil.copy2(witness, key_rotation_directory / "monotonic-witness.txt")
        shutil.copy2(
            Path(f"{witness}.previous"),
            key_rotation_directory / "monotonic-witness.txt.previous",
        )

        rotation_command = server_command_for(
            current_key=rotated_key,
            previous_key=key,
            current_witness_key=rotated_witness_key,
            previous_witness_key=witness_key,
        )
        server = subprocess.Popen(rotation_command)
        rotation_record = output.parent / "key-rotation-recovery.txt"
        for attempt in range(600):
            if rotation_record.exists():
                break
            if server.poll() is not None:
                print("key rotation server exited before record", file=sys.stderr)
                return 2
            if attempt == 599:
                print("key rotation recovery record is missing", file=sys.stderr)
                return 2
            time.sleep(0.1)
        key_rotation_after_directory.mkdir()
        key_rotation_after_mirror = (
            key_rotation_after_directory / "mirror" / "transaction-state.txt"
        )
        key_rotation_after_mirror.parent.mkdir()
        for source, destination in zip(
            [state, Path(f"{state}.previous"), mirror, Path(f"{mirror}.previous")],
            [
                key_rotation_after_directory / "transaction-state.txt",
                key_rotation_after_directory / "transaction-state.txt.previous",
                key_rotation_after_mirror,
                Path(f"{key_rotation_after_mirror}.previous"),
            ],
        ):
            shutil.copy2(source, destination)
        shutil.copy2(
            witness, key_rotation_after_directory / "monotonic-witness.txt"
        )
        shutil.copy2(
            Path(f"{witness}.previous"),
            key_rotation_after_directory / "monotonic-witness.txt.previous",
        )
        if run_phase(arguments, "after-key-rotation") != 0:
            return 2
        if not wait_for_status(server, 0, "fixture-key rotation"):
            return 2
        if not rotation_record.exists():
            print("key rotation recovery record is missing", file=sys.stderr)
            return 2

        server_command = server_command_for(
            current_key=rotated_key,
            current_witness_key=rotated_witness_key,
        )
        server = subprocess.Popen(server_command)
        if run_phase(arguments, "after-key-revocation") != 0:
            return 2
        if not wait_for_status(server, 0, "new-key-only restart"):
            return 2

        snapshots = [state, Path(f"{state}.previous"), mirror, Path(f"{mirror}.previous")]
        if not all(path.exists() for path in snapshots):
            print("four authenticated transaction snapshots are required", file=sys.stderr)
            return 2
        if list(output.parent.rglob("transaction-state.txt*.tmp.*")):
            print("orphan transaction-state temporary remains", file=sys.stderr)
            return 2
        if list(output.parent.glob("monotonic-witness.txt.tmp.*")):
            print("orphan monotonic-witness temporary remains", file=sys.stderr)
            return 2

        revoked_key_directory.mkdir()
        revoked_state = revoked_key_directory / "transaction-state.txt"
        revoked_mirror = revoked_key_directory / "mirror" / "transaction-state.txt"
        revoked_witness = revoked_key_directory / "monotonic-witness.txt"
        revoked_mirror.parent.mkdir()
        for source, destination in zip(
            pre_rotation_snapshots,
            [
                revoked_state,
                Path(f"{revoked_state}.previous"),
                revoked_mirror,
                Path(f"{revoked_mirror}.previous"),
            ],
        ):
            shutil.copy2(source, destination)
        shutil.copy2(
            key_rotation_directory / "monotonic-witness.txt", revoked_witness
        )
        revoked_server = subprocess.Popen(
            server_command_for(
                server_state=revoked_state,
                server_mirror=revoked_mirror,
                current_key=rotated_key,
                server_witness=revoked_witness,
                current_witness_key=rotated_witness_key,
            )
        )
        if not wait_for_status(revoked_server, 88, "revoked-key recovery"):
            return 2
        if not (revoked_key_directory / "recovery-failure.txt").exists():
            print("revoked-key failure record is missing", file=sys.stderr)
            return 2

        key_epoch_collision_directory.mkdir()
        key_epoch_server = subprocess.Popen(
            server_command_for(
                server_state=key_epoch_collision_directory / "transaction-state.txt",
                server_mirror=(
                    key_epoch_collision_directory
                    / "mirror"
                    / "transaction-state.txt"
                ),
                current_key=rotated_key,
                previous_key=rotated_key,
                server_witness=(
                    key_epoch_collision_directory / "monotonic-witness.txt"
                ),
                current_witness_key=rotated_witness_key,
            )
        )
        if not wait_for_status(key_epoch_server, 88, "key-epoch collision"):
            return 2
        if not (
            key_epoch_collision_directory / "key-policy-failure.txt"
        ).exists():
            print("key-epoch policy failure record is missing", file=sys.stderr)
            return 2

        key_domain_collision_directory.mkdir()
        key_domain_server = subprocess.Popen(
            server_command_for(
                server_state=key_domain_collision_directory / "transaction-state.txt",
                server_mirror=(
                    key_domain_collision_directory
                    / "mirror"
                    / "transaction-state.txt"
                ),
                current_key=rotated_key,
                server_witness=(
                    key_domain_collision_directory / "monotonic-witness.txt"
                ),
                current_witness_key=rotated_key,
            )
        )
        if not wait_for_status(key_domain_server, 88, "key-domain collision"):
            return 2
        if not (
            key_domain_collision_directory / "key-policy-failure.txt"
        ).exists():
            print("key-domain policy failure record is missing", file=sys.stderr)
            return 2

        key_symlink_failure_directory.mkdir()
        symlink_key = key_symlink_failure_directory / "authority.key"
        symlink_key.symlink_to(Path("..") / rotated_key.name)
        key_symlink_server = subprocess.Popen(
            server_command_for(
                server_state=key_symlink_failure_directory / "transaction-state.txt",
                server_mirror=(
                    key_symlink_failure_directory
                    / "mirror"
                    / "transaction-state.txt"
                ),
                current_key=symlink_key,
                server_witness=(
                    key_symlink_failure_directory / "monotonic-witness.txt"
                ),
                current_witness_key=rotated_witness_key,
            )
        )
        if not wait_for_status(key_symlink_server, 88, "key symlink policy"):
            return 2
        if not (
            key_symlink_failure_directory / "key-file-policy-failure.txt"
        ).exists():
            print("key symlink policy failure record is missing", file=sys.stderr)
            return 2

        key_permission_failure_directory.mkdir()
        permission_key = key_permission_failure_directory / "authority.key"
        permission_key.write_text(ROTATED_AUTHENTICATION_KEY + "\n")
        permission_key.chmod(0o644)
        key_permission_server = subprocess.Popen(
            server_command_for(
                server_state=(
                    key_permission_failure_directory / "transaction-state.txt"
                ),
                server_mirror=(
                    key_permission_failure_directory
                    / "mirror"
                    / "transaction-state.txt"
                ),
                current_key=permission_key,
                server_witness=(
                    key_permission_failure_directory / "monotonic-witness.txt"
                ),
                current_witness_key=rotated_witness_key,
            )
        )
        if not wait_for_status(
            key_permission_server, 88, "key permission policy"
        ):
            return 2
        if not (
            key_permission_failure_directory / "key-file-policy-failure.txt"
        ).exists():
            print("key permission policy failure record is missing", file=sys.stderr)
            return 2

        short_key_failure_directory.mkdir()
        short_key = short_key_failure_directory / "authority.key"
        short_key.write_text("short-fixture-key\n")
        short_key.chmod(0o600)
        short_key_server = subprocess.Popen(
            server_command_for(
                server_state=short_key_failure_directory / "transaction-state.txt",
                server_mirror=(
                    short_key_failure_directory
                    / "mirror"
                    / "transaction-state.txt"
                ),
                current_key=short_key,
                server_witness=(
                    short_key_failure_directory / "monotonic-witness.txt"
                ),
                current_witness_key=rotated_witness_key,
            )
        )
        if not wait_for_status(short_key_server, 88, "short key policy"):
            return 2
        if not (
            short_key_failure_directory / "key-file-policy-failure.txt"
        ).exists():
            print("short key policy failure record is missing", file=sys.stderr)
            return 2

        wrong_key_directory.mkdir()
        wrong_state = wrong_key_directory / "transaction-state.txt"
        wrong_mirror = wrong_key_directory / "mirror" / "transaction-state.txt"
        wrong_witness = wrong_key_directory / "monotonic-witness.txt"
        wrong_mirror.parent.mkdir()
        for source, destination in zip(
            snapshots,
            [wrong_state, Path(f"{wrong_state}.previous"), wrong_mirror, Path(f"{wrong_mirror}.previous")],
        ):
            shutil.copy2(source, destination)
        shutil.copy2(witness, wrong_witness)
        wrong_server = subprocess.Popen(
            server_command_for(
                server_state=wrong_state,
                server_mirror=wrong_mirror,
                current_key=wrong_key,
                server_witness=wrong_witness,
                current_witness_key=rotated_witness_key,
            )
        )
        if not wait_for_status(wrong_server, 88, "wrong-key recovery"):
            return 2
        if not (wrong_key_directory / "recovery-failure.txt").exists():
            print("wrong-key failure record is missing", file=sys.stderr)
            return 2

        all_failure_directory.mkdir()
        failure_state = all_failure_directory / "transaction-state.txt"
        failure_mirror = all_failure_directory / "mirror" / "transaction-state.txt"
        failure_mirror.parent.mkdir()
        failure_snapshots = [
            failure_state,
            Path(f"{failure_state}.previous"),
            failure_mirror,
            Path(f"{failure_mirror}.previous"),
        ]
        for source, destination in zip(snapshots, failure_snapshots):
            shutil.copy2(source, destination)
            corrupt_checksum(destination)
        shutil.copy2(witness, all_failure_directory / "monotonic-witness.txt")
        failure_server = subprocess.Popen(
            server_command_for(
                server_state=failure_state,
                server_mirror=failure_mirror,
                current_key=rotated_key,
                server_witness=all_failure_directory / "monotonic-witness.txt",
                current_witness_key=rotated_witness_key,
            )
        )
        if not wait_for_status(failure_server, 88, "all-generation recovery"):
            return 2
        if not (all_failure_directory / "recovery-failure.txt").exists():
            print("all-generation failure record is missing", file=sys.stderr)
            return 2

        fork_failure_directory.mkdir()
        fork_state = fork_failure_directory / "transaction-state.txt"
        fork_mirror = fork_failure_directory / "mirror" / "transaction-state.txt"
        fork_mirror.parent.mkdir()
        fork_snapshots = [
            fork_state,
            Path(f"{fork_state}.previous"),
            fork_mirror,
            Path(f"{fork_mirror}.previous"),
        ]
        for source, destination in zip(snapshots, fork_snapshots):
            shutil.copy2(source, destination)
        shutil.copy2(witness, fork_failure_directory / "monotonic-witness.txt")
        forge_authenticated_fork(fork_mirror, ROTATED_AUTHENTICATION_KEY)
        fork_server = subprocess.Popen(
            server_command_for(
                server_state=fork_state,
                server_mirror=fork_mirror,
                current_key=rotated_key,
                server_witness=fork_failure_directory / "monotonic-witness.txt",
                current_witness_key=rotated_witness_key,
            )
        )
        if not wait_for_status(fork_server, 88, "authenticated fork recovery"):
            return 2
        if not (fork_failure_directory / "recovery-failure.txt").exists():
            print("authenticated fork failure record is missing", file=sys.stderr)
            return 2

        rollback_failure_directory.mkdir()
        rollback_state = rollback_failure_directory / "transaction-state.txt"
        rollback_mirror = rollback_failure_directory / "mirror" / "transaction-state.txt"
        rollback_witness = rollback_failure_directory / "monotonic-witness.txt"
        rollback_mirror.parent.mkdir()
        for destination in [
            rollback_state,
            Path(f"{rollback_state}.previous"),
            rollback_mirror,
            Path(f"{rollback_mirror}.previous"),
        ]:
            shutil.copy2(Path(f"{state}.previous"), destination)
        shutil.copy2(witness, rollback_witness)
        rollback_server = subprocess.Popen(
            server_command_for(
                server_state=rollback_state,
                server_mirror=rollback_mirror,
                current_key=rotated_key,
                server_witness=rollback_witness,
                current_witness_key=rotated_witness_key,
            )
        )
        if not wait_for_status(rollback_server, 88, "monotonic rollback recovery"):
            return 2
        if not (rollback_failure_directory / "recovery-failure.txt").exists():
            print("monotonic rollback failure record is missing", file=sys.stderr)
            return 2

        wrong_witness_directory.mkdir()
        wrong_witness_state = wrong_witness_directory / "transaction-state.txt"
        wrong_witness_mirror = (
            wrong_witness_directory / "mirror" / "transaction-state.txt"
        )
        wrong_witness_copy = wrong_witness_directory / "monotonic-witness.txt"
        wrong_witness_mirror.parent.mkdir()
        for source, destination in zip(
            snapshots,
            [
                wrong_witness_state,
                Path(f"{wrong_witness_state}.previous"),
                wrong_witness_mirror,
                Path(f"{wrong_witness_mirror}.previous"),
            ],
        ):
            shutil.copy2(source, destination)
        shutil.copy2(witness, wrong_witness_copy)
        wrong_witness_server = subprocess.Popen(
            server_command_for(
                server_state=wrong_witness_state,
                server_mirror=wrong_witness_mirror,
                current_key=rotated_key,
                server_witness=wrong_witness_copy,
                current_witness_key=wrong_witness_key,
            )
        )
        if not wait_for_status(
            wrong_witness_server, 88, "wrong-witness-key recovery"
        ):
            return 2
        if not (wrong_witness_directory / "recovery-failure.txt").exists():
            print("wrong-witness-key failure record is missing", file=sys.stderr)
            return 2
        return 0
    finally:
        if server.poll() is None:
            server.terminate()
            try:
                server.wait(timeout=5)
            except subprocess.TimeoutExpired:
                server.kill()


if __name__ == "__main__":
    raise SystemExit(main())
