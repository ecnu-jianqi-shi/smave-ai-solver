#include "smave/tensor.hpp"

#include "smave/device.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace smave {
namespace {

double dot(const std::vector<double>& left, const std::vector<double>& right) {
    double result = 0.0;
    for (std::size_t index = 0; index < left.size(); ++index) result += left[index] * right[index];
    return result;
}

std::vector<double> multiply(
    const std::vector<std::vector<double>>& matrix,
    const std::vector<double>& vector) {
    std::vector<double> result(matrix.size());
    for (std::size_t row = 0; row < matrix.size(); ++row) {
        result[row] = dot(matrix[row], vector);
    }
    return result;
}

bool same_matrix(const LinearSystem& left, const LinearSystem& right) {
    if (left.matrix.size() != right.matrix.size()) return false;
    for (std::size_t row = 0; row < left.matrix.size(); ++row) {
        for (std::size_t column = 0; column < left.matrix.size(); ++column) {
            if (std::abs(left.matrix[row][column] - right.matrix[row][column]) >
                1.0e-12 * (1.0 + std::abs(left.matrix[row][column]))) return false;
        }
    }
    return true;
}

bool has_context_independent_linear_matrix(
    const BlockIR& block,
    const std::unordered_map<std::string, Expression>& residuals) {
    for (std::size_t row = 0; row < block.equation_ids.size(); ++row) {
        std::vector<std::string> local_unknowns;
        local_unknowns.reserve(block.jacobian_sparsity.row(row).size());
        for (const auto column : block.jacobian_sparsity.row(row)) {
            local_unknowns.push_back(block.unknowns[column]);
        }
        if (!residuals.at(block.equation_ids[row])
                 .constant_linear_coefficients(local_unknowns).has_value()) {
            return false;
        }
    }
    return true;
}

}  // namespace

std::string BatchKey::value() const {
    std::ostringstream output;
    output << expert_version << '|' << block_fingerprint << '|' << shape << '|'
           << dtype << '|' << tolerance_class << '|' << mode;
    return output.str();
}

TensorBucketScheduler::TensorBucketScheduler(
    std::size_t maximum_batch,
    std::string device,
    bool cpu_batch_fallback)
    : maximum_batch_(maximum_batch),
      device_(std::move(device)),
      cpu_batch_fallback_(cpu_batch_fallback) {
    if (maximum_batch_ == 0) throw std::invalid_argument("maximum batch must be positive");
    if (device_ != "auto" && device_ != "cpu" && device_ != "metal-gpu" &&
        device_ != "coreml-neural-engine") {
        throw std::invalid_argument(
            "tensor device must be auto, cpu, metal-gpu, or coreml-neural-engine");
    }
}

BatchSolveResult TensorBucketScheduler::solve_linear_batch(
    const ModelIR& model,
    const BlockIR& block,
    const Expert& preconditioner,
    const std::vector<std::unordered_map<std::string, double>>& scenarios,
    const Runtime& fallback_runtime,
    const std::filesystem::path& trace_directory) const {
    if (!block.linear || !preconditioner.match(block).preconditioner) {
        throw std::invalid_argument("tensor batch requires a linear preconditioner expert");
    }
    BatchSolveResult batch_result;
    batch_result.metrics.device = device_;
    batch_result.metrics.requests = scenarios.size();
    batch_result.outcomes.resize(scenarios.size());
    if (scenarios.empty()) return batch_result;
    std::unordered_map<std::string, Expression> residual_expressions;
    for (const auto& equation : model.equations) {
        residual_expressions.emplace(equation.id, Expression(equation.residual));
    }
    const bool shared_matrix = has_context_independent_linear_matrix(
        block, residual_expressions);
    const auto total_started = std::chrono::steady_clock::now();
    for (std::size_t begin = 0; begin < scenarios.size(); begin += maximum_batch_) {
        const std::size_t end = std::min(scenarios.size(), begin + maximum_batch_);
        ++batch_result.metrics.batches;
        batch_result.metrics.maximum_batch = std::max(
            batch_result.metrics.maximum_batch, end - begin);
        std::vector<LinearSystem> systems;
        LinearSystem shared_system;
        std::vector<BlockContext> contexts;
        std::vector<std::vector<double>> residuals;
        systems.reserve(end - begin);
        contexts.reserve(end - begin);
        residuals.reserve(end - begin);
        for (std::size_t index = begin; index < end; ++index) {
            if (shared_matrix) {
                if (index == begin) {
                    shared_system = assemble_linear_system(
                        model, block, residual_expressions, scenarios[index]);
                } else {
                    update_linear_right_hand_side(
                        shared_system, model, block, residual_expressions,
                        scenarios[index]);
                }
            } else {
                systems.push_back(assemble_linear_system(
                    model, block, residual_expressions, scenarios[index]));
                if (!same_matrix(systems.front(), systems.back())) {
                    throw std::invalid_argument("tensor bucket contains different matrices");
                }
            }
            BlockContext context;
            context.values = scenarios[index];
            contexts.push_back(std::move(context));
            residuals.push_back(shared_matrix
                ? shared_system.right_hand_side
                : systems.back().right_hand_side);
        }
        std::vector<std::vector<double>> preconditioned;
        const auto kernel_started = std::chrono::steady_clock::now();
        DeviceExecutionResult execution;
        const auto batch_operations = (end - begin) * block.unknowns.size() *
            block.unknowns.size();
        const std::string selected_device = device_ == "auto" &&
                end - begin >= 64 && batch_operations >= 16U * 1024U * 1024U &&
                preconditioner.device_batch_is_resident(
                    "coreml-neural-engine", end - begin, block.unknowns.size())
            ? "coreml-neural-engine"
            : (device_ == "auto" ? "cpu" : device_);
        bool applied = preconditioner.apply_preconditioner_batch_on_device(
            selected_device, block, contexts, residuals, preconditioned, &execution);
        if (selected_device != "cpu") {
            batch_result.metrics.device_backend = execution.backend;
            batch_result.metrics.device_name = execution.device_name;
            batch_result.metrics.device_upload_us += execution.upload_us;
            batch_result.metrics.device_kernel_us += execution.kernel_us;
            batch_result.metrics.device_download_us += execution.download_us;
            batch_result.metrics.device_maximum_absolute_error = std::max(
                batch_result.metrics.device_maximum_absolute_error,
                execution.maximum_absolute_error);
            batch_result.metrics.device_maximum_relative_error = std::max(
                batch_result.metrics.device_maximum_relative_error,
                execution.maximum_relative_error);
            if (execution.executed && execution.verified) {
                ++batch_result.metrics.device_batches;
            } else {
                ++batch_result.metrics.device_rejections;
                if (cpu_batch_fallback_) {
                    applied = preconditioner.apply_preconditioner_batch_on_device(
                        "cpu", block, contexts, residuals, preconditioned, nullptr);
                }
            }
        }
        batch_result.metrics.kernel_us += std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - kernel_started).count();
        for (std::size_t local = 0; local < end - begin; ++local) {
            const std::size_t global = begin + local;
            bool accepted = false;
            if (applied && local < preconditioned.size()) {
                const auto& system = shared_matrix ? shared_system : systems[local];
                const auto matrix_direction = multiply(
                    system.matrix, preconditioned[local]);
                const double numerator = dot(residuals[local], preconditioned[local]);
                const double denominator = dot(preconditioned[local], matrix_direction);
                if (numerator > 0.0 && denominator > 0.0 &&
                    std::isfinite(numerator) && std::isfinite(denominator)) {
                    const double alpha = numerator / denominator;
                    std::vector<double> solution = preconditioned[local];
                    for (double& value : solution) value *= alpha;
                    std::vector<double> final_residual(residuals[local].size());
                    for (std::size_t index = 0; index < final_residual.size(); ++index) {
                        final_residual[index] = residuals[local][index] -
                            alpha * matrix_direction[index];
                    }
                    if (selected_device != "cpu") {
                        const double right_hand_side_inf = *std::max_element(
                            residuals[local].begin(), residuals[local].end(),
                            [](double left, double right) {
                                return std::abs(left) < std::abs(right);
                            });
                        const double refinement_tolerance = 1.0e-10 *
                            std::max(1.0, std::abs(right_hand_side_inf));
                        for (int refinement = 0; refinement < 4; ++refinement) {
                            double residual_inf{};
                            for (const auto value : final_residual) {
                                residual_inf = std::max(
                                    residual_inf, std::abs(value));
                            }
                            if (residual_inf <= refinement_tolerance) break;
                            std::vector<double> correction;
                            if (!preconditioner.apply_preconditioner(
                                    block, contexts[local], final_residual,
                                    correction)) break;
                            const auto matrix_correction = multiply(
                                system.matrix, correction);
                            const double correction_numerator = dot(
                                final_residual, correction);
                            const double correction_denominator = dot(
                                correction, matrix_correction);
                            if (!(correction_numerator > 0.0) ||
                                !(correction_denominator > 0.0) ||
                                !std::isfinite(correction_numerator) ||
                                !std::isfinite(correction_denominator)) break;
                            const double correction_alpha =
                                correction_numerator / correction_denominator;
                            for (std::size_t index = 0; index < solution.size(); ++index) {
                                solution[index] += correction_alpha * correction[index];
                                final_residual[index] -=
                                    correction_alpha * matrix_correction[index];
                            }
                            ++batch_result.metrics.cpu_refinement_steps;
                        }
                    }
                    const auto values = linear_solution_values(
                        system, solution, scenarios[global]);
                    const auto gate_started = std::chrono::steady_clock::now();
                    const auto gate = fallback_runtime.evaluate_gate_with_residuals(
                        block, values, final_residual, true);
                    batch_result.metrics.gate_us += std::chrono::duration<double, std::micro>(
                        std::chrono::steady_clock::now() - gate_started).count();
                    if (gate.decision == GateDecision::direct_accept) {
                        SolveOutcome outcome;
                        outcome.success = true;
                        outcome.values = values;
                        outcome.message = "batched learned preconditioner passed runtime gate";
                        BlockOutcome block_outcome;
                        block_outcome.block_id = block.id;
                        block_outcome.path = SolvePath::corrected_accept;
                        block_outcome.solution = values;
                        block_outcome.gate = gate;
                        block_outcome.krylov_iterations = 1;
                        block_outcome.preconditioner_version = preconditioner.version();
                        block_outcome.attempted_experts = {preconditioner.version()};
                        outcome.blocks.push_back(std::move(block_outcome));
                        outcome.corrected_count = 1;
                        batch_result.outcomes[global] = std::move(outcome);
                        ++batch_result.metrics.accepted;
                        accepted = true;
                    }
                }
            }
            if (!accepted) {
                const auto fallback_started = std::chrono::steady_clock::now();
                batch_result.outcomes[global] = fallback_runtime.solve(
                    scenarios[global], trace_directory / "fallback");
                batch_result.metrics.fallback_us += std::chrono::duration<double, std::micro>(
                    std::chrono::steady_clock::now() - fallback_started).count();
                ++batch_result.metrics.fallback_count;
            }
        }
    }
    batch_result.metrics.average_batch = static_cast<double>(scenarios.size()) /
        static_cast<double>(batch_result.metrics.batches);
    batch_result.metrics.utilization = batch_result.metrics.average_batch /
        static_cast<double>(maximum_batch_);
    batch_result.metrics.total_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - total_started).count();
    std::filesystem::create_directories(trace_directory);
    {
        std::ofstream trace(trace_directory / "tensor-batch.trace");
        if (!trace) throw std::runtime_error("cannot write tensor batch trace");
        const BatchKey key{
            .expert_version = preconditioner.version(),
            .block_fingerprint = block.fingerprint,
            .shape = block.unknowns.size(),
            .dtype = "fp64",
            .tolerance_class = "default",
            .mode = block.mode,
        };
        trace << std::setprecision(17)
              << "SMAVE_TENSOR_BATCH 1\n"
              << "key=" << std::quoted(key.value()) << '\n'
              << "requests=" << batch_result.metrics.requests << '\n'
              << "device=" << std::quoted(batch_result.metrics.device) << '\n'
              << "device_backend=" << std::quoted(batch_result.metrics.device_backend) << '\n'
              << "device_name=" << std::quoted(batch_result.metrics.device_name) << '\n'
              << "device_batches=" << batch_result.metrics.device_batches << '\n'
              << "device_rejections=" << batch_result.metrics.device_rejections << '\n'
              << "cpu_refinement_steps="
              << batch_result.metrics.cpu_refinement_steps << '\n'
              << "device_upload_us=" << batch_result.metrics.device_upload_us << '\n'
              << "device_kernel_us=" << batch_result.metrics.device_kernel_us << '\n'
              << "device_download_us=" << batch_result.metrics.device_download_us << '\n'
              << "device_maximum_absolute_error="
              << batch_result.metrics.device_maximum_absolute_error << '\n'
              << "device_maximum_relative_error="
              << batch_result.metrics.device_maximum_relative_error << '\n'
              << "batches=" << batch_result.metrics.batches << '\n'
              << "accepted=" << batch_result.metrics.accepted << '\n'
              << "fallback_count=" << batch_result.metrics.fallback_count << '\n'
              << "kernel_us=" << batch_result.metrics.kernel_us << '\n'
              << "gate_us=" << batch_result.metrics.gate_us << '\n'
              << "fallback_us=" << batch_result.metrics.fallback_us << '\n'
              << "total_us=" << batch_result.metrics.total_us << '\n';
        for (std::size_t index = 0; index < batch_result.outcomes.size(); ++index) {
            trace << "ITEM " << index << ' '
                  << (batch_result.outcomes[index].success ? "success" : "failure") << ' '
                  << (batch_result.outcomes[index].blocks.empty()
                          ? "none"
                          : to_string(batch_result.outcomes[index].blocks.front().path))
                  << '\n';
        }
        trace << "END\n";
    }
    const auto baseline_started = std::chrono::steady_clock::now();
    for (const auto& scenario : scenarios) {
        const auto outcome = fallback_runtime.solve(
            scenario, trace_directory / "sequential-baseline");
        if (!outcome.success) ++batch_result.metrics.baseline_failures;
    }
    batch_result.metrics.sequential_baseline_us =
        std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - baseline_started).count();
    batch_result.metrics.throughput_speedup = batch_result.metrics.total_us > 0.0
        ? batch_result.metrics.sequential_baseline_us / batch_result.metrics.total_us
        : 0.0;
    return batch_result;
}

}  // namespace smave
