#pragma once

#include "smave/runtime.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace smave {

struct DistributionSummary {
    double minimum{};
    double mean{};
    double median{};
    double p90{};
    double p99{};
    double maximum{};
};

struct PerformanceReport {
    int schema_version{1};
    std::string dataset_id;
    std::string dataset_version;
    std::string dataset_manifest_hash;
    std::size_t scenarios{};
    std::size_t repetitions{};
    std::size_t samples{};
    DistributionSummary baseline_wall_us;
    DistributionSummary accelerated_wall_us;
    DistributionSummary baseline_iterations;
    DistributionSummary accelerated_iterations;
    double median_speedup{};
    double p99_speedup{};
    double paired_median_speedup{};
    double paired_p01_speedup{};
    double paired_win_rate{};
    double paired_speedup_ci95_lower{};
    double paired_speedup_ci95_upper{};
    std::size_t bootstrap_samples{};
    double maximum_mixed_qoi_error{};
    std::size_t baseline_failures{};
    std::size_t accelerated_failures{};
    std::size_t gate_mismatches{};
    bool same_accuracy{false};
    bool p99_not_regressed{false};
    std::string report_hash;

    void seal();
    void validate() const;
};

[[nodiscard]] PerformanceReport benchmark_runtimes(
    const Runtime& baseline,
    const Runtime& accelerated,
    const std::filesystem::path& scenario_directory,
    const std::filesystem::path& trace_directory,
    std::size_t repetitions = 20,
    std::size_t warmup_repetitions = 3,
    std::size_t bootstrap_samples = 2000);

void write_performance_report(
    const PerformanceReport& report,
    const std::filesystem::path& path);

[[nodiscard]] PerformanceReport read_performance_report(
    const std::filesystem::path& path);

}  // namespace smave
