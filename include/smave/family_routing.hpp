#pragma once

#include "smave/benchmark.hpp"
#include "smave/competition.hpp"
#include "smave/data_registry.hpp"
#include "smave/embedding.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace smave {

struct FamilyRouterEvaluation {
    int schema_version{2};
    std::string source_dataset_id;
    std::string source_dataset_version;
    std::string source_dataset_manifest_hash;
    std::string heldout_dataset_id;
    std::string heldout_dataset_version;
    std::string heldout_dataset_manifest_hash;
    std::string source_block_fingerprint;
    std::string heldout_block_fingerprint;
    std::string source_competition_hash;
    std::string heldout_competition_hash;
    double embedding_similarity{};
    double minimum_similarity{};
    double minimum_speedup{};
    std::size_t repetitions{};
    std::string fixed_expert;
    std::string calibrated_expert;
    std::string oracle_expert;
    double fixed_median_wall_us{};
    double calibrated_median_wall_us{};
    double oracle_median_wall_us{};
    double calibrated_speedup{};
    std::size_t scenarios{};
    std::size_t paired_samples{};
    double fixed_p99_wall_us{};
    double calibrated_p99_wall_us{};
    double paired_median_speedup{};
    double paired_win_rate{};
    double paired_speedup_ci95_lower{};
    double paired_speedup_ci95_upper{};
    std::size_t bootstrap_samples{};
    double maximum_mixed_qoi_error{};
    std::size_t fixed_failures{};
    std::size_t calibrated_failures{};
    std::size_t gate_mismatches{};
    bool p99_not_regressed{};
    bool same_accuracy{};
    std::size_t fixed_dangerous_misroutes{};
    std::size_t calibrated_dangerous_misroutes{};
    bool distinct_instance{};
    bool improved{};
    bool safe{};
    std::string report_hash;

    void seal();
    void validate() const;
};

[[nodiscard]] FamilyRouterEvaluation evaluate_family_router(
    const ModelIR& source_model,
    const CompetitionReport& source_report,
    const ModelIR& heldout_model,
    const Registry& heldout_registry,
    const RuntimeBundle& heldout_bundle,
    const std::filesystem::path& heldout_scenarios,
    const std::filesystem::path& trace_directory,
    double minimum_similarity = 0.95,
    double minimum_speedup = 1.01,
    std::size_t repetitions = 20,
    Tolerance tolerance = {},
    std::optional<DatasetManifest> heldout_dataset = std::nullopt);

[[nodiscard]] FamilyRouterEvaluation read_family_router_evaluation(
    const std::filesystem::path& path);

void apply_family_router_evaluation(
    const FamilyRouterEvaluation& evaluation,
    RoutingConfig& routing);

void write_family_router_evaluation(
    const FamilyRouterEvaluation& evaluation,
    const std::filesystem::path& path);

}  // namespace smave
