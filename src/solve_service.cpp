#include "smave/solve_service.hpp"

#include "smave/dae.hpp"
#include "smave/routing.hpp"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <queue>
#include <sstream>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>

namespace smave {
namespace {

bool finite_values(const std::vector<double>& values) {
    return std::all_of(values.begin(), values.end(), [](double value) {
        return std::isfinite(value);
    });
}

bool cancellation_requested(const CancellationRequestedFunction& requested) {
    return requested && requested();
}

double infinity_norm(const std::vector<double>& values) {
    double result{};
    for (const double value : values) result = std::max(result, std::abs(value));
    return result;
}

double right_hand_side_roughness(const std::vector<double>& values) {
    if (values.size() < 2) return 0.0;
    double total{};
    for (std::size_t index = 1; index < values.size(); ++index) {
        total += std::abs(values[index] - values[index - 1]);
    }
    return total / static_cast<double>(values.size() - 1) /
        std::max(1.0, infinity_norm(values));
}

double right_hand_side_sign_change_fraction(const std::vector<double>& values) {
    if (values.size() < 2) return 0.0;
    std::size_t comparisons{};
    std::size_t changes{};
    double previous{};
    bool have_previous{};
    for (const double value : values) {
        if (value == 0.0) continue;
        if (have_previous) {
            ++comparisons;
            changes += std::signbit(value) != std::signbit(previous) ? 1U : 0U;
        }
        previous = value;
        have_previous = true;
    }
    return comparisons == 0 ? 0.0
        : static_cast<double>(changes) / static_cast<double>(comparisons);
}

bool valid_linear_matrix(const LinearSystem& system) {
    const std::size_t dimension = system.size();
    if (system.has_dense_matrix()) {
        return std::all_of(system.matrix.begin(), system.matrix.end(), [](const auto& row) {
            return finite_values(row);
        });
    }
    if (system.sparsity.row_count != dimension ||
        system.sparsity.column_count != dimension ||
        system.sparsity.row_offsets.size() != dimension + 1 ||
        system.sparsity.row_offsets.empty() ||
        system.sparsity.row_offsets.front() != 0 ||
        system.sparsity.row_offsets.back() != system.sparsity.column_indices.size() ||
        system.sparse_values.size() != system.sparsity.column_indices.size() ||
        !finite_values(system.sparse_values)) return false;
    for (std::size_t row = 0; row < dimension; ++row) {
        if (system.sparsity.row_offsets[row] > system.sparsity.row_offsets[row + 1]) {
            return false;
        }
        std::size_t previous = dimension;
        for (std::size_t offset = system.sparsity.row_offsets[row];
             offset < system.sparsity.row_offsets[row + 1]; ++offset) {
            const std::size_t column = system.sparsity.column_indices[offset];
            if (column >= dimension || (previous != dimension && column <= previous)) {
                return false;
            }
            previous = column;
        }
    }
    return true;
}

std::string linear_fingerprint(const LinearSystem& system) {
    std::uint64_t hash = UINT64_C(1469598103934665603);
    auto append = [&](std::uint64_t value) {
        for (unsigned shift = 0; shift < 64; shift += 8) {
            hash ^= (value >> shift) & UINT64_C(0xff);
            hash *= UINT64_C(1099511628211);
        }
    };
    append(system.size());
    append(system.nonzeros());
    append(system.symmetric);
    append(system.positive_definite);
    if (system.has_sparse_matrix()) {
        for (std::size_t value : system.sparsity.row_offsets) append(value);
        for (std::size_t value : system.sparsity.column_indices) append(value);
        for (double value : system.sparse_values) append(std::bit_cast<std::uint64_t>(value));
    } else {
        for (const auto& row : system.matrix) {
            for (double value : row) append(std::bit_cast<std::uint64_t>(value));
        }
    }
    std::ostringstream output;
    output << "linear-" << std::hex << std::setfill('0') << std::setw(16) << hash;
    return output.str();
}

std::string nonlinear_fingerprint(
    std::size_t dimension, bool jacobian_available) {
    std::ostringstream output;
    output << "nonlinear-callback-v1-d" << dimension << "-j" << jacobian_available;
    return output.str();
}

FullyImplicitDaeIR callback_dae_ir(const std::vector<std::uint8_t>& differential_mask) {
    FullyImplicitDaeIR model;
    model.model_id = "callback-fully-implicit-dae";
    model.source_hash = "callback-fully-implicit-dae-v1-d" +
        std::to_string(differential_mask.size());
    for (std::size_t index = 0; index < differential_mask.size(); ++index) {
        const std::string name = "x" + std::to_string(index);
        if (differential_mask[index]) {
            model.states.push_back({.name = name, .start = 0.0, .nominal = 1.0});
            model.equations.push_back({
                .id = "f" + std::to_string(index),
                .residual = name + "+__smave_der_" + name,
                .variables = {name, "__smave_der_" + name},
            });
        } else {
            model.algebraics.push_back({.name = name, .start = 0.0, .nominal = 1.0});
            model.equations.push_back({
                .id = "f" + std::to_string(index),
                .residual = name,
                .variables = {name},
            });
        }
    }
    return model;
}

struct GateMetrics {
    double residual_inf{};
    double backward_error{};
    bool accepted{};
};

double norm_inf(const std::vector<double>& values) {
    double result{};
    for (double value : values) result = std::max(result, std::abs(value));
    return result;
}

double scaled_pivot_margin(std::vector<std::vector<double>> matrix) {
    if (matrix.empty() || std::any_of(
            matrix.begin(), matrix.end(), [&](const auto& row) {
                return row.size() != matrix.size() || !finite_values(row);
            })) return 0.0;
    double scale{};
    for (const auto& row : matrix) {
        for (const double value : row) scale = std::max(scale, std::abs(value));
    }
    if (!(scale > 0.0)) return 0.0;
    double minimum = 1.0;
    for (std::size_t column = 0; column < matrix.size(); ++column) {
        std::size_t pivot = column;
        for (std::size_t row = column + 1; row < matrix.size(); ++row) {
            if (std::abs(matrix[row][column]) > std::abs(matrix[pivot][column])) pivot = row;
        }
        const double pivot_value = std::abs(matrix[pivot][column]);
        if (pivot_value <= 64.0 * std::numeric_limits<double>::epsilon() * scale) return 0.0;
        minimum = std::min(minimum, pivot_value / scale);
        if (pivot != column) std::swap(matrix[pivot], matrix[column]);
        for (std::size_t row = column + 1; row < matrix.size(); ++row) {
            const double factor = matrix[row][column] / matrix[column][column];
            for (std::size_t entry = column + 1; entry < matrix.size(); ++entry) {
                matrix[row][entry] -= factor * matrix[column][entry];
            }
        }
    }
    return minimum;
}

GateMetrics evaluate_original_gate(
    const LinearSystem& system,
    const std::vector<double>& solution,
    double absolute_tolerance,
    double relative_tolerance) {
    GateMetrics metrics;
    if (solution.size() != system.size() || !finite_values(solution)) return metrics;
    double matrix_norm_inf{};
    double solution_norm_inf{};
    double right_norm_inf{};
    for (double value : solution) {
        solution_norm_inf = std::max(solution_norm_inf, std::abs(value));
    }
    for (std::size_t row = 0; row < system.size(); ++row) {
        double product{};
        double row_sum{};
        if (system.has_sparse_matrix()) {
            for (std::size_t offset = system.sparsity.row_offsets[row];
                 offset < system.sparsity.row_offsets[row + 1]; ++offset) {
                const double coefficient = system.sparse_values[offset];
                product += coefficient * solution[system.sparsity.column_indices[offset]];
                row_sum += std::abs(coefficient);
            }
        } else {
            for (std::size_t column = 0; column < system.size(); ++column) {
                const double coefficient = system.matrix[row][column];
                product += coefficient * solution[column];
                row_sum += std::abs(coefficient);
            }
        }
        metrics.residual_inf = std::max(
            metrics.residual_inf,
            std::abs(product - system.right_hand_side[row]));
        matrix_norm_inf = std::max(matrix_norm_inf, row_sum);
        right_norm_inf = std::max(
            right_norm_inf, std::abs(system.right_hand_side[row]));
    }
    const double scale = matrix_norm_inf * solution_norm_inf + right_norm_inf;
    metrics.backward_error = metrics.residual_inf /
        std::max(scale, std::numeric_limits<double>::min());
    metrics.accepted = std::isfinite(metrics.backward_error) &&
        metrics.residual_inf <= absolute_tolerance + relative_tolerance * scale;
    return metrics;
}

bool evaluate_nonlinear_residual(
    const VerifiedNonlinearSolveProblem& problem,
    const std::vector<double>& state,
    std::vector<double>& residual) {
    if (!problem.residual || state.size() != problem.initial_state.size() ||
        !finite_values(state) || !problem.residual(state, residual)) return false;
    return residual.size() == state.size() && finite_values(residual);
}

bool nonlinear_jacobian(
    const VerifiedNonlinearSolveProblem& problem,
    const std::vector<double>& state,
    const std::vector<double>& residual,
    bool use_callback,
    std::vector<std::vector<double>>& jacobian) {
    const std::size_t dimension = state.size();
    jacobian.assign(dimension, std::vector<double>(dimension));
    if (use_callback && problem.jacobian) {
        if (!problem.jacobian(state, jacobian) || jacobian.size() != dimension) return false;
        return std::all_of(jacobian.begin(), jacobian.end(), [&](const auto& row) {
            return row.size() == dimension && finite_values(row);
        });
    }
    std::vector<double> perturbed = state;
    std::vector<double> perturbed_residual;
    for (std::size_t column = 0; column < dimension; ++column) {
        const double step = std::sqrt(std::numeric_limits<double>::epsilon()) *
            std::max(1.0, std::abs(state[column]));
        perturbed[column] += step;
        if (!evaluate_nonlinear_residual(problem, perturbed, perturbed_residual)) return false;
        perturbed[column] = state[column];
        for (std::size_t row = 0; row < dimension; ++row) {
            jacobian[row][column] = (perturbed_residual[row] - residual[row]) / step;
        }
    }
    return true;
}

bool damped_newton(
    const VerifiedNonlinearSolveProblem& problem,
    bool use_callback_jacobian,
    const VerifiedNonlinearSolveOptions& options,
    std::vector<double>& state,
    double& residual_inf) {
    state = problem.initial_state;
    std::vector<double> residual;
    if (!evaluate_nonlinear_residual(problem, state, residual)) return false;
    const double initial_residual = norm_inf(residual);
    const double threshold = options.absolute_tolerance +
        options.relative_tolerance * std::max(1.0, initial_residual);
    for (int iteration = 0; iteration <= options.maximum_iterations; ++iteration) {
        if (cancellation_requested(options.cancellation_requested)) return false;
        residual_inf = norm_inf(residual);
        if (residual_inf <= threshold) return true;
        if (iteration == options.maximum_iterations) break;
        std::vector<std::vector<double>> jacobian;
        if (!nonlinear_jacobian(
                problem, state, residual, use_callback_jacobian, jacobian)) return false;
        LinearSystem linear;
        linear.matrix = std::move(jacobian);
        linear.right_hand_side.resize(state.size());
        for (std::size_t index = 0; index < state.size(); ++index) {
            linear.right_hand_side[index] = -residual[index];
        }
        std::vector<double> correction;
        if (!dense_direct_solve(linear, correction)) return false;
        bool accepted = false;
        double damping = 1.0;
        std::vector<double> candidate(state.size());
        std::vector<double> candidate_residual;
        for (int line_search = 0; line_search < 16; ++line_search) {
            if (cancellation_requested(options.cancellation_requested)) return false;
            for (std::size_t index = 0; index < state.size(); ++index) {
                candidate[index] = state[index] + damping * correction[index];
            }
            if (evaluate_nonlinear_residual(problem, candidate, candidate_residual) &&
                norm_inf(candidate_residual) < residual_inf) {
                state = candidate;
                residual = candidate_residual;
                accepted = true;
                break;
            }
            damping *= 0.5;
        }
        if (!accepted) return false;
    }
    return false;
}

std::string ode_fingerprint(std::size_t dimension) {
    std::ostringstream output;
    output << "explicit-ode-callback-dimension-" << dimension;
    return output.str();
}

bool evaluate_ode_rhs(
    const VerifiedOdeSolveProblem& problem,
    double time,
    const std::vector<double>& state,
    std::vector<double>& derivative) {
    if (!problem.right_hand_side || !std::isfinite(time) ||
        state.size() != problem.initial_state.size() || !finite_values(state) ||
        !problem.right_hand_side(time, state, derivative)) return false;
    return derivative.size() == state.size() && finite_values(derivative);
}

bool rk4_step(
    const VerifiedOdeSolveProblem& problem,
    double time,
    const std::vector<double>& state,
    double step,
    std::vector<double>& result) {
    std::vector<double> k1, k2, k3, k4;
    if (!evaluate_ode_rhs(problem, time, state, k1)) return false;
    std::vector<double> work(state.size());
    for (std::size_t index = 0; index < state.size(); ++index) {
        work[index] = state[index] + 0.5 * step * k1[index];
    }
    if (!evaluate_ode_rhs(problem, time + 0.5 * step, work, k2)) return false;
    for (std::size_t index = 0; index < state.size(); ++index) {
        work[index] = state[index] + 0.5 * step * k2[index];
    }
    if (!evaluate_ode_rhs(problem, time + 0.5 * step, work, k3)) return false;
    for (std::size_t index = 0; index < state.size(); ++index) {
        work[index] = state[index] + step * k3[index];
    }
    if (!evaluate_ode_rhs(problem, time + step, work, k4)) return false;
    result.resize(state.size());
    for (std::size_t index = 0; index < state.size(); ++index) {
        result[index] = state[index] + step *
            (k1[index] + 2.0 * k2[index] + 2.0 * k3[index] + k4[index]) / 6.0;
    }
    return finite_values(result);
}

struct OdeIntegration {
    bool success{};
    std::vector<double> state;
    double time{};
    double maximum_scaled_error{};
    std::size_t accepted_steps{};
    std::size_t rejected_steps{};
    std::size_t event_count{};
    double last_event_time{};
    VerifiedSolveDiagnosticCode diagnostic_code{
        VerifiedSolveDiagnosticCode::numerical_failure};
};

double scaled_ode_error(
    const std::vector<double>& lower,
    const std::vector<double>& higher,
    const VerifiedOdeSolveOptions& options);

bool evaluate_event_guard(
    const VerifiedOdeEvent& event,
    double time,
    const std::vector<double>& state,
    double& guard) {
    return event.guard && event.guard(time, state, guard) && std::isfinite(guard);
}

bool crosses_event(double left, double right, int direction) {
    if (direction > 0) return left < 0.0 && right >= 0.0;
    if (direction < 0) return left > 0.0 && right <= 0.0;
    return (left < 0.0 && right >= 0.0) || (left > 0.0 && right <= 0.0);
}

bool rk4_propagate(
    const VerifiedOdeSolveProblem& problem,
    double time,
    const std::vector<double>& state,
    double step,
    std::vector<double>& propagated) {
    if (step == 0.0) {
        propagated = state;
        return true;
    }
    std::vector<double> half;
    return rk4_step(problem, time, state, 0.5 * step, half) &&
        rk4_step(problem, time + 0.5 * step, half, 0.5 * step, propagated);
}

struct LocatedEvent {
    std::size_t index{};
    double time{};
    std::vector<double> state;
};

bool locate_event(
    const VerifiedOdeSolveProblem& problem,
    const VerifiedOdeEvent& event,
    std::size_t index,
    double left_time,
    const std::vector<double>& left_state,
    double right_time,
    const std::vector<double>& right_state,
    double left_guard,
    double right_guard,
    LocatedEvent& located,
    const CancellationRequestedFunction& cancellation) {
    constexpr int maximum_iterations = 64;
    const double root_tolerance = 1.0e-10 * std::max(1.0, std::abs(right_time));
    double lower_time = left_time;
    double upper_time = right_time;
    double lower_guard = left_guard;
    std::vector<double> upper_state = right_state;
    for (int iteration = 0; iteration < maximum_iterations &&
         upper_time - lower_time > root_tolerance; ++iteration) {
        if (cancellation_requested(cancellation)) return false;
        const double midpoint = lower_time + 0.5 * (upper_time - lower_time);
        std::vector<double> midpoint_state;
        if (!rk4_propagate(
                problem, left_time, left_state, midpoint - left_time, midpoint_state)) {
            return false;
        }
        double midpoint_guard{};
        if (!evaluate_event_guard(event, midpoint, midpoint_state, midpoint_guard)) return false;
        if (crosses_event(lower_guard, midpoint_guard, event.direction)) {
            upper_time = midpoint;
            upper_state = std::move(midpoint_state);
            right_guard = midpoint_guard;
        } else {
            lower_time = midpoint;
            lower_guard = midpoint_guard;
        }
    }
    static_cast<void>(right_guard);
    located = {.index = index, .time = upper_time, .state = std::move(upper_state)};
    return true;
}

OdeIntegration integrate_event_rk4(
    const VerifiedOdeSolveProblem& problem,
    const VerifiedOdeSolveOptions& options) {
    OdeIntegration integration{.state = problem.initial_state, .time = options.start_time};
    double step = std::min(options.maximum_step, options.end_time - options.start_time);
    for (int attempt = 0; attempt < options.maximum_steps &&
         integration.time < options.end_time; ++attempt) {
        if (cancellation_requested(options.cancellation_requested)) {
            integration.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
            return integration;
        }
        const double remaining = options.end_time - integration.time;
        const double time_resolution = 16.0 * std::numeric_limits<double>::epsilon() *
            std::max({1.0, std::abs(integration.time), std::abs(options.end_time)});
        if (remaining <= time_resolution) {
            integration.time = options.end_time;
            break;
        }
        step = std::min(step, remaining);
        std::vector<double> full, half, candidate;
        if (!rk4_step(problem, integration.time, integration.state, step, full) ||
            !rk4_step(problem, integration.time, integration.state, 0.5 * step, half) ||
            !rk4_step(
                problem, integration.time + 0.5 * step, half, 0.5 * step, candidate)) {
            return integration;
        }
        const double error = scaled_ode_error(full, candidate, options) / 15.0;
        if (error > 1.0) {
            ++integration.rejected_steps;
            step *= std::clamp(0.9 * std::pow(1.0 / error, 0.2), 0.2, 0.9);
            continue;
        }
        const double next_time = step == remaining
            ? options.end_time : integration.time + step;
        std::vector<LocatedEvent> located_events;
        for (std::size_t index = 0; index < problem.events.size(); ++index) {
            double left_guard{};
            double right_guard{};
            if (!evaluate_event_guard(
                    problem.events[index], integration.time, integration.state, left_guard) ||
                !evaluate_event_guard(
                    problem.events[index], next_time, candidate, right_guard)) {
                return integration;
            }
            if (!crosses_event(left_guard, right_guard, problem.events[index].direction)) {
                continue;
            }
            LocatedEvent located;
            if (!locate_event(
                    problem,
                    problem.events[index],
                    index,
                    integration.time,
                    integration.state,
                    next_time,
                    candidate,
                    left_guard,
                    right_guard,
                    located,
                    options.cancellation_requested)) return integration;
            located_events.push_back(std::move(located));
        }
        if (!located_events.empty()) {
            const double event_time = std::min_element(
                located_events.begin(), located_events.end(), [](const auto& left, const auto& right) {
                    return left.time < right.time;
                })->time;
            const double simultaneous_tolerance = 1.0e-9 * std::max(1.0, std::abs(event_time));
            std::vector<std::size_t> simultaneous;
            for (const auto& located : located_events) {
                if (std::abs(located.time - event_time) <= simultaneous_tolerance) {
                    simultaneous.push_back(located.index);
                }
            }
            std::sort(simultaneous.begin(), simultaneous.end(), [&](const auto left, const auto right) {
                const auto& left_event = problem.events[left];
                const auto& right_event = problem.events[right];
                return left_event.priority != right_event.priority
                    ? left_event.priority > right_event.priority
                    : left < right;
            });
            std::vector<double> committed;
            if (!rk4_propagate(
                    problem,
                    integration.time,
                    integration.state,
                    event_time - integration.time,
                    committed)) return integration;
            std::size_t committed_events{};
            if (problem.event_cluster_reset) {
                std::vector<double> reset_state;
                VerifiedSolveDiagnosticCode failure_code =
                    VerifiedSolveDiagnosticCode::callback_failure;
                if (!problem.event_cluster_reset(
                        event_time,
                        simultaneous,
                        committed,
                        reset_state,
                        committed_events,
                        failure_code) ||
                    reset_state.size() != committed.size() || !finite_values(reset_state) ||
                    committed_events < simultaneous.size()) {
                    integration.diagnostic_code =
                        cancellation_requested(options.cancellation_requested)
                            ? VerifiedSolveDiagnosticCode::cancelled
                            : failure_code;
                    return integration;
                }
                committed = std::move(reset_state);
            } else {
                for (const auto event_index : simultaneous) {
                    if (cancellation_requested(options.cancellation_requested)) {
                        integration.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
                        return integration;
                    }
                    std::vector<double> reset_state;
                    if (!problem.events[event_index].reset(
                            event_time, committed, reset_state) ||
                        reset_state.size() != committed.size() || !finite_values(reset_state)) {
                        return integration;
                    }
                    committed = std::move(reset_state);
                }
                committed_events = simultaneous.size();
            }
            for (const auto event_index : simultaneous) {
                if (cancellation_requested(options.cancellation_requested)) {
                    integration.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
                    return integration;
                }
                double post_guard{};
                if (!evaluate_event_guard(
                        problem.events[event_index], event_time, committed, post_guard)) {
                    return integration;
                }
                constexpr double guard_release = 1.0e-10;
                if ((problem.events[event_index].direction > 0 &&
                     post_guard >= -guard_release) ||
                    (problem.events[event_index].direction < 0 &&
                     post_guard <= guard_release) ||
                    (problem.events[event_index].direction == 0 &&
                     std::abs(post_guard) <= guard_release)) {
                    return integration;
                }
            }
            std::vector<double> post_rhs;
            if (!evaluate_ode_rhs(problem, event_time, committed, post_rhs)) return integration;
            if (cancellation_requested(options.cancellation_requested)) {
                integration.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
                return integration;
            }
            integration.state = std::move(committed);
            integration.time = event_time;
            integration.last_event_time = event_time;
            integration.event_count += committed_events;
            integration.maximum_scaled_error = std::max(
                integration.maximum_scaled_error, error);
            ++integration.accepted_steps;
            step = std::min(options.maximum_step, options.end_time - integration.time);
            continue;
        }
        if (cancellation_requested(options.cancellation_requested)) {
            integration.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
            return integration;
        }
        integration.state = std::move(candidate);
        integration.time = next_time;
        integration.maximum_scaled_error = std::max(integration.maximum_scaled_error, error);
        ++integration.accepted_steps;
        const double factor = error == 0.0 ? 2.0 :
            std::clamp(0.9 * std::pow(1.0 / error, 0.2), 0.2, 2.0);
        step *= factor;
    }
    integration.success = integration.time == options.end_time;
    if (integration.success) {
        integration.diagnostic_code = VerifiedSolveDiagnosticCode::success;
    }
    return integration;
}

double scaled_ode_error(
    const std::vector<double>& lower,
    const std::vector<double>& higher,
    const VerifiedOdeSolveOptions& options) {
    double error{};
    for (std::size_t index = 0; index < lower.size(); ++index) {
        const double scale = options.absolute_tolerance + options.relative_tolerance *
            std::max({1.0, std::abs(lower[index]), std::abs(higher[index])});
        error = std::max(error, std::abs(higher[index] - lower[index]) / scale);
    }
    return error;
}

OdeIntegration integrate_rk4(
    const VerifiedOdeSolveProblem& problem,
    const VerifiedOdeSolveOptions& options) {
    OdeIntegration integration{.state = problem.initial_state, .time = options.start_time};
    double step = std::min(options.maximum_step, options.end_time - options.start_time);
    for (int attempt = 0; attempt < options.maximum_steps &&
         integration.time < options.end_time; ++attempt) {
        if (cancellation_requested(options.cancellation_requested)) {
            integration.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
            return integration;
        }
        step = std::min(step, options.end_time - integration.time);
        std::vector<double> full, half, two_half;
        if (!rk4_step(problem, integration.time, integration.state, step, full) ||
            !rk4_step(problem, integration.time, integration.state, 0.5 * step, half) ||
            !rk4_step(problem, integration.time + 0.5 * step, half, 0.5 * step, two_half)) {
            return integration;
        }
        const double error = scaled_ode_error(full, two_half, options) / 15.0;
        if (error <= 1.0) {
            if (cancellation_requested(options.cancellation_requested)) {
                integration.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
                return integration;
            }
            integration.state = std::move(two_half);
            integration.time += step;
            integration.maximum_scaled_error = std::max(
                integration.maximum_scaled_error, error);
            ++integration.accepted_steps;
        } else {
            ++integration.rejected_steps;
        }
        const double factor = error == 0.0 ? 2.0 :
            std::clamp(0.9 * std::pow(1.0 / error, 0.2), 0.2, 2.0);
        step *= factor;
        if (!std::isfinite(step) || step <= std::numeric_limits<double>::epsilon() *
                std::max(1.0, std::abs(integration.time))) return integration;
    }
    integration.success = integration.time == options.end_time;
    return integration;
}

OdeIntegration integrate_heun(
    const VerifiedOdeSolveProblem& problem,
    const VerifiedOdeSolveOptions& options) {
    OdeIntegration integration{.state = problem.initial_state, .time = options.start_time};
    double step = std::min(options.maximum_step, options.end_time - options.start_time);
    for (int attempt = 0; attempt < options.maximum_steps &&
         integration.time < options.end_time; ++attempt) {
        if (cancellation_requested(options.cancellation_requested)) {
            integration.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
            return integration;
        }
        step = std::min(step, options.end_time - integration.time);
        std::vector<double> first;
        if (!evaluate_ode_rhs(problem, integration.time, integration.state, first)) {
            return integration;
        }
        std::vector<double> euler(integration.state.size());
        for (std::size_t index = 0; index < euler.size(); ++index) {
            euler[index] = integration.state[index] + step * first[index];
        }
        std::vector<double> second;
        if (!evaluate_ode_rhs(problem, integration.time + step, euler, second)) {
            return integration;
        }
        std::vector<double> heun(euler.size());
        for (std::size_t index = 0; index < heun.size(); ++index) {
            heun[index] = integration.state[index] +
                0.5 * step * (first[index] + second[index]);
        }
        const double error = scaled_ode_error(euler, heun, options);
        if (error <= 1.0) {
            if (cancellation_requested(options.cancellation_requested)) {
                integration.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
                return integration;
            }
            integration.state = std::move(heun);
            integration.time += step;
            integration.maximum_scaled_error = std::max(
                integration.maximum_scaled_error, error);
            ++integration.accepted_steps;
        } else {
            ++integration.rejected_steps;
        }
        const double factor = error == 0.0 ? 2.0 :
            std::clamp(0.9 * std::sqrt(1.0 / error), 0.2, 2.0);
        step *= factor;
        if (!std::isfinite(step) || step <= std::numeric_limits<double>::epsilon() *
                std::max(1.0, std::abs(integration.time))) return integration;
    }
    integration.success = integration.time == options.end_time;
    return integration;
}

OdeIntegration integrate_external_dense_stepper(
    const VerifiedOdeSolveProblem& problem,
    const VerifiedOdeSolveOptions& options) {
    OdeIntegration integration{.state = problem.initial_state, .time = options.start_time};
    for (int attempt = 0; attempt < options.maximum_steps &&
         integration.time < options.end_time; ++attempt) {
        if (cancellation_requested(options.cancellation_requested)) {
            integration.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
            return integration;
        }
        const double remaining = options.end_time - integration.time;
        const double time_resolution = 16.0 * std::numeric_limits<double>::epsilon() *
            std::max({1.0, std::abs(integration.time), std::abs(options.end_time)});
        if (remaining <= time_resolution) {
            integration.time = options.end_time;
            break;
        }
        const double step = std::min(options.maximum_step, remaining);
        const double next_time = step == remaining
            ? options.end_time : integration.time + step;
        std::vector<double> quarter = integration.state;
        std::vector<double> midpoint = integration.state;
        std::vector<double> three_quarter = integration.state;
        std::vector<double> next = integration.state;
        bool callback_succeeded = false;
        try {
            callback_succeeded = options.external_step_fallback(
                integration.time,
                integration.state,
                next_time,
                quarter,
                midpoint,
                three_quarter,
                next);
        } catch (...) {
            callback_succeeded = false;
        }
        if (cancellation_requested(options.cancellation_requested)) {
            integration.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
            return integration;
        }
        const std::size_t dimension = integration.state.size();
        if (!callback_succeeded) {
            integration.diagnostic_code = VerifiedSolveDiagnosticCode::callback_failure;
            return integration;
        }
        if (quarter.size() != dimension || midpoint.size() != dimension ||
            three_quarter.size() != dimension || next.size() != dimension ||
            !finite_values(quarter) || !finite_values(midpoint) ||
            !finite_values(three_quarter) || !finite_values(next)) {
            integration.diagnostic_code =
                VerifiedSolveDiagnosticCode::original_gate_rejected;
            ++integration.rejected_steps;
            return integration;
        }
        const double quarter_time = integration.time + 0.25 * step;
        const double midpoint_time = integration.time + 0.5 * step;
        const double three_quarter_time = integration.time + 0.75 * step;
        std::vector<double> derivative0, derivative_quarter, derivative_midpoint,
            derivative_three_quarter, derivative1;
        if (!evaluate_ode_rhs(
                problem, integration.time, integration.state, derivative0) ||
            !evaluate_ode_rhs(problem, quarter_time, quarter, derivative_quarter) ||
            !evaluate_ode_rhs(problem, midpoint_time, midpoint, derivative_midpoint) ||
            !evaluate_ode_rhs(
                problem, three_quarter_time, three_quarter,
                derivative_three_quarter) ||
            !evaluate_ode_rhs(problem, next_time, next, derivative1)) {
            integration.diagnostic_code = VerifiedSolveDiagnosticCode::callback_failure;
            ++integration.rejected_steps;
            return integration;
        }
        double maximum_scaled_defect{};
        for (std::size_t index = 0; index < dimension; ++index) {
            const double first_defect = midpoint[index] - integration.state[index] -
                step * (derivative0[index] + 4.0 * derivative_quarter[index] +
                        derivative_midpoint[index]) / 12.0;
            const double second_defect = next[index] - midpoint[index] -
                step * (derivative_midpoint[index] +
                        4.0 * derivative_three_quarter[index] + derivative1[index]) /
                    12.0;
            const double scale = std::max({
                1.0,
                std::abs(integration.state[index]),
                std::abs(quarter[index]),
                std::abs(midpoint[index]),
                std::abs(three_quarter[index]),
                std::abs(next[index]),
                step * std::abs(derivative0[index]),
                step * std::abs(derivative_quarter[index]),
                step * std::abs(derivative_midpoint[index]),
                step * std::abs(derivative_three_quarter[index]),
                step * std::abs(derivative1[index]),
            });
            const double threshold = options.absolute_tolerance +
                options.relative_tolerance * scale;
            const double denominator = std::max(
                threshold, std::numeric_limits<double>::min());
            maximum_scaled_defect = std::max({
                maximum_scaled_defect,
                std::abs(first_defect) / denominator,
                std::abs(second_defect) / denominator,
            });
        }
        if (!std::isfinite(maximum_scaled_defect) || maximum_scaled_defect > 1.0) {
            integration.diagnostic_code =
                VerifiedSolveDiagnosticCode::original_gate_rejected;
            ++integration.rejected_steps;
            return integration;
        }
        if (cancellation_requested(options.cancellation_requested)) {
            integration.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
            return integration;
        }
        integration.state = std::move(next);
        integration.time = next_time;
        integration.maximum_scaled_error = std::max(
            integration.maximum_scaled_error, maximum_scaled_defect);
        ++integration.accepted_steps;
    }
    integration.success = integration.time == options.end_time;
    if (!integration.success &&
        integration.diagnostic_code == VerifiedSolveDiagnosticCode::numerical_failure) {
        integration.diagnostic_code = VerifiedSolveDiagnosticCode::iteration_limit;
    }
    return integration;
}

}

VerifiedLinearSolveResult verified_linear_solve(
    const LinearSystem& system,
    const VerifiedLinearSolveOptions& options) {
    VerifiedLinearSolveResult result;
    const std::string prefix = std::string(verified_linear_solve_service_v1) + ": ";
    if (system.size() == 0 || system.right_hand_side.size() != system.size() ||
        !valid_linear_matrix(system) ||
        !finite_values(system.right_hand_side) ||
        !std::isfinite(options.absolute_tolerance) ||
        !std::isfinite(options.relative_tolerance) ||
        options.absolute_tolerance < 0.0 || options.relative_tolerance < 0.0) {
        result.diagnostic = prefix + "invalid linear system or tolerance contract";
        return result;
    }
    if (options.maximum_work_iterations <= 0 || options.restart_dimension <= 0) {
        result.diagnostic = prefix + "invalid linear work budget";
        return result;
    }
    if (cancellation_requested(options.cancellation_requested)) {
        result.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
        result.diagnostic = prefix + "cancelled before candidate execution";
        return result;
    }

    const auto structured = structured_tridiagonal_direct_solve(system);
    SparseLinearProfile profile{
        .fingerprint = linear_fingerprint(system),
        .rows = system.size(),
        .columns = system.size(),
        .nonzeros = system.nonzeros(),
        .structurally_symmetric = system.symmetric,
        .numerically_symmetric = system.symmetric,
        .numerically_positive_definite = system.positive_definite,
        .diagonal_condition_estimate = system.diagonal_condition_estimate,
        .coefficient_dynamic_range = system.coefficient_dynamic_range,
        .row_nonzero_coefficient_of_variation =
            system.row_nonzero_coefficient_of_variation,
        .row_l1_condition_estimate = system.row_l1_condition_estimate,
        .diagonal_dominance_fraction = system.diagonal_dominance_fraction,
        .mean_diagonal_row_l1_fraction = system.mean_diagonal_row_l1_fraction,
        .normalized_mean_bandwidth = system.normalized_mean_bandwidth,
        .structured_direct_backend = structured.eligible && structured.solved
            ? structured.backend : "",
        .dense_direct_available = system.has_dense_matrix() || system.size() <= 64,
        .right_hand_side_inf = infinity_norm(system.right_hand_side),
        .right_hand_side_roughness = right_hand_side_roughness(system.right_hand_side),
        .right_hand_side_sign_change_fraction =
            right_hand_side_sign_change_fraction(system.right_hand_side),
        .absolute_tolerance = options.absolute_tolerance,
        .relative_tolerance = options.relative_tolerance,
        .maximum_work_iterations = options.maximum_work_iterations,
        .restart_dimension = options.restart_dimension,
    };
    std::size_t amg_grid_width{};
    profile.regular_grid = aggregation_amg_five_point_eligible(
        system, &amg_grid_width, nullptr);
    profile.grid_dimension = profile.regular_grid ? 2 : 0;
    std::set<std::string> service_backends{
        "pcg-aggregation-amg-cpu-v1", "pcg-ic0-cpu-v1",
        "pcg-jacobi-cpu-v1", "gmres-ilut-cpu-v1",
        "gmres-ilu0-cpu-v1", "dense-direct-cpu-v1"};
    const bool built_in_sparse_direct_eligible =
        system.has_sparse_matrix() &&
        system.size() <= options.built_in_sparse_direct_row_limit;
    if (built_in_sparse_direct_eligible) {
        service_backends.insert("sparse-ordered-threshold-pivot-cpu-v2");
    }
    if (!profile.structured_direct_backend.empty()) {
        service_backends.insert(profile.structured_direct_backend);
    }
    if (industrial_sparse_direct_available()) {
        service_backends.insert(industrial_sparse_direct_backend());
    }
    if (superlu_sparse_direct_available()) {
        service_backends.insert(superlu_sparse_direct_backend());
    }
    RoutingConfig routing = options.routing.value_or(RoutingConfig{});
    if (options.routing.has_value()) {
        if (routing.expert_allowlist.empty()) {
            routing.expert_allowlist = service_backends;
        } else {
            std::erase_if(routing.expert_allowlist, [&](const std::string& expert) {
                return !service_backends.contains(expert);
            });
            if (routing.expert_allowlist.empty()) {
                result.diagnostic = prefix + "no requested linear expert is available";
                return result;
            }
        }
    } else {
        routing.top_k = service_backends.size();
        routing.require_original_fallback = true;
        routing.expert_allowlist = service_backends;
    }
    SolvePlan plan;
    try {
        plan = route_sparse_linear_system(profile, routing);
    } catch (const std::invalid_argument& error) {
        result.diagnostic = prefix + error.what();
        result.diagnostic_code = VerifiedSolveDiagnosticCode::invalid_contract;
        return result;
    }
    result.equation_family = plan.assessment.equation_family;
    result.plan_id = plan.plan_id;
    if (options.external_fallback) {
        result.plan_id += "|caller-linear-fallback-v1";
    }

    using AttemptRunner = std::function<bool(std::vector<double>&, int&)>;
    std::set<std::pair<std::string, int>> executed_actions;
    bool cancelled_during_attempt{};
    auto run_attempt = [&](const std::string& backend, int work_iterations,
                           bool fallback, const AttemptRunner& runner) {
        if (cancelled_during_attempt) return false;
        if (!executed_actions.emplace(backend, work_iterations).second) return false;
        VerifiedLinearSolveAttempt attempt{
            .backend = backend,
            .work_iterations = work_iterations,
            .residual_inf = std::numeric_limits<double>::infinity(),
        };
        const auto started = std::chrono::steady_clock::now();
        std::vector<double> candidate;
        bool candidate_available = false;
        if (cancellation_requested(options.cancellation_requested)) {
            cancelled_during_attempt = true;
        } else {
            try {
                candidate_available = runner(candidate, attempt.executed_iterations);
            } catch (...) {
                candidate_available = false;
            }
        }
        if (cancellation_requested(options.cancellation_requested)) {
            cancelled_during_attempt = true;
            attempt.status = "cancelled";
        } else if (!candidate_available || candidate.size() != system.size() ||
                   !finite_values(candidate)) {
            attempt.status = "solver-failed";
        } else {
            const GateMetrics gate = evaluate_original_gate(
                system, candidate,
                options.absolute_tolerance, options.relative_tolerance);
            attempt.residual_inf = gate.residual_inf;
            if (cancellation_requested(options.cancellation_requested)) {
                attempt.status = "cancelled";
            } else if (!gate.accepted) {
                attempt.status = "gate-rejected";
            } else {
                attempt.status = "accepted";
                result.solution = std::move(candidate);
                result.backend = backend;
                result.residual_inf = gate.residual_inf;
                result.backward_error = gate.backward_error;
                result.used_fallback = fallback;
                result.success = true;
                result.diagnostic_code = VerifiedSolveDiagnosticCode::success;
                if (fallback && backend != "caller-linear-fallback-v1") {
                    result.plan_id += "|terminal-numerical-linear-cascade-v1";
                }
                result.diagnostic = prefix + "plan=" + result.plan_id + ";family=" +
                    result.equation_family +
                    ";accepted by original-matrix residual gate";
            }
        }
        attempt.wall_us = std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - started).count();
        result.attempts.push_back(std::move(attempt));
        if (cancelled_during_attempt) {
            result.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
            result.diagnostic = prefix + "cancelled during linear candidate attempt";
        }
        return result.success;
    };
    const auto run_backend = [&](const std::string& backend, int work_iterations,
                                 bool fallback) {
        return run_attempt(
            backend, work_iterations, fallback,
            [&](std::vector<double>& candidate, int& executed_iterations) {
                if (backend == structured.backend && structured.solved) {
                    candidate = structured.solution;
                    return true;
                }
                if (backend == "pcg-aggregation-amg-cpu-v1") {
                    if (work_iterations <= 0) return false;
                    const auto amg = aggregation_amg_pcg_solve(
                        system, options.absolute_tolerance,
                        options.relative_tolerance, work_iterations);
                    executed_iterations = amg.iterations;
                    candidate = amg.solution;
                    return amg.solved || candidate.size() == system.size();
                }
                if (backend == "pcg-ic0-cpu-v1" ||
                    backend == "pcg-jacobi-cpu-v1") {
                    if (work_iterations <= 0) return false;
                    const Preconditioner preconditioner = backend == "pcg-ic0-cpu-v1"
                        ? incomplete_cholesky_zero_preconditioner(system, system.sparsity)
                        : jacobi_preconditioner(system);
                    if (!preconditioner) return false;
                    auto krylov = preconditioned_conjugate_gradient(
                        system, std::vector<double>(system.size()), preconditioner,
                        options.absolute_tolerance, options.relative_tolerance,
                        work_iterations);
                    executed_iterations = krylov.iterations;
                    candidate = std::move(krylov.solution);
                    return candidate.size() == system.size();
                }
                if (backend == "gmres-ilut-cpu-v1" ||
                    backend == "gmres-ilu0-cpu-v1" ||
                    backend == "gmres-identity-cpu-v1") {
                    if (work_iterations <= 0) return false;
                    Preconditioner preconditioner;
                    if (backend == "gmres-ilut-cpu-v1") {
                        preconditioner = incomplete_lu_threshold_preconditioner(
                            system, 1.0e-3, 40);
                    } else if (backend == "gmres-ilu0-cpu-v1") {
                        preconditioner = incomplete_lu_zero_preconditioner(
                            system, system.sparsity);
                    } else {
                        preconditioner = [](const std::vector<double>& residual,
                                            std::vector<double>& output) {
                            output = residual;
                            return finite_values(output);
                        };
                    }
                    if (!preconditioner) return false;
                    auto krylov = restarted_gmres(
                        system, std::vector<double>(system.size()), preconditioner,
                        options.absolute_tolerance, options.relative_tolerance,
                        work_iterations, options.restart_dimension);
                    executed_iterations = krylov.iterations;
                    candidate = std::move(krylov.solution);
                    return candidate.size() == system.size();
                }
                if (backend == "lsqr-identity-cpu-v1") {
                    if (work_iterations <= 0) return false;
                    auto krylov = least_squares_qr(
                        system, std::vector<double>(system.size()),
                        options.absolute_tolerance, options.relative_tolerance,
                        work_iterations);
                    executed_iterations = krylov.iterations;
                    candidate = std::move(krylov.solution);
                    return candidate.size() == system.size();
                }
                if (system.has_sparse_matrix() &&
                    backend == industrial_sparse_direct_backend()) {
                    const auto direct = industrial_sparse_direct_solve(system);
                    candidate = direct.solution;
                    return direct.solved;
                }
                if (system.has_sparse_matrix() &&
                    backend == superlu_sparse_direct_backend()) {
                    const auto direct = superlu_sparse_direct_solve(system);
                    candidate = direct.solution;
                    return direct.solved;
                }
                if (system.has_sparse_matrix() &&
                    backend == "sparse-ordered-threshold-pivot-cpu-v2") {
                    const auto direct = sparse_ordered_threshold_pivot_solve(system);
                    candidate = direct.solution;
                    return direct.solved;
                }
                if (backend == "dense-direct-cpu-v1") {
                    return dense_direct_solve(system, candidate);
                }
                return false;
            });
    };

    for (const auto& step : plan.steps) {
        if (cancellation_requested(options.cancellation_requested)) {
            result.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
            result.diagnostic = prefix + "cancelled between candidate attempts";
            return result;
        }
        if (run_backend(step.expert_version, step.budget.work_iterations, false)) {
            return result;
        }
    }

    if (structured.solved && run_backend(structured.backend, 0, true)) return result;
    if (system.positive_definite) {
        if (run_backend("pcg-ic0-cpu-v1", options.maximum_work_iterations, true)) {
            return result;
        }
        if (run_backend("pcg-jacobi-cpu-v1", options.maximum_work_iterations, true)) {
            return result;
        }
    } else {
        if (plan.assessment.scale_class != "large" &&
            run_backend("gmres-ilut-cpu-v1", options.maximum_work_iterations, true)) {
            return result;
        }
        if (run_backend("gmres-ilu0-cpu-v1", options.maximum_work_iterations, true)) {
            return result;
        }
    }
    if (system.has_sparse_matrix() && industrial_sparse_direct_available() &&
        run_backend(industrial_sparse_direct_backend(), 0, true)) return result;
    if (system.has_sparse_matrix() && superlu_sparse_direct_available() &&
        run_backend(superlu_sparse_direct_backend(), 0, true)) return result;
    if (built_in_sparse_direct_eligible &&
        run_backend("sparse-ordered-threshold-pivot-cpu-v2", 0, true)) return result;
    if (profile.dense_direct_available &&
        run_backend("dense-direct-cpu-v1", 0, true)) return result;
    if (system.has_sparse_matrix() &&
        run_backend("gmres-identity-cpu-v1", options.maximum_work_iterations, true)) {
        return result;
    }
    const std::size_t dimension_lsqr_iterations = system.size() >= 500
        ? 5000
        : 10 * system.size();
    const int lsqr_iterations = std::max({
        options.maximum_work_iterations,
        500,
        static_cast<int>(dimension_lsqr_iterations)});
    if (system.has_sparse_matrix() &&
        run_backend("lsqr-identity-cpu-v1", lsqr_iterations, true)) {
        return result;
    }
    if (cancelled_during_attempt) return result;

    if (options.external_fallback) {
        if (cancellation_requested(options.cancellation_requested)) {
            result.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
            result.diagnostic = prefix + "cancelled before caller linear fallback";
            return result;
        }
        bool callback_succeeded = false;
        if (run_attempt(
                "caller-linear-fallback-v1", 0, true,
                [&](std::vector<double>& candidate, int&) {
                    candidate.assign(system.size(), 0.0);
                    callback_succeeded = options.external_fallback(candidate);
                    return callback_succeeded;
                })) {
            return result;
        }
        if (!callback_succeeded) {
            result.diagnostic = prefix + "plan=" + result.plan_id + ";family=" +
                result.equation_family + ";caller linear fallback callback failed";
            result.diagnostic_code = VerifiedSolveDiagnosticCode::callback_failure;
            return result;
        }
        if (cancellation_requested(options.cancellation_requested)) {
            result.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
            result.diagnostic = prefix + "cancelled during caller linear fallback gate";
            return result;
        }
    }
    result.diagnostic = prefix + "plan=" + result.plan_id + ";family=" +
        result.equation_family + ";" +
        "all candidates failed or were rejected by the original-matrix gate";
    result.diagnostic_code = VerifiedSolveDiagnosticCode::original_gate_rejected;
    return result;
}

VerifiedComplementaritySolveResult verified_complementarity_solve(
    const ComplementarityIR& problem,
    const ComplementarityTolerance& tolerance) {
    VerifiedComplementaritySolveResult result;
    const std::string prefix =
        std::string(verified_complementarity_solve_service_v1) + ": ";
    try {
        problem.validate();
        if (cancellation_requested(tolerance.cancellation_requested)) {
            result.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
            result.diagnostic = prefix + "cancelled before candidate execution";
            return result;
        }
        const auto solved = solve_complementarity(problem, tolerance);
        result.success = solved.success;
        result.used_fallback = solved.terminal_fallback_used;
        result.solution = solved.solution;
        result.gap = solved.gap;
        result.backend = solved.accepted_backend;
        result.equation_family = problem.structural_class;
        result.plan_id = solved.plan_id;
        result.diagnostic = prefix + solved.reason;
        result.attempts = solved.attempts.size();
        if (!solved.attempts.empty()) {
            const auto& final_attempt = solved.attempts.back();
            result.primal_violation = final_attempt.primal_violation;
            result.dual_violation = final_attempt.dual_violation;
            result.complementarity_violation =
                final_attempt.complementarity_violation;
            result.residual_inf = final_attempt.equation_residual_inf;
        }
        result.diagnostic_code = cancellation_requested(tolerance.cancellation_requested)
            ? VerifiedSolveDiagnosticCode::cancelled
            : (solved.success
                ? VerifiedSolveDiagnosticCode::success
                : VerifiedSolveDiagnosticCode::original_gate_rejected);
        if (result.diagnostic_code == VerifiedSolveDiagnosticCode::cancelled) {
            result.success = false;
            result.diagnostic = prefix + "cancelled during candidate execution";
        }
    } catch (const std::invalid_argument& error) {
        result.diagnostic = prefix + error.what();
        result.diagnostic_code = VerifiedSolveDiagnosticCode::invalid_contract;
    } catch (const std::exception& error) {
        result.diagnostic = prefix + error.what();
        result.diagnostic_code = VerifiedSolveDiagnosticCode::numerical_failure;
    }
    return result;
}

VerifiedBlockGraphSolveResult verified_block_graph_solve(
    const VerifiedBlockGraphProblem& problem,
    const VerifiedBlockGraphSolveOptions& options) {
    VerifiedBlockGraphSolveResult result;
    const auto fail = [&](VerifiedSolveDiagnosticCode code, std::string message) {
        result.diagnostic_code = code;
        result.diagnostic = std::string(verified_block_graph_solve_service_v1) + ": " + message;
        return result;
    };
    if (problem.nodes.empty() || !std::isfinite(problem.end_time) ||
        !std::isfinite(problem.base_step) || problem.end_time < 0.0 ||
        problem.base_step <= 0.0 || !std::isfinite(options.absolute_tolerance) ||
        !std::isfinite(options.relative_tolerance) || options.absolute_tolerance <= 0.0 ||
        options.relative_tolerance < 0.0 || options.maximum_fixed_point_iterations <= 0) {
        return fail(VerifiedSolveDiagnosticCode::invalid_contract, "invalid time or empty graph");
    }
    if (cancellation_requested(options.cancellation_requested)) {
        return fail(VerifiedSolveDiagnosticCode::cancelled, "cancelled before graph execution");
    }
    result.backend = "deterministic-multirate-block-runtime-v2";
    result.equation_family = "scalar-multiphysics-block-graph";
    result.plan_id = "block-graph-scc-fixed-point-zero-order-hold-local-fallback-v2";
    const double final_tick_ratio = problem.end_time / problem.base_step;
    const auto final_tick = static_cast<std::size_t>(std::llround(final_tick_ratio));
    if (std::abs(final_tick_ratio - static_cast<double>(final_tick)) > 1.0e-9) {
        return fail(
            VerifiedSolveDiagnosticCode::invalid_contract,
            "end time must be an integer base-step multiple");
    }

    std::vector<std::size_t> input_counts(problem.nodes.size());
    std::vector<std::size_t> periods(problem.nodes.size());
    std::vector<std::size_t> offsets(problem.nodes.size());
    std::vector<std::vector<std::size_t>> successors(problem.nodes.size());
    std::vector<std::unordered_set<std::size_t>> driven_ports(problem.nodes.size());
    const auto no_source = std::pair{
        std::numeric_limits<std::size_t>::max(),
        std::numeric_limits<std::size_t>::max()};
    std::vector<std::vector<std::pair<std::size_t, std::size_t>>> sources;
    sources.reserve(problem.nodes.size());
    result.output_offsets.reserve(problem.nodes.size() + 1);
    result.output_offsets.push_back(0);
    for (std::size_t index = 0; index < problem.nodes.size(); ++index) {
        const auto& node = problem.nodes[index];
        const bool known_kind = node.kind == VerifiedBlockNodeKind::constant ||
            node.kind == VerifiedBlockNodeKind::gain ||
            node.kind == VerifiedBlockNodeKind::sum ||
            node.kind == VerifiedBlockNodeKind::unit_delay ||
            node.kind == VerifiedBlockNodeKind::switch_gt ||
            node.kind == VerifiedBlockNodeKind::switch_ge ||
            node.kind == VerifiedBlockNodeKind::switch_ne_zero ||
            node.kind == VerifiedBlockNodeKind::callback;
        if (!std::isfinite(node.sample_time) || !std::isfinite(node.sample_offset) ||
            !std::isfinite(node.parameter) || !std::isfinite(node.initial_output) ||
            node.sample_time < 0.0 || node.sample_offset < 0.0 ||
            node.output_count == 0 || !finite_values(node.sum_weights) || !known_kind) {
            return fail(VerifiedSolveDiagnosticCode::invalid_contract, "invalid node descriptor");
        }
        const double period_ratio = node.sample_time == 0.0
            ? 1.0 : node.sample_time / problem.base_step;
        const auto period = static_cast<std::size_t>(std::llround(period_ratio));
        const double offset_ratio = node.sample_offset / problem.base_step;
        const auto offset = static_cast<std::size_t>(std::llround(offset_ratio));
        if (period == 0 || std::abs(period_ratio - static_cast<double>(period)) > 1.0e-9 ||
            std::abs(offset_ratio - static_cast<double>(offset)) > 1.0e-9 || offset >= period) {
            return fail(
                VerifiedSolveDiagnosticCode::invalid_contract,
                "sample time and offset must align to the base step");
        }
        if ((node.kind == VerifiedBlockNodeKind::constant && node.input_count != 0) ||
            (node.kind == VerifiedBlockNodeKind::gain && node.input_count != 1) ||
            (node.kind == VerifiedBlockNodeKind::unit_delay && node.input_count != 1) ||
            ((node.kind == VerifiedBlockNodeKind::switch_gt ||
              node.kind == VerifiedBlockNodeKind::switch_ge ||
              node.kind == VerifiedBlockNodeKind::switch_ne_zero) && node.input_count != 3) ||
            (node.kind == VerifiedBlockNodeKind::sum &&
             (node.input_count == 0 || node.sum_weights.size() != node.input_count)) ||
            (node.kind == VerifiedBlockNodeKind::callback && !node.evaluate) ||
            (node.kind != VerifiedBlockNodeKind::callback && node.output_count != 1)) {
            return fail(VerifiedSolveDiagnosticCode::invalid_contract, "invalid node arity");
        }
        input_counts[index] = node.input_count;
        periods[index] = period;
        offsets[index] = offset;
        sources.emplace_back(node.input_count, no_source);
        result.output_offsets.push_back(result.output_offsets.back() + node.output_count);
    }
    for (const auto& connection : problem.connections) {
        if (connection.source_node >= problem.nodes.size() ||
            connection.target_node >= problem.nodes.size() ||
            connection.source_port >= problem.nodes[connection.source_node].output_count ||
            connection.target_port >= input_counts[connection.target_node] ||
            !driven_ports[connection.target_node].insert(connection.target_port).second) {
            return fail(VerifiedSolveDiagnosticCode::invalid_contract, "invalid or duplicate connection");
        }
        sources[connection.target_node][connection.target_port] = {
            connection.source_node, connection.source_port};
        if (problem.nodes[connection.target_node].kind != VerifiedBlockNodeKind::unit_delay) {
            successors[connection.source_node].push_back(connection.target_node);
        }
    }
    for (std::size_t node = 0; node < problem.nodes.size(); ++node) {
        if (driven_ports[node].size() != input_counts[node]) {
            return fail(VerifiedSolveDiagnosticCode::invalid_contract, "node input is unconnected");
        }
    }
    for (auto& node_successors : successors) {
        std::sort(node_successors.begin(), node_successors.end());
        node_successors.erase(
            std::unique(node_successors.begin(), node_successors.end()),
            node_successors.end());
    }

    std::vector<int> discovery(problem.nodes.size(), -1);
    std::vector<int> low(problem.nodes.size());
    std::vector<bool> on_stack(problem.nodes.size());
    std::vector<std::size_t> stack;
    std::vector<std::vector<std::size_t>> components;
    int next_discovery{};
    std::function<void(std::size_t)> visit = [&](std::size_t node) {
        discovery[node] = next_discovery;
        low[node] = next_discovery;
        ++next_discovery;
        stack.push_back(node);
        on_stack[node] = true;
        for (const auto successor : successors[node]) {
            if (discovery[successor] < 0) {
                visit(successor);
                low[node] = std::min(low[node], low[successor]);
            } else if (on_stack[successor]) {
                low[node] = std::min(low[node], discovery[successor]);
            }
        }
        if (low[node] != discovery[node]) return;
        std::vector<std::size_t> component;
        while (true) {
            const auto member = stack.back();
            stack.pop_back();
            on_stack[member] = false;
            component.push_back(member);
            if (member == node) break;
        }
        std::sort(component.begin(), component.end());
        components.push_back(std::move(component));
    };
    for (std::size_t node = 0; node < problem.nodes.size(); ++node) {
        if (discovery[node] < 0) visit(node);
    }

    std::vector<std::size_t> component_of(problem.nodes.size());
    std::vector<bool> cyclic_component(components.size());
    std::vector<std::size_t> component_key(components.size());
    for (std::size_t component = 0; component < components.size(); ++component) {
        component_key[component] = components[component].front();
        for (const auto node : components[component]) component_of[node] = component;
        cyclic_component[component] = components[component].size() > 1;
        if (!cyclic_component[component]) {
            const auto node = components[component].front();
            cyclic_component[component] = std::find(
                successors[node].begin(), successors[node].end(), node) != successors[node].end();
        }
        if (!cyclic_component[component]) continue;
        const auto first = components[component].front();
        for (const auto node : components[component]) {
            if (periods[node] != periods[first] || offsets[node] != offsets[first] ||
                problem.nodes[node].kind == VerifiedBlockNodeKind::unit_delay) {
                return fail(
                    VerifiedSolveDiagnosticCode::invalid_contract,
                    "algebraic feedback component must share one sample period and offset");
            }
        }
    }

    std::vector<std::unordered_set<std::size_t>> component_successors(components.size());
    std::vector<std::size_t> component_indegree(components.size());
    for (std::size_t source = 0; source < successors.size(); ++source) {
        for (const auto target : successors[source]) {
            const auto source_component = component_of[source];
            const auto target_component = component_of[target];
            if (source_component == target_component ||
                !component_successors[source_component].insert(target_component).second) continue;
            ++component_indegree[target_component];
        }
    }
    const auto compare_components = [&](std::size_t left, std::size_t right) {
        return component_key[left] > component_key[right];
    };
    std::priority_queue<
        std::size_t, std::vector<std::size_t>, decltype(compare_components)>
        ready(compare_components);
    for (std::size_t component = 0; component < components.size(); ++component) {
        if (component_indegree[component] == 0) ready.push(component);
    }
    std::vector<std::size_t> component_order;
    while (!ready.empty()) {
        const auto component = ready.top();
        ready.pop();
        component_order.push_back(component);
        result.commit_order.insert(
            result.commit_order.end(),
            components[component].begin(), components[component].end());
        std::vector<std::size_t> ordered_successors(
            component_successors[component].begin(),
            component_successors[component].end());
        std::sort(
            ordered_successors.begin(), ordered_successors.end(),
            [&](std::size_t left, std::size_t right) {
                return component_key[left] < component_key[right];
            });
        for (const auto successor : ordered_successors) {
            if (--component_indegree[successor] == 0) ready.push(successor);
        }
    }
    if (component_order.size() != components.size()) {
        return fail(VerifiedSolveDiagnosticCode::invalid_contract, "invalid component schedule");
    }

    result.outputs.assign(result.output_offsets.back(), 0.0);
    std::vector<double> delay_state(problem.nodes.size());
    std::vector<bool> available(result.outputs.size(), true);
    for (std::size_t node = 0; node < problem.nodes.size(); ++node) {
        const auto& descriptor = problem.nodes[node];
        if (descriptor.kind == VerifiedBlockNodeKind::unit_delay) {
            delay_state[node] = descriptor.initial_output;
        }
        for (std::size_t port = 0; port < descriptor.output_count; ++port) {
            result.outputs[result.output_offsets[node] + port] = descriptor.initial_output;
        }
    }
    const auto magnitude = [](const std::vector<double>& values) {
        double result{};
        for (const double value : values) result = std::max(result, std::abs(value));
        return result;
    };
    VerifiedSolveDiagnosticCode node_failure_code{VerifiedSolveDiagnosticCode::numerical_failure};
    std::string node_failure_message;
    const auto evaluate_node = [&]
        (std::size_t node_index, double time,
         const std::vector<double>& read_outputs,
         const std::vector<bool>& read_available,
         std::vector<double>& outputs) {
        const auto& node = problem.nodes[node_index];
        std::vector<double> inputs(node.input_count);
        if (node.kind != VerifiedBlockNodeKind::unit_delay) {
            for (std::size_t port = 0; port < node.input_count; ++port) {
                const auto [source_node, source_port] = sources[node_index][port];
                const auto packed = result.output_offsets[source_node] + source_port;
                if (!read_available[packed] || !std::isfinite(read_outputs[packed])) {
                    node_failure_code = VerifiedSolveDiagnosticCode::numerical_failure;
                    node_failure_message = "source signal is unavailable";
                    return false;
                }
                inputs[port] = read_outputs[packed];
            }
        }
        outputs.assign(node.output_count, 0.0);
        bool used_node_fallback = false;
        double gate_residual{};
        switch (node.kind) {
            case VerifiedBlockNodeKind::constant:
                outputs[0] = node.parameter;
                break;
            case VerifiedBlockNodeKind::gain:
                outputs[0] = inputs[0] * node.parameter;
                break;
            case VerifiedBlockNodeKind::sum:
                for (std::size_t port = 0; port < inputs.size(); ++port) {
                    outputs[0] += inputs[port] * node.sum_weights[port];
                }
                break;
            case VerifiedBlockNodeKind::unit_delay:
                outputs[0] = delay_state[node_index];
                break;
            case VerifiedBlockNodeKind::switch_gt:
                outputs[0] = inputs[1] > node.parameter ? inputs[0] : inputs[2];
                break;
            case VerifiedBlockNodeKind::switch_ge:
                outputs[0] = inputs[1] >= node.parameter ? inputs[0] : inputs[2];
                break;
            case VerifiedBlockNodeKind::switch_ne_zero:
                outputs[0] = inputs[1] != 0.0 ? inputs[0] : inputs[2];
                break;
            case VerifiedBlockNodeKind::callback: {
                const auto accepted = [&](const BlockGraphEvaluateFunction& callback) {
                    gate_residual = 0.0;
                    if (!callback || !callback(time, inputs, outputs, gate_residual) ||
                        outputs.size() != node.output_count || !finite_values(outputs) ||
                        !std::isfinite(gate_residual) || gate_residual < 0.0) return false;
                    const double threshold = options.absolute_tolerance +
                        options.relative_tolerance * std::max(
                            1.0, std::max(magnitude(inputs), magnitude(outputs)));
                    return gate_residual <= threshold;
                };
                if (!accepted(node.evaluate)) {
                    if (!accepted(node.fallback)) {
                        node_failure_code = VerifiedSolveDiagnosticCode::original_gate_rejected;
                        node_failure_message =
                            "callback and local fallback failed the output gate";
                        return false;
                    }
                    used_node_fallback = true;
                }
                break;
            }
        }
        if (!finite_values(outputs)) {
            node_failure_code = VerifiedSolveDiagnosticCode::original_gate_rejected;
            node_failure_message = "non-finite node output";
            return false;
        }
        result.maximum_original_gate_residual = std::max(
            result.maximum_original_gate_residual, gate_residual);
        result.fallback_count += used_node_fallback ? 1 : 0;
        ++result.node_executions;
        return true;
    };

    for (std::size_t tick = 0; tick <= final_tick; ++tick) {
        if (cancellation_requested(options.cancellation_requested)) {
            return fail(
                VerifiedSolveDiagnosticCode::cancelled,
                "cancelled before next atomic tick");
        }
        auto pending_outputs = result.outputs;
        auto pending_available = available;
        auto pending_delay_state = delay_state;
        const double time = static_cast<double>(tick) * problem.base_step;
        for (const auto component_index : component_order) {
            const auto& component = components[component_index];
            const auto first = component.front();
            if (tick < offsets[first] || (tick - offsets[first]) % periods[first] != 0) continue;
            if (!cyclic_component[component_index]) {
                std::vector<double> outputs;
                if (!evaluate_node(
                        first, time, pending_outputs, pending_available, outputs)) {
                    return fail(node_failure_code, node_failure_message);
                }
                for (std::size_t port = 0; port < outputs.size(); ++port) {
                    const auto packed = result.output_offsets[first] + port;
                    pending_outputs[packed] = outputs[port];
                    pending_available[packed] = true;
                }
                continue;
            }

            auto iteration_outputs = pending_outputs;
            auto iteration_available = pending_available;
            bool converged = false;
            double final_residual{};
            for (int iteration = 0;
                 iteration < options.maximum_fixed_point_iterations; ++iteration) {
                if (cancellation_requested(options.cancellation_requested)) {
                    return fail(
                        VerifiedSolveDiagnosticCode::cancelled,
                        "cancelled during uncommitted algebraic fixed point");
                }
                auto candidate_outputs = iteration_outputs;
                auto candidate_available = iteration_available;
                for (const auto node : component) {
                    std::vector<double> outputs;
                    if (!evaluate_node(
                            node, time, iteration_outputs, iteration_available, outputs)) {
                        return fail(node_failure_code, node_failure_message);
                    }
                    for (std::size_t port = 0; port < outputs.size(); ++port) {
                        const auto packed = result.output_offsets[node] + port;
                        candidate_outputs[packed] = outputs[port];
                        candidate_available[packed] = true;
                    }
                }
                double residual{};
                double scale{1.0};
                for (const auto node : component) {
                    for (std::size_t port = 0;
                         port < problem.nodes[node].output_count; ++port) {
                        const auto packed = result.output_offsets[node] + port;
                        residual = std::max(
                            residual,
                            std::abs(candidate_outputs[packed] - iteration_outputs[packed]));
                        scale = std::max(scale, std::abs(candidate_outputs[packed]));
                    }
                }
                ++result.fixed_point_iterations;
                iteration_outputs = std::move(candidate_outputs);
                iteration_available = std::move(candidate_available);
                if (residual <= options.absolute_tolerance + options.relative_tolerance * scale) {
                    converged = true;
                    final_residual = residual;
                    break;
                }
            }
            if (!converged) {
                return fail(
                    VerifiedSolveDiagnosticCode::iteration_limit,
                    "algebraic feedback fixed point did not converge");
            }
            ++result.fixed_point_components;
            result.maximum_fixed_point_residual = std::max(
                result.maximum_fixed_point_residual, final_residual);
            result.maximum_connection_error = std::max(
                result.maximum_connection_error, final_residual);
            pending_outputs = std::move(iteration_outputs);
            pending_available = std::move(iteration_available);
        }
        for (std::size_t node = 0; node < problem.nodes.size(); ++node) {
            if (problem.nodes[node].kind != VerifiedBlockNodeKind::unit_delay ||
                tick < offsets[node] || (tick - offsets[node]) % periods[node] != 0) continue;
            const auto [source_node, source_port] = sources[node][0];
            const auto packed = result.output_offsets[source_node] + source_port;
            if (!pending_available[packed] || !std::isfinite(pending_outputs[packed])) {
                return fail(VerifiedSolveDiagnosticCode::numerical_failure, "delay input is unavailable");
            }
            pending_delay_state[node] = pending_outputs[packed];
        }
        if (cancellation_requested(options.cancellation_requested)) {
            return fail(
                VerifiedSolveDiagnosticCode::cancelled,
                "cancelled before atomic tick commit");
        }
        result.outputs = std::move(pending_outputs);
        available = std::move(pending_available);
        delay_state = std::move(pending_delay_state);
        ++result.ticks;
        result.final_time = time;
    }
    result.success = true;
    result.used_fallback = result.fallback_count > 0;
    result.diagnostic_code = VerifiedSolveDiagnosticCode::success;
    result.diagnostic = std::string(verified_block_graph_solve_service_v1) +
        ": all ticks committed atomically";
    result.final_time = problem.end_time;
    return result;
}

VerifiedNonlinearSolveResult verified_nonlinear_solve(
    const VerifiedNonlinearSolveProblem& problem,
    const VerifiedNonlinearSolveOptions& options) {
    VerifiedNonlinearSolveResult result;
    const std::string prefix = std::string(verified_nonlinear_solve_service_v1) + ": ";
    if (problem.initial_state.empty() || !finite_values(problem.initial_state) ||
        !problem.residual || !std::isfinite(options.absolute_tolerance) ||
        !std::isfinite(options.relative_tolerance) || options.absolute_tolerance < 0.0 ||
        options.relative_tolerance < 0.0 || options.maximum_iterations <= 0) {
        result.diagnostic = prefix + "invalid nonlinear problem or tolerance contract";
        return result;
    }
    if (cancellation_requested(options.cancellation_requested)) {
        result.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
        result.diagnostic = prefix + "cancelled before candidate execution";
        return result;
    }
    const NonlinearAlgebraicProfile profile{
        .fingerprint = nonlinear_fingerprint(
            problem.initial_state.size(), static_cast<bool>(problem.jacobian)),
        .dimension = problem.initial_state.size(),
        .jacobian_nonzeros = problem.jacobian
            ? problem.initial_state.size() * problem.initial_state.size() : 0,
        .jacobian_available = static_cast<bool>(problem.jacobian),
        .smooth = true,
    };
    const SolvePlan plan = route_nonlinear_algebraic_system(profile);
    result.equation_family = plan.assessment.equation_family;
    result.plan_id = plan.plan_id;
    if (options.external_fallback) {
        result.plan_id += "|caller-nonlinear-fallback-v1";
    }
    bool candidate_rejected = false;
    const auto accept = [&](std::vector<double> candidate,
                            std::string backend,
                            bool fallback) {
        if (cancellation_requested(options.cancellation_requested)) return false;
        std::vector<double> independent_residual;
        const bool gate_evaluated = evaluate_nonlinear_residual(
            problem, candidate, independent_residual);
        if (cancellation_requested(options.cancellation_requested)) return false;
        const double gate_residual = gate_evaluated
            ? norm_inf(independent_residual) : std::numeric_limits<double>::infinity();
        const double threshold = options.absolute_tolerance +
            options.relative_tolerance * std::max(1.0, gate_residual);
        if (!gate_evaluated || gate_residual > threshold) {
            candidate_rejected = true;
            return false;
        }
        result.solution = std::move(candidate);
        result.backend = std::move(backend);
        result.residual_inf = gate_residual;
        result.backward_error = gate_residual;
        result.used_fallback = fallback;
        result.success = true;
        result.diagnostic_code = VerifiedSolveDiagnosticCode::success;
        result.diagnostic = prefix + "plan=" + result.plan_id + ";family=" +
            result.equation_family + ";" +
            "accepted by independent original nonlinear residual gate";
        return true;
    };
    for (const auto& step : plan.steps) {
        if (cancellation_requested(options.cancellation_requested)) {
            result.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
            result.diagnostic = prefix + "plan=" + result.plan_id + ";family=" +
                result.equation_family + ";cancelled between candidate attempts";
            return result;
        }
        std::vector<double> candidate;
        double residual_inf{};
        bool converged = false;
        bool fallback = false;
        if (step.expert_version == "callback-jacobian-damped-newton-v1") {
            converged = damped_newton(problem, true, options, candidate, residual_inf);
        } else if (step.expert_version ==
                   "finite-difference-damped-newton-fallback-v1") {
            fallback = static_cast<bool>(problem.jacobian);
            converged = damped_newton(problem, false, options, candidate, residual_inf);
        }
        if (converged && accept(candidate, step.expert_version, fallback)) return result;
    }
    if (cancellation_requested(options.cancellation_requested)) {
        result.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
        result.diagnostic = prefix + "plan=" + result.plan_id + ";family=" +
            result.equation_family + ";cancelled during candidate execution";
        return result;
    }
    if (options.external_fallback) {
        if (cancellation_requested(options.cancellation_requested)) {
            result.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
            result.diagnostic = prefix + "cancelled before caller nonlinear fallback";
            return result;
        }
        std::vector<double> candidate = problem.initial_state;
        bool callback_succeeded = false;
        try {
            callback_succeeded = options.external_fallback(candidate);
        } catch (...) {
            callback_succeeded = false;
        }
        if (cancellation_requested(options.cancellation_requested)) {
            result.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
            result.diagnostic = prefix + "cancelled after caller nonlinear fallback";
            return result;
        }
        if (!callback_succeeded) {
            result.diagnostic = prefix + "plan=" + result.plan_id + ";family=" +
                result.equation_family + ";caller nonlinear fallback callback failed";
            result.diagnostic_code = VerifiedSolveDiagnosticCode::callback_failure;
            return result;
        }
        if (accept(
                std::move(candidate), "caller-nonlinear-fallback-v1", true)) {
            return result;
        }
        if (cancellation_requested(options.cancellation_requested)) {
            result.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
            result.diagnostic = prefix + "cancelled during caller nonlinear fallback gate";
            return result;
        }
    }
    result.diagnostic = prefix + "plan=" + result.plan_id + ";family=" +
        result.equation_family + ";" +
        "nonlinear candidates failed or were rejected by the original residual gate";
    result.diagnostic_code = candidate_rejected
        ? VerifiedSolveDiagnosticCode::original_gate_rejected
        : VerifiedSolveDiagnosticCode::numerical_failure;
    return result;
}

VerifiedOdeSolveResult verified_ode_solve(
    const VerifiedOdeSolveProblem& problem,
    const VerifiedOdeSolveOptions& options) {
    VerifiedOdeSolveResult result;
    const std::string prefix = std::string(verified_ode_solve_service_v1) + ": ";
    if (problem.initial_state.empty() || !finite_values(problem.initial_state) ||
        !problem.right_hand_side || !std::isfinite(options.start_time) ||
        !std::isfinite(options.end_time) || !std::isfinite(options.maximum_step) ||
        !std::isfinite(options.absolute_tolerance) ||
        !std::isfinite(options.relative_tolerance) ||
        options.end_time <= options.start_time || options.maximum_step <= 0.0 ||
        options.absolute_tolerance < 0.0 || options.relative_tolerance < 0.0 ||
        options.maximum_steps <= 0) {
        result.diagnostic = prefix + "invalid explicit ODE problem or tolerance contract";
        return result;
    }
    if (options.external_step_fallback && !problem.events.empty()) {
        result.diagnostic = prefix +
            "caller ODE dense stepper does not support event problems";
        return result;
    }
    if (cancellation_requested(options.cancellation_requested)) {
        result.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
        result.diagnostic = prefix + "cancelled before integration";
        return result;
    }
    for (const auto& event : problem.events) {
        if ((event.direction < -1 || event.direction > 1) || !event.guard || !event.reset) {
            result.diagnostic = prefix + "invalid event callback, direction, or reset contract";
            return result;
        }
        double initial_guard{};
        if (!evaluate_event_guard(
                event, options.start_time, problem.initial_state, initial_guard)) {
            result.diagnostic_code = VerifiedSolveDiagnosticCode::callback_failure;
            result.diagnostic = prefix + "initial event guard evaluation failed";
            return result;
        }
    }
    const ExplicitOdeProfile profile{
        .fingerprint = ode_fingerprint(problem.initial_state.size()),
        .state_dimension = problem.initial_state.size(),
        .smooth = true,
        .events = !problem.events.empty(),
    };
    const SolvePlan plan = route_explicit_ode(profile);
    result.equation_family = plan.assessment.equation_family;
    result.plan_id = plan.plan_id;
    if (options.external_step_fallback) {
        result.plan_id += "|caller-ode-dense-stepper-fallback-v1";
    }
    OdeIntegration integration;
    for (const auto& step : plan.steps) {
        if (cancellation_requested(options.cancellation_requested)) break;
        if (step.expert_version == "adaptive-rk4-step-doubling-v1") {
            integration = integrate_rk4(problem, options);
        } else if (step.expert_version == "adaptive-heun-euler-fallback-v1") {
            result.used_fallback = true;
            integration = integrate_heun(problem, options);
        } else if (step.expert_version == "adaptive-rk4-event-localization-v1") {
            integration = integrate_event_rk4(problem, options);
        } else if (step.expert_version == "adaptive-rk4-event-localization-retry-v1") {
            result.used_fallback = true;
            auto retry = options;
            retry.maximum_step *= 0.5;
            integration = integrate_event_rk4(problem, retry);
        }
        if (integration.success) {
            result.backend = step.expert_version;
            break;
        }
    }
    if (!integration.success && options.external_step_fallback &&
        integration.diagnostic_code != VerifiedSolveDiagnosticCode::cancelled &&
        !cancellation_requested(options.cancellation_requested)) {
        integration = integrate_external_dense_stepper(problem, options);
        result.used_fallback = true;
        if (integration.success) {
            result.backend = "caller-ode-dense-stepper-fallback-v1";
        }
    }
    if (integration.diagnostic_code == VerifiedSolveDiagnosticCode::cancelled ||
        cancellation_requested(options.cancellation_requested)) {
        result.solution = std::move(integration.state);
        result.final_time = integration.time;
        result.maximum_scaled_local_error = integration.maximum_scaled_error;
        result.accepted_steps = integration.accepted_steps;
        result.rejected_steps = integration.rejected_steps;
        result.event_count = integration.event_count;
        result.last_event_time = integration.last_event_time;
        result.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
        result.diagnostic = prefix + "plan=" + result.plan_id + ";family=" +
            result.equation_family + ";cancelled before next integration commit";
        return result;
    }
    std::vector<double> final_rhs;
    bool guards_finite = true;
    for (const auto& event : problem.events) {
        double guard{};
        guards_finite = guards_finite && evaluate_event_guard(
            event, options.end_time, integration.state, guard);
    }
    const bool gate_passed = integration.success &&
        integration.maximum_scaled_error <= 1.0 &&
        evaluate_ode_rhs(problem, options.end_time, integration.state, final_rhs) &&
        guards_finite;
    result.solution = std::move(integration.state);
    result.final_time = integration.time;
    result.maximum_scaled_local_error = integration.maximum_scaled_error;
    result.accepted_steps = integration.accepted_steps;
    result.rejected_steps = integration.rejected_steps;
    result.event_count = integration.event_count;
    result.last_event_time = integration.last_event_time;
    result.success = gate_passed;
    result.diagnostic_code = gate_passed
        ? VerifiedSolveDiagnosticCode::success
        : (integration.success
            ? VerifiedSolveDiagnosticCode::original_gate_rejected
            : integration.diagnostic_code);
    result.diagnostic = prefix + "plan=" + result.plan_id + ";family=" +
        result.equation_family + ";" + (gate_passed
            ? (problem.events.empty()
                ? "accepted by embedded local-error and original RHS finite gate"
                : "accepted by local-error, bracketed event, atomic reset, RHS, and guard gates")
            : "all ODE candidates failed or were rejected by the local-error/RHS/event gate");
    return result;
}

VerifiedHybridSolveResult verified_hybrid_solve(
    const VerifiedHybridSolveProblem& problem,
    const VerifiedOdeSolveOptions& options) {
    VerifiedHybridSolveResult result;
    const std::string prefix = std::string(verified_hybrid_solve_service_v1) + ": ";
    if (problem.initial_state.empty() || !finite_values(problem.initial_state) ||
        problem.modes.empty() || problem.initial_mode >= problem.modes.size()) {
        result.diagnostic = prefix + "invalid hybrid state or initial mode contract";
        return result;
    }
    for (const auto& mode : problem.modes) {
        if (!mode.right_hand_side) {
            result.diagnostic = prefix + "hybrid mode is missing a right-hand-side callback";
            return result;
        }
    }
    for (const auto& transition : problem.transitions) {
        if (transition.source_mode >= problem.modes.size() ||
            transition.target_mode >= problem.modes.size() ||
            transition.direction < -1 || transition.direction > 1 ||
            !transition.guard || (!transition.reset && !transition.stable_reset) ||
            (static_cast<bool>(transition.stable_reset) !=
             !transition.write_mask.empty()) ||
            (!transition.write_mask.empty() &&
             (transition.write_mask.size() != problem.initial_state.size() ||
              std::any_of(
                  transition.write_mask.begin(),
                  transition.write_mask.end(),
                  [](std::uint8_t value) { return value > 1; })))) {
            result.diagnostic = prefix + "invalid hybrid transition contract";
            return result;
        }
    }
    const std::size_t physical_dimension = problem.initial_state.size();
    const auto unpack_mode = [&](const std::vector<double>& augmented,
                                 std::size_t& mode) {
        if (augmented.size() != physical_dimension + 1 || !finite_values(augmented)) {
            return false;
        }
        const double encoded = augmented.back();
        const double rounded = std::round(encoded);
        if (std::abs(encoded - rounded) > 1.0e-12 || rounded < 0.0 ||
            rounded >= static_cast<double>(problem.modes.size())) return false;
        mode = static_cast<std::size_t>(rounded);
        return true;
    };
    const auto physical_state = [physical_dimension](
        const std::vector<double>& augmented) {
        return std::vector<double>(augmented.begin(),
                                   augmented.begin() +
                                       static_cast<std::ptrdiff_t>(physical_dimension));
    };

    VerifiedOdeSolveProblem ode;
    ode.initial_state = problem.initial_state;
    ode.initial_state.push_back(static_cast<double>(problem.initial_mode));
    ode.right_hand_side = [&](double time,
                              const std::vector<double>& augmented,
                              std::vector<double>& derivative) {
        std::size_t mode{};
        if (!unpack_mode(augmented, mode)) return false;
        std::vector<double> physical_derivative;
        if (!problem.modes[mode].right_hand_side(
                time, physical_state(augmented), physical_derivative) ||
            physical_derivative.size() != physical_dimension ||
            !finite_values(physical_derivative)) return false;
        derivative = std::move(physical_derivative);
        derivative.push_back(0.0);
        return true;
    };
    ode.events.reserve(problem.transitions.size());
    for (std::size_t index = 0; index < problem.transitions.size(); ++index) {
        const auto& transition = problem.transitions[index];
        VerifiedOdeEvent event;
        event.direction = transition.direction;
        event.priority = transition.priority;
        event.guard = [&, index](double time,
                                 const std::vector<double>& augmented,
                                 double& guard) {
            std::size_t mode{};
            if (!unpack_mode(augmented, mode)) return false;
            const auto& source = problem.transitions[index];
            if (mode != source.source_mode) {
                guard = source.direction > 0 ? -1.0 : 1.0;
                return true;
            }
            return source.guard(time, physical_state(augmented), guard) &&
                std::isfinite(guard);
        };
        event.reset = [&, index](double time,
                                 const std::vector<double>& augmented,
                                 std::vector<double>& reset_state) {
            std::size_t mode{};
            if (!unpack_mode(augmented, mode)) return false;
            const auto& source = problem.transitions[index];
            if (mode != source.source_mode) return false;
            std::vector<double> physical_reset;
            if (!source.reset(time, physical_state(augmented), physical_reset) ||
                physical_reset.size() != physical_dimension ||
                !finite_values(physical_reset)) return false;
            reset_state = std::move(physical_reset);
            reset_state.push_back(static_cast<double>(source.target_mode));
            return true;
        };
        ode.events.push_back(std::move(event));
    }
    ode.event_cluster_reset = [&](double time,
                                  const std::vector<std::size_t>& initial_events,
                                  const std::vector<double>& pre_state,
                                  std::vector<double>& post_state,
                                  std::size_t& committed_events,
                                  VerifiedSolveDiagnosticCode& failure_code) {
        constexpr std::size_t maximum_superdense_steps = 64;
        constexpr double activation_tolerance = 1.0e-10;
        if (initial_events.empty()) return false;
        std::vector<double> transaction = pre_state;
        const std::vector<double> stable_pre = physical_state(pre_state);
        const auto apply_transition = [&](std::size_t index) {
            if (index >= problem.transitions.size()) {
                failure_code = VerifiedSolveDiagnosticCode::numerical_failure;
                return false;
            }
            std::size_t mode{};
            if (!unpack_mode(transaction, mode) ||
                mode != problem.transitions[index].source_mode) {
                failure_code = VerifiedSolveDiagnosticCode::numerical_failure;
                return false;
            }
            std::vector<double> physical_reset;
            const auto& transition = problem.transitions[index];
            if (!transition.reset(
                    time, physical_state(transaction), physical_reset) ||
                physical_reset.size() != physical_dimension ||
                !finite_values(physical_reset)) {
                failure_code = VerifiedSolveDiagnosticCode::numerical_failure;
                return false;
            }
            transaction = std::move(physical_reset);
            transaction.push_back(static_cast<double>(transition.target_mode));
            ++committed_events;
            return true;
        };
        const auto apply_batch = [&](const std::vector<std::size_t>& indices) {
            if (indices.empty()) return false;
            std::size_t mode{};
            if (!unpack_mode(transaction, mode)) return false;
            const auto current = physical_state(transaction);
            std::vector<double> merged = current;
            std::vector<std::uint8_t> written(physical_dimension, 0);
            std::size_t target_mode = problem.modes.size();
            for (const std::size_t index : indices) {
                if (index >= problem.transitions.size()) return false;
                const auto& transition = problem.transitions[index];
                if (transition.source_mode != mode || !transition.stable_reset ||
                    transition.write_mask.size() != physical_dimension) return false;
                if (target_mode == problem.modes.size()) {
                    target_mode = transition.target_mode;
                } else if (target_mode != transition.target_mode) {
                    failure_code = VerifiedSolveDiagnosticCode::event_reset_conflict;
                    return false;
                }
                std::vector<double> proposed;
                if (!transition.stable_reset(
                        time, stable_pre, current, proposed) ||
                    proposed.size() != physical_dimension || !finite_values(proposed)) {
                    failure_code = VerifiedSolveDiagnosticCode::callback_failure;
                    return false;
                }
                for (std::size_t state_index = 0;
                     state_index < physical_dimension;
                     ++state_index) {
                    if (transition.write_mask[state_index] == 0) continue;
                    if (written[state_index] != 0) {
                        failure_code = VerifiedSolveDiagnosticCode::event_reset_conflict;
                        return false;
                    }
                    written[state_index] = 1;
                    merged[state_index] = proposed[state_index];
                }
            }
            transaction = std::move(merged);
            transaction.push_back(static_cast<double>(target_mode));
            committed_events += indices.size();
            return true;
        };
        const auto apply_microstep = [&](const std::vector<std::size_t>& indices) {
            const bool stable_batch = std::all_of(
                indices.begin(), indices.end(), [&](const std::size_t index) {
                    return index < problem.transitions.size() &&
                        static_cast<bool>(problem.transitions[index].stable_reset);
                });
            if (stable_batch) return apply_batch(indices);
            for (const std::size_t index : indices) {
                if (!apply_transition(index)) return false;
            }
            return true;
        };
        if (!apply_microstep(initial_events)) return false;
        for (std::size_t microstep = 1; microstep < maximum_superdense_steps; ++microstep) {
            if (cancellation_requested(options.cancellation_requested)) {
                failure_code = VerifiedSolveDiagnosticCode::cancelled;
                return false;
            }
            std::size_t mode{};
            if (!unpack_mode(transaction, mode)) return false;
            std::vector<std::size_t> enabled;
            for (std::size_t index = 0; index < problem.transitions.size(); ++index) {
                const auto& transition = problem.transitions[index];
                if (transition.source_mode != mode) continue;
                double guard{};
                if (!transition.guard(time, physical_state(transaction), guard) ||
                    !std::isfinite(guard)) return false;
                const bool active = transition.direction > 0
                    ? guard >= -activation_tolerance
                    : (transition.direction < 0
                        ? guard <= activation_tolerance
                        : std::abs(guard) <= activation_tolerance);
                if (active) enabled.push_back(index);
            }
            if (enabled.empty()) {
                post_state = std::move(transaction);
                return true;
            }
            std::sort(enabled.begin(), enabled.end(), [&](const auto left, const auto right) {
                const auto& left_transition = problem.transitions[left];
                const auto& right_transition = problem.transitions[right];
                return left_transition.priority != right_transition.priority
                    ? left_transition.priority > right_transition.priority
                    : left < right;
            });
            if (!apply_microstep(enabled)) return false;
        }
        failure_code = VerifiedSolveDiagnosticCode::numerical_failure;
        return false;
    };

    const VerifiedOdeSolveResult ode_result = verified_ode_solve(ode, options);
    static_cast<VerifiedOdeSolveResult&>(result) = ode_result;
    result.equation_family = "explicit-hybrid-multimode";
    if (!result.plan_id.empty()) result.plan_id += "-hybrid-mode-v1";
    result.diagnostic = prefix + ode_result.diagnostic;
    if (!result.solution.empty()) {
        std::size_t final_mode{};
        if (!unpack_mode(result.solution, final_mode)) {
            result.success = false;
            result.diagnostic_code = VerifiedSolveDiagnosticCode::numerical_failure;
            result.diagnostic = prefix + "invalid final hybrid mode encoding";
            return result;
        }
        result.final_mode = final_mode;
        result.solution.pop_back();
    }
    return result;
}

VerifiedDaeSolveResult verified_dae_solve(
    const VerifiedDaeSolveProblem& problem,
    const VerifiedDaeSolveOptions& options) {
    struct DaeStepOutcome {
        bool success{};
        bool used_fallback{};
        bool used_external_fallback{};
        std::vector<double> state;
        std::vector<double> derivative;
        double residual_inf{};
        std::string diagnostic;
        VerifiedSolveDiagnosticCode diagnostic_code{
            VerifiedSolveDiagnosticCode::numerical_failure};
    };
    enum class DaeStructureKind {
        ordinary,
        hessenberg_index_two,
        unsupported_high_index,
    };
    struct DaeStructureProbe {
        DaeStructureKind kind{DaeStructureKind::ordinary};
        double hidden_rank_margin{};
        double hidden_residual_inf{};
    };
    VerifiedDaeSolveResult result;
    const std::string prefix = std::string(verified_dae_solve_service_v1) + ": ";
    bool enforce_index_two{};
    bool external_stepper_used{};
    const auto gate_threshold = [&](const std::vector<double>& candidate_state,
                                    const std::vector<double>& candidate_derivative) {
        return options.absolute_tolerance + options.relative_tolerance * std::max(
            {1.0, norm_inf(candidate_state), norm_inf(candidate_derivative)});
    };
    const auto evaluate_original_residual = [&problem, &gate_threshold](
        double time,
        const std::vector<double>& state,
        const std::vector<double>& derivative,
        double& residual_inf) {
        std::vector<double> residual;
        if (!problem.residual(time, state, derivative, residual) ||
            residual.size() != state.size() || !finite_values(residual)) return false;
        residual_inf = norm_inf(residual);
        return residual_inf <= gate_threshold(state, derivative);
    };
    const auto probe_structure = [&](double time,
                                     const std::vector<double>& state,
                                     const std::vector<double>& derivative) {
        DaeStructureProbe probe;
        if (!problem.jacobian) return probe;
        const std::size_t dimension = state.size();
        std::vector<std::vector<double>> state_jacobian;
        std::vector<std::vector<double>> combined_jacobian;
        if (!problem.jacobian(
                time, state, derivative, 0.0, state_jacobian) ||
            !problem.jacobian(
                time, state, derivative, 1.0, combined_jacobian) ||
            state_jacobian.size() != dimension ||
            combined_jacobian.size() != dimension) {
            return probe;
        }
        for (std::size_t row = 0; row < dimension; ++row) {
            if (state_jacobian[row].size() != dimension ||
                combined_jacobian[row].size() != dimension ||
                !finite_values(state_jacobian[row]) ||
                !finite_values(combined_jacobian[row])) {
                return probe;
            }
        }
        std::vector<std::vector<double>> derivative_jacobian(
            dimension, std::vector<double>(dimension));
        double derivative_scale{};
        for (std::size_t row = 0; row < dimension; ++row) {
            for (std::size_t column = 0; column < dimension; ++column) {
                derivative_jacobian[row][column] =
                    combined_jacobian[row][column] - state_jacobian[row][column];
                derivative_scale = std::max(
                    derivative_scale, std::abs(derivative_jacobian[row][column]));
            }
        }
        const double row_tolerance = 1.0e-10 * std::max(1.0, derivative_scale);
        std::vector<std::size_t> differential_columns;
        std::vector<std::size_t> algebraic_columns;
        std::vector<std::size_t> dynamic_rows;
        std::vector<std::size_t> algebraic_rows;
        for (std::size_t column = 0; column < dimension; ++column) {
            (problem.differential_mask[column]
                ? differential_columns : algebraic_columns).push_back(column);
        }
        for (std::size_t row = 0; row < dimension; ++row) {
            double row_norm{};
            for (const double value : derivative_jacobian[row]) {
                row_norm = std::max(row_norm, std::abs(value));
            }
            (row_norm > row_tolerance ? dynamic_rows : algebraic_rows).push_back(row);
        }
        if (dynamic_rows.size() != differential_columns.size() ||
            algebraic_rows.size() != algebraic_columns.size() ||
            algebraic_rows.empty()) return probe;

        std::vector<std::vector<double>> differential_block(
            dynamic_rows.size(), std::vector<double>(differential_columns.size()));
        std::vector<std::vector<double>> algebraic_block(
            algebraic_rows.size(), std::vector<double>(algebraic_columns.size()));
        for (std::size_t row = 0; row < dynamic_rows.size(); ++row) {
            for (std::size_t column = 0; column < differential_columns.size(); ++column) {
                differential_block[row][column] =
                    derivative_jacobian[dynamic_rows[row]][differential_columns[column]];
            }
        }
        for (std::size_t row = 0; row < algebraic_rows.size(); ++row) {
            for (std::size_t column = 0; column < algebraic_columns.size(); ++column) {
                algebraic_block[row][column] =
                    state_jacobian[algebraic_rows[row]][algebraic_columns[column]];
            }
        }
        if (scaled_pivot_margin(differential_block) <= 1.0e-10) {
            return probe;
        }
        if (scaled_pivot_margin(algebraic_block) > 1.0e-10) return probe;

        std::vector<std::vector<double>> hidden(
            algebraic_rows.size(), std::vector<double>(algebraic_columns.size()));
        for (std::size_t algebraic = 0; algebraic < algebraic_columns.size(); ++algebraic) {
            LinearSystem linear;
            linear.matrix = differential_block;
            linear.right_hand_side.resize(dynamic_rows.size());
            for (std::size_t row = 0; row < dynamic_rows.size(); ++row) {
                linear.right_hand_side[row] =
                    state_jacobian[dynamic_rows[row]][algebraic_columns[algebraic]];
            }
            std::vector<double> response;
            if (!dense_direct_solve(linear, response)) {
                probe.kind = DaeStructureKind::unsupported_high_index;
                return probe;
            }
            for (std::size_t constraint = 0; constraint < algebraic_rows.size(); ++constraint) {
                for (std::size_t differential = 0;
                     differential < differential_columns.size(); ++differential) {
                    hidden[constraint][algebraic] -=
                        state_jacobian[algebraic_rows[constraint]]
                                      [differential_columns[differential]] *
                        response[differential];
                }
            }
        }
        probe.hidden_rank_margin = scaled_pivot_margin(hidden);
        if (probe.hidden_rank_margin <= 1.0e-10) {
            probe.kind = DaeStructureKind::unsupported_high_index;
            return probe;
        }
        std::vector<double> base_residual;
        std::vector<double> future_residual;
        const double time_step = std::sqrt(std::numeric_limits<double>::epsilon()) *
            std::max(1.0, std::abs(time));
        if (!problem.residual(time, state, derivative, base_residual) ||
            !problem.residual(
                time + time_step, state, derivative, future_residual) ||
            base_residual.size() != dimension || future_residual.size() != dimension ||
            !finite_values(base_residual) || !finite_values(future_residual)) {
            probe.kind = DaeStructureKind::unsupported_high_index;
            return probe;
        }
        for (const auto row : algebraic_rows) {
            double hidden_residual =
                (future_residual[row] - base_residual[row]) / time_step;
            for (std::size_t column = 0; column < dimension; ++column) {
                hidden_residual += state_jacobian[row][column] * derivative[column];
            }
            probe.hidden_residual_inf = std::max(
                probe.hidden_residual_inf, std::abs(hidden_residual));
        }
        probe.kind = DaeStructureKind::hessenberg_index_two;
        return probe;
    };
    const auto solve_step = [&](double from_time,
                                const std::vector<double>& previous_state,
                                const std::vector<double>& previous_derivative,
                                double to_time) {
        DaeStepOutcome outcome;
        const double step = to_time - from_time;
        if (!(step > 0.0) || previous_state.size() != previous_derivative.size()) {
            outcome.diagnostic = "invalid implicit DAE substep";
            return outcome;
        }
        std::vector<double> predictor(previous_state.size());
        for (std::size_t index = 0; index < predictor.size(); ++index) {
            predictor[index] = problem.differential_mask[index]
                ? previous_state[index] + step * previous_derivative[index]
                : previous_state[index];
        }
        VerifiedNonlinearSolveProblem nonlinear;
        nonlinear.initial_state = std::move(predictor);
        nonlinear.residual = [&](const std::vector<double>& candidate,
                                 std::vector<double>& candidate_residual) {
            std::vector<double> candidate_derivative(candidate.size());
            for (std::size_t index = 0; index < candidate.size(); ++index) {
                candidate_derivative[index] = problem.differential_mask[index]
                    ? (candidate[index] - previous_state[index]) / step
                    : 0.0;
            }
            return problem.residual(
                to_time, candidate, candidate_derivative, candidate_residual);
        };
        if (problem.jacobian) {
            nonlinear.jacobian = [&](const std::vector<double>& candidate,
                                     std::vector<std::vector<double>>& jacobian) {
                std::vector<double> candidate_derivative(candidate.size());
                for (std::size_t index = 0; index < candidate.size(); ++index) {
                    candidate_derivative[index] = problem.differential_mask[index]
                        ? (candidate[index] - previous_state[index]) / step
                        : 0.0;
                }
                return problem.jacobian(
                    to_time, candidate, candidate_derivative, 1.0 / step, jacobian);
            };
        }
        const auto nonlinear_result = verified_nonlinear_solve(
            nonlinear,
            {.absolute_tolerance = options.absolute_tolerance,
             .relative_tolerance = options.relative_tolerance,
             .maximum_iterations = options.maximum_newton_iterations,
             .cancellation_requested = options.cancellation_requested});
        const auto run_external_stepper = [&] {
            if (!options.external_step_fallback) return false;
            if (cancellation_requested(options.cancellation_requested)) {
                outcome.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
                outcome.diagnostic = "cancelled before caller DAE stepper";
                return false;
            }
            outcome.state = previous_state;
            outcome.derivative = previous_derivative;
            bool callback_succeeded = false;
            try {
                callback_succeeded = options.external_step_fallback(
                    from_time,
                    previous_state,
                    previous_derivative,
                    to_time,
                    outcome.state,
                    outcome.derivative);
            } catch (...) {
                callback_succeeded = false;
            }
            if (cancellation_requested(options.cancellation_requested)) {
                outcome.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
                outcome.diagnostic = "cancelled after caller DAE stepper";
                return false;
            }
            if (!callback_succeeded) {
                outcome.diagnostic_code = VerifiedSolveDiagnosticCode::callback_failure;
                outcome.diagnostic = "caller DAE stepper callback failed";
                return false;
            }
            if (outcome.state.size() != previous_state.size() ||
                outcome.derivative.size() != previous_state.size() ||
                !finite_values(outcome.state) || !finite_values(outcome.derivative)) {
                outcome.diagnostic_code =
                    VerifiedSolveDiagnosticCode::original_gate_rejected;
                outcome.diagnostic = "caller DAE stepper returned an invalid candidate";
                return false;
            }
            const double kinematic_threshold = gate_threshold(
                outcome.state, outcome.derivative);
            for (std::size_t index = 0; index < outcome.state.size(); ++index) {
                const double expected_derivative = problem.differential_mask[index]
                    ? (outcome.state[index] - previous_state[index]) / step
                    : 0.0;
                if (std::abs(outcome.derivative[index] - expected_derivative) >
                    kinematic_threshold) {
                    outcome.diagnostic_code =
                        VerifiedSolveDiagnosticCode::original_gate_rejected;
                    outcome.diagnostic =
                        "caller DAE stepper rejected by backward-Euler kinematic gate";
                    return false;
                }
            }
            outcome.used_fallback = true;
            outcome.used_external_fallback = true;
            return true;
        };
        outcome.used_fallback = nonlinear_result.used_fallback;
        if (nonlinear_result.success) {
            outcome.state = nonlinear_result.solution;
            outcome.derivative.resize(outcome.state.size());
            for (std::size_t index = 0; index < outcome.state.size(); ++index) {
                outcome.derivative[index] = problem.differential_mask[index]
                    ? (outcome.state[index] - previous_state[index]) / step
                    : 0.0;
            }
        } else if (!run_external_stepper()) {
            if (outcome.diagnostic.empty()) {
                outcome.diagnostic = "implicit step failed: " + nonlinear_result.diagnostic;
            }
            return outcome;
        }
        if (!evaluate_original_residual(
                to_time, outcome.state, outcome.derivative, outcome.residual_inf)) {
            if (!outcome.used_external_fallback && options.external_step_fallback) {
                if (!run_external_stepper()) return outcome;
                if (evaluate_original_residual(
                        to_time, outcome.state, outcome.derivative,
                        outcome.residual_inf)) {
                    outcome.diagnostic_code = VerifiedSolveDiagnosticCode::success;
                } else {
                    outcome.diagnostic_code =
                        VerifiedSolveDiagnosticCode::original_gate_rejected;
                    outcome.diagnostic =
                        "caller DAE stepper rejected by original DAE residual gate";
                    return outcome;
                }
            } else {
                outcome.diagnostic_code =
                    VerifiedSolveDiagnosticCode::original_gate_rejected;
                outcome.diagnostic =
                    "implicit candidate rejected by original DAE residual gate";
                return outcome;
            }
        }
        if (enforce_index_two) {
            const auto structure = probe_structure(
                to_time, outcome.state, outcome.derivative);
            if (structure.kind != DaeStructureKind::hessenberg_index_two ||
                structure.hidden_residual_inf > gate_threshold(
                    outcome.state, outcome.derivative)) {
                outcome.diagnostic =
                    "implicit candidate rejected by index-2 hidden constraint or rank gate";
                outcome.diagnostic_code =
                    VerifiedSolveDiagnosticCode::original_gate_rejected;
                return outcome;
            }
            ++result.hidden_rank_checks;
            result.minimum_hidden_rank_margin = std::min(
                result.minimum_hidden_rank_margin, structure.hidden_rank_margin);
            result.maximum_hidden_residual_inf = std::max(
                result.maximum_hidden_residual_inf, structure.hidden_residual_inf);
        }
        outcome.success = true;
        outcome.diagnostic_code = VerifiedSolveDiagnosticCode::success;
        return outcome;
    };
    const auto evaluate_guard = [](const VerifiedDaeEvent& event,
                                   double time,
                                   const std::vector<double>& state,
                                   const std::vector<double>& derivative,
                                   double& guard) {
        return event.guard && event.guard(time, state, derivative, guard) &&
            std::isfinite(guard);
    };

    if (problem.initial_state.empty() ||
        problem.differential_mask.size() != problem.initial_state.size() ||
        problem.initial_derivative.size() != problem.initial_state.size() ||
        !finite_values(problem.initial_state) || !finite_values(problem.initial_derivative) ||
        !problem.residual || !std::isfinite(options.start_time) ||
        !std::isfinite(options.end_time) || !std::isfinite(options.maximum_step) ||
        !std::isfinite(options.absolute_tolerance) ||
        !std::isfinite(options.relative_tolerance) ||
        options.end_time <= options.start_time || options.maximum_step <= 0.0 ||
        options.absolute_tolerance < 0.0 || options.relative_tolerance < 0.0 ||
        options.maximum_iterations <= 0 || options.maximum_newton_iterations <= 0) {
        result.diagnostic = prefix + "invalid fully implicit DAE problem or tolerance contract";
        return result;
    }
    if (options.external_step_fallback && !problem.events.empty()) {
        result.diagnostic = prefix +
            "caller DAE stepper does not support event problems";
        return result;
    }
    if (cancellation_requested(options.cancellation_requested)) {
        result.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
        result.diagnostic = prefix + "cancelled before integration";
        return result;
    }

    for (std::size_t index = 0; index < problem.differential_mask.size(); ++index) {
        if (problem.differential_mask[index] == 0 && problem.initial_derivative[index] != 0.0) {
            result.diagnostic = prefix + "algebraic entries require zero initial derivative";
            return result;
        }
    }
    for (const auto& event : problem.events) {
        if (event.direction < -1 || event.direction > 1 || !event.guard || !event.reset) {
            result.diagnostic = prefix + "invalid DAE event callback, direction, or reinit contract";
            return result;
        }
        double initial_guard{};
        if (!evaluate_guard(
                event,
                options.start_time,
                problem.initial_state,
                problem.initial_derivative,
                initial_guard)) {
            result.diagnostic_code = VerifiedSolveDiagnosticCode::callback_failure;
            result.diagnostic = prefix + "initial DAE event guard evaluation failed";
            return result;
        }
    }
    const SolvePlan plan = route_fully_implicit_dae(callback_dae_ir(problem.differential_mask));
    result.equation_family = problem.events.empty()
        ? plan.assessment.equation_family
        : "dae-fully-implicit-first-order-with-events";
    result.plan_id = plan.plan_id + (problem.events.empty() ? "" : "-event-reinit-v1");
    if (options.external_step_fallback) {
        result.plan_id += "|caller-dae-stepper-fallback-v1";
    }
    std::vector<double> state = problem.initial_state;
    std::vector<double> derivative = problem.initial_derivative;
    if (!evaluate_original_residual(
            options.start_time, state, derivative, result.maximum_residual_inf)) {
        result.diagnostic_code = VerifiedSolveDiagnosticCode::original_gate_rejected;
        result.diagnostic = prefix + "inconsistent initial state/derivative rejected by original DAE residual gate";
        return result;
    }
    const auto initial_structure = probe_structure(
        options.start_time, state, derivative);
    if (initial_structure.kind == DaeStructureKind::unsupported_high_index) {
        result.diagnostic_code = VerifiedSolveDiagnosticCode::invalid_contract;
        result.diagnostic = prefix +
            "Jacobian indicates an unsupported high-index or rank-deficient DAE";
        return result;
    }
    if (initial_structure.kind == DaeStructureKind::hessenberg_index_two) {
        result.differentiation_index = 2;
        result.hidden_rank_checks = 1;
        result.minimum_hidden_rank_margin = initial_structure.hidden_rank_margin;
        result.maximum_hidden_residual_inf = initial_structure.hidden_residual_inf;
        result.equation_family = "dae-fully-implicit-hessenberg-index2";
        result.plan_id += "-hessenberg-index2-hidden-rank-v1";
        if (!problem.events.empty()) {
            result.diagnostic_code = VerifiedSolveDiagnosticCode::invalid_contract;
            result.diagnostic = prefix +
                "index-2 event/reinit is outside the verified public contract";
            return result;
        }
        if (initial_structure.hidden_residual_inf > gate_threshold(state, derivative)) {
            result.diagnostic_code = VerifiedSolveDiagnosticCode::original_gate_rejected;
            result.diagnostic = prefix +
                "inconsistent index-2 initial derivative rejected by hidden constraint gate";
            return result;
        }
        enforce_index_two = true;
    }

    double time = options.start_time;
    while (time < options.end_time) {
        if (cancellation_requested(options.cancellation_requested)) {
            result.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
            result.solution = state;
            result.final_time = time;
            result.diagnostic = prefix + "cancelled before next implicit step commit";
            return result;
        }
        const double remaining = options.end_time - time;
        const double time_resolution = 16.0 * std::numeric_limits<double>::epsilon() *
            std::max({1.0, std::abs(time), std::abs(options.end_time)});
        if (remaining <= time_resolution) {
            time = options.end_time;
            break;
        }
        if (result.accepted_steps >= static_cast<std::size_t>(options.maximum_iterations)) {
            result.diagnostic_code = VerifiedSolveDiagnosticCode::iteration_limit;
            result.solution = state;
            result.final_time = time;
            result.diagnostic = prefix + "maximum DAE step count exceeded";
            return result;
        }
        const double step = std::min(options.maximum_step, remaining);
        const double next_time = step == remaining ? options.end_time : time + step;
        DaeStepOutcome step_result = solve_step(time, state, derivative, next_time);
        external_stepper_used = external_stepper_used ||
            step_result.used_external_fallback;
        if (!step_result.success) {
            if (cancellation_requested(options.cancellation_requested)) {
                result.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
                result.solution = state;
                result.final_time = time;
                result.diagnostic = prefix + "cancelled during uncommitted implicit step";
                return result;
            }
            result.diagnostic_code = step_result.diagnostic_code;
            ++result.rejected_steps;
            result.solution = state;
            result.final_time = time;
            result.used_fallback = result.used_fallback || step_result.used_fallback;
            result.diagnostic = prefix + step_result.diagnostic;
            return result;
        }
        std::vector<std::pair<std::size_t, double>> located_events;
        for (std::size_t event_index = 0; event_index < problem.events.size(); ++event_index) {
            double left_guard{};
            double right_guard{};
            if (!evaluate_guard(
                    problem.events[event_index], time, state, derivative, left_guard) ||
                !evaluate_guard(
                    problem.events[event_index],
                    next_time,
                    step_result.state,
                    step_result.derivative,
                    right_guard)) {
                result.diagnostic_code = VerifiedSolveDiagnosticCode::callback_failure;
                ++result.rejected_steps;
                result.solution = state;
                result.final_time = time;
                result.diagnostic = prefix + "DAE event guard evaluation failed";
                return result;
            }
            if (!crosses_event(left_guard, right_guard, problem.events[event_index].direction)) {
                continue;
            }
            double lower_time = time;
            double upper_time = next_time;
            double lower_guard = left_guard;
            constexpr int maximum_root_iterations = 64;
            const double root_tolerance =
                1.0e-10 * std::max(1.0, std::abs(next_time));
            for (int iteration = 0;
                 iteration < maximum_root_iterations &&
                 upper_time - lower_time > root_tolerance;
                 ++iteration) {
                if (cancellation_requested(options.cancellation_requested)) {
                    result.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
                    result.solution = state;
                    result.final_time = time;
                    result.diagnostic = prefix +
                        "cancelled during uncommitted DAE event localization";
                    return result;
                }
                const double midpoint = lower_time + 0.5 * (upper_time - lower_time);
                DaeStepOutcome midpoint_result = solve_step(time, state, derivative, midpoint);
                result.used_fallback = result.used_fallback || midpoint_result.used_fallback;
                if (!midpoint_result.success) {
                    if (cancellation_requested(options.cancellation_requested)) {
                        result.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
                        result.solution = state;
                        result.final_time = time;
                        result.diagnostic = prefix +
                            "cancelled during uncommitted DAE event localization";
                        return result;
                    }
                    result.diagnostic_code = VerifiedSolveDiagnosticCode::numerical_failure;
                    ++result.rejected_steps;
                    result.solution = state;
                    result.final_time = time;
                    result.diagnostic = prefix +
                        "DAE event root implicit substep failed: " + midpoint_result.diagnostic;
                    return result;
                }
                result.maximum_residual_inf = std::max(
                    result.maximum_residual_inf, midpoint_result.residual_inf);
                double midpoint_guard{};
                if (!evaluate_guard(
                        problem.events[event_index],
                        midpoint,
                        midpoint_result.state,
                        midpoint_result.derivative,
                        midpoint_guard)) {
                    result.diagnostic_code = VerifiedSolveDiagnosticCode::callback_failure;
                    ++result.rejected_steps;
                    result.solution = state;
                    result.final_time = time;
                    result.diagnostic = prefix + "DAE event root guard evaluation failed";
                    return result;
                }
                if (crosses_event(
                        lower_guard, midpoint_guard, problem.events[event_index].direction)) {
                    upper_time = midpoint;
                } else {
                    lower_time = midpoint;
                    lower_guard = midpoint_guard;
                }
            }
            located_events.emplace_back(event_index, upper_time);
        }
        result.used_fallback = result.used_fallback || step_result.used_fallback;
        result.maximum_residual_inf = std::max(
            result.maximum_residual_inf, step_result.residual_inf);
        if (located_events.empty()) {
            state = std::move(step_result.state);
            derivative = std::move(step_result.derivative);
            ++result.accepted_steps;
            time = next_time;
            continue;
        }

        const double event_time = std::min_element(
            located_events.begin(), located_events.end(), [](const auto& left, const auto& right) {
                return left.second < right.second;
            })->second;
        const double simultaneous_tolerance =
            1.0e-9 * std::max(1.0, std::abs(event_time));
        std::vector<std::size_t> simultaneous;
        for (const auto& located : located_events) {
            if (std::abs(located.second - event_time) <= simultaneous_tolerance) {
                simultaneous.push_back(located.first);
            }
        }
        std::sort(simultaneous.begin(), simultaneous.end(), [&](const auto left, const auto right) {
            const auto& left_event = problem.events[left];
            const auto& right_event = problem.events[right];
            return left_event.priority != right_event.priority
                ? left_event.priority > right_event.priority
                : left < right;
        });
        if (cancellation_requested(options.cancellation_requested)) {
            result.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
            result.solution = state;
            result.final_time = time;
            result.diagnostic = prefix + "cancelled before DAE event transaction";
            return result;
        }
        DaeStepOutcome event_step = solve_step(time, state, derivative, event_time);
        result.used_fallback = result.used_fallback || event_step.used_fallback;
        if (!event_step.success) {
            if (cancellation_requested(options.cancellation_requested)) {
                result.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
                result.solution = state;
                result.final_time = time;
                result.diagnostic = prefix + "cancelled before DAE event transaction";
                return result;
            }
            result.diagnostic_code = VerifiedSolveDiagnosticCode::numerical_failure;
            ++result.rejected_steps;
            result.solution = state;
            result.final_time = time;
            result.diagnostic = prefix +
                "DAE event commit implicit substep failed: " + event_step.diagnostic;
            return result;
        }
        result.maximum_residual_inf = std::max(
            result.maximum_residual_inf, event_step.residual_inf);
        std::vector<double> transaction_state = std::move(event_step.state);
        std::vector<double> transaction_derivative = std::move(event_step.derivative);
        std::size_t committed_events{};
        if (problem.event_cluster_reset) {
            std::vector<double> post_state;
            std::vector<double> post_derivative;
            VerifiedSolveDiagnosticCode failure_code =
                VerifiedSolveDiagnosticCode::event_reinit_callback_failure;
            if (!problem.event_cluster_reset(
                    event_time,
                    simultaneous,
                    transaction_state,
                    transaction_derivative,
                    post_state,
                    post_derivative,
                    committed_events,
                    failure_code) ||
                post_state.size() != state.size() ||
                post_derivative.size() != state.size() ||
                !finite_values(post_state) || !finite_values(post_derivative) ||
                committed_events < simultaneous.size()) {
                if (cancellation_requested(options.cancellation_requested)) {
                    result.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
                    result.solution = state;
                    result.final_time = time;
                    result.diagnostic = prefix +
                        "cancelled during uncommitted DAE event transaction";
                    return result;
                }
                result.diagnostic_code = failure_code;
                ++result.rejected_steps;
                result.solution = state;
                result.final_time = time;
                result.diagnostic = prefix + "DAE event cluster transaction failed";
                return result;
            }
            transaction_state = std::move(post_state);
            transaction_derivative = std::move(post_derivative);
        } else {
            for (const std::size_t event_index : simultaneous) {
                if (cancellation_requested(options.cancellation_requested)) {
                    result.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
                    result.solution = state;
                    result.final_time = time;
                    result.diagnostic = prefix +
                        "cancelled during uncommitted DAE event transaction";
                    return result;
                }
                std::vector<double> post_state;
                std::vector<double> post_derivative;
                if (!problem.events[event_index].reset(
                        event_time,
                        transaction_state,
                        transaction_derivative,
                        post_state,
                        post_derivative) ||
                    post_state.size() != state.size() ||
                    post_derivative.size() != state.size() ||
                    !finite_values(post_state) || !finite_values(post_derivative)) {
                    result.diagnostic_code =
                        VerifiedSolveDiagnosticCode::event_reinit_callback_failure;
                    ++result.rejected_steps;
                    result.solution = state;
                    result.final_time = time;
                    result.diagnostic = prefix + "DAE event reinit callback failed";
                    return result;
                }
                transaction_state = std::move(post_state);
                transaction_derivative = std::move(post_derivative);
                ++committed_events;
            }
        }
        for (std::size_t index = 0; index < transaction_derivative.size(); ++index) {
            if (problem.differential_mask[index] == 0 &&
                transaction_derivative[index] != 0.0) {
                result.diagnostic_code =
                    VerifiedSolveDiagnosticCode::event_reinit_consistency_rejected;
                ++result.rejected_steps;
                result.solution = state;
                result.final_time = time;
                result.diagnostic = prefix +
                    "DAE event reinit supplied nonzero algebraic derivative";
                return result;
            }
        }
        double post_residual_inf{};
        if (!evaluate_original_residual(
                event_time,
                transaction_state,
                transaction_derivative,
                post_residual_inf)) {
            result.diagnostic_code =
                VerifiedSolveDiagnosticCode::event_reinit_consistency_rejected;
            ++result.rejected_steps;
            result.solution = state;
            result.final_time = time;
            result.diagnostic = prefix +
                "DAE event reinit rejected by original residual gate";
            return result;
        }
        result.maximum_residual_inf = std::max(
            result.maximum_residual_inf, post_residual_inf);
        constexpr double guard_release = 1.0e-10;
        for (const std::size_t event_index : simultaneous) {
            double post_guard{};
            if (!evaluate_guard(
                    problem.events[event_index],
                    event_time,
                    transaction_state,
                    transaction_derivative,
                    post_guard)) {
                result.diagnostic_code = VerifiedSolveDiagnosticCode::callback_failure;
                ++result.rejected_steps;
                result.solution = state;
                result.final_time = time;
                result.diagnostic = prefix + "DAE event post-reinit guard evaluation failed";
                return result;
            }
            const int direction = problem.events[event_index].direction;
            if ((direction > 0 && post_guard >= -guard_release) ||
                (direction < 0 && post_guard <= guard_release) ||
                (direction == 0 && std::abs(post_guard) <= guard_release)) {
                result.diagnostic_code =
                    VerifiedSolveDiagnosticCode::event_guard_not_released;
                ++result.rejected_steps;
                result.solution = state;
                result.final_time = time;
                result.diagnostic = prefix + "DAE event reinit did not leave guard surface";
                return result;
            }
        }
        if (cancellation_requested(options.cancellation_requested)) {
            result.diagnostic_code = VerifiedSolveDiagnosticCode::cancelled;
            result.solution = state;
            result.final_time = time;
            result.diagnostic = prefix + "cancelled before DAE event transaction commit";
            return result;
        }
        state = std::move(transaction_state);
        derivative = std::move(transaction_derivative);
        time = event_time;
        ++result.accepted_steps;
        result.event_count += committed_events;
        result.last_event_time = event_time;
    }
    result.solution = std::move(state);
    result.final_time = time;
    result.success = true;
    result.diagnostic_code = VerifiedSolveDiagnosticCode::success;
    if (problem.events.empty()) {
        result.backend = external_stepper_used
            ? "caller-dae-stepper-fallback-v1"
            : result.used_fallback
            ? "backward-euler-finite-difference-damped-newton-fallback-v1"
            : "backward-euler-callback-jacobian-damped-newton-v1";
    } else {
        result.backend = result.used_fallback
            ? "backward-euler-event-reinit-finite-difference-fallback-v1"
            : "backward-euler-event-reinit-callback-jacobian-v1";
    }
    result.diagnostic = prefix + "plan=" + result.plan_id + ";family=" +
        result.equation_family + (problem.events.empty()
            ? ";accepted by original fully implicit DAE residual gate"
            : ";accepted by implicit event localization, consistent reinit, and original DAE residual gates");
    return result;
}

VerifiedHybridDaeSolveResult verified_hybrid_dae_solve(
    const VerifiedHybridDaeSolveProblem& problem,
    const VerifiedDaeSolveOptions& options) {
    VerifiedHybridDaeSolveResult result;
    const std::string prefix =
        std::string(verified_hybrid_dae_solve_service_v1) + ": ";
    const std::size_t physical_dimension = problem.initial_state.size();
    if (physical_dimension == 0 ||
        problem.differential_mask.size() != physical_dimension ||
        problem.initial_derivative.size() != physical_dimension ||
        !finite_values(problem.initial_state) ||
        !finite_values(problem.initial_derivative) || problem.modes.empty() ||
        problem.initial_mode >= problem.modes.size()) {
        result.diagnostic = prefix + "invalid hybrid DAE state or initial mode contract";
        return result;
    }
    for (const auto& mode : problem.modes) {
        if (!mode.residual) {
            result.diagnostic = prefix + "hybrid DAE mode is missing a residual callback";
            return result;
        }
    }
    for (const auto& transition : problem.transitions) {
        const bool stable = static_cast<bool>(transition.stable_reset);
        const bool masks_valid =
            transition.state_write_mask.size() == physical_dimension &&
            transition.derivative_write_mask.size() == physical_dimension &&
            std::none_of(
                transition.state_write_mask.begin(),
                transition.state_write_mask.end(),
                [](std::uint8_t value) { return value > 1; }) &&
            std::none_of(
                transition.derivative_write_mask.begin(),
                transition.derivative_write_mask.end(),
                [](std::uint8_t value) { return value > 1; });
        if (transition.source_mode >= problem.modes.size() ||
            transition.target_mode >= problem.modes.size() ||
            transition.direction < -1 || transition.direction > 1 ||
            !transition.guard || (!transition.reset && !stable) ||
            (stable != masks_valid) ||
            (!stable && (!transition.state_write_mask.empty() ||
                         !transition.derivative_write_mask.empty()))) {
            result.diagnostic = prefix + "invalid hybrid DAE transition contract";
            return result;
        }
    }
    const auto unpack_mode = [&](const std::vector<double>& augmented,
                                 std::size_t& mode) {
        if (augmented.size() != physical_dimension + 1 || !finite_values(augmented)) {
            return false;
        }
        const double rounded = std::round(augmented.back());
        if (std::abs(augmented.back() - rounded) > 1.0e-8 || rounded < 0.0 ||
            rounded >= static_cast<double>(problem.modes.size())) return false;
        mode = static_cast<std::size_t>(rounded);
        return true;
    };
    const auto physical_values = [physical_dimension](
        const std::vector<double>& augmented) {
        return std::vector<double>(
            augmented.begin(),
            augmented.begin() + static_cast<std::ptrdiff_t>(physical_dimension));
    };
    const auto consistent = [&](double time,
                                std::size_t mode,
                                const std::vector<double>& state,
                                const std::vector<double>& derivative) {
        if (state.size() != physical_dimension ||
            derivative.size() != physical_dimension || !finite_values(state) ||
            !finite_values(derivative)) return false;
        for (std::size_t index = 0; index < physical_dimension; ++index) {
            if (problem.differential_mask[index] == 0 && derivative[index] != 0.0) {
                return false;
            }
        }
        std::vector<double> residual;
        if (!problem.modes[mode].residual(time, state, derivative, residual) ||
            residual.size() != physical_dimension || !finite_values(residual)) return false;
        const double threshold = options.absolute_tolerance + options.relative_tolerance *
            std::max({1.0, norm_inf(state), norm_inf(derivative)});
        return norm_inf(residual) <= threshold;
    };
    std::size_t consistency_projection_count{};
    const auto project_consistent = [&](double time,
                                        std::size_t mode,
                                        std::vector<double>& state,
                                        std::vector<double>& derivative,
                                        bool& projected) {
        projected = false;
        if (consistent(time, mode, state, derivative)) return true;
        if (state.size() != physical_dimension ||
            derivative.size() != physical_dimension || !finite_values(state) ||
            !finite_values(derivative)) return false;
        std::vector<double> initial_unknown(physical_dimension);
        for (std::size_t index = 0; index < physical_dimension; ++index) {
            initial_unknown[index] = problem.differential_mask[index]
                ? derivative[index] : state[index];
        }
        VerifiedNonlinearSolveProblem projection;
        projection.initial_state = std::move(initial_unknown);
        projection.residual = [&](const std::vector<double>& unknown,
                                  std::vector<double>& residual) {
            if (unknown.size() != physical_dimension || !finite_values(unknown)) {
                return false;
            }
            std::vector<double> projected_state = state;
            std::vector<double> projected_derivative = derivative;
            for (std::size_t index = 0; index < physical_dimension; ++index) {
                if (problem.differential_mask[index]) {
                    projected_derivative[index] = unknown[index];
                } else {
                    projected_state[index] = unknown[index];
                    projected_derivative[index] = 0.0;
                }
            }
            return problem.modes[mode].residual(
                time, projected_state, projected_derivative, residual);
        };
        const auto projection_result = verified_nonlinear_solve(
            projection,
            {.absolute_tolerance = options.absolute_tolerance,
             .relative_tolerance = options.relative_tolerance,
             .maximum_iterations = options.maximum_newton_iterations,
             .cancellation_requested = options.cancellation_requested});
        if (!projection_result.success ||
            projection_result.solution.size() != physical_dimension) return false;
        for (std::size_t index = 0; index < physical_dimension; ++index) {
            if (problem.differential_mask[index]) {
                derivative[index] = projection_result.solution[index];
            } else {
                state[index] = projection_result.solution[index];
                derivative[index] = 0.0;
            }
        }
        if (!consistent(time, mode, state, derivative)) return false;
        projected = true;
        return true;
    };

    VerifiedDaeSolveProblem dae;
    dae.differential_mask = problem.differential_mask;
    dae.differential_mask.push_back(0);
    dae.initial_state = problem.initial_state;
    dae.initial_state.push_back(static_cast<double>(problem.initial_mode));
    dae.initial_derivative = problem.initial_derivative;
    dae.initial_derivative.push_back(0.0);
    dae.residual = [&](double time,
                       const std::vector<double>& augmented_state,
                       const std::vector<double>& augmented_derivative,
                       std::vector<double>& residual) {
        std::size_t mode{};
        if (!unpack_mode(augmented_state, mode) ||
            augmented_derivative.size() != physical_dimension + 1 ||
            !finite_values(augmented_derivative)) return false;
        if (!problem.modes[mode].residual(
                time,
                physical_values(augmented_state),
                physical_values(augmented_derivative),
                residual) ||
            residual.size() != physical_dimension || !finite_values(residual)) return false;
        residual.push_back(
            augmented_state.back() - static_cast<double>(mode));
        return true;
    };
    dae.jacobian = [&](double time,
                       const std::vector<double>& augmented_state,
                       const std::vector<double>& augmented_derivative,
                       double derivative_scale,
                       std::vector<std::vector<double>>& jacobian) {
        std::size_t mode{};
        if (!unpack_mode(augmented_state, mode) ||
            augmented_derivative.size() != physical_dimension + 1 ||
            !problem.modes[mode].jacobian) return false;
        std::vector<std::vector<double>> physical_jacobian;
        if (!problem.modes[mode].jacobian(
                time,
                physical_values(augmented_state),
                physical_values(augmented_derivative),
                derivative_scale,
                physical_jacobian) ||
            physical_jacobian.size() != physical_dimension) return false;
        jacobian.assign(
            physical_dimension + 1,
            std::vector<double>(physical_dimension + 1, 0.0));
        for (std::size_t row = 0; row < physical_dimension; ++row) {
            if (physical_jacobian[row].size() != physical_dimension ||
                !finite_values(physical_jacobian[row])) return false;
            std::copy(
                physical_jacobian[row].begin(),
                physical_jacobian[row].end(),
                jacobian[row].begin());
        }
        jacobian.back().back() = 1.0;
        return true;
    };
    dae.events.reserve(problem.transitions.size());
    for (std::size_t index = 0; index < problem.transitions.size(); ++index) {
        const auto& transition = problem.transitions[index];
        VerifiedDaeEvent event;
        event.direction = transition.direction;
        event.priority = transition.priority;
        event.guard = [&, index](double time,
                                 const std::vector<double>& augmented_state,
                                 const std::vector<double>& augmented_derivative,
                                 double& guard) {
            std::size_t mode{};
            if (!unpack_mode(augmented_state, mode) ||
                augmented_derivative.size() != physical_dimension + 1) return false;
            const auto& source = problem.transitions[index];
            if (mode != source.source_mode) {
                guard = source.direction > 0 ? -1.0 : 1.0;
                return true;
            }
            return source.guard(
                       time,
                       physical_values(augmented_state),
                       physical_values(augmented_derivative),
                       guard) && std::isfinite(guard);
        };
        event.reset = [&, index](double time,
                                 const std::vector<double>& augmented_state,
                                 const std::vector<double>& augmented_derivative,
                                 std::vector<double>& reset_state,
                                 std::vector<double>& reset_derivative) {
            std::size_t mode{};
            if (!unpack_mode(augmented_state, mode) ||
                augmented_derivative.size() != physical_dimension + 1) return false;
            const auto& source = problem.transitions[index];
            bool projected{};
            if (mode != source.source_mode) return false;
            const auto current_state = physical_values(augmented_state);
            const auto current_derivative = physical_values(augmented_derivative);
            const bool reset_ok = source.reset
                ? source.reset(
                    time,
                    current_state,
                    current_derivative,
                    reset_state,
                    reset_derivative)
                : source.stable_reset(
                    time,
                    current_state,
                    current_derivative,
                    current_state,
                    current_derivative,
                    reset_state,
                    reset_derivative);
            if (!reset_ok ||
                !project_consistent(
                    time,
                    source.target_mode,
                    reset_state,
                    reset_derivative,
                    projected)) return false;
            reset_state.push_back(static_cast<double>(source.target_mode));
            reset_derivative.push_back(0.0);
            return true;
        };
        dae.events.push_back(std::move(event));
    }
    dae.event_cluster_reset = [&problem,
                               &unpack_mode,
                               &physical_values,
                               &project_consistent,
                               &consistency_projection_count,
                               &options,
                               physical_dimension](
        double time,
        const std::vector<std::size_t>& initial_events,
        const std::vector<double>& pre_state,
        const std::vector<double>& pre_derivative,
        std::vector<double>& post_state,
        std::vector<double>& post_derivative,
        std::size_t& committed_events,
        VerifiedSolveDiagnosticCode& failure_code) {
        constexpr std::size_t maximum_superdense_steps = 64;
        constexpr double activation_tolerance = 1.0e-10;
        if (initial_events.empty()) return false;
        std::vector<double> transaction_state = pre_state;
        std::vector<double> transaction_derivative = pre_derivative;
        const std::vector<double> stable_pre_state = physical_values(pre_state);
        const std::vector<double> stable_pre_derivative = physical_values(pre_derivative);
        std::size_t transaction_projection_count{};
        const auto apply_transition = [&](std::size_t index) {
            if (index >= problem.transitions.size()) return false;
            std::size_t mode{};
            if (!unpack_mode(transaction_state, mode) ||
                transaction_derivative.size() != physical_dimension + 1 ||
                mode != problem.transitions[index].source_mode) return false;
            const auto& transition = problem.transitions[index];
            std::vector<double> physical_state;
            std::vector<double> physical_derivative;
            bool projected{};
            if (!transition.reset(
                    time,
                    physical_values(transaction_state),
                    physical_values(transaction_derivative),
                    physical_state,
                    physical_derivative)) {
                failure_code =
                    VerifiedSolveDiagnosticCode::event_reinit_callback_failure;
                return false;
            }
            if (!project_consistent(
                    time,
                    transition.target_mode,
                    physical_state,
                    physical_derivative,
                    projected)) {
                failure_code =
                    VerifiedSolveDiagnosticCode::event_reinit_consistency_rejected;
                return false;
            }
            transaction_state = std::move(physical_state);
            transaction_state.push_back(static_cast<double>(transition.target_mode));
            transaction_derivative = std::move(physical_derivative);
            transaction_derivative.push_back(0.0);
            if (projected) ++transaction_projection_count;
            ++committed_events;
            return true;
        };
        const auto apply_batch = [&](const std::vector<std::size_t>& indices) {
            if (indices.empty()) return false;
            std::size_t mode{};
            if (!unpack_mode(transaction_state, mode) ||
                transaction_derivative.size() != physical_dimension + 1) return false;
            const auto current_state = physical_values(transaction_state);
            const auto current_derivative = physical_values(transaction_derivative);
            std::vector<double> merged_state = current_state;
            std::vector<double> merged_derivative = current_derivative;
            std::vector<std::uint8_t> state_written(physical_dimension, 0);
            std::vector<std::uint8_t> derivative_written(physical_dimension, 0);
            std::size_t target_mode = problem.modes.size();
            for (const std::size_t index : indices) {
                if (index >= problem.transitions.size()) return false;
                const auto& transition = problem.transitions[index];
                if (transition.source_mode != mode || !transition.stable_reset ||
                    transition.state_write_mask.size() != physical_dimension ||
                    transition.derivative_write_mask.size() != physical_dimension) {
                    return false;
                }
                if (target_mode == problem.modes.size()) {
                    target_mode = transition.target_mode;
                } else if (target_mode != transition.target_mode) {
                    failure_code = VerifiedSolveDiagnosticCode::event_reset_conflict;
                    return false;
                }
                std::vector<double> proposed_state;
                std::vector<double> proposed_derivative;
                if (!transition.stable_reset(
                        time,
                        stable_pre_state,
                        stable_pre_derivative,
                        current_state,
                        current_derivative,
                        proposed_state,
                        proposed_derivative) ||
                    proposed_state.size() != physical_dimension ||
                    proposed_derivative.size() != physical_dimension ||
                    !finite_values(proposed_state) ||
                    !finite_values(proposed_derivative)) {
                    failure_code =
                        VerifiedSolveDiagnosticCode::event_reinit_callback_failure;
                    return false;
                }
                for (std::size_t value_index = 0;
                     value_index < physical_dimension;
                     ++value_index) {
                    if (transition.state_write_mask[value_index] != 0) {
                        if (state_written[value_index] != 0) {
                            failure_code =
                                VerifiedSolveDiagnosticCode::event_reset_conflict;
                            return false;
                        }
                        state_written[value_index] = 1;
                        merged_state[value_index] = proposed_state[value_index];
                    }
                    if (transition.derivative_write_mask[value_index] != 0) {
                        if (derivative_written[value_index] != 0) {
                            failure_code =
                                VerifiedSolveDiagnosticCode::event_reset_conflict;
                            return false;
                        }
                        derivative_written[value_index] = 1;
                        merged_derivative[value_index] = proposed_derivative[value_index];
                    }
                }
            }
            bool projected{};
            if (!project_consistent(
                    time,
                    target_mode,
                    merged_state,
                    merged_derivative,
                    projected)) {
                failure_code =
                    VerifiedSolveDiagnosticCode::event_reinit_consistency_rejected;
                return false;
            }
            transaction_state = std::move(merged_state);
            transaction_state.push_back(static_cast<double>(target_mode));
            transaction_derivative = std::move(merged_derivative);
            transaction_derivative.push_back(0.0);
            if (projected) ++transaction_projection_count;
            committed_events += indices.size();
            return true;
        };
        const auto apply_microstep = [&](const std::vector<std::size_t>& indices) {
            const bool stable_batch = std::all_of(
                indices.begin(), indices.end(), [&](const std::size_t index) {
                    return index < problem.transitions.size() &&
                        static_cast<bool>(problem.transitions[index].stable_reset);
                });
            return stable_batch ? apply_batch(indices) : apply_transition(indices.front());
        };
        if (!apply_microstep(initial_events)) return false;
        for (std::size_t microstep = 1; microstep < maximum_superdense_steps; ++microstep) {
            if (cancellation_requested(options.cancellation_requested)) {
                failure_code = VerifiedSolveDiagnosticCode::cancelled;
                return false;
            }
            std::size_t mode{};
            if (!unpack_mode(transaction_state, mode)) return false;
            std::vector<std::size_t> enabled;
            for (std::size_t index = 0; index < problem.transitions.size(); ++index) {
                const auto& transition = problem.transitions[index];
                if (transition.source_mode != mode) continue;
                double guard{};
                if (!transition.guard(
                        time,
                        physical_values(transaction_state),
                        physical_values(transaction_derivative),
                        guard) || !std::isfinite(guard)) {
                    failure_code = VerifiedSolveDiagnosticCode::callback_failure;
                    return false;
                }
                const bool active = transition.direction > 0
                    ? guard >= -activation_tolerance
                    : (transition.direction < 0
                        ? guard <= activation_tolerance
                        : std::abs(guard) <= activation_tolerance);
                if (active) enabled.push_back(index);
            }
            if (enabled.empty()) {
                consistency_projection_count += transaction_projection_count;
                post_state = std::move(transaction_state);
                post_derivative = std::move(transaction_derivative);
                return true;
            }
            std::sort(enabled.begin(), enabled.end(), [&](const auto left, const auto right) {
                const auto& left_transition = problem.transitions[left];
                const auto& right_transition = problem.transitions[right];
                return left_transition.priority != right_transition.priority
                    ? left_transition.priority > right_transition.priority
                    : left < right;
            });
            if (!apply_microstep(enabled)) return false;
        }
        failure_code = VerifiedSolveDiagnosticCode::iteration_limit;
        return false;
    };

    const VerifiedDaeSolveResult dae_result = verified_dae_solve(dae, options);
    static_cast<VerifiedDaeSolveResult&>(result) = dae_result;
    result.equation_family = "dae-fully-implicit-hybrid-multimode";
    if (!result.plan_id.empty()) result.plan_id += "-hybrid-mode-v1";
    result.diagnostic = prefix + dae_result.diagnostic;
    result.consistency_projection_count = consistency_projection_count;
    if (!result.solution.empty()) {
        std::size_t final_mode{};
        if (!unpack_mode(result.solution, final_mode)) {
            result.success = false;
            result.diagnostic_code = VerifiedSolveDiagnosticCode::numerical_failure;
            result.diagnostic = prefix + "invalid final hybrid DAE mode encoding";
            return result;
        }
        result.final_mode = final_mode;
        result.solution.pop_back();
    }
    return result;
}

}
