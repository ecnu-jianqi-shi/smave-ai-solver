#pragma once

#include "smave/expert.hpp"
#include "smave/runtime.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace smave {

struct CompetitionEntry {
    std::string expert_version;
    std::size_t attempts{};
    std::size_t passes{};
    std::size_t fallbacks{};
    std::size_t failures{};
    std::size_t erroneous_accepts{};
    double empirical_pass_rate{};
    double predicted_pass_rate{};
    double calibration_error{};
    double median_wall_us{};
    double p90_wall_us{};
    double p99_wall_us{};
    double median_iterations{};
};

struct CompetitionReport {
    int schema_version{3};
    std::string dataset_id;
    std::string dataset_version;
    std::string dataset_manifest_hash;
    std::string block_fingerprint;
    std::vector<CompetitionEntry> entries;
    std::string winner;
    std::string report_hash;

    void seal();
    void validate() const;
};

[[nodiscard]] CompetitionReport compete_experts(
    const ModelIR& model,
    const Registry& registry,
    const RuntimeBundle& bundle,
    const std::filesystem::path& scenario_directory,
    const std::filesystem::path& trace_directory,
    Tolerance tolerance = {},
    std::size_t repetitions = 1);

[[nodiscard]] CompetitionReport read_competition_report(
    const std::filesystem::path& path);

void apply_competition_profile(
    const CompetitionReport& report,
    RoutingConfig& routing);

void write_competition_report(
    const CompetitionReport& report,
    const std::filesystem::path& path);

}  // namespace smave
