#include "smave/cpp_api.hpp"

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <utility>

namespace {

std::atomic_size_t allocations;
std::atomic_size_t deallocations;

void* tracked_allocate(std::size_t size, void*) {
    ++allocations;
    return std::malloc(size);
}

void tracked_deallocate(void* memory, void*) {
    ++deallocations;
    std::free(memory);
}

smave_library_options library_options() {
    return {
        sizeof(smave_library_options), SMAVE_ABI_VERSION,
        tracked_allocate, tracked_deallocate, nullptr,
    };
}

smave_linear_problem_desc regular_descriptor() {
    static constexpr std::array matrix{2.0, -1.0, -1.0, 2.0};
    static constexpr std::array right_hand_side{1.0, 1.0};
    return {
        sizeof(smave_linear_problem_desc), SMAVE_ABI_VERSION,
        SMAVE_MATRIX_DENSE_ROW_MAJOR,
        SMAVE_LINEAR_SYMMETRIC | SMAVE_LINEAR_POSITIVE_DEFINITE,
        2, matrix.data(), nullptr, nullptr, nullptr, 0, right_hand_side.data(),
    };
}

smave_linear_problem_desc singular_descriptor() {
    static constexpr std::array matrix{1.0, 1.0, 2.0, 2.0};
    static constexpr std::array right_hand_side{2.0, 4.0};
    return {
        sizeof(smave_linear_problem_desc), SMAVE_ABI_VERSION,
        SMAVE_MATRIX_DENSE_ROW_MAJOR, 0,
        2, matrix.data(), nullptr, nullptr, nullptr, 0, right_hand_side.data(),
    };
}

smave::sdk::Solver detached_solver() {
    auto options = library_options();
    smave::sdk::Library library(&options);
    auto problem = library.linear(regular_descriptor());
    problem.finalize();
    return problem.solver();
}

smave::sdk::Result detached_result() {
    auto solver = detached_solver();
    auto outcome = solver.solve();
    if (outcome.status != SMAVE_STATUS_OK) throw smave::sdk::Error(outcome.status);
    return std::move(outcome.result);
}

int linear_fallback(std::size_t dimension, double* solution, void*) {
    if (dimension != 2) return 1;
    solution[0] = 2.0;
    solution[1] = 0.0;
    return 0;
}

int nonlinear_residual(
    std::size_t dimension, const double* state, double* residual, void*) {
    if (dimension != 1) return 1;
    residual[0] = state[0] * state[0] - 4.0;
    return 0;
}

int failing_nonlinear_jacobian(
    std::size_t, const double*, double*, void*) {
    return 1;
}

int nonlinear_fallback(std::size_t dimension, double* solution, void*) {
    if (dimension != 1) return 1;
    solution[0] = 2.0;
    return 0;
}

int exponential_rhs(
    std::size_t dimension, double, const double* state,
    double* derivative, void*) {
    if (dimension != 1) return 1;
    derivative[0] = state[0];
    return 0;
}

int ode_dense_stepper(
    std::size_t dimension,
    double from_time,
    const double* previous_state,
    double to_time,
    double* quarter_state,
    double* midpoint_state,
    double* three_quarter_state,
    double* next_state,
    void*) {
    if (dimension != 1) return 1;
    const double step = to_time - from_time;
    quarter_state[0] = previous_state[0] * std::exp(0.25 * step);
    midpoint_state[0] = previous_state[0] * std::exp(0.5 * step);
    three_quarter_state[0] = previous_state[0] * std::exp(0.75 * step);
    next_state[0] = previous_state[0] * std::exp(step);
    return 0;
}

int dae_quadratic_residual(
    std::size_t dimension,
    double,
    const double* state,
    const double* derivative,
    double* residual,
    void*) {
    if (dimension != 1) return 1;
    residual[0] = derivative[0] - state[0] * state[0];
    return 0;
}

int failing_dae_jacobian(
    std::size_t, double, const double*, const double*,
    double, double*, void*) {
    return 1;
}

int dae_stepper(
    std::size_t dimension,
    const std::uint8_t* mask,
    double from_time,
    const double* previous_state,
    const double*,
    double to_time,
    double* next_state,
    double* next_derivative,
    void*) {
    if (dimension != 1 || mask == nullptr || mask[0] != 1) return 1;
    const double step = to_time - from_time;
    next_state[0] = (1.0 - std::sqrt(1.0 - 4.0 * step * previous_state[0])) /
        (2.0 * step);
    next_derivative[0] = (next_state[0] - previous_state[0]) / step;
    return 0;
}

}

int main() {
    bool parent_lifetime{};
    bool result_lifetime{};
    bool cancellation_deadline{};
    bool external_fallback{};
    bool external_nonlinear_fallback{};
    bool external_ode_fallback{};
    bool external_dae_fallback{};
    bool status_exception{};
    bool error_records{};
    {
        auto solver = detached_solver();
        auto outcome = solver.solve();
        const auto solution = outcome.result.solution();
        const auto provenance = outcome.result.provenance();
        parent_lifetime = outcome.status == SMAVE_STATUS_OK && outcome.success() &&
            solution.size() == 2 && std::abs(solution[0] - 1.0) <= 1.0e-12 &&
            std::abs(solution[1] - 1.0) <= 1.0e-12 &&
            provenance.service_id == "smave.verified-linear-solve.v1";
    }
    {
        auto result = detached_result();
        const auto info = result.info();
        result_lifetime = info.success && info.dimension == 2 &&
            !info.backend.empty() && !info.diagnostic.empty();
    }
    {
        auto options = library_options();
        smave::sdk::Library library(&options);
        auto token = library.cancel_token();
        auto problem = library.linear(regular_descriptor());
        problem.finalize();
        auto solver = problem.solver();
        token.request();
        auto cancelled = solver.solve(token);
        token.reset();
        auto deadline = solver.solve_for(0, &token);
        cancellation_deadline = cancelled.status == SMAVE_STATUS_CANCELLED &&
            cancelled.result.diagnostic_code() == SMAVE_DIAGNOSTIC_CANCELLED &&
            deadline.status == SMAVE_STATUS_DEADLINE_EXCEEDED &&
            deadline.result.diagnostic_code() == SMAVE_DIAGNOSTIC_DEADLINE_EXCEEDED;
    }
    {
        auto options = library_options();
        smave::sdk::Library library(&options);
        auto problem = library.linear(singular_descriptor());
        problem.finalize();
        auto solver = problem.solver();
        const smave_linear_fallback_desc fallback{
            sizeof(fallback), SMAVE_ABI_VERSION, linear_fallback, nullptr};
        auto outcome = solver.solve_linear(fallback);
        const auto info = outcome.result.info();
        const auto solution = outcome.result.solution();
        external_fallback = outcome.status == SMAVE_STATUS_OK && info.success &&
            info.used_fallback && info.backend == "caller-linear-fallback-v1" &&
            solution.size() == 2 && solution[0] == 2.0 && solution[1] == 0.0;
    }
    {
        auto options = library_options();
        smave::sdk::Library library(&options);
        constexpr std::array initial{0.0};
        const smave_nonlinear_problem_desc descriptor{
            sizeof(descriptor), SMAVE_ABI_VERSION, 1, initial.data(),
            nonlinear_residual, failing_nonlinear_jacobian, nullptr};
        auto problem = library.nonlinear(descriptor);
        problem.finalize();
        const smave_solver_options solver_options{
            sizeof(solver_options), SMAVE_ABI_VERSION,
            1.0e-12, 1.0e-10, 1};
        auto solver = problem.solver(&solver_options);
        const smave_nonlinear_fallback_desc fallback{
            sizeof(fallback), SMAVE_ABI_VERSION, nonlinear_fallback, nullptr};
        auto outcome = solver.solve_nonlinear(fallback);
        const auto info = outcome.result.info();
        const auto solution = outcome.result.solution();
        external_nonlinear_fallback = outcome.status == SMAVE_STATUS_OK &&
            info.success && info.used_fallback &&
            info.backend == "caller-nonlinear-fallback-v1" &&
            solution.size() == 1 && solution[0] == 2.0;
    }
    {
        auto options = library_options();
        smave::sdk::Library library(&options);
        constexpr std::array initial{1.0};
        const smave_ode_problem_desc descriptor{
            sizeof(descriptor), SMAVE_ABI_VERSION, 1, initial.data(),
            0.0, 1.0, 1.0, exponential_rhs, nullptr};
        auto problem = library.ode(descriptor);
        problem.finalize();
        const smave_solver_options solver_options{
            sizeof(solver_options), SMAVE_ABI_VERSION,
            3.0e-5, 0.0, 1};
        auto solver = problem.solver(&solver_options);
        const smave_ode_dense_step_fallback_desc fallback{
            sizeof(fallback), SMAVE_ABI_VERSION, ode_dense_stepper, nullptr};
        auto outcome = solver.solve_ode(fallback);
        const auto info = outcome.result.info();
        const auto solution = outcome.result.solution();
        external_ode_fallback = outcome.status == SMAVE_STATUS_OK &&
            info.success && info.used_fallback &&
            info.backend == "caller-ode-dense-stepper-fallback-v1" &&
            solution.size() == 1 &&
            std::abs(solution[0] - std::exp(1.0)) <= 1.0e-12;
    }
    {
        auto options = library_options();
        smave::sdk::Library library(&options);
        constexpr std::array<std::uint8_t, 1> mask{1};
        constexpr std::array initial_state{1.0};
        constexpr std::array initial_derivative{1.0};
        const smave_dae_problem_desc descriptor{
            sizeof(descriptor), SMAVE_ABI_VERSION, 1, mask.data(),
            initial_state.data(), initial_derivative.data(), 0.0, 0.1, 0.1,
            dae_quadratic_residual, failing_dae_jacobian, nullptr};
        auto problem = library.dae(descriptor);
        problem.finalize();
        const smave_solver_options solver_options{
            sizeof(solver_options), SMAVE_ABI_VERSION,
            1.0e-10, 1.0e-10, 1};
        auto solver = problem.solver(&solver_options);
        const smave_dae_step_fallback_desc fallback{
            sizeof(fallback), SMAVE_ABI_VERSION, dae_stepper, nullptr};
        auto outcome = solver.solve_dae(fallback);
        const auto info = outcome.result.info();
        external_dae_fallback = outcome.status == SMAVE_STATUS_OK &&
            info.success && info.used_fallback &&
            info.backend == "caller-dae-stepper-fallback-v1";
    }
    {
        auto options = library_options();
        smave::sdk::Library library(&options);
        constexpr std::array initial{0.0};
        const smave_nonlinear_problem_desc invalid{
            sizeof(invalid), SMAVE_ABI_VERSION, 0, initial.data(),
            nonlinear_residual, nullptr, nullptr};
        try {
            (void)library.nonlinear(invalid);
        } catch (const smave::sdk::Error& error) {
            status_exception = error.status() == SMAVE_STATUS_INVALID_ARGUMENT;
            auto records = library.errors();
            if (records.size() == 1) {
                auto copied = records.front();
                library.clear_errors();
                error_records = copied.trace_id != 0 &&
                    copied.status == SMAVE_STATUS_INVALID_ARGUMENT &&
                    copied.operation == "smave_nonlinear_problem_create" &&
                    copied.message ==
                        "descriptor shape or residual callback is invalid" &&
                    library.errors().empty();
            }
        }
    }
    if (!parent_lifetime || !result_lifetime || !cancellation_deadline ||
        !external_fallback || !external_nonlinear_fallback ||
        !external_ode_fallback || !external_dae_fallback || !status_exception ||
        !error_records ||
        allocations.load() != deallocations.load()) return 1;
    std::cout << "SMAVE_CPP_RAII_HOST 1\n"
                 "CPP_RAII_PARENT_LIFETIME 1\n"
                 "CPP_RAII_RESULT_LIFETIME 1\n"
                 "CPP_RAII_CANCELLATION_DEADLINE 1\n"
                 "CPP_RAII_EXTERNAL_LINEAR_FALLBACK 1\n"
                 "CPP_RAII_EXTERNAL_NONLINEAR_FALLBACK 1\n"
                 "CPP_RAII_EXTERNAL_ODE_STEPPER_FALLBACK 1\n"
                 "CPP_RAII_EXTERNAL_DAE_STEPPER_FALLBACK 1\n"
                 "CPP_RAII_STATUS_EXCEPTION 1\n"
                 "CPP_RAII_ERROR_RECORDS 1\n"
                 "CPP_RAII_ALLOCATOR_BALANCED 1\n";
    return 0;
}
