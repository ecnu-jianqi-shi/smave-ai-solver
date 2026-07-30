#!/usr/bin/env python3

import argparse
import csv
import hashlib
import math
import shlex
import statistics
from pathlib import Path


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


def parse_action(value: str) -> tuple[str, int]:
    if value == "terminal":
        return "", 0
    expert, budget = value.rsplit("@", 1)
    return expert, int(budget)


def parse_family_actions(value: str) -> dict[str, tuple[str, int]]:
    result = {}
    for item in value.split(","):
        family, action = item.split("=", 1)
        result[family] = parse_action(action)
    return result


def numeric_family(value: str) -> str:
    if value == "spd":
        return "spd"
    if value == "symmetric-non-spd":
        return "symmetric-indefinite"
    if value == "nonsymmetric":
        return "nonsymmetric"
    raise ValueError(f"unknown numeric class: {value}")


def read_heldout_families(path: Path) -> dict[str, str]:
    result = {}
    with path.open(newline="") as source:
        rows = [row for row in csv.reader(source, delimiter="\t") if row]
    header = [value.removeprefix("# ") for value in rows[0]]
    for values in rows[1:]:
        row = dict(zip(header, values))
        result[row["name"]] = numeric_family(row["numeric_class"])
    return result


def logistic(value: float) -> float:
    if value >= 0.0:
        inverse = math.exp(-value)
        return 1.0 / (1.0 + inverse)
    exponential = math.exp(value)
    return exponential / (1.0 + exponential)


def parse_model(path: Path) -> dict[str, object]:
    lines = path.read_text().splitlines()
    if lines[0] != "SMAVE_REQUEST_CONDITIONED_ROUTING_MODEL 2":
        raise ValueError("unsupported routing model schema")
    feature_tokens = shlex.split(lines[1])
    feature_count = int(feature_tokens[1])
    feature_names = feature_tokens[2:]
    if len(feature_names) != feature_count:
        raise ValueError("routing model feature count mismatch")
    means = [float(value) for value in lines[2].split()[1:]]
    scales = [float(value) for value in lines[3].split()[1:]]
    actions = {}
    index = 4
    while lines[index].startswith("ACTION "):
        action_tokens = shlex.split(lines[index])
        expert = action_tokens[1]
        budget = int(action_tokens[2])
        predictor = {
            "cost_relative": int(action_tokens[6]) == 1,
            "cost_error": float(action_tokens[7]),
            "pass_error": float(action_tokens[8]),
            "cost_upper": float(action_tokens[9]),
            "pass_upper": float(action_tokens[10]),
            "log_offset": float(action_tokens[11]),
            "pass_offset": float(action_tokens[12]),
        }
        predictor["cost"] = [float(value) for value in lines[index + 1].split()[1:]]
        predictor["pass"] = [float(value) for value in lines[index + 2].split()[1:]]
        predictor["support_min"] = [
            float(value) for value in lines[index + 3].split()[1:]
        ]
        predictor["support_max"] = [
            float(value) for value in lines[index + 4].split()[1:]
        ]
        joint_tokens = lines[index + 5].split()
        joint_indices = int(joint_tokens[1])
        joint_centers = int(joint_tokens[2])
        predictor["joint_upper"] = float(joint_tokens[3])
        cursor = 4
        predictor["joint_indices"] = [
            int(value) for value in joint_tokens[cursor : cursor + joint_indices]
        ]
        cursor += joint_indices
        center_values = [float(value) for value in joint_tokens[cursor:]]
        expected_center_values = joint_indices * joint_centers
        if len(center_values) != expected_center_values:
            raise ValueError("routing model joint-support width mismatch")
        predictor["joint_centers"] = [
            center_values[offset : offset + joint_indices]
            for offset in range(0, len(center_values), joint_indices)
        ] if joint_indices else []
        prior_tokens = shlex.split(lines[index + 6])
        prior_count = int(prior_tokens[1])
        cursor = 2
        priors = {}
        for _ in range(prior_count):
            family = prior_tokens[cursor]
            priors[family] = {
                "pooled_log_cost": float(prior_tokens[cursor + 3]),
                "pooled_pass_probability": float(prior_tokens[cursor + 4]),
                "cost_weight": float(prior_tokens[cursor + 5]),
                "pass_weight": float(prior_tokens[cursor + 6]),
                "cost_upper": float(prior_tokens[cursor + 7]),
                "pass_upper": float(prior_tokens[cursor + 8]),
            }
            cursor += 9
        if cursor != len(prior_tokens):
            raise ValueError("routing model family-prior width mismatch")
        predictor["priors"] = priors
        actions[(expert, budget)] = predictor
        index += 7
    if not lines[index].startswith("ACTION_COUNT ") or lines[index + 1] != "END":
        raise ValueError("routing model footer mismatch")
    if int(lines[index].split()[1]) != len(actions):
        raise ValueError("routing model action count mismatch")
    return {
        "feature_names": feature_names,
        "means": means,
        "scales": scales,
        "actions": actions,
    }


def predict(
    model: dict[str, object],
    action: tuple[str, int],
    features: dict[str, float],
    terminal_reference: float,
    family: str,
) -> dict[str, float]:
    predictor = model["actions"][action]
    normalized = [
        (features[name] - mean) / scale
        for name, mean, scale in zip(
            model["feature_names"], model["means"], model["scales"]
        )
    ]
    row = [1.0, *normalized]
    log_cost = sum(
        coefficient * value for coefficient, value in zip(predictor["cost"], row)
    ) + predictor["log_offset"]
    log_cost = min(40.0, max(-40.0, log_cost))
    pass_probability = logistic(
        sum(
            coefficient * value
            for coefficient, value in zip(predictor["pass"], row)
        ) + predictor["pass_offset"]
    )
    squared_extrapolation = 0.0
    for value, minimum, maximum in zip(
        normalized, predictor["support_min"], predictor["support_max"]
    ):
        extrapolation = max(0.0, minimum - value, value - maximum)
        squared_extrapolation += extrapolation * extrapolation
    support = math.sqrt(squared_extrapolation)
    if predictor["joint_indices"]:
        nearest = min(
            math.sqrt(
                sum(
                    (normalized[index] - center[position]) ** 2
                    for position, index in enumerate(predictor["joint_indices"])
                ) / len(predictor["joint_indices"])
            )
            for center in predictor["joint_centers"]
        )
        support = max(support, max(0.0, nearest - predictor["joint_upper"]))
    cost_uncertainty = max(predictor["cost_error"], predictor["cost_upper"])
    pass_uncertainty = max(predictor["pass_error"], predictor["pass_upper"])
    if family in predictor["priors"]:
        prior = predictor["priors"][family]
        log_cost = (
            prior["cost_weight"] * log_cost
            + (1.0 - prior["cost_weight"]) * prior["pooled_log_cost"]
        )
        pass_probability = (
            prior["pass_weight"] * pass_probability
            + (1.0 - prior["pass_weight"]) * prior["pooled_pass_probability"]
        )
        cost_uncertainty = max(cost_uncertainty, prior["cost_upper"])
        pass_uncertainty = max(pass_uncertainty, prior["pass_upper"])
    attempt_wall = math.exp(log_cost)
    if predictor["cost_relative"]:
        attempt_wall *= terminal_reference
    return {
        "attempt_wall_us": attempt_wall,
        "pass_probability": pass_probability,
        "cost_uncertainty": cost_uncertainty,
        "pass_uncertainty": pass_uncertainty,
        "support": support,
    }


def conservative_single_cost(prediction: dict[str, float], terminal_cost: float) -> float:
    support_multiplier = math.exp(min(prediction["support"], math.log(1.0e6)))
    inflated_cost = (
        prediction["attempt_wall_us"]
        * (1.0 + prediction["cost_uncertainty"])
        * support_multiplier
    )
    lower_pass = min(
        1.0,
        max(
            0.0,
            prediction["pass_probability"]
            - prediction["pass_uncertainty"]
            - min(1.0, prediction["support"]),
        ),
    )
    return inflated_cost + (1.0 - lower_pass) * terminal_cost


def read_observations(
    path: Path, feature_names: list[str]
) -> tuple[dict[tuple[str, str], dict[str, float]], dict[tuple[str, str], dict[tuple[str, int], tuple[float, bool]]]]:
    rows = list(csv.DictReader(path.open(), delimiter="\t"))
    features = {}
    samples = {}
    for row in rows:
        if row["split"] != "heldout":
            continue
        request = (row["matrix"], row["request"])
        features.setdefault(
            request, {name: float(row[name]) for name in feature_names}
        )
        action = (row["expert"], int(row["work_iterations"]))
        samples.setdefault(request, {}).setdefault(action, []).append(row)
    aggregated = {}
    for request, actions in samples.items():
        aggregated[request] = {}
        for action, action_rows in actions.items():
            walls = [float(row["attempt_wall_us"]) for row in action_rows]
            passes = sum(int(row["passed"]) for row in action_rows)
            aggregated[request][action] = (
                statistics.median(walls), passes * 2 >= len(action_rows)
            )
    return features, aggregated


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--evidence-dir", type=Path, required=True)
    parser.add_argument("--heldout-lock", type=Path, required=True)
    parser.add_argument("--routing-source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--decisions", type=Path, required=True)
    arguments = parser.parse_args()
    evidence_path = arguments.evidence_dir / "evidence.txt"
    model_path = arguments.evidence_dir / "request-conditioned-model.txt"
    observations_path = arguments.evidence_dir / "action-observations.tsv"
    summary_path = arguments.evidence_dir / "request-summary.tsv"
    evidence = read_evidence(evidence_path)
    model = parse_model(model_path)
    families = read_heldout_families(arguments.heldout_lock)
    family_actions = parse_family_actions(
        evidence.get("training_family_fixed_actions", evidence["family_fixed_actions"])
    )
    fixed_action = parse_action(evidence["fixed_action"])
    terminal_action = ("terminal-numerical-linear-cascade-v1", 0)
    features, actions = read_observations(
        observations_path, model["feature_names"]
    )
    summaries = {
        (row["matrix"], row["request"]): row
        for row in csv.DictReader(summary_path.open(), delimiter="\t")
        if row["split"] == "heldout"
    }
    if set(features) != set(summaries):
        raise ValueError("held-out replay request set mismatch")

    oracle_total = 0.0
    fixed_total = 0.0
    family_total = 0.0
    replay_total = 0.0
    switched = 0
    decision_rows = []
    for request in sorted(features):
        matrix, request_id = request
        family = families[matrix]
        terminal_actual = float(summaries[request]["terminal_wall_us"])
        terminal_prediction = predict(
            model, terminal_action, features[request], 0.0, family
        )["attempt_wall_us"]

        def realized(action: tuple[str, int]) -> float:
            if not action[0] or action not in actions[request]:
                return terminal_actual
            wall, passed = actions[request][action]
            return wall + (0.0 if passed else terminal_actual)

        family_action = family_actions[family]
        fixed_realized = realized(fixed_action)
        family_realized = realized(family_action)
        fixed_total += fixed_realized
        family_total += family_realized
        oracle_total += min(
            [terminal_actual]
            + [realized(action) for action in actions[request]]
        )

        fixed_prediction = predict(
            model, fixed_action, features[request], terminal_prediction, family
        ) if fixed_action[0] else None
        fixed_upper = terminal_prediction if fixed_prediction is None else (
            conservative_single_cost(fixed_prediction, terminal_prediction)
        )
        family_prediction = predict(
            model, family_action, features[request], terminal_prediction, family
        ) if family_action[0] and family_action in model["actions"] else None
        family_upper = terminal_prediction if family_prediction is None else (
            conservative_single_cost(family_prediction, terminal_prediction)
        )
        out_of_support = (
            family_prediction is not None
            and family_prediction["support"] >= math.log(4.0)
        )
        choose_family = (
            family_action != fixed_action
            and not out_of_support
            and family_upper < fixed_upper * 0.95
        )
        chosen = family_action if choose_family else fixed_action
        replay_total += realized(chosen)
        switched += chosen != family_action
        decision_rows.append(
            {
                "matrix": matrix,
                "request": request_id,
                "family": family,
                "family_action": "terminal" if not family_action[0] else f"{family_action[0]}@{family_action[1]}",
                "fixed_action": "terminal" if not fixed_action[0] else f"{fixed_action[0]}@{fixed_action[1]}",
                "family_upper_us": family_upper,
                "fixed_upper_us": fixed_upper,
                "family_support_extrapolation": 0.0 if family_prediction is None else family_prediction["support"],
                "selected_action": "terminal" if not chosen[0] else f"{chosen[0]}@{chosen[1]}",
                "selected_realized_us": realized(chosen),
            }
        )

    fixed_regret = fixed_total / oracle_total
    family_regret = family_total / oracle_total
    replay_regret = replay_total / oracle_total
    official_fixed = float(evidence["fixed_action_heldout_regret"])
    official_family = float(
        evidence.get(
            "training_family_fixed_action_heldout_regret",
            evidence["family_fixed_action_heldout_regret"],
        )
    )
    tolerance = 1.0e-12
    if abs(fixed_regret - official_fixed) > tolerance:
        raise ValueError("frozen fixed-control replay mismatch")
    if abs(family_regret - official_family) > tolerance:
        raise ValueError("frozen family-control replay mismatch")
    official_control = evidence.get("control_aware_anchor_heldout_regret")
    if official_control is not None and abs(replay_regret - float(official_control)) > tolerance:
        raise ValueError("official control-aware replay mismatch")

    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    with arguments.decisions.open("w", newline="") as target:
        writer = csv.DictWriter(target, fieldnames=decision_rows[0].keys(), delimiter="\t")
        writer.writeheader()
        writer.writerows(decision_rows)
    arguments.output.write_text(
        "\n".join(
            [
                "SMAVE_CONTROL_AWARE_ANCHOR_REPLAY 1",
                "contract=frozen-observation-counterfactual-no-solver-reexecution",
                f"source_evidence_sha256={digest(evidence_path)}",
                f"source_model_sha256={digest(model_path)}",
                f"source_observations_sha256={digest(observations_path)}",
                f"source_request_summary_sha256={digest(summary_path)}",
                f"source_heldout_lock_sha256={digest(arguments.heldout_lock)}",
                f"routing_source_sha256={digest(arguments.routing_source)}",
                f"heldout_requests={len(features)}",
                f"official_fixed_regret={official_fixed:.17g}",
                f"recomputed_fixed_regret={fixed_regret:.17g}",
                f"official_training_family_fixed_regret={official_family:.17g}",
                f"recomputed_training_family_fixed_regret={family_regret:.17g}",
                f"control_aware_replay_regret={replay_regret:.17g}",
                f"control_aware_vs_fixed_ratio={replay_regret / fixed_regret:.17g}",
                f"control_aware_vs_training_family_ratio={replay_regret / family_regret:.17g}",
                f"requests_switched_from_training_family={switched}",
                "minimum_family_anchor_gain_fraction=0.05",
                "severe_support_extrapolation=log(4)",
                "solver_reexecution=0",
                f"decisions_sha256={digest(arguments.decisions)}",
                "END",
                "",
            ]
        )
    )
    print("SMAVE_CONTROL_AWARE_ANCHOR_REPLAY 1")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
