#!/usr/bin/env python3

import argparse
import hashlib
import hmac
from pathlib import Path


EVIDENCE_FIELDS = {
    "contract": "functional-fail-closed-authenticated-mirrored-monotonic-witness-tcp-authority-transaction-probe",
    "transport": "tcp-ipv4",
    "authority": "fp64-fused-original-expression-gate",
    "same_physical_host": "1",
    "multi_host": "0",
    "performance_evidence": "0",
    "production_distributed_commit": "0",
    "consensus_protocol": "0",
    "durability": "hmac-sha256-authenticated-hash-chained-mirrored-snapshot-plus-monotonic-witness",
    "state_format": "authenticated-hash-chained-v6",
    "state_checksum": "sha256",
    "state_authentication": "hmac-sha256",
    "state_parent_hash": "sha256",
    "authentication_key_external": "1",
    "authentication_key_fixture_for_test": "1",
    "fixture_key_rotation_and_revocation": "1",
    "key_rotation_requires_explicit_previous_keys": "1",
    "key_rotation_republished_before_listen": "1",
    "pairwise_distinct_configured_keys_enforced": "1",
    "key_epoch_separation_enforced": "1",
    "key_domain_separation_enforced": "1",
    "key_files_regular_enforced": "1",
    "key_files_non_symlink_enforced": "1",
    "key_files_owner_only_permissions_enforced": "1",
    "key_file_minimum_bytes": "32",
    "production_key_lifecycle": "0",
    "kms_hsm_integration": "0",
    "mirrored_state_stores": "2",
    "independent_failure_domains": "0",
    "highest_valid_generation_across_stores": "1",
    "authenticated_highest_generation_fork_detection": "1",
    "local_monotonic_witness": "1",
    "authenticated_one_generation_witness_lag_recovery": "1",
    "witness_lag_generation_delta": "1",
    "external_monotonic_anchor": "0",
    "witness_preserved_all_state_rollback_detection": "1",
    "joint_state_and_witness_rollback_detection": "0",
    "witness_independent_failure_domain": "0",
    "witness_key_external": "1",
    "witness_key_fixture_for_test": "1",
    "previous_generation_retained": "1",
    "commit_ack_order": "persist-before-reply",
    "server_crash_exit_code": "86",
    "snapshot_prepublication_crash_exit_code": "87",
    "witness_lag_crash_exit_code": "89",
    "authenticated_recovery_failure_exit_code": "88",
    "server_starts": "10",
    "server_restarts": "9",
    "failed_start_attempts": "11",
    "recovered_transactions": "4",
    "accepted_transactions": "3",
    "gate_rejections": "1",
    "lost_reply_injections": "3",
    "server_crash_after_commit_injections": "1",
    "snapshot_prepublication_crash_injections": "1",
    "witness_lag_crash_injections": "1",
    "witness_lag_recoveries": "1",
    "witness_lag_recovery_records": "1",
    "witness_lag_crash_snapshots": "4",
    "witness_lag_prepared_witnesses": "1",
    "state_key_rotations": "1",
    "witness_key_rotations": "1",
    "key_rotation_records": "1",
    "key_rotation_pre_snapshots": "4",
    "key_rotation_pre_witnesses": "2",
    "key_rotation_post_snapshots": "4",
    "key_rotation_post_witnesses": "2",
    "revoked_state_key_generations_rejected": "4",
    "revoked_witness_key_generations_rejected": "1",
    "torn_temporary_recoveries": "4",
    "corrupted_current_generations_rejected": "1",
    "forged_plain_checksums_rejected_by_hmac": "2",
    "stale_current_generations_rejected": "1",
    "previous_generation_recoveries": "1",
    "mirror_recoveries": "2",
    "primary_generation_pair_losses": "1",
    "durable_idempotent_replays": "4",
    "wrong_key_generations_rejected": "4",
    "all_corrupted_generations_rejected": "4",
    "authenticated_same_generation_forks_rejected": "1",
    "monotonic_rollbacks_rejected": "1",
    "wrong_witness_generations_rejected": "1",
    "revoked_key_startups_rejected": "1",
    "authenticated_fail_closed_startups": "6",
    "key_epoch_collisions_rejected": "1",
    "key_domain_collisions_rejected": "1",
    "key_policy_fail_closed_startups": "2",
    "key_symlink_startups_rejected": "1",
    "key_permission_startups_rejected": "1",
    "short_key_startups_rejected": "1",
    "key_file_policy_fail_closed_startups": "3",
    "total_fail_closed_startups": "11",
    "blank_state_reinitializations": "0",
    "listen_before_recovery_failures": "0",
    "transaction_conflicts_rejected": "1",
    "malformed_requests_rejected": "1",
    "partial_commits": "0",
    "decision_mismatches": "0",
    "residual_mismatches": "0",
    "strict_equivalence": "1",
}

SNAPSHOT_FIELDS = {
    "committed_transactions": "3",
    "gate_rejections": "1",
    "duplicate_replays": "4",
    "transaction_conflicts": "1",
    "server_starts": "10",
    "torn_temporary_recoveries": "4",
    "witness_lag_crash_injections": "1",
    "witness_lag_recoveries": "1",
    "checksum_failures": "1",
    "authentication_failures": "2",
    "previous_generation_recoveries": "1",
    "stale_current_generations_rejected": "1",
    "mirror_recoveries": "2",
    "state_key_rotations": "1",
    "witness_key_rotations": "1",
    "transaction_count": "4",
}

FAILURE_FIELDS = {
    "contract": "authenticated-mirrored-monotonic-witness-fail-closed-startup",
    "failure_stage": "state-load-before-listen",
    "reason": "no-valid-authenticated-generation",
    "current_present": "1",
    "current_valid": "0",
    "previous_present": "1",
    "previous_valid": "0",
    "mirror_current_present": "1",
    "mirror_current_valid": "0",
    "mirror_previous_present": "1",
    "mirror_previous_valid": "0",
    "witness_present": "1",
    "witness_valid": "1",
    "fail_closed": "1",
    "blank_state_started": "0",
    "state_reinitialized": "0",
    "listen_socket_created": "0",
    "server_exit_code": "88",
    "performance_evidence": "0",
}

FORK_FAILURE_FIELDS = {
    **FAILURE_FIELDS,
    "reason": "authenticated-highest-generation-fork",
    "current_valid": "1",
    "previous_valid": "1",
    "mirror_current_valid": "1",
    "mirror_previous_valid": "1",
}

ROLLBACK_FAILURE_FIELDS = {
    **FAILURE_FIELDS,
    "reason": "authenticated-state-below-monotonic-witness",
    "current_valid": "1",
    "previous_valid": "1",
    "mirror_current_valid": "1",
    "mirror_previous_valid": "1",
}

WRONG_WITNESS_FAILURE_FIELDS = {
    **FAILURE_FIELDS,
    "reason": "no-valid-monotonic-witness",
    "current_valid": "1",
    "previous_valid": "1",
    "mirror_current_valid": "1",
    "mirror_previous_valid": "1",
    "witness_valid": "0",
}

REVOKED_KEY_FAILURE_FIELDS = {
    **FAILURE_FIELDS,
    "witness_valid": "0",
}

WITNESS_LAG_RECOVERY_FIELDS = {
    "recovery_stage": "state-load-before-listen",
    "generation_delta": "1",
    "parent_digest_match": "1",
    "witness_republished": "1",
    "listen_socket_created": "0",
    "performance_evidence": "0",
}

KEY_ROTATION_RECOVERY_FIELDS = {
    "recovery_stage": "state-load-before-listen",
    "generation_delta": "1",
    "state_key_republished": "1",
    "witness_key_republished": "1",
    "previous_state_key_configured": "1",
    "previous_witness_key_configured": "1",
    "listen_socket_created": "0",
    "fixture_key_rotation": "1",
    "production_key_custody": "0",
    "kms_hsm_integration": "0",
    "performance_evidence": "0",
}

KEY_POLICY_FAILURE_FIELDS = {
    "contract": "pairwise-distinct-state-witness-authentication-keys",
    "failure_stage": "key-policy-before-state-load",
    "reason": "duplicate-key-material-across-roles",
    "pairwise_distinct": "0",
    "fail_closed": "1",
    "state_loaded": "0",
    "listen_socket_created": "0",
    "server_exit_code": "88",
    "performance_evidence": "0",
}

KEY_EPOCH_FAILURE_FIELDS = {
    **KEY_POLICY_FAILURE_FIELDS,
    "first_role": "state-current",
    "second_role": "state-previous",
    "configured_key_count": "3",
}

KEY_DOMAIN_FAILURE_FIELDS = {
    **KEY_POLICY_FAILURE_FIELDS,
    "first_role": "state-current",
    "second_role": "witness-current",
    "configured_key_count": "2",
}

KEY_FILE_POLICY_FAILURE_FIELDS = {
    "contract": "owner-only-regular-non-symlink-authentication-key-files",
    "failure_stage": "key-file-policy-before-state-load",
    "role": "state-current",
    "minimum_key_bytes": "32",
    "fail_closed": "1",
    "state_loaded": "0",
    "listen_socket_created": "0",
    "server_exit_code": "88",
    "performance_evidence": "0",
}

KEY_SYMLINK_FAILURE_FIELDS = {
    **KEY_FILE_POLICY_FAILURE_FIELDS,
    "reason": "symbolic-link-key-file",
    "key_file_name": "authority.key",
    "regular_file": "0",
    "symbolic_link": "1",
    "owner_only_permissions": "0",
    "key_bytes_read": "0",
}

KEY_PERMISSION_FAILURE_FIELDS = {
    **KEY_FILE_POLICY_FAILURE_FIELDS,
    "reason": "group-or-other-accessible-key-file",
    "key_file_name": "authority.key",
    "key_file_mode": "0644",
    "regular_file": "1",
    "symbolic_link": "0",
    "owner_only_permissions": "0",
    "key_bytes_read": "0",
}

SHORT_KEY_FAILURE_FIELDS = {
    **KEY_FILE_POLICY_FAILURE_FIELDS,
    "reason": "key-file-shorter-than-32-bytes",
    "key_file_name": "authority.key",
    "key_file_mode": "0600",
    "regular_file": "1",
    "symbolic_link": "0",
    "owner_only_permissions": "1",
    "key_bytes_read": "17",
}


def parse_fields(path: Path, header: str) -> dict[str, str]:
    lines = path.read_text().splitlines()
    if not lines or lines[0] != header or lines[-1] != "END":
        raise ValueError(f"invalid evidence envelope: {path}")
    fields = {}
    for line in lines[1:-1]:
        if "=" not in line:
            raise ValueError(f"invalid evidence field: {path}: {line}")
        key, value = line.split("=", 1)
        if key in fields:
            raise ValueError(f"duplicate evidence field: {path}: {key}")
        fields[key] = value
    return fields


def require_fields(fields: dict[str, str], expected: dict[str, str], path: Path) -> None:
    for key, value in expected.items():
        if fields.get(key) != value:
            raise ValueError(
                f"expected {key}={value}, observed {fields.get(key)} in {path}"
            )


def snapshot_body(path: Path, key: bytes) -> dict[str, str]:
    contents = path.read_text()
    header = "SMAVE_GATE_TXN_STATE 6\n"
    if not contents.startswith(header) or not contents.endswith("\nEND\n"):
        raise ValueError(f"invalid authenticated snapshot envelope: {path}")
    checksum_marker = "state_sha256="
    key_marker = "key_id="
    hmac_marker = "state_hmac_sha256="
    checksum_position = contents.index(checksum_marker)
    body = contents[len(header):checksum_position]
    checksum = contents[
        checksum_position + len(checksum_marker):
        contents.index("\n", checksum_position)
    ]
    key_position = contents.index(key_marker, checksum_position)
    key_id = contents[
        key_position + len(key_marker):contents.index("\n", key_position)
    ]
    hmac_position = contents.index(hmac_marker, key_position)
    signature = contents[
        hmac_position + len(hmac_marker):contents.index("\n", hmac_position)
    ]
    expected_checksum = hashlib.sha256(body.encode()).hexdigest()
    expected_key_id = hashlib.sha256(key).hexdigest()[:16]
    expected_signature = hmac.new(key, body.encode(), hashlib.sha256).hexdigest()
    if checksum != expected_checksum:
        raise ValueError(f"snapshot checksum mismatch: {path}")
    if key_id != expected_key_id or not hmac.compare_digest(signature, expected_signature):
        raise ValueError(f"snapshot authentication mismatch: {path}")
    fields = parse_body(body, path)
    fields["_authenticated_body_sha256"] = expected_checksum
    return fields


def witness_body(path: Path, key: bytes) -> dict[str, str]:
    contents = path.read_text()
    header = "SMAVE_GATE_TXN_WITNESS 1\n"
    if not contents.startswith(header) or not contents.endswith("\nEND\n"):
        raise ValueError(f"invalid monotonic witness envelope: {path}")
    checksum_marker = "witness_sha256="
    key_marker = "witness_key_id="
    hmac_marker = "witness_hmac_sha256="
    checksum_position = contents.index(checksum_marker)
    body = contents[len(header):checksum_position]
    checksum = contents[
        checksum_position + len(checksum_marker):
        contents.index("\n", checksum_position)
    ]
    key_position = contents.index(key_marker, checksum_position)
    key_id = contents[
        key_position + len(key_marker):contents.index("\n", key_position)
    ]
    hmac_position = contents.index(hmac_marker, key_position)
    signature = contents[
        hmac_position + len(hmac_marker):contents.index("\n", hmac_position)
    ]
    if checksum != hashlib.sha256(body.encode()).hexdigest():
        raise ValueError(f"monotonic witness checksum mismatch: {path}")
    expected_key_id = hashlib.sha256(key).hexdigest()[:16]
    expected_signature = hmac.new(key, body.encode(), hashlib.sha256).hexdigest()
    if key_id != expected_key_id or not hmac.compare_digest(signature, expected_signature):
        raise ValueError(f"monotonic witness authentication mismatch: {path}")
    return parse_body(body, path)


def parse_body(body: str, path: Path) -> dict[str, str]:
    fields = {}
    for line in body.splitlines():
        key, value = line.split("=", 1)
        if key in fields:
            raise ValueError(f"duplicate snapshot field: {path}: {key}")
        fields[key] = value
    return fields


def expect_snapshot_failure(path: Path, key: bytes, failure: str) -> None:
    try:
        snapshot_body(path, key)
    except ValueError as error:
        if failure not in str(error):
            raise
        return
    raise ValueError(f"snapshot unexpectedly verified: {path}")


def key_id(key: bytes) -> str:
    return hashlib.sha256(key).hexdigest()[:16]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--expect-docker-bridge", action="store_true")
    arguments = parser.parse_args()

    evidence = arguments.evidence.resolve()
    directory = evidence.parent
    fields = parse_fields(evidence, "SMAVE_GATE_NETWORK_AUTHORITY 1")
    require_fields(fields, EVIDENCE_FIELDS, evidence)
    if int(fields.get("state_generation_at_evidence", "0")) <= 0:
        raise ValueError("state generation is not positive")
    if arguments.expect_docker_bridge:
        require_fields(
            fields,
            {
                "deployment": "docker-bridge-two-container",
                "distinct_network_namespaces": "1",
            },
            evidence,
        )

    previous_key = (directory / "authority.key").read_text().strip().encode()
    key = (directory / "rotated-authority.key").read_text().strip().encode()
    wrong_key = (directory / "wrong-authority.key").read_text().strip().encode()
    previous_witness_key = (directory / "witness.key").read_text().strip().encode()
    witness_key = (directory / "rotated-witness.key").read_text().strip().encode()
    wrong_witness_key = (directory / "wrong-witness.key").read_text().strip().encode()
    configured_key_ids = {
        key_id(previous_key),
        key_id(key),
        key_id(previous_witness_key),
        key_id(witness_key),
    }
    if len(configured_key_ids) != 4:
        raise ValueError("healthy state/witness epoch keys are not pairwise distinct")
    state = directory / "transaction-state.txt"
    mirror = directory / "mirror" / "transaction-state.txt"
    snapshots = [state, Path(f"{state}.previous"), mirror, Path(f"{mirror}.previous")]
    snapshot_fields = [snapshot_body(snapshot, key) for snapshot in snapshots]
    for snapshot, fields_for_snapshot in zip(snapshots, snapshot_fields):
        require_fields(fields_for_snapshot, SNAPSHOT_FIELDS, snapshot)
        if len(fields_for_snapshot.get("previous_body_sha256", "")) != 64:
            raise ValueError(f"invalid state parent digest: {snapshot}")
    if snapshot_fields[0]["_authenticated_body_sha256"] != snapshot_fields[2]["_authenticated_body_sha256"]:
        raise ValueError("healthy primary and mirror current snapshots differ")
    if snapshot_fields[1]["_authenticated_body_sha256"] != snapshot_fields[3]["_authenticated_body_sha256"]:
        raise ValueError("healthy primary and mirror previous snapshots differ")
    if snapshot_fields[0]["previous_body_sha256"] != snapshot_fields[1]["_authenticated_body_sha256"]:
        raise ValueError("primary state hash chain is broken")
    if snapshot_fields[2]["previous_body_sha256"] != snapshot_fields[3]["_authenticated_body_sha256"]:
        raise ValueError("mirror state hash chain is broken")

    witness = directory / "monotonic-witness.txt"
    witness_previous = Path(f"{witness}.previous")
    witness_fields = witness_body(witness, witness_key)
    witness_previous_fields = witness_body(witness_previous, witness_key)
    if witness_fields["generation"] != snapshot_fields[0]["generation"]:
        raise ValueError("monotonic witness generation does not match current state")
    if witness_fields["state_sha256"] != snapshot_fields[0]["_authenticated_body_sha256"]:
        raise ValueError("monotonic witness digest does not match current state")
    if witness_previous_fields["generation"] != snapshot_fields[1]["generation"]:
        raise ValueError("previous witness generation does not match previous state")
    if witness_previous_fields["state_sha256"] != snapshot_fields[1]["_authenticated_body_sha256"]:
        raise ValueError("previous witness digest does not match previous state")

    witness_lag_directory = directory / "witness-lag-crash"
    witness_lag_state = witness_lag_directory / "transaction-state.txt"
    witness_lag_mirror = (
        witness_lag_directory / "mirror" / "transaction-state.txt"
    )
    witness_lag_snapshots = [
        witness_lag_state,
        Path(f"{witness_lag_state}.previous"),
        witness_lag_mirror,
        Path(f"{witness_lag_mirror}.previous"),
    ]
    witness_lag_bodies = [
        snapshot_body(snapshot, previous_key) for snapshot in witness_lag_snapshots
    ]
    if witness_lag_bodies[0]["_authenticated_body_sha256"] != witness_lag_bodies[2]["_authenticated_body_sha256"]:
        raise ValueError("lag-crash current state copies differ")
    if witness_lag_bodies[1]["_authenticated_body_sha256"] != witness_lag_bodies[3]["_authenticated_body_sha256"]:
        raise ValueError("lag-crash previous state copies differ")
    if witness_lag_bodies[0]["previous_body_sha256"] != witness_lag_bodies[1]["_authenticated_body_sha256"]:
        raise ValueError("lag-crash state parent digest is broken")

    lag_witness = witness_body(
        witness_lag_directory / "monotonic-witness.txt", previous_witness_key
    )
    lag_previous_witness = witness_body(
        witness_lag_directory / "monotonic-witness.txt.previous",
        previous_witness_key,
    )
    prepared_witness = witness_body(
        witness_lag_directory / "prepared-witness.txt", previous_witness_key
    )
    if int(witness_lag_bodies[0]["generation"]) != int(lag_witness["generation"]) + 1:
        raise ValueError("lag-crash state is not exactly one generation ahead")
    if witness_lag_bodies[0]["previous_body_sha256"] != lag_witness["state_sha256"]:
        raise ValueError("lag-crash state does not chain from the prior witness")
    if lag_witness["generation"] != witness_lag_bodies[1]["generation"]:
        raise ValueError("lag-crash witness does not match previous state generation")
    if lag_witness["state_sha256"] != witness_lag_bodies[1]["_authenticated_body_sha256"]:
        raise ValueError("lag-crash witness does not match previous state digest")
    if lag_previous_witness["generation"] != str(int(lag_witness["generation"]) - 1):
        raise ValueError("lag-crash previous witness generation is not contiguous")
    if witness_lag_bodies[1]["previous_body_sha256"] != lag_previous_witness["state_sha256"]:
        raise ValueError("lag-crash previous witness does not extend the state chain")
    if prepared_witness["generation"] != witness_lag_bodies[0]["generation"]:
        raise ValueError("prepared witness generation does not match lagged state")
    if prepared_witness["state_sha256"] != witness_lag_bodies[0]["_authenticated_body_sha256"]:
        raise ValueError("prepared witness digest does not match lagged state")

    lag_record_path = directory / "witness-lag-recovery.txt"
    lag_record = parse_fields(
        lag_record_path, "SMAVE_GATE_TXN_WITNESS_LAG_RECOVERY 1"
    )
    require_fields(lag_record, WITNESS_LAG_RECOVERY_FIELDS, lag_record_path)
    if lag_record["prior_witness_generation"] != lag_witness["generation"]:
        raise ValueError("recovery record prior witness generation differs")
    if lag_record["selected_state_generation"] != witness_lag_bodies[0]["generation"]:
        raise ValueError("recovery record selected generation differs")
    if lag_record["prior_witness_state_sha256"] != lag_witness["state_sha256"]:
        raise ValueError("recovery record prior witness digest differs")
    if lag_record["selected_previous_body_sha256"] != witness_lag_bodies[0]["previous_body_sha256"]:
        raise ValueError("recovery record parent digest differs")
    if lag_record["selected_state_sha256"] != witness_lag_bodies[0]["_authenticated_body_sha256"]:
        raise ValueError("recovery record selected state digest differs")

    rotation_before_directory = directory / "key-rotation-before"
    rotation_before_state = rotation_before_directory / "transaction-state.txt"
    rotation_before_mirror = (
        rotation_before_directory / "mirror" / "transaction-state.txt"
    )
    rotation_before_snapshots = [
        rotation_before_state,
        Path(f"{rotation_before_state}.previous"),
        rotation_before_mirror,
        Path(f"{rotation_before_mirror}.previous"),
    ]
    rotation_before_bodies = [
        snapshot_body(snapshot, previous_key)
        for snapshot in rotation_before_snapshots
    ]
    if rotation_before_bodies[0]["_authenticated_body_sha256"] != rotation_before_bodies[2]["_authenticated_body_sha256"]:
        raise ValueError("pre-rotation current state copies differ")
    if rotation_before_bodies[1]["_authenticated_body_sha256"] != rotation_before_bodies[3]["_authenticated_body_sha256"]:
        raise ValueError("pre-rotation previous state copies differ")
    if rotation_before_bodies[0]["previous_body_sha256"] != rotation_before_bodies[1]["_authenticated_body_sha256"]:
        raise ValueError("pre-rotation state chain is broken")
    rotation_before_witness = witness_body(
        rotation_before_directory / "monotonic-witness.txt",
        previous_witness_key,
    )
    rotation_before_previous_witness = witness_body(
        rotation_before_directory / "monotonic-witness.txt.previous",
        previous_witness_key,
    )
    if rotation_before_witness["generation"] != rotation_before_bodies[0]["generation"]:
        raise ValueError("pre-rotation witness generation differs")
    if rotation_before_witness["state_sha256"] != rotation_before_bodies[0]["_authenticated_body_sha256"]:
        raise ValueError("pre-rotation witness digest differs")
    if rotation_before_previous_witness["generation"] != rotation_before_bodies[1]["generation"]:
        raise ValueError("pre-rotation previous witness generation differs")

    rotation_after_directory = directory / "key-rotation-after"
    rotation_after_state = rotation_after_directory / "transaction-state.txt"
    rotation_after_mirror = (
        rotation_after_directory / "mirror" / "transaction-state.txt"
    )
    rotation_after_snapshots = [
        rotation_after_state,
        Path(f"{rotation_after_state}.previous"),
        rotation_after_mirror,
        Path(f"{rotation_after_mirror}.previous"),
    ]
    rotation_after_bodies = [
        snapshot_body(rotation_after_snapshots[0], key),
        snapshot_body(rotation_after_snapshots[1], previous_key),
        snapshot_body(rotation_after_snapshots[2], key),
        snapshot_body(rotation_after_snapshots[3], previous_key),
    ]
    if rotation_after_bodies[0]["_authenticated_body_sha256"] != rotation_after_bodies[2]["_authenticated_body_sha256"]:
        raise ValueError("post-rotation current state copies differ")
    if rotation_after_bodies[1]["_authenticated_body_sha256"] != rotation_after_bodies[3]["_authenticated_body_sha256"]:
        raise ValueError("post-rotation previous state copies differ")
    if rotation_after_bodies[0]["previous_body_sha256"] != rotation_after_bodies[1]["_authenticated_body_sha256"]:
        raise ValueError("post-rotation state chain is broken")
    if rotation_after_bodies[1]["_authenticated_body_sha256"] != rotation_before_bodies[0]["_authenticated_body_sha256"]:
        raise ValueError("post-rotation previous state does not preserve selected bytes")
    rotation_after_witness = witness_body(
        rotation_after_directory / "monotonic-witness.txt", witness_key
    )
    rotation_after_previous_witness = witness_body(
        rotation_after_directory / "monotonic-witness.txt.previous",
        previous_witness_key,
    )
    if rotation_after_witness["generation"] != rotation_after_bodies[0]["generation"]:
        raise ValueError("post-rotation witness generation differs")
    if rotation_after_witness["state_sha256"] != rotation_after_bodies[0]["_authenticated_body_sha256"]:
        raise ValueError("post-rotation witness digest differs")
    if rotation_after_previous_witness["state_sha256"] != rotation_after_bodies[1]["_authenticated_body_sha256"]:
        raise ValueError("post-rotation previous witness digest differs")

    rotation_record_path = directory / "key-rotation-recovery.txt"
    rotation_record = parse_fields(
        rotation_record_path, "SMAVE_GATE_TXN_KEY_ROTATION 1"
    )
    require_fields(
        rotation_record, KEY_ROTATION_RECOVERY_FIELDS, rotation_record_path
    )
    if rotation_record["selected_state_generation"] != rotation_before_bodies[0]["generation"]:
        raise ValueError("rotation record selected generation differs")
    if rotation_record["published_state_generation"] != rotation_after_bodies[0]["generation"]:
        raise ValueError("rotation record published generation differs")
    if rotation_record["selected_state_sha256"] != rotation_before_bodies[0]["_authenticated_body_sha256"]:
        raise ValueError("rotation record selected digest differs")
    if rotation_record["published_previous_body_sha256"] != rotation_after_bodies[0]["previous_body_sha256"]:
        raise ValueError("rotation record parent digest differs")
    if rotation_record["published_state_sha256"] != rotation_after_bodies[0]["_authenticated_body_sha256"]:
        raise ValueError("rotation record published digest differs")
    if rotation_record["selected_state_key_id"] != key_id(previous_key):
        raise ValueError("rotation record previous state key id differs")
    if rotation_record["current_state_key_id"] != key_id(key):
        raise ValueError("rotation record current state key id differs")
    if rotation_record["selected_witness_key_id"] != key_id(previous_witness_key):
        raise ValueError("rotation record previous witness key id differs")
    if rotation_record["current_witness_key_id"] != key_id(witness_key):
        raise ValueError("rotation record current witness key id differs")

    for snapshot in snapshots:
        expect_snapshot_failure(snapshot, previous_key, "authentication")
    try:
        witness_body(witness, previous_witness_key)
    except ValueError as error:
        if "authentication" not in str(error):
            raise
    else:
        raise ValueError("final witness unexpectedly verifies with revoked key")

    revoked_directory = directory / "revoked-key-failure"
    revoked_state = revoked_directory / "transaction-state.txt"
    revoked_mirror = revoked_directory / "mirror" / "transaction-state.txt"
    revoked_snapshots = [
        revoked_state,
        Path(f"{revoked_state}.previous"),
        revoked_mirror,
        Path(f"{revoked_mirror}.previous"),
    ]
    require_fields(
        parse_fields(
            revoked_directory / "recovery-failure.txt",
            "SMAVE_GATE_TXN_RECOVERY_FAILURE 1",
        ),
        REVOKED_KEY_FAILURE_FIELDS,
        revoked_directory / "recovery-failure.txt",
    )
    for snapshot in revoked_snapshots:
        snapshot_body(snapshot, previous_key)
        expect_snapshot_failure(snapshot, key, "authentication")
    revoked_witness = revoked_directory / "monotonic-witness.txt"
    witness_body(revoked_witness, previous_witness_key)
    try:
        witness_body(revoked_witness, witness_key)
    except ValueError as error:
        if "authentication" not in str(error):
            raise
    else:
        raise ValueError("revoked witness unexpectedly verified with current key")

    key_epoch_directory = directory / "key-epoch-collision-failure"
    key_epoch_record_path = key_epoch_directory / "key-policy-failure.txt"
    key_epoch_record = parse_fields(
        key_epoch_record_path, "SMAVE_GATE_TXN_KEY_POLICY_FAILURE 1"
    )
    require_fields(
        key_epoch_record, KEY_EPOCH_FAILURE_FIELDS, key_epoch_record_path
    )
    if key_epoch_record["duplicate_key_id"] != key_id(key):
        raise ValueError("key-epoch collision record key id differs")
    if list(key_epoch_directory.rglob("transaction-state.txt*")):
        raise ValueError("key-epoch collision loaded transaction state")
    if list(key_epoch_directory.rglob("monotonic-witness.txt*")):
        raise ValueError("key-epoch collision loaded monotonic witness")

    key_domain_directory = directory / "key-domain-collision-failure"
    key_domain_record_path = key_domain_directory / "key-policy-failure.txt"
    key_domain_record = parse_fields(
        key_domain_record_path, "SMAVE_GATE_TXN_KEY_POLICY_FAILURE 1"
    )
    require_fields(
        key_domain_record, KEY_DOMAIN_FAILURE_FIELDS, key_domain_record_path
    )
    if key_domain_record["duplicate_key_id"] != key_id(key):
        raise ValueError("key-domain collision record key id differs")
    if list(key_domain_directory.rglob("transaction-state.txt*")):
        raise ValueError("key-domain collision loaded transaction state")
    if list(key_domain_directory.rglob("monotonic-witness.txt*")):
        raise ValueError("key-domain collision loaded monotonic witness")

    key_file_failures = [
        ("key-symlink-failure", KEY_SYMLINK_FAILURE_FIELDS),
        ("key-permission-failure", KEY_PERMISSION_FAILURE_FIELDS),
        ("short-key-failure", SHORT_KEY_FAILURE_FIELDS),
    ]
    for name, expected_fields in key_file_failures:
        failure_directory = directory / name
        failure_record_path = (
            failure_directory / "key-file-policy-failure.txt"
        )
        failure_record = parse_fields(
            failure_record_path,
            "SMAVE_GATE_TXN_KEY_FILE_POLICY_FAILURE 1",
        )
        require_fields(failure_record, expected_fields, failure_record_path)
        if list(failure_directory.rglob("transaction-state.txt*")):
            raise ValueError(f"{name} loaded transaction state")
        if list(failure_directory.rglob("monotonic-witness.txt*")):
            raise ValueError(f"{name} loaded monotonic witness")

    wrong_directory = directory / "wrong-key-failure"
    wrong_state = wrong_directory / "transaction-state.txt"
    wrong_mirror = wrong_directory / "mirror" / "transaction-state.txt"
    wrong_snapshots = [
        wrong_state,
        Path(f"{wrong_state}.previous"),
        wrong_mirror,
        Path(f"{wrong_mirror}.previous"),
    ]
    require_fields(
        parse_fields(
            wrong_directory / "recovery-failure.txt",
            "SMAVE_GATE_TXN_RECOVERY_FAILURE 1",
        ),
        FAILURE_FIELDS,
        wrong_directory / "recovery-failure.txt",
    )
    for snapshot in wrong_snapshots:
        snapshot_body(snapshot, key)
        expect_snapshot_failure(snapshot, wrong_key, "authentication")
    witness_body(wrong_directory / "monotonic-witness.txt", witness_key)

    failure_directory = directory / "all-generation-failure"
    failure_state = failure_directory / "transaction-state.txt"
    failure_mirror = failure_directory / "mirror" / "transaction-state.txt"
    failure_snapshots = [
        failure_state,
        Path(f"{failure_state}.previous"),
        failure_mirror,
        Path(f"{failure_mirror}.previous"),
    ]
    require_fields(
        parse_fields(
            failure_directory / "recovery-failure.txt",
            "SMAVE_GATE_TXN_RECOVERY_FAILURE 1",
        ),
        FAILURE_FIELDS,
        failure_directory / "recovery-failure.txt",
    )
    for snapshot in failure_snapshots:
        expect_snapshot_failure(snapshot, key, "checksum")
    witness_body(failure_directory / "monotonic-witness.txt", witness_key)

    fork_directory = directory / "fork-failure"
    fork_state = fork_directory / "transaction-state.txt"
    fork_mirror = fork_directory / "mirror" / "transaction-state.txt"
    fork_snapshots = [
        fork_state,
        Path(f"{fork_state}.previous"),
        fork_mirror,
        Path(f"{fork_mirror}.previous"),
    ]
    require_fields(
        parse_fields(
            fork_directory / "recovery-failure.txt",
            "SMAVE_GATE_TXN_RECOVERY_FAILURE 1",
        ),
        FORK_FAILURE_FIELDS,
        fork_directory / "recovery-failure.txt",
    )
    fork_bodies = [snapshot_body(snapshot, key) for snapshot in fork_snapshots]
    if fork_bodies[0]["generation"] != fork_bodies[2]["generation"]:
        raise ValueError("fork fixture does not share the highest generation")
    if fork_state.read_bytes() == fork_mirror.read_bytes():
        raise ValueError("fork fixture snapshots are not divergent")
    witness_body(fork_directory / "monotonic-witness.txt", witness_key)

    rollback_directory = directory / "rollback-failure"
    rollback_state = rollback_directory / "transaction-state.txt"
    rollback_mirror = rollback_directory / "mirror" / "transaction-state.txt"
    rollback_snapshots = [
        rollback_state,
        Path(f"{rollback_state}.previous"),
        rollback_mirror,
        Path(f"{rollback_mirror}.previous"),
    ]
    require_fields(
        parse_fields(
            rollback_directory / "recovery-failure.txt",
            "SMAVE_GATE_TXN_RECOVERY_FAILURE 1",
        ),
        ROLLBACK_FAILURE_FIELDS,
        rollback_directory / "recovery-failure.txt",
    )
    rollback_bodies = [snapshot_body(snapshot, key) for snapshot in rollback_snapshots]
    rollback_witness = witness_body(
        rollback_directory / "monotonic-witness.txt", witness_key
    )
    if max(int(body["generation"]) for body in rollback_bodies) >= int(rollback_witness["generation"]):
        raise ValueError("rollback fixture is not below the preserved witness")

    wrong_witness_directory = directory / "wrong-witness-key-failure"
    wrong_witness_state = wrong_witness_directory / "transaction-state.txt"
    wrong_witness_mirror = (
        wrong_witness_directory / "mirror" / "transaction-state.txt"
    )
    wrong_witness_snapshots = [
        wrong_witness_state,
        Path(f"{wrong_witness_state}.previous"),
        wrong_witness_mirror,
        Path(f"{wrong_witness_mirror}.previous"),
    ]
    require_fields(
        parse_fields(
            wrong_witness_directory / "recovery-failure.txt",
            "SMAVE_GATE_TXN_RECOVERY_FAILURE 1",
        ),
        WRONG_WITNESS_FAILURE_FIELDS,
        wrong_witness_directory / "recovery-failure.txt",
    )
    for snapshot in wrong_witness_snapshots:
        snapshot_body(snapshot, key)
    wrong_witness = wrong_witness_directory / "monotonic-witness.txt"
    witness_body(wrong_witness, witness_key)
    try:
        witness_body(wrong_witness, wrong_witness_key)
    except ValueError as error:
        if "authentication" not in str(error):
            raise
    else:
        raise ValueError("wrong witness key unexpectedly verified")

    if list(directory.rglob("transaction-state.txt*.tmp.*")):
        raise ValueError("orphan transaction-state temporary remains")
    if list(directory.rglob("monotonic-witness.txt*.tmp.*")):
        raise ValueError("orphan monotonic-witness temporary remains")
    print(
        "SMAVE_GATE_NETWORK_AUTHORITY_CHECK 1 "
        "snapshots=4 recoveries=2 failures=11"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
