#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace smave {

inline constexpr const char* kContinuousHybridSchemaVersion =
    "SMAVE_CONTINUOUS_HYBRID_1";

struct ContinuousStateIR {
    std::string name;
    double start{0.0};
    double nominal{1.0};
    bool has_minimum{false};
    double minimum{0.0};
    bool has_maximum{false};
    double maximum{0.0};
    std::string derivative;
};

struct ContinuousResetIR {
    std::string variable;
    std::string expression;
};

struct ContinuousEventIR {
    std::string id;
    std::string guard;
    int direction{0};
    int priority{0};
    std::size_t source_order{0};
    std::vector<ContinuousResetIR> resets;
};

struct ContinuousHybridIR {
    std::string schema_version{kContinuousHybridSchemaVersion};
    std::string model_id;
    std::string source_hash;
    std::map<std::string, double> parameters;
    std::vector<ContinuousStateIR> states;
    std::vector<ContinuousEventIR> events;

    void validate() const;
    void write(const std::filesystem::path& path) const;
    static ContinuousHybridIR read(const std::filesystem::path& path);
};

[[nodiscard]] ContinuousHybridIR compile_continuous_model(
    const std::filesystem::path& source,
    const std::string& top = {});

struct ContinuousTolerance {
    double absolute{1.0e-9};
    double relative{1.0e-7};
    double root_time{1.0e-9};
    double guard{1.0e-8};
};

struct ContinuousEventRecord {
    std::string id;
    double time{0.0};
    double guard_residual{0.0};
    bool grazing{false};
    std::unordered_map<std::string, double> pre_state;
    std::unordered_map<std::string, double> post_state;
};

struct ContinuousRunResult {
    bool success{false};
    double final_time{0.0};
    std::unordered_map<std::string, double> final_state;
    std::vector<ContinuousEventRecord> events;
    std::size_t accepted_steps{0};
    std::size_t rejected_steps{0};
    double maximum_scaled_local_error{0.0};
    double maximum_guard_residual{0.0};
    double maximum_reset_error{0.0};
    double maximum_event_time_error{0.0};
    std::size_t grazing_events{0};
    bool reference_order_matched{true};
    bool reference_time_matched{true};
    std::string message;
};

struct ContinuousBoundaryEventResult {
    bool success{false};
    std::unordered_map<std::string, double> final_state;
    std::vector<ContinuousEventRecord> events;
    double maximum_guard_residual{0.0};
    double maximum_reset_error{0.0};
    std::string message;
};

[[nodiscard]] ContinuousRunResult simulate_continuous(
    const ContinuousHybridIR& model,
    double start_time,
    double end_time,
    double maximum_step,
    ContinuousTolerance tolerance = {},
    bool process_initial_events = true);

[[nodiscard]] ContinuousBoundaryEventResult process_continuous_parameter_events(
    const ContinuousHybridIR& model,
    const std::map<std::string, double>& previous_parameters,
    double event_time,
    const std::unordered_map<std::string, double>& state,
    const std::unordered_map<std::string, double>& physical_pre_state,
    const std::vector<std::string>& already_processed,
    ContinuousTolerance tolerance = {});

void validate_continuous_reference(
    ContinuousRunResult& result,
    const std::filesystem::path& reference_path);

void write_continuous_report(
    const ContinuousHybridIR& model,
    const ContinuousRunResult& result,
    const std::filesystem::path& path);

}  // namespace smave
