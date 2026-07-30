#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace smave {

inline constexpr const char* kFmiBlackboxSchemaVersion = "SMAVE_FMI_BLACKBOX_4";
inline constexpr const char* kPreviousFmiBlackboxSchemaVersion = "SMAVE_FMI_BLACKBOX_3";
inline constexpr const char* kOlderFmiBlackboxSchemaVersion = "SMAVE_FMI_BLACKBOX_2";
inline constexpr const char* kLegacyFmiBlackboxSchemaVersion = "SMAVE_FMI_BLACKBOX_1";

struct FmiInterfaceIR {
    std::string kind;
    std::string model_identifier;
    std::map<std::string, std::string> capabilities;
};

struct FmiDimensionIR {
    std::optional<std::size_t> fixed_extent;
    std::optional<std::uint64_t> extent_value_reference;
};

struct FmiVariableIR {
    std::string name;
    std::string type;
    std::uint64_t value_reference{0};
    std::string causality;
    std::string variability;
    std::string initial;
    std::string unit;
    std::string start;
    std::size_t dimensions{0};
    std::vector<FmiDimensionIR> dimension_descriptors;
    std::size_t derivative_of{0};
    std::optional<std::uint32_t> clock_priority;
};

struct FmiBlackboxIR {
    std::string schema_version{kFmiBlackboxSchemaVersion};
    std::string fmi_version;
    std::string model_name;
    std::string instantiation_token;
    std::string source_hash;
    std::string generation_tool;
    std::string generation_date_time;
    std::string variable_naming_convention;
    std::string host_platform;
    bool host_binary_candidate_available{false};
    std::size_t number_of_event_indicators{0};
    std::map<std::string, std::string> default_experiment;
    std::vector<FmiInterfaceIR> interfaces;
    std::vector<FmiVariableIR> variables;
    std::vector<std::size_t> derivative_variable_order;
    std::vector<std::string> archive_entries;
    std::vector<std::string> binary_platforms;
    std::vector<std::string> warnings;
    bool trajectory_proxy_allowed{true};
    bool differential_test_allowed{true};
    bool equation_level_validation_allowed{false};
    bool direct_expert_allowed{false};

    void validate() const;
    void write(const std::filesystem::path& path) const;
    static FmiBlackboxIR read(const std::filesystem::path& path);
};

struct FmiSmokeSample {
    double time{0.0};
    std::map<std::string, double> outputs;
    std::map<std::string, std::vector<double>> array_outputs;
    std::map<std::string, std::string> string_outputs;
    std::map<std::string, std::vector<std::string>> string_array_outputs;
    std::map<std::string, std::vector<std::uint8_t>> binary_outputs;
    std::map<std::string, std::vector<std::vector<std::uint8_t>>> binary_array_outputs;
};

struct FmiPartitionActivation {
    double time{0.0};
    std::string clock_name;
    std::uint64_t clock_value_reference{0};
    std::uint32_t clock_priority{0};
};

struct FmiSmokeResult {
    bool success{false};
    std::string model_name;
    std::string source_hash;
    std::string interface_kind;
    std::string model_identifier;
    double start_time{0.0};
    double end_time{0.0};
    double step_size{0.0};
    std::vector<FmiSmokeSample> samples;
    bool state_roundtrip_attempted{false};
    bool state_roundtrip_passed{false};
    double maximum_state_replay_error{0.0};
    bool state_serialization_attempted{false};
    bool state_serialization_passed{false};
    std::size_t serialized_state_bytes{0};
    std::size_t do_step_calls{0};
    std::size_t pending_steps{0};
    std::size_t step_finished_callbacks{0};
    std::size_t cross_thread_callbacks{0};
    std::size_t cancelled_steps{0};
    std::size_t asynchronous_timeout_ms{0};
    std::size_t early_returns{0};
    std::size_t discard_recoveries{0};
    std::size_t time_event_splits{0};
    std::size_t time_events{0};
    std::size_t model_exchange_roots{0};
    std::size_t model_exchange_grazing_roots{0};
    std::size_t continuous_state_nominal_updates{0};
    double minimum_continuous_state_nominal{0.0};
    double maximum_continuous_state_nominal{0.0};
    std::size_t event_mode_entries{0};
    std::size_t discrete_update_iterations{0};
    std::size_t model_partition_activations{0};
    std::vector<FmiPartitionActivation> partition_activation_order;
    std::size_t clock_update_callbacks{0};
    std::size_t lock_preemption_callbacks{0};
    std::size_t unlock_preemption_callbacks{0};
    std::map<std::string, double> clock_intervals;
    std::map<std::string, double> clock_shifts;
    std::map<std::string, std::uint32_t> clock_priorities;
    std::map<std::string, std::string> clock_interval_qualifiers;
    std::size_t warnings{0};
    std::string message;
};

[[nodiscard]] FmiBlackboxIR import_fmu(const std::filesystem::path& path);

[[nodiscard]] FmiSmokeResult smoke_fmi3_co_simulation(
    const std::filesystem::path& path,
    double end_time,
    double step_size,
    const std::map<std::string, double>& inputs = {},
    bool allow_native_execution = false);

[[nodiscard]] FmiSmokeResult smoke_fmi3_scheduled_execution(
    const std::filesystem::path& path,
    double end_time,
    double interval,
    const std::map<std::string, double>& inputs = {},
    bool allow_native_execution = false);

[[nodiscard]] FmiSmokeResult smoke_fmi2_co_simulation(
    const std::filesystem::path& path,
    double end_time,
    double step_size,
    const std::map<std::string, double>& inputs = {},
    bool allow_native_execution = false,
    std::size_t asynchronous_timeout_ms = 100);

[[nodiscard]] FmiSmokeResult smoke_fmi3_model_exchange(
    const std::filesystem::path& path,
    double end_time,
    double step_size,
    const std::map<std::string, double>& inputs = {},
    bool allow_native_execution = false,
    const std::map<std::string, std::string>& string_inputs = {},
    const std::map<std::string, std::vector<std::uint8_t>>& binary_inputs = {},
    const std::map<std::string, std::vector<double>>& array_inputs = {},
    const std::map<std::string, std::vector<std::string>>& string_array_inputs = {},
    const std::map<std::string, std::vector<std::vector<std::uint8_t>>>&
        binary_array_inputs = {});

[[nodiscard]] FmiSmokeResult smoke_fmi2_model_exchange(
    const std::filesystem::path& path,
    double end_time,
    double step_size,
    const std::map<std::string, double>& inputs = {},
    bool allow_native_execution = false,
    const std::map<std::string, std::string>& string_inputs = {});

void write_fmi_import_report(
    const FmiBlackboxIR& model,
    const std::filesystem::path& path);

void write_fmi_smoke_report(
    const FmiBlackboxIR& model,
    const FmiSmokeResult& result,
    const std::filesystem::path& path);

}  // namespace smave
