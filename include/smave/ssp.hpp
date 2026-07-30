#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace smave {

struct SspComponentSample {
    double time{0.0};
    std::map<std::string, double> outputs;
};

struct SspComponentResult {
    std::string name;
    std::string source;
    std::string fmi_version;
    std::string model_name;
    std::string source_hash;
    std::size_t event_mode_entries{0};
    std::size_t discrete_update_iterations{0};
    std::size_t time_events{0};
    std::size_t early_returns{0};
    std::size_t rollback_replays{0};
};

struct SspConnectionResult {
    std::string source_component;
    std::string source_connector;
    std::string target_component;
    std::string target_connector;
    std::string source_unit;
    std::string target_unit;
    double unit_factor{1.0};
    double unit_offset{0.0};
    double factor{1.0};
    double offset{0.0};
};

struct SspSimulationResult {
    bool success{false};
    std::string system_name;
    std::string source_hash;
    double end_time{0.0};
    double step_size{0.0};
    std::vector<SspComponentResult> components;
    std::vector<SspConnectionResult> connections;
    std::vector<std::string> step_order;
    std::vector<SspComponentSample> samples;
    std::size_t communication_steps{0};
    std::size_t signal_exchanges{0};
    std::size_t event_mode_entries{0};
    std::size_t discrete_update_iterations{0};
    std::size_t time_event_splits{0};
    std::size_t time_events{0};
    std::size_t early_returns{0};
    std::size_t rollback_replays{0};
    std::string message;
};

[[nodiscard]] SspSimulationResult simulate_ssp(
    const std::filesystem::path& path,
    double end_time,
    double step_size,
    bool allow_native_execution = false);

void write_ssp_report(
    const SspSimulationResult& result,
    const std::filesystem::path& path);

}  // namespace smave
