#pragma once

#include "smave/runtime.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace smave {

struct ValidationReport {
    int schema_version{2};
    std::string dataset_id;
    std::string dataset_version;
    std::string dataset_manifest_hash;
    std::size_t scenarios{};
    std::size_t successful_scenarios{};
    std::size_t admitted_invocations{};
    std::size_t top_k_passes{};
    std::size_t full_fallbacks{};
    std::size_t original_solver_failures{};
    std::size_t erroneous_accepts{};
    std::size_t safety_evaluations{};
    double top_k_pass_rate{};
    double fallback_rate{};
    double safety_confidence_level{0.95};
    double erroneous_accept_rate_upper_bound{1.0};
    double maximum_erroneous_accept_rate{0.05};
    bool top_k_target_met{false};
    bool safety_target_met{false};
    bool confidence_target_met{false};
    std::vector<std::string> failed_scenarios;
};

[[nodiscard]] double binomial_proportion_upper_bound(
    std::size_t events,
    std::size_t trials,
    double confidence_level = 0.95);

[[nodiscard]] ValidationReport validate_scenarios(
    const Runtime& runtime,
    const std::filesystem::path& scenario_directory,
    const std::filesystem::path& trace_directory);
void write_validation_report(
    const ValidationReport& report,
    const std::filesystem::path& path);

}  // namespace smave
