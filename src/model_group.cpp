#include "smave/model_group.hpp"

#include "smave/runtime.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace smave {
namespace {

double parse_number(const BlockGraphNode& node, const std::string& key, double fallback = 0.0) {
    const auto iterator = node.attributes.find(key);
    if (iterator == node.attributes.end()) return fallback;
    std::size_t consumed{};
    const double value = std::stod(iterator->second, &consumed);
    if (consumed != iterator->second.size() || !std::isfinite(value)) {
        throw std::invalid_argument(node.id + ": invalid numeric attribute " + key);
    }
    return value;
}

const BlockGraphNode& find_node(const BlockGraphIR& graph, const std::string& id) {
    const auto iterator = std::find_if(
        graph.nodes.begin(), graph.nodes.end(),
        [&](const BlockGraphNode& node) { return node.id == id; });
    if (iterator == graph.nodes.end()) throw std::logic_error("unknown scheduled node: " + id);
    return *iterator;
}

std::unordered_map<std::string, double> node_inputs(
    const BlockGraphIR& graph, const std::string& node_id,
    const std::unordered_map<std::string, double>& signals) {
    std::unordered_map<std::string, double> result;
    for (const auto& connection : graph.connections) {
        if (connection.target_node != node_id) continue;
        const auto key = connection.source_node + "." + connection.source_port;
        const auto iterator = signals.find(key);
        if (iterator == signals.end()) {
            throw std::runtime_error(node_id + ": unavailable source signal " + key);
        }
        result[connection.target_port] = iterator->second;
    }
    return result;
}

void emit_outputs(
    const std::string& node_id, const std::unordered_map<std::string, double>& outputs,
    std::unordered_map<std::string, double>& signals) {
    for (const auto& [port, value] : outputs) signals[node_id + "." + port] = value;
}

}  // namespace

ModelGroupRuntime::ModelGroupRuntime(
    BlockGraphIR graph, std::filesystem::path graph_directory)
    : graph_(std::move(graph)), graph_directory_(std::move(graph_directory)) {
    graph_.validate(graph_directory_);
    for (const auto& node : graph_.nodes) {
        if (node.type == "unit_delay") delay_state_[node.id] = parse_number(node, "initial");
        if (node.sample_offset > 0.0) {
            held_signals_[node.id + ".out"] = node.type == "unit_delay"
                ? delay_state_.at(node.id)
                : parse_number(node, "initial_output");
        }
    }
}

ModelGroupResult ModelGroupRuntime::execute(
    const std::unordered_map<std::string, double>& external_inputs,
    const std::filesystem::path& trace_path) {
    return execute_nodes(graph_.commit_order, external_inputs, trace_path);
}

ModelGroupResult ModelGroupRuntime::execute_nodes(
    const std::vector<std::string>& node_ids,
    const std::unordered_map<std::string, double>& external_inputs,
    const std::filesystem::path& trace_path) {
    ModelGroupResult result;
    result.commit_order = node_ids;
    auto signals = held_signals_;
    for (const auto& [name, value] : external_inputs) signals[name] = value;
    auto pending_delay_state = delay_state_;
    const std::unordered_set<std::string> due(node_ids.begin(), node_ids.end());
    try {
        for (const auto& node_id : node_ids) {
            const auto& node = find_node(graph_, node_id);
            ModelGroupNodeResult node_result;
            node_result.node_id = node.id;
            const auto inputs = node.type == "unit_delay"
                ? std::unordered_map<std::string, double>{}
                : node_inputs(graph_, node.id, signals);
            if (node.type == "constant") {
                node_result.outputs["out"] = parse_number(node, "value");
            } else if (node.type == "gain") {
                node_result.outputs["out"] = inputs.at("in") * parse_number(node, "gain", 1.0);
            } else if (node.type == "sum") {
                double sum{};
                const auto signs = node.attributes.find("signs");
                if (signs == node.attributes.end()) {
                    for (const auto& [port, value] : inputs) {
                        (void)port;
                        sum += value;
                    }
                } else {
                    if (inputs.size() != signs->second.size()) {
                        throw std::runtime_error(node.id + ": signed sum input count mismatch");
                    }
                    for (std::size_t index = 0; index < signs->second.size(); ++index) {
                        const double value = inputs.at("in" + std::to_string(index + 1));
                        sum += signs->second[index] == '+' ? value : -value;
                    }
                }
                node_result.outputs["out"] = sum;
            } else if (node.type == "switch") {
                const double control = inputs.at("control");
                const auto criterion = node.attributes.find("criterion");
                bool select_true{};
                if (criterion == node.attributes.end()) {
                    select_true = control > 0.0;
                } else if (criterion->second == "gt") {
                    select_true = control > parse_number(node, "threshold");
                } else if (criterion->second == "ge") {
                    select_true = control >= parse_number(node, "threshold");
                } else if (criterion->second == "ne_zero") {
                    select_true = control != 0.0;
                } else {
                    throw std::runtime_error(node.id + ": invalid switch criterion");
                }
                node_result.outputs["out"] = inputs.at(select_true ? "true" : "false");
            } else if (node.type == "unit_delay") {
                node_result.outputs["out"] = delay_state_.at(node.id);
            } else if (node.type == "algebraic_model") {
                const auto model = ModelIR::read(graph_directory_ / node.attributes.at("ir"));
                const auto outcome = Runtime(model).solve(inputs, trace_path.empty()
                    ? std::filesystem::path(".smave/model-group-traces")
                    : trace_path.parent_path() / (node.id + "-runtime"));
                if (!outcome.success) {
                    throw std::runtime_error(node.id + ": Runtime failed: " + outcome.message);
                }
                node_result.used_local_fallback = outcome.fallback_count > 0;
                result.local_fallback_count += outcome.fallback_count;
                for (const auto& block : model.blocks) {
                    for (const auto& unknown : block.unknowns) {
                        node_result.outputs[unknown] = outcome.values.at(unknown);
                    }
                }
            }
            node_result.success = true;
            emit_outputs(node.id, node_result.outputs, signals);
            result.nodes.push_back(std::move(node_result));
        }
        for (const auto& connection : graph_.connections) {
            const auto& target = find_node(graph_, connection.target_node);
            if (target.type != "unit_delay" || connection.target_port != "in" ||
                !due.contains(target.id)) continue;
            pending_delay_state[target.id] =
                signals.at(connection.source_node + "." + connection.source_port);
        }
        delay_state_ = std::move(pending_delay_state);
        held_signals_ = signals;
        result.outputs = signals;
        for (const auto& connection : graph_.connections) {
            const auto source = signals.at(connection.source_node + "." + connection.source_port);
            const auto target_key = connection.target_node + ".input." + connection.target_port;
            result.outputs[target_key] = source;
            result.maximum_connection_error = std::max(
                result.maximum_connection_error, std::abs(result.outputs.at(target_key) - source));
        }
        result.success = true;
        result.message = "all nodes passed; group state committed atomically";
    } catch (const std::exception& error) {
        result.message = error.what();
    }
    if (!trace_path.empty()) write_model_group_report(graph_, result, trace_path);
    return result;
}

MultirateModelGroupResult ModelGroupRuntime::execute_multirate(
    double end_time,
    double base_step,
    const std::unordered_map<std::string, double>& external_inputs,
    const std::filesystem::path& trace_directory) {
    if (!(end_time >= 0.0) || !std::isfinite(end_time) ||
        !(base_step > 0.0) || !std::isfinite(base_step)) {
        throw std::invalid_argument("multirate end time/base step is invalid");
    }
    const double tick_ratio = end_time / base_step;
    const auto final_tick = static_cast<std::size_t>(std::llround(tick_ratio));
    if (std::abs(tick_ratio - static_cast<double>(final_tick)) > 1.0e-9) {
        throw std::invalid_argument("multirate end time must be an integer base-step multiple");
    }
    std::unordered_map<std::string, std::size_t> periods;
    std::unordered_map<std::string, std::size_t> offsets;
    for (const auto& node : graph_.nodes) {
        const double ratio = node.sample_time == 0.0 ? 1.0 : node.sample_time / base_step;
        const auto period = static_cast<std::size_t>(std::llround(ratio));
        if (period == 0 || std::abs(ratio - static_cast<double>(period)) > 1.0e-9) {
            throw std::invalid_argument(
                node.id + ": sample time must be a positive integer base-step multiple");
        }
        const double offset_ratio = node.sample_offset / base_step;
        const auto offset = static_cast<std::size_t>(std::llround(offset_ratio));
        if (std::abs(offset_ratio - static_cast<double>(offset)) > 1.0e-9 ||
            offset >= period) {
            throw std::invalid_argument(
                node.id + ": sample offset must be an integer base-step multiple below its period");
        }
        periods[node.id] = period;
        offsets[node.id] = offset;
    }
    MultirateModelGroupResult result;
    result.base_step = base_step;
    result.end_time = end_time;
    for (std::size_t tick = 0; tick <= final_tick; ++tick) {
        std::vector<std::string> due;
        for (const auto& node_id : graph_.commit_order) {
            const auto offset = offsets.at(node_id);
            if (tick >= offset &&
                (tick - offset) % periods.at(node_id) == 0) {
                due.push_back(node_id);
            }
        }
        const auto trace = trace_directory.empty()
            ? std::filesystem::path{}
            : trace_directory / ("tick-" + std::to_string(tick) + ".trace");
        const auto tick_result = execute_nodes(due, external_inputs, trace);
        if (!tick_result.success) {
            result.message = "tick " + std::to_string(tick) + " failed: " + tick_result.message;
            return result;
        }
        result.local_fallback_count += tick_result.local_fallback_count;
        result.maximum_connection_error = std::max(
            result.maximum_connection_error, tick_result.maximum_connection_error);
        result.ticks.push_back(ModelGroupTickResult{
            tick,
            static_cast<double>(tick) * base_step,
            due,
            tick_result.outputs,
            tick_result.local_fallback_count,
            tick_result.maximum_connection_error});
    }
    result.success = true;
    result.final_outputs = held_signals_;
    result.message =
        "all phased multirate ticks committed atomically with zero-order holds";
    return result;
}

void write_model_group_report(
    const BlockGraphIR& graph, const ModelGroupResult& result,
    const std::filesystem::path& path) {
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write model group report: " + path.string());
    output << "SMAVE_MODEL_GROUP_REPORT 1\n"
           << "MODEL " << std::quoted(graph.model_id) << '\n'
           << "SOURCE_HASH " << std::quoted(graph.source_hash) << '\n'
           << "SUCCESS " << result.success << '\n'
           << "LOCAL_FALLBACKS " << result.local_fallback_count << '\n'
           << "MAX_CONNECTION_ERROR " << std::setprecision(17)
           << result.maximum_connection_error << '\n'
           << "COMMIT_ORDER " << result.commit_order.size();
    for (const auto& id : result.commit_order) output << ' ' << std::quoted(id);
    output << "\nNODES " << result.nodes.size() << '\n';
    for (const auto& node : result.nodes) {
        output << "NODE " << std::quoted(node.node_id) << ' ' << node.success << ' '
               << node.used_local_fallback << ' ' << node.outputs.size();
        for (const auto& [port, value] : node.outputs) {
            output << ' ' << std::quoted(port) << ' ' << std::setprecision(17) << value;
        }
        output << '\n';
    }
    output << "MESSAGE " << std::quoted(result.message) << "\nEND\n";
}

void write_multirate_model_group_report(
    const BlockGraphIR& graph,
    const MultirateModelGroupResult& result,
    const std::filesystem::path& path) {
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write multirate model group report: " + path.string());
    output << "SMAVE_MULTIRATE_MODEL_GROUP_REPORT 1\n"
           << "MODEL " << std::quoted(graph.model_id) << '\n'
           << "SOURCE_HASH " << std::quoted(graph.source_hash) << '\n'
           << "SUCCESS " << result.success << '\n'
           << "BASE_STEP " << std::setprecision(17) << result.base_step << '\n'
           << "END_TIME " << result.end_time << '\n'
           << "LOCAL_FALLBACKS " << result.local_fallback_count << '\n'
           << "MAX_CONNECTION_ERROR " << result.maximum_connection_error << '\n'
           << "TICKS " << result.ticks.size() << '\n';
    for (const auto& tick : result.ticks) {
        output << "TICK " << tick.tick << ' ' << std::setprecision(17) << tick.time << ' '
               << tick.executed_nodes.size();
        for (const auto& node : tick.executed_nodes) output << ' ' << std::quoted(node);
        std::vector<std::pair<std::string, double>> signals(
            tick.held_outputs.begin(), tick.held_outputs.end());
        std::sort(signals.begin(), signals.end());
        output << ' ' << signals.size();
        for (const auto& [name, value] : signals) {
            output << ' ' << std::quoted(name) << ' ' << value;
        }
        output << '\n';
    }
    std::vector<std::pair<std::string, double>> final_outputs(
        result.final_outputs.begin(), result.final_outputs.end());
    std::sort(final_outputs.begin(), final_outputs.end());
    output << "FINAL_OUTPUTS " << final_outputs.size();
    for (const auto& [name, value] : final_outputs) {
        output << ' ' << std::quoted(name) << ' ' << std::setprecision(17) << value;
    }
    output << '\n';
    output << "MESSAGE " << std::quoted(result.message) << "\nEND\n";
}

}  // namespace smave
