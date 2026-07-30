#pragma once

#include "smave/expression.hpp"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace smave {

inline constexpr const char* kHybridSchemaVersion = "SMAVE_HYBRID_2";

struct HybridModeIR {
    std::string id;
    std::map<std::string, std::string> updates;
};

struct HybridResetIR {
    std::string variable;
    std::string expression;
};

struct HybridTransitionIR {
    std::string id;
    std::string source_mode;
    std::string target_mode;
    int priority{0};
    std::string guard_expression;
    std::vector<HybridResetIR> resets;
    std::size_t source_order{0};
};

struct HybridProgramIR {
    std::string schema_version{kHybridSchemaVersion};
    std::string model_id;
    double sample_time{0.0};
    double sample_offset{0.0};
    std::string initial_mode;
    std::map<std::string, double> initial_state;
    std::vector<HybridModeIR> modes;
    std::vector<HybridTransitionIR> transitions;

    void validate() const;
    static HybridProgramIR read(const std::filesystem::path& path);
};

struct EventCandidate {
    std::size_t tick{0};
    std::string transition_id;
    std::string source_mode;
};

struct HybridEventRecord {
    std::size_t tick{0};
    double time{0.0};
    std::string transition_id;
    std::string source_mode;
    std::string target_mode;
    bool candidate_present{false};
    bool candidate_accepted{false};
};

struct HybridRunResult {
    bool success{false};
    std::string final_mode;
    std::unordered_map<std::string, double> final_state;
    std::vector<HybridEventRecord> events;
    std::size_t candidate_count{0};
    std::size_t accepted_candidates{0};
    std::size_t rejected_candidates{0};
    std::size_t missed_events{0};
    double event_recall{1.0};
    double maximum_event_time_error{0.0};
    double maximum_reset_error{0.0};
    std::string message;
};

[[nodiscard]] std::vector<EventCandidate> read_event_candidates(
    const std::filesystem::path& path);
[[nodiscard]] HybridRunResult run_hybrid(
    const HybridProgramIR& program, std::size_t ticks,
    const std::vector<EventCandidate>& candidates = {});
void write_hybrid_report(
    const HybridProgramIR& program, const HybridRunResult& result,
    const std::filesystem::path& path);

}  // namespace smave
