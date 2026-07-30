#pragma once

#include "smave/expert.hpp"
#include "smave/linear.hpp"
#include "smave/runtime.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace smave {

struct BatchKey {
    std::string expert_version;
    std::string block_fingerprint;
    std::size_t shape{};
    std::string dtype{"fp64"};
    std::string tolerance_class{"default"};
    std::string mode{"continuous"};

    [[nodiscard]] std::string value() const;
};

struct BatchMetrics {
    std::string device{"cpu"};
    std::string device_backend;
    std::string device_name;
    std::size_t requests{};
    std::size_t batches{};
    std::size_t maximum_batch{};
    double average_batch{};
    double utilization{};
    double kernel_us{};
    double gate_us{};
    double fallback_us{};
    double total_us{};
    double sequential_baseline_us{};
    double throughput_speedup{};
    std::size_t accepted{};
    std::size_t fallback_count{};
    std::size_t baseline_failures{};
    std::size_t device_batches{};
    std::size_t device_rejections{};
    std::size_t cpu_refinement_steps{};
    double device_upload_us{};
    double device_kernel_us{};
    double device_download_us{};
    double device_maximum_absolute_error{};
    double device_maximum_relative_error{};
};

struct BatchSolveResult {
    std::vector<SolveOutcome> outcomes;
    BatchMetrics metrics;
};

class TensorBucketScheduler {
public:
    explicit TensorBucketScheduler(
        std::size_t maximum_batch = 32,
        std::string device = "cpu",
        bool cpu_batch_fallback = true);

    [[nodiscard]] BatchSolveResult solve_linear_batch(
        const ModelIR& model,
        const BlockIR& block,
        const Expert& preconditioner,
        const std::vector<std::unordered_map<std::string, double>>& scenarios,
        const Runtime& fallback_runtime,
        const std::filesystem::path& trace_directory) const;

private:
    std::size_t maximum_batch_;
    std::string device_;
    bool cpu_batch_fallback_;
};

}  // namespace smave
