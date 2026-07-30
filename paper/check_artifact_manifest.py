#!/usr/bin/env python3

import argparse
import hashlib
import sys
from pathlib import Path


def tree_digest(root: Path, paths: list[Path]) -> str:
    digest = hashlib.sha256()
    for path in sorted(paths):
        relative = path.relative_to(root).as_posix()
        digest.update(relative.encode())
        digest.update(b"\0")
        digest.update(path.read_bytes())
        digest.update(b"\0")
    return digest.hexdigest()


def native_external_performance_paths(directory: Path) -> list[Path]:
    return [
        directory / "evidence.txt",
        directory / "components/gate-parallel/evidence.txt",
        directory / "components/gate-parallel/raw-samples.txt",
        directory / "components/risk-adaptive/evidence.txt",
        directory / "components/risk-adaptive/raw-samples.txt",
        directory / "components/operator-replication/evidence.txt",
        directory / "components/operator-replication/performance.txt",
        directory / "components/operator-replication/operator-statistics.txt",
    ]


def directory_files(directory: Path) -> list[Path]:
    return sorted(path for path in directory.iterdir() if path.is_file())


def parse_manifest(path: Path) -> dict[str, str]:
    values = {}
    for line in path.read_text().splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            values[key] = value
    return values


def collect(
    repository_root: Path,
    pde_run_identifier: str,
    order_run_identifier: str,
) -> tuple[str, int, int]:
    paper_directory = repository_root / "paper"
    paper_sources = [
        *paper_directory.glob("*.tex"),
        *paper_directory.glob("sections/*.tex"),
        *paper_directory.glob("figures/*.tex"),
        *paper_directory.glob("data/*.dat"),
        paper_directory / "references.bib",
        paper_directory / "README.md",
        paper_directory / "REVIEW.md",
        paper_directory / "CLAIM_EVIDENCE.md",
        paper_directory / "ARTIFACT_SNAPSHOT.md",
        paper_directory / "check.sh",
        paper_directory / "check_evidence.py",
        paper_directory / "check_artifact_manifest.py",
        paper_directory / "generated/gate_scaling_values.tex",
        paper_directory / "generated/pde_timing_values.tex",
        paper_directory / "generated/order_sensitivity_values.tex",
        paper_directory / "generated/operator_shared_baseline_values.tex",
        paper_directory / "generated/hints_schedule_values.tex",
        paper_directory / "generated/hints_native_values.tex",
        paper_directory / "generated/solver_analysis_values.tex",
        paper_directory / "generated/joint_route_budget_values.tex",
        paper_directory / "generated/joint_route_scaling_values.tex",
        paper_directory / "generated/frozen_interaction_prevalence_values.tex",
        paper_directory / "generated/frozen_transition_attrition_values.tex",
        paper_directory / "generated/joint_route_budget_shift_values.tex",
        paper_directory / "generated/request_conditioned_joint_route_values.tex",
        paper_directory / "generated/suitesparse_request_conditioned_route_values.tex",
        repository_root / "kb/closest-work-refresh-2026-07-27.md",
    ]
    benchmark_harness = [
        repository_root / ".github/workflows/native-external-performance.yml",
        repository_root / "README.md",
        repository_root / "CMakeLists.txt",
        repository_root / "CMakePresets.json",
        repository_root / "artifact/CORE_REPRO_BUNDLE.md",
        repository_root / "artifact/make_core_repro_bundle.py",
        repository_root / "artifact/verify_core_repro_bundle.py",
        repository_root / "benchmark/run_pdebench_repeated_timing.sh",
        repository_root / "benchmark/analyze_pdebench_repeated_timing.py",
        repository_root / "benchmark/run_pdebench_order_sensitivity.sh",
        repository_root / "benchmark/analyze_pdebench_order_sensitivity.py",
        repository_root / "benchmark/analyze_operator_shared_baseline.py",
        repository_root / "benchmark/README.md",
        repository_root / "benchmark/native_external_performance_contract.py",
        repository_root / "benchmark/collect_native_external_performance.py",
        repository_root / "benchmark/verify_native_external_performance.py",
        repository_root / "benchmark/aggregate_native_external_performance.py",
        repository_root / "benchmark/verify_native_external_performance_campaign.py",
        repository_root / "benchmark/check_native_external_performance_campaign_contract.py",
        repository_root / "benchmark/analyze_router_shift.py",
        repository_root / "benchmark/analyze_router_shift_matrix.py",
        repository_root / "benchmark/analyze_complete_cost_decomposition.py",
        repository_root / "benchmark/analyze_hints_schedule_baseline.py",
        repository_root / "benchmark/prepare_hints_native_baseline.py",
        repository_root / "benchmark/run_hints_native_baseline.py",
        repository_root / "benchmark/analyze_hints_native_baseline.py",
        repository_root / "benchmark/analyze_frozen_interaction_prevalence.py",
        repository_root / "benchmark/replay_control_aware_anchor.py",
        repository_root / "benchmark/run_native_external_performance.sh",
        repository_root / "benchmark/Dockerfile.linux-ci",
        repository_root / "benchmark/run_linux_portability_checks.sh",
        repository_root / "benchmark/data-lock/README.md",
        repository_root / "benchmark/data-lock/acquire_suitesparse.py",
        repository_root / "benchmark/data-lock/verify_data_lock.py",
        repository_root / "benchmark/data-lock/verify_suitesparse_final_v3_selection.py",
        repository_root / "benchmark/data-lock/verify_suitesparse_final_v4_selection.py",
        repository_root / "benchmark/data-lock/verify_suitesparse_final_v5_selection.py",
        repository_root / "benchmark/data-lock/verify_suitesparse_final_v6_selection.py",
        repository_root / "benchmark/data-lock/pdebench.tsv",
        repository_root / "benchmark/data-lock/suitesparse.tsv",
        repository_root / "benchmark/data-lock/SUITESPARSE_FINAL_HELDOUT_V3.md",
        repository_root / "benchmark/data-lock/suitesparse-final-heldout-v3-selection.tsv",
        repository_root / "benchmark/data-lock/suitesparse-final-heldout-v3-payload.tsv",
        repository_root / "benchmark/data-lock/suitesparse-final-heldout-v3.tsv",
        repository_root / "benchmark/data-lock/SUITESPARSE_FINAL_HELDOUT_V4.md",
        repository_root / "benchmark/data-lock/suitesparse-final-heldout-v4-selection.tsv",
        repository_root / "benchmark/data-lock/suitesparse-final-heldout-v4-payload.tsv",
        repository_root / "benchmark/data-lock/suitesparse-final-heldout-v4.tsv",
        repository_root / "benchmark/data-lock/SUITESPARSE_FINAL_HELDOUT_V5.md",
        repository_root / "benchmark/data-lock/suitesparse-final-heldout-v5-selection.tsv",
        repository_root / "benchmark/data-lock/suitesparse-final-heldout-v5-payload.tsv",
        repository_root / "benchmark/data-lock/suitesparse-final-heldout-v5.tsv",
        repository_root / "benchmark/data-lock/suitesparse-final-heldout-v5-prefirst-run-contract.txt",
        repository_root / "benchmark/data-lock/SUITESPARSE_FINAL_HELDOUT_V6.md",
        repository_root / "benchmark/data-lock/suitesparse-final-heldout-v6-selection.tsv",
        repository_root / "benchmark/data-lock/suitesparse-final-heldout-v6-payload.tsv",
        repository_root / "benchmark/data-lock/suitesparse-final-heldout-v6.tsv",
        repository_root / "benchmark/data-lock/suitesparse-final-heldout-v6-prefirst-run-contract.txt",
        repository_root / "benchmark/data-lock/joint-route-scaling-round51-prefirst-run-contract.txt",
        repository_root / "benchmark/data-lock/frozen-interaction-prevalence-round52-analysis-contract.txt",
        repository_root / "benchmark/data-lock/frozen-transition-attrition-round53-analysis-contract.txt",
        repository_root / "docs/NATIVE_EXTERNAL_PERFORMANCE.md",
        repository_root / "benchmark/pdebench/files.tsv",
        repository_root / "benchmark/pdebench/download_full.sh",
        repository_root / "benchmark/pdebench/download_parallel_resume.sh",
        repository_root / "src/linear.cpp",
        repository_root / "src/superlu_sparse.cpp",
        repository_root / "src/operator.cpp",
        repository_root / "src/routing.cpp",
        repository_root / "src/runtime.cpp",
        repository_root / "src/solve_service.cpp",
        repository_root / "include/smave/routing.hpp",
        repository_root / "include/smave/runtime.hpp",
        repository_root / "include/smave/solve_service.hpp",
        repository_root / "tests/periodic_linear_unit.cpp",
        repository_root / "tests/test_smave.cpp",
        repository_root / "tests/gate_parallel_scaling_evidence.cpp",
        repository_root / "tests/cascade_ordering_evidence.cpp",
        repository_root / "tests/calibrated_correction_router_evidence.cpp",
        repository_root / "tests/joint_route_budget_evidence.cpp",
        repository_root / "tests/joint_route_scaling_evidence.cpp",
        repository_root / "tests/joint_route_budget_shift_evidence.cpp",
        repository_root / "tests/request_conditioned_joint_route_evidence.cpp",
        repository_root / "tests/suitesparse_request_conditioned_route_evidence.cpp",
        repository_root / "tests/suitesparse_lsqr_probe.cpp",
        repository_root / "tests/solve_service_unit.cpp",
        repository_root / "tests/risk_adaptive_gate_evidence.cpp",
        repository_root / "tests/verify_cascade_ordering.cmake",
        repository_root / "tests/verify_calibrated_correction_router.cmake",
        repository_root / "tests/verify_joint_route_budget.cmake",
        repository_root / "tests/verify_joint_route_scaling.cmake",
        repository_root / "tests/verify_frozen_interaction_prevalence.cmake",
        repository_root / "tests/verify_frozen_transition_attrition.cmake",
        repository_root / "tests/verify_joint_route_budget_shift.cmake",
        repository_root / "tests/verify_request_conditioned_joint_route.cmake",
        repository_root / "tests/verify_suitesparse_request_conditioned_route.cmake",
        repository_root / "tests/verify_router_shift.cmake",
        repository_root / "tests/generate_router_shift_matrix.py",
        repository_root / "tests/verify_router_shift_matrix.cmake",
        repository_root / "tests/verify_complete_cost_decomposition.cmake",
        repository_root / "tests/verify_phase5.cmake",
        repository_root / "tests/verify_nonlinear_operator.cmake",
        repository_root / "tests/verify_operator_shared_baseline.cmake",
        repository_root / "tests/verify_hints_schedule_baseline.cmake",
        repository_root / "tests/hints_native_smave_evidence.cpp",
        repository_root / "tests/verify_hints_native_baseline.cmake",
        repository_root / "tests/pdebench_benchmark_order.hpp",
        repository_root / "tests/pdebench_advection_benchmark.cpp",
        repository_root / "tests/pdebench_darcy_benchmark.cpp",
        repository_root / "tests/pdebench_burgers_benchmark.cpp",
        repository_root / "tests/pdebench_diffusion_sorption_benchmark.cpp",
        repository_root / "tests/pdebench_shallow_water_benchmark.cpp",
        repository_root / "tests/pdebench_ns_incompressible_benchmark.cpp",
        repository_root / "tests/pdebench_cfd_1d_benchmark.cpp",
    ]
    evidence_summaries = [
        repository_root / "build/release/gate-parallel-scaling/evidence.txt",
        repository_root / "build/release/cascade-ordering/evidence.txt",
        repository_root / "build/release/router-shift/evidence.txt",
        repository_root / "build/release/router-shift-matrix/evidence.txt",
        repository_root / "build/release/calibrated-correction-router/evidence.txt",
        repository_root / "build/release/joint-route-budget/evidence.txt",
        *directory_files(repository_root / "build/release/joint-route-scaling-round51"),
        *directory_files(repository_root / "build/release/frozen-interaction-prevalence-round52"),
        *directory_files(repository_root / "build/release/frozen-transition-attrition-round53"),
        repository_root / "build/release/joint-route-budget-shift/evidence.txt",
        repository_root / "build/release/request-conditioned-joint-route/evidence.txt",
        repository_root / "build/release/request-conditioned-joint-route/action-observations.tsv",
        repository_root / "build/release/request-conditioned-joint-route/request-conditioned-model.txt",
        *directory_files(repository_root / "build/release/suitesparse-request-conditioned-route-final-heldout-v4-first-run"),
        repository_root / "build/release/suitesparse-request-conditioned-route-final-heldout-v4-first-run.console.txt",
        *directory_files(repository_root / "build/release/suitesparse-request-conditioned-route-final-heldout-v5-first-run"),
        repository_root / "build/release/suitesparse-request-conditioned-route-final-heldout-v5-first-run.console.txt",
        *directory_files(repository_root / "build/release/suitesparse-request-conditioned-route-final-heldout-v6-first-run"),
        repository_root / "build/release/suitesparse-request-conditioned-route-final-heldout-v6-first-run.console.txt",
        *directory_files(repository_root / "build/release/suitesparse-control-aware-replay-v5"),
        *directory_files(repository_root / "build/release/suitesparse-control-aware-replay-v6"),
        repository_root / "build/release/complete-cost-decomposition/evidence.txt",
        repository_root / "build/release/phase4/source-repeat.competition",
        repository_root / "build/release/phase4/heldout-traces/heldout.competition",
        repository_root / "build/release/phase5/benchmark-traces/operator-ablation.txt",
        repository_root / "build/release/nonlinear-operator/benchmark-traces/operator-ablation.txt",
        repository_root / "build/release/pdebench-repeated-timing/evidence.txt",
        repository_root / "build/release/pdebench-order-sensitivity/evidence.txt",
        repository_root / "build/release/parallel-scaling/evidence.txt",
        repository_root / "build/release/batch-scaling/evidence.txt",
        repository_root / "build/release/operator-shared-baseline/evidence.txt",
        repository_root / "build/release/hints-schedule-baseline/evidence.txt",
        repository_root / "build/release/hints-native-baseline/evidence.txt",
        repository_root / "build/release/hints-native-baseline/official/evidence.txt",
        repository_root / "build/release/hints-native-baseline/official/raw-samples.txt",
        repository_root / "build/release/hints-native-baseline/smave/evidence.txt",
        repository_root / "build/release/hints-native-baseline/smave/raw-samples.txt",
        repository_root / "build/release/hints-native-baseline/workload/contract.txt",
        repository_root / "build/release/phase5/benchmark-traces/operator-hints-schedule-baseline.txt",
        repository_root / "build/release/phase5/benchmark-traces/operator-shared-hybrid-baseline.txt",
        repository_root / "build/release/nonlinear-operator/benchmark-traces/operator-shared-hybrid-baseline.txt",
        repository_root / "build/portability/linux-arm64/evidence.txt",
        repository_root / "build/portability/linux-amd64/evidence.txt",
        repository_root / "build/release/data-lock/evidence.txt",
        *native_external_performance_paths(
            repository_root / "build/native-external-dry-run-output"
        ),
    ]
    pde_raw_directory = (
        repository_root
        / "build/release/pdebench-repeated-timing"
        / pde_run_identifier
    )
    order_raw_directory = (
        repository_root
        / "build/release/pdebench-order-sensitivity"
        / order_run_identifier
    )
    pde_raw_reports = list(pde_raw_directory.glob("*.txt"))
    order_raw_reports = list(order_raw_directory.glob("*.txt"))
    values = [
        "SMAVE_ARTIFACT_MANIFEST 1",
        "snapshot_date=2026-07-29",
        f"pde_report_run={pde_run_identifier}",
        f"order_report_run={order_run_identifier}",
        f"paper_sources_sha256={tree_digest(repository_root, paper_sources)}",
        f"benchmark_harness_sha256={tree_digest(repository_root, benchmark_harness)}",
        f"evidence_summaries_sha256={tree_digest(repository_root, evidence_summaries)}",
        f"pde_raw_reports_sha256={tree_digest(repository_root, pde_raw_reports)}",
        f"pde_raw_report_count={len(pde_raw_reports)}",
        f"order_raw_reports_sha256={tree_digest(repository_root, order_raw_reports)}",
        f"order_raw_report_count={len(order_raw_reports)}",
        "END",
    ]
    return (
        "\n".join(values) + "\n",
        len(pde_raw_reports),
        len(order_raw_reports),
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--print", action="store_true", dest="print_manifest")
    parser.add_argument("--run")
    parser.add_argument("--order-run")
    arguments = parser.parse_args()

    paper_directory = Path(__file__).resolve().parent
    repository_root = paper_directory.parent
    manifest_path = paper_directory / "ARTIFACT_MANIFEST.txt"
    manifest_values = parse_manifest(manifest_path)
    pde_run_identifier = arguments.run or manifest_values["pde_report_run"]
    order_run_identifier = (
        arguments.order_run or manifest_values["order_report_run"]
    )
    expected, pde_raw_report_count, order_raw_report_count = collect(
        repository_root,
        pde_run_identifier,
        order_run_identifier,
    )
    if pde_raw_report_count != 217:
        raise ValueError(
            f"expected 217 pinned PDE reports, found {pde_raw_report_count}"
        )
    if order_raw_report_count != 434:
        raise ValueError(
            f"expected 434 pinned order reports, found {order_raw_report_count}"
        )
    if arguments.print_manifest:
        print(expected, end="")
        return 0
    if manifest_path.read_text() != expected:
        raise ValueError("artifact manifest is stale")
    print("SMAVE_ARTIFACT_MANIFEST_CHECK 1")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (KeyError, OSError, ValueError) as error:
        print(f"artifact manifest check failed: {error}", file=sys.stderr)
        raise SystemExit(1)
