#pragma once

#include "smave/block_graph.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace smave {

struct ModelGroupNodeResult {
    std::string node_id;
    bool success{false};
    bool used_local_fallback{false};
    std::unordered_map<std::string, double> outputs;
    std::string message;
};

struct ModelGroupResult {
    bool success{false};
    std::vector<ModelGroupNodeResult> nodes;
    std::vector<std::string> commit_order;
    std::unordered_map<std::string, double> outputs;
    double maximum_connection_error{0.0};
    std::size_t local_fallback_count{0};
    std::string message;
};

struct ModelGroupTickResult {
    std::size_t tick{0};
    double time{0.0};
    std::vector<std::string> executed_nodes;
    std::unordered_map<std::string, double> held_outputs;
    std::size_t local_fallback_count{0};
    double maximum_connection_error{0.0};
};

struct MultirateModelGroupResult {
    bool success{false};
    double base_step{0.0};
    double end_time{0.0};
    std::vector<ModelGroupTickResult> ticks;
    std::unordered_map<std::string, double> final_outputs;
    std::size_t local_fallback_count{0};
    double maximum_connection_error{0.0};
    std::string message;
};

class ModelGroupRuntime {
public:
    ModelGroupRuntime(BlockGraphIR graph, std::filesystem::path graph_directory);

    [[nodiscard]] ModelGroupResult execute(
        const std::unordered_map<std::string, double>& external_inputs,
        const std::filesystem::path& trace_path = {});
    [[nodiscard]] MultirateModelGroupResult execute_multirate(
        double end_time,
        double base_step,
        const std::unordered_map<std::string, double>& external_inputs = {},
        const std::filesystem::path& trace_directory = {});

private:
    [[nodiscard]] ModelGroupResult execute_nodes(
        const std::vector<std::string>& node_ids,
        const std::unordered_map<std::string, double>& external_inputs,
        const std::filesystem::path& trace_path);
    BlockGraphIR graph_;
    std::filesystem::path graph_directory_;
    std::unordered_map<std::string, double> delay_state_;
    std::unordered_map<std::string, double> held_signals_;
};

void write_model_group_report(
    const BlockGraphIR& graph, const ModelGroupResult& result,
    const std::filesystem::path& path);
void write_multirate_model_group_report(
    const BlockGraphIR& graph,
    const MultirateModelGroupResult& result,
    const std::filesystem::path& path);

}  // namespace smave
