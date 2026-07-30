#include "smave/family_routing.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace smave {
namespace {

const CompetitionEntry& entry_for(
    const CompetitionReport& report,
    const std::string& expert_version) {
    const auto entry = std::find_if(
        report.entries.begin(), report.entries.end(),
        [&](const CompetitionEntry& item) {
            return item.expert_version == expert_version;
        });
    if (entry == report.entries.end()) {
        throw std::invalid_argument("family Router selected an unavailable expert");
    }
    return *entry;
}

std::filesystem::path first_scenario(const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> paths;
    for (const auto& item : std::filesystem::directory_iterator(directory)) {
        if (item.is_regular_file() && item.path().extension() == ".conf") {
            paths.push_back(item.path());
        }
    }
    std::sort(paths.begin(), paths.end());
    if (paths.empty()) throw std::invalid_argument("held-out family suite has no scenarios");
    return paths.front();
}

std::string evaluation_hash(const FamilyRouterEvaluation& evaluation) {
    std::ostringstream contract;
    contract << std::setprecision(17)
             << evaluation.source_block_fingerprint << '|'
             << evaluation.heldout_block_fingerprint << '|'
             << evaluation.source_competition_hash << '|'
             << evaluation.heldout_competition_hash << '|';
    if (evaluation.schema_version >= 3) {
        contract << evaluation.source_dataset_id << '|'
                 << evaluation.source_dataset_version << '|'
                 << evaluation.source_dataset_manifest_hash << '|'
                 << evaluation.heldout_dataset_id << '|'
                 << evaluation.heldout_dataset_version << '|'
                 << evaluation.heldout_dataset_manifest_hash << '|';
    }
    contract
             << evaluation.embedding_similarity << '|'
             << evaluation.minimum_similarity << '|'
             << evaluation.minimum_speedup << '|'
             << evaluation.repetitions << '|'
             << evaluation.fixed_expert << '|'
             << evaluation.calibrated_expert << '|'
             << evaluation.oracle_expert << '|'
             << evaluation.fixed_median_wall_us << '|'
             << evaluation.calibrated_median_wall_us << '|'
             << evaluation.oracle_median_wall_us << '|'
             << evaluation.calibrated_speedup << '|'
             << evaluation.scenarios << '|'
             << evaluation.paired_samples << '|'
             << evaluation.fixed_p99_wall_us << '|'
             << evaluation.calibrated_p99_wall_us << '|'
             << evaluation.paired_median_speedup << '|'
             << evaluation.paired_win_rate << '|'
             << evaluation.paired_speedup_ci95_lower << '|'
             << evaluation.paired_speedup_ci95_upper << '|'
             << evaluation.bootstrap_samples << '|'
             << evaluation.maximum_mixed_qoi_error << '|'
             << evaluation.fixed_failures << '|'
             << evaluation.calibrated_failures << '|'
             << evaluation.gate_mismatches << '|'
             << evaluation.p99_not_regressed << '|'
             << evaluation.same_accuracy << '|'
             << evaluation.fixed_dangerous_misroutes << '|'
             << evaluation.calibrated_dangerous_misroutes << '|'
             << evaluation.distinct_instance << '|'
             << evaluation.improved << '|'
             << evaluation.safe;
    std::uint64_t hash = 1469598103934665603ULL;
    for (const unsigned char character : contract.str()) {
        hash ^= character;
        hash *= 1099511628211ULL;
    }
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << hash;
    return output.str();
}

std::string value(const std::string& line, const std::string& key) {
    if (!line.starts_with(key)) throw std::invalid_argument("invalid family Router field");
    return line.substr(key.size());
}

double real_value(const std::string& line, const std::string& key) {
    const auto text = value(line, key);
    std::size_t consumed{};
    const double result = std::stod(text, &consumed);
    if (consumed != text.size()) throw std::invalid_argument("invalid family Router number");
    return result;
}

std::size_t size_value(const std::string& line, const std::string& key) {
    const auto text = value(line, key);
    std::size_t consumed{};
    const auto result = std::stoull(text, &consumed);
    if (consumed != text.size()) throw std::invalid_argument("invalid family Router count");
    return static_cast<std::size_t>(result);
}

bool bool_value(const std::string& line, const std::string& key) {
    const auto text = value(line, key);
    if (text == "1") return true;
    if (text == "0") return false;
    throw std::invalid_argument("invalid family Router boolean");
}

}  // namespace

void FamilyRouterEvaluation::seal() {
    report_hash = evaluation_hash(*this);
}

void FamilyRouterEvaluation::validate() const {
    if ((schema_version != 2 && schema_version != 3) ||
        source_block_fingerprint.empty() || heldout_block_fingerprint.empty() ||
        source_competition_hash.empty() || heldout_competition_hash.empty() ||
        fixed_expert.empty() ||
        calibrated_expert.empty() || oracle_expert.empty() ||
        !distinct_instance || embedding_similarity < 0.0 ||
        embedding_similarity > 1.0 || minimum_similarity < 0.0 ||
        minimum_similarity > 1.0 || minimum_speedup <= 1.0 || repetitions == 0 ||
        embedding_similarity < minimum_similarity || fixed_median_wall_us < 0.0 ||
        calibrated_median_wall_us < 0.0 || oracle_median_wall_us < 0.0 ||
        calibrated_speedup < 0.0 || scenarios == 0 || paired_samples == 0 ||
        fixed_p99_wall_us < 0.0 || calibrated_p99_wall_us < 0.0 ||
        paired_median_speedup < 0.0 || paired_win_rate < 0.0 ||
        paired_win_rate > 1.0 || paired_speedup_ci95_lower < 0.0 ||
        paired_speedup_ci95_upper < paired_speedup_ci95_lower ||
        bootstrap_samples == 0 || maximum_mixed_qoi_error < 0.0) {
        throw std::invalid_argument("invalid family Router evaluation");
    }
    const bool has_source_dataset = !source_dataset_id.empty() ||
        !source_dataset_version.empty() || !source_dataset_manifest_hash.empty();
    const bool has_heldout_dataset = !heldout_dataset_id.empty() ||
        !heldout_dataset_version.empty() || !heldout_dataset_manifest_hash.empty();
    if (schema_version == 2 && (has_source_dataset || has_heldout_dataset)) {
        throw std::invalid_argument("family Router evaluation v2 cannot contain lineage");
    }
    if (schema_version == 3 &&
        (source_dataset_id.empty() || source_dataset_version.empty() ||
         source_dataset_manifest_hash.empty() || heldout_dataset_id.empty() ||
         heldout_dataset_version.empty() || heldout_dataset_manifest_hash.empty())) {
        throw std::invalid_argument(
            "family Router evaluation v3 requires source and heldout lineage");
    }
    const double expected_speedup = calibrated_median_wall_us > 0.0
        ? fixed_median_wall_us / calibrated_median_wall_us
        : 0.0;
    if (std::abs(expected_speedup - calibrated_speedup) > 1.0e-12 ||
        improved != (paired_speedup_ci95_lower >= minimum_speedup && same_accuracy) ||
        safe != (calibrated_dangerous_misroutes <= fixed_dangerous_misroutes &&
                 calibrated_failures == 0 && gate_mismatches == 0 && same_accuracy)) {
        throw std::invalid_argument("inconsistent family Router evaluation");
    }
    if (report_hash != evaluation_hash(*this)) {
        throw std::invalid_argument("family Router evaluation integrity check failed");
    }
}

FamilyRouterEvaluation evaluate_family_router(
    const ModelIR& source_model,
    const CompetitionReport& source_report,
    const ModelIR& heldout_model,
    const Registry& heldout_registry,
    const RuntimeBundle& heldout_bundle,
    const std::filesystem::path& heldout_scenarios,
    const std::filesystem::path& trace_directory,
    double minimum_similarity,
    double minimum_speedup,
    std::size_t repetitions,
    Tolerance tolerance,
    std::optional<DatasetManifest> heldout_dataset) {
    source_report.validate();
    if (source_model.blocks.size() != 1 || heldout_model.blocks.size() != 1) {
        throw std::invalid_argument("family Router evaluation requires one block per model");
    }
    if (minimum_similarity < 0.0 || minimum_similarity > 1.0 ||
        minimum_speedup <= 1.0 || repetitions == 0) {
        throw std::invalid_argument("invalid family Router evaluation threshold");
    }
    const auto& source_block = source_model.blocks.front();
    const auto& heldout_block = heldout_model.blocks.front();
    if (source_report.block_fingerprint != source_block.fingerprint) {
        throw std::invalid_argument("source competition report targets another block");
    }
    FamilyRouterEvaluation evaluation;
    if (heldout_dataset) {
        if (source_report.schema_version != 4) {
            throw std::invalid_argument(
                "snapshot family evaluation requires a lineage-bound source competition");
        }
        evaluation.schema_version = 3;
        evaluation.source_dataset_id = source_report.dataset_id;
        evaluation.source_dataset_version = source_report.dataset_version;
        evaluation.source_dataset_manifest_hash = source_report.dataset_manifest_hash;
        evaluation.heldout_dataset_id = heldout_dataset->dataset_id;
        evaluation.heldout_dataset_version = heldout_dataset->version;
        evaluation.heldout_dataset_manifest_hash = heldout_dataset->manifest_hash;
    } else if (source_report.schema_version != 3) {
        throw std::invalid_argument(
            "directory family evaluation requires a legacy source competition");
    }
    evaluation.source_block_fingerprint = source_block.fingerprint;
    evaluation.heldout_block_fingerprint = heldout_block.fingerprint;
    evaluation.source_competition_hash = source_report.report_hash;
    evaluation.minimum_similarity = minimum_similarity;
    evaluation.minimum_speedup = minimum_speedup;
    evaluation.repetitions = repetitions;
    evaluation.distinct_instance = source_block.fingerprint != heldout_block.fingerprint;
    evaluation.embedding_similarity = embedding_similarity(
        encode_block(source_model, source_block),
        encode_block(heldout_model, heldout_block));
    if (!evaluation.distinct_instance) {
        throw std::invalid_argument("held-out family instance must have a distinct fingerprint");
    }
    if (evaluation.embedding_similarity < minimum_similarity) {
        throw std::invalid_argument("held-out block is outside the source equation family");
    }

    auto target_report = compete_experts(
        heldout_model,
        heldout_registry,
        heldout_bundle,
        heldout_scenarios,
        trace_directory,
        tolerance,
        repetitions);
    if (heldout_dataset) {
        target_report.schema_version = 4;
        target_report.dataset_id = heldout_dataset->dataset_id;
        target_report.dataset_version = heldout_dataset->version;
        target_report.dataset_manifest_hash = heldout_dataset->manifest_hash;
        target_report.seal();
        target_report.validate();
    }
    std::filesystem::create_directories(trace_directory);
    write_competition_report(
        target_report, trace_directory / "heldout.competition");
    evaluation.heldout_competition_hash = target_report.report_hash;
    const auto candidates = CompileRouter{}.lookup(
        heldout_block, heldout_registry, heldout_bundle);
    BlockContext context;
    context.values = read_scenario(first_scenario(heldout_scenarios));
    const auto fixed_plan = RuntimeRouter{}.route(
        heldout_block, context, candidates, heldout_registry, heldout_bundle);
    evaluation.fixed_expert = fixed_plan.steps.empty()
        ? heldout_bundle.terminal_fallback
        : fixed_plan.steps.front().expert_version;
    const bool source_winner_available = source_report.winner == heldout_bundle.terminal_fallback ||
        std::any_of(candidates.begin(), candidates.end(), [&](const CandidateExpert& candidate) {
            return candidate.builtin && candidate.expert_version == source_report.winner;
        });
    evaluation.calibrated_expert = source_winner_available
        ? source_report.winner
        : evaluation.fixed_expert;
    evaluation.oracle_expert = target_report.winner;

    const auto& fixed = entry_for(target_report, evaluation.fixed_expert);
    const auto& calibrated = entry_for(target_report, evaluation.calibrated_expert);
    const auto& oracle = entry_for(target_report, evaluation.oracle_expert);
    evaluation.fixed_median_wall_us = fixed.median_wall_us;
    evaluation.calibrated_median_wall_us = calibrated.median_wall_us;
    evaluation.oracle_median_wall_us = oracle.median_wall_us;
    RoutingConfig fixed_routing;
    fixed_routing.top_k = 1;
    fixed_routing.expert_allowlist.insert(evaluation.fixed_expert);
    RoutingConfig calibrated_routing;
    calibrated_routing.top_k = 1;
    calibrated_routing.calibration_block_fingerprint = heldout_block.fingerprint;
    calibrated_routing.calibration_winner = evaluation.calibrated_expert;
    calibrated_routing.expert_allowlist.insert(evaluation.calibrated_expert);
    const Runtime fixed_runtime(
        heldout_model, heldout_registry, heldout_bundle, tolerance, fixed_routing);
    const Runtime calibrated_runtime(
        heldout_model, heldout_registry, heldout_bundle, tolerance, calibrated_routing);
    const auto paired = benchmark_runtimes(
        fixed_runtime,
        calibrated_runtime,
        heldout_scenarios,
        trace_directory / "paired-runtime",
        repetitions,
        3);
    struct ExternalComparison {
        std::string backend;
        const CompetitionEntry* competition{};
        PerformanceReport paired;
    };
    std::vector<ExternalComparison> external_comparisons;
    for (const std::string backend : {
             "superlu-dgssv-cpu-v1", "accelerate-sparse-qr-cpu-v1"}) {
        const auto candidate = std::find_if(
            candidates.begin(), candidates.end(), [&](const CandidateExpert& item) {
                return item.builtin && item.expert_version == backend;
            });
        if (candidate == candidates.end()) continue;
        const auto& competition = entry_for(target_report, backend);
        if (competition.passes == 0 || competition.fallbacks != 0 ||
            competition.failures != 0 || competition.erroneous_accepts != 0) {
            continue;
        }
        RoutingConfig external_routing;
        external_routing.top_k = 1;
        external_routing.calibration_block_fingerprint = heldout_block.fingerprint;
        external_routing.calibration_winner = backend;
        external_routing.expert_allowlist.insert(backend);
        const Runtime external_runtime(
            heldout_model, heldout_registry, heldout_bundle, tolerance, external_routing);
        external_comparisons.push_back({
            .backend = backend,
            .competition = &competition,
            .paired = benchmark_runtimes(
                external_runtime,
                calibrated_runtime,
                heldout_scenarios,
                trace_directory / "external-paired" / backend,
                repetitions,
                3),
        });
    }
    std::ofstream external_report(trace_directory / "external-baselines.txt");
    if (!external_report) {
        throw std::runtime_error("cannot write external baseline report");
    }
    external_report << std::setprecision(17)
                    << "SMAVE_EXTERNAL_BASELINES 1\n"
                    << "contract=paired-complete-runtime-external-vs-calibrated\n"
                    << "entries=" << external_comparisons.size() << '\n';
    for (const auto& comparison : external_comparisons) {
        external_report
            << "BASELINE " << std::quoted(comparison.backend)
            << " competition_attempts=" << comparison.competition->attempts
            << " competition_passes=" << comparison.competition->passes
            << " competition_fallbacks=" << comparison.competition->fallbacks
            << " competition_failures=" << comparison.competition->failures
            << " competition_erroneous_accepts="
            << comparison.competition->erroneous_accepts
            << " paired_samples=" << comparison.paired.samples
            << " external_median_us=" << comparison.paired.baseline_wall_us.median
            << " calibrated_median_us=" << comparison.paired.accelerated_wall_us.median
            << " calibrated_paired_median_speedup="
            << comparison.paired.paired_median_speedup
            << " calibrated_bootstrap_95_lower="
            << comparison.paired.paired_speedup_ci95_lower
            << " calibrated_bootstrap_95_upper="
            << comparison.paired.paired_speedup_ci95_upper
            << " maximum_mixed_qoi_error="
            << comparison.paired.maximum_mixed_qoi_error
            << " external_failures=" << comparison.paired.baseline_failures
            << " calibrated_failures=" << comparison.paired.accelerated_failures
            << " gate_mismatches=" << comparison.paired.gate_mismatches
            << " same_accuracy=" << comparison.paired.same_accuracy << '\n';
    }
    external_report << "END\n";
    evaluation.scenarios = paired.scenarios;
    evaluation.paired_samples = paired.samples;
    evaluation.fixed_median_wall_us = paired.baseline_wall_us.median;
    evaluation.calibrated_median_wall_us = paired.accelerated_wall_us.median;
    evaluation.fixed_p99_wall_us = paired.baseline_wall_us.p99;
    evaluation.calibrated_p99_wall_us = paired.accelerated_wall_us.p99;
    evaluation.calibrated_speedup = paired.median_speedup;
    evaluation.paired_median_speedup = paired.paired_median_speedup;
    evaluation.paired_win_rate = paired.paired_win_rate;
    evaluation.paired_speedup_ci95_lower = paired.paired_speedup_ci95_lower;
    evaluation.paired_speedup_ci95_upper = paired.paired_speedup_ci95_upper;
    evaluation.bootstrap_samples = paired.bootstrap_samples;
    evaluation.maximum_mixed_qoi_error = paired.maximum_mixed_qoi_error;
    evaluation.fixed_failures = paired.baseline_failures;
    evaluation.calibrated_failures = paired.accelerated_failures;
    evaluation.gate_mismatches = paired.gate_mismatches;
    evaluation.p99_not_regressed = paired.p99_not_regressed;
    evaluation.same_accuracy = paired.same_accuracy;
    evaluation.fixed_dangerous_misroutes =
        fixed.failures + fixed.erroneous_accepts + evaluation.fixed_failures;
    evaluation.calibrated_dangerous_misroutes = calibrated.failures +
        calibrated.erroneous_accepts + evaluation.calibrated_failures +
        evaluation.gate_mismatches;
    evaluation.improved = evaluation.paired_speedup_ci95_lower >= minimum_speedup &&
        evaluation.same_accuracy;
    evaluation.safe = evaluation.calibrated_dangerous_misroutes <=
            evaluation.fixed_dangerous_misroutes &&
        evaluation.calibrated_failures == 0 && evaluation.gate_mismatches == 0 &&
        evaluation.same_accuracy;
    evaluation.seal();
    return evaluation;
}

FamilyRouterEvaluation read_family_router_evaluation(
    const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read family Router evaluation");
    std::string line;
    if (!std::getline(input, line) ||
        (line != "SMAVE_FAMILY_ROUTER_EVALUATION 2" &&
         line != "SMAVE_FAMILY_ROUTER_EVALUATION 3")) {
        throw std::invalid_argument("unsupported family Router evaluation");
    }
    FamilyRouterEvaluation evaluation;
    evaluation.schema_version = line.ends_with(" 3") ? 3 : 2;
    bool ended = false;
    while (std::getline(input, line)) {
        if (line == "END") { ended = true; break; }
        if (line.starts_with("source_block_fingerprint=")) {
            evaluation.source_block_fingerprint = value(line, "source_block_fingerprint=");
        } else if (line.starts_with("heldout_block_fingerprint=")) {
            evaluation.heldout_block_fingerprint = value(line, "heldout_block_fingerprint=");
        } else if (line.starts_with("source_competition_hash=")) {
            evaluation.source_competition_hash = value(line, "source_competition_hash=");
        } else if (line.starts_with("heldout_competition_hash=")) {
            evaluation.heldout_competition_hash = value(line, "heldout_competition_hash=");
        } else if (line.starts_with("source_dataset_id=")) {
            evaluation.source_dataset_id = value(line, "source_dataset_id=");
        } else if (line.starts_with("source_dataset_version=")) {
            evaluation.source_dataset_version = value(line, "source_dataset_version=");
        } else if (line.starts_with("source_dataset_manifest_hash=")) {
            evaluation.source_dataset_manifest_hash = value(
                line, "source_dataset_manifest_hash=");
        } else if (line.starts_with("heldout_dataset_id=")) {
            evaluation.heldout_dataset_id = value(line, "heldout_dataset_id=");
        } else if (line.starts_with("heldout_dataset_version=")) {
            evaluation.heldout_dataset_version = value(line, "heldout_dataset_version=");
        } else if (line.starts_with("heldout_dataset_manifest_hash=")) {
            evaluation.heldout_dataset_manifest_hash = value(
                line, "heldout_dataset_manifest_hash=");
        } else if (line.starts_with("embedding_similarity=")) {
            evaluation.embedding_similarity = real_value(line, "embedding_similarity=");
        } else if (line.starts_with("minimum_similarity=")) {
            evaluation.minimum_similarity = real_value(line, "minimum_similarity=");
        } else if (line.starts_with("minimum_speedup=")) {
            evaluation.minimum_speedup = real_value(line, "minimum_speedup=");
        } else if (line.starts_with("repetitions=")) {
            evaluation.repetitions = size_value(line, "repetitions=");
        } else if (line.starts_with("fixed_expert=")) {
            evaluation.fixed_expert = value(line, "fixed_expert=");
        } else if (line.starts_with("calibrated_expert=")) {
            evaluation.calibrated_expert = value(line, "calibrated_expert=");
        } else if (line.starts_with("oracle_expert=")) {
            evaluation.oracle_expert = value(line, "oracle_expert=");
        } else if (line.starts_with("fixed_median_wall_us=")) {
            evaluation.fixed_median_wall_us = real_value(line, "fixed_median_wall_us=");
        } else if (line.starts_with("calibrated_median_wall_us=")) {
            evaluation.calibrated_median_wall_us = real_value(line, "calibrated_median_wall_us=");
        } else if (line.starts_with("oracle_median_wall_us=")) {
            evaluation.oracle_median_wall_us = real_value(line, "oracle_median_wall_us=");
        } else if (line.starts_with("calibrated_speedup=")) {
            evaluation.calibrated_speedup = real_value(line, "calibrated_speedup=");
        } else if (line.starts_with("scenarios=")) {
            evaluation.scenarios = size_value(line, "scenarios=");
        } else if (line.starts_with("paired_samples=")) {
            evaluation.paired_samples = size_value(line, "paired_samples=");
        } else if (line.starts_with("fixed_p99_wall_us=")) {
            evaluation.fixed_p99_wall_us = real_value(line, "fixed_p99_wall_us=");
        } else if (line.starts_with("calibrated_p99_wall_us=")) {
            evaluation.calibrated_p99_wall_us = real_value(line, "calibrated_p99_wall_us=");
        } else if (line.starts_with("paired_median_speedup=")) {
            evaluation.paired_median_speedup = real_value(line, "paired_median_speedup=");
        } else if (line.starts_with("paired_win_rate=")) {
            evaluation.paired_win_rate = real_value(line, "paired_win_rate=");
        } else if (line.starts_with("paired_speedup_ci95_lower=")) {
            evaluation.paired_speedup_ci95_lower = real_value(
                line, "paired_speedup_ci95_lower=");
        } else if (line.starts_with("paired_speedup_ci95_upper=")) {
            evaluation.paired_speedup_ci95_upper = real_value(
                line, "paired_speedup_ci95_upper=");
        } else if (line.starts_with("bootstrap_samples=")) {
            evaluation.bootstrap_samples = size_value(line, "bootstrap_samples=");
        } else if (line.starts_with("maximum_mixed_qoi_error=")) {
            evaluation.maximum_mixed_qoi_error = real_value(
                line, "maximum_mixed_qoi_error=");
        } else if (line.starts_with("fixed_failures=")) {
            evaluation.fixed_failures = size_value(line, "fixed_failures=");
        } else if (line.starts_with("calibrated_failures=")) {
            evaluation.calibrated_failures = size_value(line, "calibrated_failures=");
        } else if (line.starts_with("gate_mismatches=")) {
            evaluation.gate_mismatches = size_value(line, "gate_mismatches=");
        } else if (line.starts_with("p99_not_regressed=")) {
            evaluation.p99_not_regressed = bool_value(line, "p99_not_regressed=");
        } else if (line.starts_with("same_accuracy=")) {
            evaluation.same_accuracy = bool_value(line, "same_accuracy=");
        } else if (line.starts_with("fixed_dangerous_misroutes=")) {
            evaluation.fixed_dangerous_misroutes = size_value(
                line, "fixed_dangerous_misroutes=");
        } else if (line.starts_with("calibrated_dangerous_misroutes=")) {
            evaluation.calibrated_dangerous_misroutes = size_value(
                line, "calibrated_dangerous_misroutes=");
        } else if (line.starts_with("distinct_instance=")) {
            evaluation.distinct_instance = bool_value(line, "distinct_instance=");
        } else if (line.starts_with("improved=")) {
            evaluation.improved = bool_value(line, "improved=");
        } else if (line.starts_with("safe=")) {
            evaluation.safe = bool_value(line, "safe=");
        } else if (line.starts_with("report_hash=")) {
            evaluation.report_hash = value(line, "report_hash=");
        } else {
            throw std::invalid_argument("unknown family Router evaluation field");
        }
    }
    if (!ended) throw std::invalid_argument("truncated family Router evaluation");
    evaluation.validate();
    return evaluation;
}

void apply_family_router_evaluation(
    const FamilyRouterEvaluation& evaluation,
    RoutingConfig& routing) {
    evaluation.validate();
    if (!evaluation.improved || !evaluation.safe ||
        evaluation.calibrated_dangerous_misroutes != 0) {
        throw std::invalid_argument("family Router evaluation did not pass release gates");
    }
    routing.calibration_block_fingerprint = evaluation.heldout_block_fingerprint;
    routing.calibration_winner = evaluation.calibrated_expert;
    routing.expert_allowlist.clear();
    routing.expert_allowlist.insert(evaluation.calibrated_expert);
}

void write_family_router_evaluation(
    const FamilyRouterEvaluation& evaluation,
    const std::filesystem::path& path) {
    evaluation.validate();
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write family Router evaluation");
    output << std::setprecision(17)
           << "SMAVE_FAMILY_ROUTER_EVALUATION " << evaluation.schema_version << '\n';
    if (evaluation.schema_version >= 3) {
        output << "source_dataset_id=" << evaluation.source_dataset_id << '\n'
               << "source_dataset_version=" << evaluation.source_dataset_version << '\n'
               << "source_dataset_manifest_hash="
               << evaluation.source_dataset_manifest_hash << '\n'
               << "heldout_dataset_id=" << evaluation.heldout_dataset_id << '\n'
               << "heldout_dataset_version=" << evaluation.heldout_dataset_version << '\n'
               << "heldout_dataset_manifest_hash="
               << evaluation.heldout_dataset_manifest_hash << '\n';
    }
    output << "source_block_fingerprint=" << evaluation.source_block_fingerprint << '\n'
           << "heldout_block_fingerprint=" << evaluation.heldout_block_fingerprint << '\n'
           << "source_competition_hash=" << evaluation.source_competition_hash << '\n'
           << "heldout_competition_hash=" << evaluation.heldout_competition_hash << '\n'
           << "embedding_similarity=" << evaluation.embedding_similarity << '\n'
           << "minimum_similarity=" << evaluation.minimum_similarity << '\n'
           << "minimum_speedup=" << evaluation.minimum_speedup << '\n'
           << "repetitions=" << evaluation.repetitions << '\n'
           << "fixed_expert=" << evaluation.fixed_expert << '\n'
           << "calibrated_expert=" << evaluation.calibrated_expert << '\n'
           << "oracle_expert=" << evaluation.oracle_expert << '\n'
           << "fixed_median_wall_us=" << evaluation.fixed_median_wall_us << '\n'
           << "calibrated_median_wall_us=" << evaluation.calibrated_median_wall_us << '\n'
           << "oracle_median_wall_us=" << evaluation.oracle_median_wall_us << '\n'
           << "calibrated_speedup=" << evaluation.calibrated_speedup << '\n'
           << "scenarios=" << evaluation.scenarios << '\n'
           << "paired_samples=" << evaluation.paired_samples << '\n'
           << "fixed_p99_wall_us=" << evaluation.fixed_p99_wall_us << '\n'
           << "calibrated_p99_wall_us=" << evaluation.calibrated_p99_wall_us << '\n'
           << "paired_median_speedup=" << evaluation.paired_median_speedup << '\n'
           << "paired_win_rate=" << evaluation.paired_win_rate << '\n'
           << "paired_speedup_ci95_lower="
           << evaluation.paired_speedup_ci95_lower << '\n'
           << "paired_speedup_ci95_upper="
           << evaluation.paired_speedup_ci95_upper << '\n'
           << "bootstrap_samples=" << evaluation.bootstrap_samples << '\n'
           << "maximum_mixed_qoi_error=" << evaluation.maximum_mixed_qoi_error << '\n'
           << "fixed_failures=" << evaluation.fixed_failures << '\n'
           << "calibrated_failures=" << evaluation.calibrated_failures << '\n'
           << "gate_mismatches=" << evaluation.gate_mismatches << '\n'
           << "p99_not_regressed=" << evaluation.p99_not_regressed << '\n'
           << "same_accuracy=" << evaluation.same_accuracy << '\n'
           << "fixed_dangerous_misroutes=" << evaluation.fixed_dangerous_misroutes << '\n'
           << "calibrated_dangerous_misroutes="
           << evaluation.calibrated_dangerous_misroutes << '\n'
           << "distinct_instance=" << evaluation.distinct_instance << '\n'
           << "improved=" << evaluation.improved << '\n'
           << "safe=" << evaluation.safe << '\n'
           << "report_hash=" << evaluation.report_hash << '\n'
           << "END\n";
}

}  // namespace smave
