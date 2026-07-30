#!/usr/bin/env python3

import argparse
import csv
import hashlib
from collections import Counter, defaultdict
from itertools import permutations
from pathlib import Path


Action = tuple[str, int]
Transition = tuple[Action, Action]
Request = tuple[str, str, str]


def digest(path: Path) -> str:
    result = hashlib.sha256()
    with path.open("rb") as source:
        while chunk := source.read(1024 * 1024):
            result.update(chunk)
    return result.hexdigest()


def read_evidence(path: Path) -> dict[str, str]:
    result = {}
    for line in path.read_text().splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            result[key] = value
    return result


def data_rows(path: Path) -> int:
    with path.open(newline="") as source:
        return max(0, sum(1 for _ in source) - 1)


def action_name(action: Action) -> str:
    return f"{action[0]}@{action[1]}"


def transition_name(transition: Transition) -> str:
    return f"{action_name(transition[0])}->{action_name(transition[1])}"


def pair_class(transition: Transition) -> tuple[str, str]:
    return tuple(sorted((transition[0][0], transition[1][0])))


def analyze_observations(path: Path) -> dict[str, object]:
    rows = list(csv.DictReader(path.open(newline=""), delimiter="\t"))
    grouped: dict[tuple[Request, Action], list[dict[str, str]]] = defaultdict(list)
    for row in rows:
        request = (row["split"], row["matrix"], row["request"])
        action = (row["expert"], int(row["work_iterations"]))
        grouped[(request, action)].append(row)

    stable_failures: dict[Request, list[Action]] = defaultdict(list)
    repetition_counts = set()
    for (request, action), samples in grouped.items():
        repetition_counts.add(len(samples))
        statuses = {sample["status"] for sample in samples}
        passed = sum(int(sample["passed"]) for sample in samples)
        if passed == 0 and len(statuses) == 1:
            stable_failures[request].append(action)
        else:
            stable_failures.setdefault(request, [])
    if repetition_counts != {5}:
        raise ValueError(f"expected exactly five repetitions, found {repetition_counts}")

    supports = {
        split: defaultdict(lambda: {"matrices": set(), "requests": set()})
        for split in ("training", "calibration", "heldout")
    }
    request_rows = []
    request_counts = Counter()
    qualifying_requests = Counter()
    for request in sorted(stable_failures):
        split, matrix, request_id = request
        actions = sorted(stable_failures[request])
        distinct_experts = len({action[0] for action in actions})
        transitions = {
            (previous, next_action)
            for previous, next_action in permutations(actions, 2)
            if previous[0] != next_action[0]
        }
        request_counts[split] += 1
        if transitions:
            qualifying_requests[split] += 1
        for transition in transitions:
            supports[split][transition]["matrices"].add(matrix)
            supports[split][transition]["requests"].add((matrix, request_id))
        request_rows.append(
            {
                "split": split,
                "matrix": matrix,
                "request": request_id,
                "stable_failed_actions": len(actions),
                "distinct_failed_experts": distinct_experts,
                "ordered_distinct_expert_pairs": len(transitions),
            }
        )

    pair_sets = {split: set(values) for split, values in supports.items()}
    pairs_ge_two_matrices = {
        split: {
            transition
            for transition, support in supports[split].items()
            if len(support["matrices"]) >= 2
        }
        for split in supports
    }
    development_supported = (
        pairs_ge_two_matrices["training"]
        & pairs_ge_two_matrices["calibration"]
    )
    heldout_observed = pair_sets["heldout"]

    return {
        "rows": len(rows),
        "request_actions": len(grouped),
        "request_counts": request_counts,
        "qualifying_requests": qualifying_requests,
        "supports": supports,
        "pair_sets": pair_sets,
        "pairs_ge_two_matrices": pairs_ge_two_matrices,
        "development_supported": development_supported,
        "heldout_observed": heldout_observed,
        "request_rows": request_rows,
    }


def write_request_rows(
    path: Path, analyses: dict[str, dict[str, object]]
) -> None:
    with path.open("w", newline="") as output:
        writer = csv.writer(output, delimiter="\t", lineterminator="\n")
        writer.writerow(
            [
                "version",
                "split",
                "matrix",
                "request",
                "stable_failed_actions",
                "distinct_failed_experts",
                "ordered_distinct_expert_pairs",
            ]
        )
        for version in sorted(analyses):
            for row in analyses[version]["request_rows"]:
                writer.writerow(
                    [
                        version,
                        row["split"],
                        row["matrix"],
                        row["request"],
                        row["stable_failed_actions"],
                        row["distinct_failed_experts"],
                        row["ordered_distinct_expert_pairs"],
                    ]
                )


def write_pair_rows(path: Path, analyses: dict[str, dict[str, object]]) -> None:
    with path.open("w", newline="") as output:
        writer = csv.writer(output, delimiter="\t", lineterminator="\n")
        writer.writerow(
            [
                "version",
                "transition",
                "previous_expert",
                "previous_budget",
                "next_expert",
                "next_budget",
                "training_requests",
                "training_matrices",
                "calibration_requests",
                "calibration_matrices",
                "heldout_requests",
                "heldout_matrices",
                "development_supported",
                "heldout_observed",
            ]
        )
        for version in sorted(analyses):
            analysis = analyses[version]
            all_pairs = set().union(*analysis["pair_sets"].values())
            for transition in sorted(all_pairs):
                values = []
                for split in ("training", "calibration", "heldout"):
                    support = analysis["supports"][split].get(
                        transition, {"requests": set(), "matrices": set()}
                    )
                    values.extend(
                        [len(support["requests"]), len(support["matrices"])]
                    )
                writer.writerow(
                    [
                        version,
                        transition_name(transition),
                        transition[0][0],
                        transition[0][1],
                        transition[1][0],
                        transition[1][1],
                        *values,
                        int(transition in analysis["development_supported"]),
                        int(transition in analysis["heldout_observed"]),
                    ]
                )


def write_pair_classes(path: Path, analyses: dict[str, dict[str, object]]) -> None:
    with path.open("w", newline="") as output:
        writer = csv.writer(output, delimiter="\t", lineterminator="\n")
        writer.writerow(["version", "scope", "expert_pair", "ordered_action_pairs"])
        for version in sorted(analyses):
            analysis = analyses[version]
            for scope, transitions in (
                ("development-supported", analysis["development_supported"]),
                ("heldout-observed", analysis["heldout_observed"]),
            ):
                counts = Counter(pair_class(transition) for transition in transitions)
                for experts, count in sorted(counts.items()):
                    writer.writerow(
                        [version, scope, f"{experts[0]}<->{experts[1]}", count]
                    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--v5", type=Path, required=True)
    parser.add_argument("--v6", type=Path, required=True)
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()
    arguments.output.mkdir(parents=True, exist_ok=True)

    input_directories = {"v5": arguments.v5, "v6": arguments.v6}
    analyses = {}
    frozen_evidence = {}
    for version, directory in input_directories.items():
        observations = directory / "action-observations.tsv"
        evidence = directory / "evidence.txt"
        conditional = directory / "conditional-cost-observations.tsv"
        analyses[version] = analyze_observations(observations)
        frozen_evidence[version] = read_evidence(evidence)
        if data_rows(conditional) != 0:
            raise ValueError(f"{version} unexpectedly contains conditional timings")
        for key, value in {
            "conditional_cost_heldout_excluded_from_selection_and_calibration": "1",
            "conditional_cost_candidate_transition_count": "0",
            "conditional_cost_calibration_count": "0",
            "interaction_plan_changed_requests": "0",
            "interaction_plans_with_calibrated_transition": "0",
        }.items():
            if frozen_evidence[version].get(key) != value:
                raise ValueError(f"{version} frozen interaction field changed: {key}")

    if analyses["v5"]["development_supported"] != analyses["v6"]["development_supported"]:
        raise ValueError("v5/v6 development pair support changed")

    write_request_rows(arguments.output / "request-coverage.tsv", analyses)
    write_pair_rows(arguments.output / "pair-support.tsv", analyses)
    write_pair_classes(arguments.output / "pair-classes.tsv", analyses)
    (arguments.output / arguments.contract.name).write_bytes(arguments.contract.read_bytes())

    development = analyses["v6"]["development_supported"]
    development_classes = sorted({pair_class(pair) for pair in development})
    if len(development_classes) != 1:
        raise ValueError(
            f"expected one development expert-pair class, found {development_classes}"
        )
    v5_heldout = analyses["v5"]["heldout_observed"]
    v6_heldout = analyses["v6"]["heldout_observed"]
    heldout_intersection = v5_heldout & v6_heldout
    heldout_union = v5_heldout | v6_heldout
    heldout_jaccard = len(heldout_intersection) / len(heldout_union)
    development_heldout_overlap = development & heldout_union

    evidence_lines = [
        "SMAVE_FROZEN_INTERACTION_PREVALENCE_ROUND52 1",
        "analysis_mode=posthoc-frozen-observation-diagnostic",
        "stable_failure=all-repetitions-failed-with-identical-status",
        "pair_definition=ordered-distinct-expert-actions",
        "training_support_matrices=2",
        "calibration_support_matrices=2",
        "heldout_excluded_from_selection_and_calibration=1",
        f"contract_sha256={digest(arguments.contract)}",
    ]
    for version in ("v5", "v6"):
        analysis = analyses[version]
        directory = input_directories[version]
        evidence_lines.extend(
            [
                f"{version}.action_observations_sha256={digest(directory / 'action-observations.tsv')}",
                f"{version}.frozen_evidence_sha256={digest(directory / 'evidence.txt')}",
                f"{version}.action_rows={analysis['rows']}",
                f"{version}.request_actions={analysis['request_actions']}",
                f"{version}.training_requests={analysis['request_counts']['training']}",
                f"{version}.calibration_requests={analysis['request_counts']['calibration']}",
                f"{version}.heldout_requests={analysis['request_counts']['heldout']}",
                f"{version}.training_requests_with_two_expert_failures={analysis['qualifying_requests']['training']}",
                f"{version}.calibration_requests_with_two_expert_failures={analysis['qualifying_requests']['calibration']}",
                f"{version}.heldout_requests_with_two_expert_failures={analysis['qualifying_requests']['heldout']}",
                f"{version}.training_ordered_failure_pairs={len(analysis['pair_sets']['training'])}",
                f"{version}.training_pairs_two_matrix_support={len(analysis['pairs_ge_two_matrices']['training'])}",
                f"{version}.calibration_ordered_failure_pairs={len(analysis['pair_sets']['calibration'])}",
                f"{version}.calibration_pairs_two_matrix_support={len(analysis['pairs_ge_two_matrices']['calibration'])}",
                f"{version}.development_supported_pairs={len(analysis['development_supported'])}",
                f"{version}.heldout_observed_pairs={len(analysis['heldout_observed'])}",
                f"{version}.development_heldout_overlap={len(analysis['development_supported'] & analysis['heldout_observed'])}",
                f"{version}.plan_gated_candidate_transitions={frozen_evidence[version]['conditional_cost_candidate_transition_count']}",
                f"{version}.conditional_timing_rows=0",
                f"{version}.calibrated_transitions={frozen_evidence[version]['conditional_cost_calibration_count']}",
            ]
        )
    development_experts = development_classes[0]
    evidence_lines.extend(
        [
            f"development_supported_pairs={len(development)}",
            f"development_supported_expert_pair_classes={len(development_classes)}",
            f"development_supported_expert_pair={development_experts[0]}<->{development_experts[1]}",
            f"v5_v6_development_pair_set_equal={int(analyses['v5']['development_supported'] == development)}",
            f"v5_v6_heldout_pair_intersection={len(heldout_intersection)}",
            f"v5_v6_heldout_pair_union={len(heldout_union)}",
            f"v5_v6_heldout_pair_jaccard={heldout_jaccard:.17g}",
            f"development_to_both_heldout_overlap={len(development_heldout_overlap)}",
            "isolated_failure_support_is_not_conditional_calibration=1",
            "heldout_diagnostic_only=1",
            "conditional_timing_inference=0",
            "policy_tuning=0",
            "cohort_search=0",
            "solver_execution=0",
            "request_coverage=request-coverage.tsv",
            "pair_support=pair-support.tsv",
            "pair_classes=pair-classes.tsv",
            "END",
            "",
        ]
    )
    (arguments.output / "evidence.txt").write_text("\n".join(evidence_lines))
    print("SMAVE_FROZEN_INTERACTION_PREVALENCE_ROUND52 1")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
