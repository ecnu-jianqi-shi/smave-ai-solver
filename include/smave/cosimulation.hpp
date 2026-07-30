#pragma once

#include "smave/continuous.hpp"
#include "smave/hybrid.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace smave {

struct CoupledSampleRecord {
    std::size_t tick{0};
    double time{0.0};
    std::string pre_mode;
    std::string post_mode;
    std::unordered_map<std::string, double> continuous_state;
    std::unordered_map<std::string, double> pre_discrete_state;
    std::unordered_map<std::string, double> post_discrete_state;
};

struct CoupledMicrostepRecord {
    std::size_t tick{0};
    double time{0.0};
    std::size_t ordinal{0};
    std::string domain;
    std::string event_id;
};

struct CoupledRunResult {
    bool success{false};
    double final_time{0.0};
    std::string final_mode;
    std::unordered_map<std::string, double> final_continuous_state;
    std::unordered_map<std::string, double> final_discrete_state;
    std::vector<ContinuousEventRecord> continuous_events;
    std::vector<HybridEventRecord> sampled_events;
    std::vector<CoupledSampleRecord> samples;
    std::vector<CoupledMicrostepRecord> superdense_steps;
    std::size_t accepted_steps{0};
    std::size_t rejected_steps{0};
    std::size_t rejected_candidates{0};
    std::size_t superdense_microsteps{0};
    std::size_t maximum_superdense_iterations{0};
    double maximum_scaled_local_error{0.0};
    double maximum_guard_residual{0.0};
    double maximum_reset_error{0.0};
    std::string message;
};

[[nodiscard]] CoupledRunResult simulate_coupled(
    const ContinuousHybridIR& continuous,
    const HybridProgramIR& sampled,
    double end_time,
    double maximum_step,
    ContinuousTolerance tolerance = {},
    const std::vector<EventCandidate>& candidates = {});

void write_coupled_report(
    const ContinuousHybridIR& continuous,
    const HybridProgramIR& sampled,
    const CoupledRunResult& result,
    const std::filesystem::path& path);

}  // namespace smave
