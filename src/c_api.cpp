#include "smave/c_api.h"

#include "smave/complementarity.hpp"
#include "smave/linear.hpp"
#include "smave/solve_service.hpp"

#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <new>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

struct Allocator {
    smave_allocate_fn allocate{};
    smave_deallocate_fn deallocate{};
    void* user_data{};
};

void* default_allocate(size_t size, void*) {
    return std::malloc(size);
}

void default_deallocate(void* memory, void*) {
    std::free(memory);
}

template <typename Type, typename... Arguments>
Type* allocate_object(const Allocator& allocator, Arguments&&... arguments) {
    void* memory = allocator.allocate(sizeof(Type), allocator.user_data);
    if (memory == nullptr) return nullptr;
    try {
        return new (memory) Type(std::forward<Arguments>(arguments)...);
    } catch (...) {
        allocator.deallocate(memory, allocator.user_data);
        throw;
    }
}

template <typename Type>
void deallocate_object(const Allocator& allocator, Type* object) {
    if (object == nullptr) return;
    object->~Type();
    allocator.deallocate(object, allocator.user_data);
}

bool finite_values(const std::vector<double>& values) {
    return std::all_of(values.begin(), values.end(), [](double value) {
        return std::isfinite(value);
    });
}

smave_diagnostic_code c_diagnostic_code(smave::VerifiedSolveDiagnosticCode code) {
    switch (code) {
        case smave::VerifiedSolveDiagnosticCode::success:
            return SMAVE_DIAGNOSTIC_SUCCESS;
        case smave::VerifiedSolveDiagnosticCode::invalid_contract:
            return SMAVE_DIAGNOSTIC_INVALID_CONTRACT;
        case smave::VerifiedSolveDiagnosticCode::callback_failure:
            return SMAVE_DIAGNOSTIC_CALLBACK_FAILURE;
        case smave::VerifiedSolveDiagnosticCode::numerical_failure:
            return SMAVE_DIAGNOSTIC_NUMERICAL_FAILURE;
        case smave::VerifiedSolveDiagnosticCode::original_gate_rejected:
            return SMAVE_DIAGNOSTIC_ORIGINAL_GATE_REJECTED;
        case smave::VerifiedSolveDiagnosticCode::iteration_limit:
            return SMAVE_DIAGNOSTIC_ITERATION_LIMIT;
        case smave::VerifiedSolveDiagnosticCode::event_reinit_callback_failure:
            return SMAVE_DIAGNOSTIC_EVENT_REINIT_CALLBACK_FAILURE;
        case smave::VerifiedSolveDiagnosticCode::event_reinit_consistency_rejected:
            return SMAVE_DIAGNOSTIC_EVENT_REINIT_CONSISTENCY_REJECTED;
        case smave::VerifiedSolveDiagnosticCode::event_guard_not_released:
            return SMAVE_DIAGNOSTIC_EVENT_GUARD_NOT_RELEASED;
        case smave::VerifiedSolveDiagnosticCode::event_reset_conflict:
            return SMAVE_DIAGNOSTIC_EVENT_RESET_CONFLICT;
        case smave::VerifiedSolveDiagnosticCode::cancelled:
            return SMAVE_DIAGNOSTIC_CANCELLED;
    }
    return SMAVE_DIAGNOSTIC_NUMERICAL_FAILURE;
}

enum class ProblemKind {
    linear,
    complementarity,
    block_graph,
    nonlinear,
    ode,
    dae,
    hybrid,
    hybrid_dae,
};

struct BlockGraphSystem {
    smave::VerifiedBlockGraphProblem problem;
};

std::string complementarity_fingerprint(
    const smave::ComplementarityIR& model) {
    std::uint64_t hash = 1469598103934665603ULL;
    const auto append = [&hash](std::uint64_t value) {
        for (unsigned shift = 0; shift < 64; shift += 8) {
            hash ^= static_cast<std::uint8_t>(value >> shift);
            hash *= 1099511628211ULL;
        }
    };
    append(model.variables.size());
    for (const auto& variable : model.variables) append(std::bit_cast<std::uint64_t>(variable.start));
    for (const auto& row : model.matrix) {
        for (const double value : row) append(std::bit_cast<std::uint64_t>(value));
    }
    for (const double value : model.offset) append(std::bit_cast<std::uint64_t>(value));
    std::ostringstream output;
    output << "c-api-lcp-" << std::hex << std::setfill('0') << std::setw(16) << hash;
    return output.str();
}

struct NonlinearSystem {
    size_t dimension{};
    std::vector<double> initial_state;
    smave_nonlinear_residual_fn residual{};
    smave_nonlinear_jacobian_fn jacobian{};
    void* user_data{};
};

struct OdeSystem {
    struct Event {
        int direction{};
        int priority{};
        smave_event_guard_fn guard{};
        smave_event_reset_fn reset{};
        void* user_data{};
    };
    size_t dimension{};
    std::vector<double> initial_state;
    double start_time{};
    double end_time{};
    double maximum_step{};
    smave_ode_rhs_fn right_hand_side{};
    void* user_data{};
    std::vector<Event> events;
};

struct DaeSystem {
    struct Event {
        int direction{};
        int priority{};
        smave_dae_event_guard_fn guard{};
        smave_dae_event_reset_fn reset{};
        void* user_data{};
    };
    size_t dimension{};
    std::vector<std::uint8_t> differential_mask;
    std::vector<double> initial_state;
    std::vector<double> initial_derivative;
    double start_time{};
    double end_time{};
    double maximum_step{};
    smave_dae_residual_fn residual{};
    smave_dae_jacobian_fn jacobian{};
    void* user_data{};
    std::vector<Event> events;
};

struct HybridSystem {
    struct Mode {
        smave_ode_rhs_fn right_hand_side{};
        void* user_data{};
    };
    struct Transition {
        size_t source_mode{};
        size_t target_mode{};
        int direction{};
        int priority{};
        smave_event_guard_fn guard{};
        smave_event_reset_fn reset{};
        void* user_data{};
        smave_hybrid_stable_reset_fn stable_reset{};
        std::vector<std::uint8_t> write_mask;
    };
    size_t dimension{};
    std::vector<double> initial_state;
    size_t initial_mode{};
    double start_time{};
    double end_time{};
    double maximum_step{};
    std::vector<Mode> modes;
    std::vector<Transition> transitions;
};

struct HybridDaeSystem {
    struct Mode {
        smave_dae_residual_fn residual{};
        smave_dae_jacobian_fn jacobian{};
        void* user_data{};
    };
    struct Transition {
        size_t source_mode{};
        size_t target_mode{};
        int direction{};
        int priority{};
        smave_dae_event_guard_fn guard{};
        smave_dae_event_reset_fn reset{};
        void* user_data{};
        smave_hybrid_dae_stable_reset_fn stable_reset{};
        std::vector<std::uint8_t> state_write_mask;
        std::vector<std::uint8_t> derivative_write_mask;
    };
    size_t dimension{};
    std::vector<std::uint8_t> differential_mask;
    std::vector<double> initial_state;
    std::vector<double> initial_derivative;
    size_t initial_mode{};
    double start_time{};
    double end_time{};
    double maximum_step{};
    std::vector<Mode> modes;
    std::vector<Transition> transitions;
};

smave::VerifiedNonlinearSolveProblem nonlinear_service_problem(
    const NonlinearSystem& system) {
    smave::VerifiedNonlinearSolveProblem problem;
    problem.initial_state = system.initial_state;
    problem.residual = [&system](
        const std::vector<double>& state,
        std::vector<double>& residual) {
        if (state.size() != system.dimension || !finite_values(state)) return false;
        residual.assign(system.dimension, 0.0);
        return system.residual(
                   system.dimension, state.data(), residual.data(), system.user_data) == 0 &&
            finite_values(residual);
    };
    if (system.jacobian != nullptr) {
        problem.jacobian = [&system](
            const std::vector<double>& state,
            std::vector<std::vector<double>>& jacobian) {
            if (state.size() != system.dimension || !finite_values(state)) return false;
            std::vector<double> packed(system.dimension * system.dimension);
            if (system.jacobian(
                    system.dimension, state.data(), packed.data(), system.user_data) != 0 ||
                !finite_values(packed)) return false;
            jacobian.assign(system.dimension, std::vector<double>(system.dimension));
            for (size_t row = 0; row < system.dimension; ++row) {
                std::copy_n(
                    packed.begin() + static_cast<std::ptrdiff_t>(row * system.dimension),
                    system.dimension, jacobian[row].begin());
            }
            return true;
        };
    }
    return problem;
}

smave::VerifiedOdeSolveProblem ode_service_problem(const OdeSystem& system) {
    smave::VerifiedOdeSolveProblem problem;
    problem.initial_state = system.initial_state;
    problem.right_hand_side = [&system](
        double time,
        const std::vector<double>& state,
        std::vector<double>& derivative) {
        if (state.size() != system.dimension || !finite_values(state)) return false;
        derivative.assign(system.dimension, 0.0);
        return system.right_hand_side(
                   system.dimension, time, state.data(), derivative.data(), system.user_data) == 0 &&
            finite_values(derivative);
    };
    for (const auto& source : system.events) {
        const size_t dimension = system.dimension;
        const smave_event_guard_fn guard_callback = source.guard;
        const smave_event_reset_fn reset_callback = source.reset;
        void* const event_user_data = source.user_data;
        smave::VerifiedOdeEvent event;
        event.direction = source.direction;
        event.priority = source.priority;
        event.guard = [dimension, guard_callback, event_user_data](
            double time,
            const std::vector<double>& state,
            double& guard) {
            return state.size() == dimension && finite_values(state) &&
                guard_callback(
                    dimension,
                    time,
                    state.data(),
                    &guard,
                    event_user_data) == 0 && std::isfinite(guard);
        };
        event.reset = [dimension, reset_callback, event_user_data](
            double time,
            const std::vector<double>& pre_state,
            std::vector<double>& post_state) {
            if (pre_state.size() != dimension || !finite_values(pre_state)) return false;
            post_state.assign(dimension, 0.0);
            return reset_callback(
                    dimension,
                    time,
                    pre_state.data(),
                    post_state.data(),
                    event_user_data) == 0 && finite_values(post_state);
        };
        problem.events.push_back(std::move(event));
    }
    return problem;
}

smave::VerifiedDaeSolveProblem dae_service_problem(const DaeSystem& system) {
    smave::VerifiedDaeSolveProblem problem;
    problem.differential_mask = system.differential_mask;
    problem.initial_state = system.initial_state;
    problem.initial_derivative = system.initial_derivative;
    problem.residual = [&system](
        double time,
        const std::vector<double>& state,
        const std::vector<double>& derivative,
        std::vector<double>& residual) {
        if (state.size() != system.dimension || derivative.size() != system.dimension ||
            !finite_values(state) || !finite_values(derivative)) return false;
        residual.assign(system.dimension, 0.0);
        return system.residual(
                   system.dimension,
                   time,
                   state.data(),
                   derivative.data(),
                   residual.data(),
                   system.user_data) == 0 && finite_values(residual);
    };
    if (system.jacobian != nullptr) {
        problem.jacobian = [&system](
            double time,
            const std::vector<double>& state,
            const std::vector<double>& derivative,
            double derivative_scale,
            std::vector<std::vector<double>>& jacobian) {
            std::vector<double> packed(system.dimension * system.dimension);
            if (system.jacobian(
                    system.dimension,
                    time,
                    state.data(),
                    derivative.data(),
                    derivative_scale,
                    packed.data(),
                    system.user_data) != 0 || !finite_values(packed)) return false;
            jacobian.assign(system.dimension, std::vector<double>(system.dimension));
            for (size_t row = 0; row < system.dimension; ++row) {
                std::copy_n(
                    packed.begin() + static_cast<std::ptrdiff_t>(row * system.dimension),
                    system.dimension,
                    jacobian[row].begin());
            }
            return true;
        };
    }
    for (const auto& source : system.events) {
        const size_t dimension = system.dimension;
        const smave_dae_event_guard_fn guard_callback = source.guard;
        const smave_dae_event_reset_fn reset_callback = source.reset;
        void* const event_user_data = source.user_data;
        smave::VerifiedDaeEvent event;
        event.direction = source.direction;
        event.priority = source.priority;
        event.guard = [dimension, guard_callback, event_user_data](
            double time,
            const std::vector<double>& state,
            const std::vector<double>& derivative,
            double& guard) {
            if (state.size() != dimension || derivative.size() != dimension ||
                !finite_values(state) || !finite_values(derivative)) return false;
            return guard_callback(
                       dimension,
                       time,
                       state.data(),
                       derivative.data(),
                       &guard,
                       event_user_data) == 0 && std::isfinite(guard);
        };
        event.reset = [dimension, reset_callback, event_user_data](
            double time,
            const std::vector<double>& pre_state,
            const std::vector<double>& pre_derivative,
            std::vector<double>& post_state,
            std::vector<double>& post_derivative) {
            if (pre_state.size() != dimension || pre_derivative.size() != dimension ||
                !finite_values(pre_state) || !finite_values(pre_derivative)) return false;
            post_state.assign(dimension, 0.0);
            post_derivative.assign(dimension, 0.0);
            return reset_callback(
                       dimension,
                       time,
                       pre_state.data(),
                       pre_derivative.data(),
                       post_state.data(),
                       post_derivative.data(),
                       event_user_data) == 0 &&
                finite_values(post_state) && finite_values(post_derivative);
        };
        problem.events.push_back(std::move(event));
    }
    return problem;
}

smave::VerifiedHybridSolveProblem hybrid_service_problem(const HybridSystem& system) {
    smave::VerifiedHybridSolveProblem problem;
    problem.initial_state = system.initial_state;
    problem.initial_mode = system.initial_mode;
    problem.modes.reserve(system.modes.size());
    for (const auto& source : system.modes) {
        const size_t dimension = system.dimension;
        const smave_ode_rhs_fn callback = source.right_hand_side;
        void* const user_data = source.user_data;
        smave::VerifiedHybridMode mode;
        mode.right_hand_side = [dimension, callback, user_data](
            double time,
            const std::vector<double>& state,
            std::vector<double>& derivative) {
            if (state.size() != dimension || !finite_values(state)) return false;
            derivative.assign(dimension, 0.0);
            return callback(
                       dimension, time, state.data(), derivative.data(), user_data) == 0 &&
                finite_values(derivative);
        };
        problem.modes.push_back(std::move(mode));
    }
    problem.transitions.reserve(system.transitions.size());
    for (const auto& source : system.transitions) {
        const size_t dimension = system.dimension;
        const smave_event_guard_fn guard_callback = source.guard;
        const smave_event_reset_fn reset_callback = source.reset;
        const smave_hybrid_stable_reset_fn stable_reset_callback = source.stable_reset;
        void* const user_data = source.user_data;
        smave::VerifiedHybridTransition transition;
        transition.source_mode = source.source_mode;
        transition.target_mode = source.target_mode;
        transition.direction = source.direction;
        transition.priority = source.priority;
        transition.guard = [dimension, guard_callback, user_data](
            double time, const std::vector<double>& state, double& guard) {
            return state.size() == dimension && finite_values(state) &&
                guard_callback(
                    dimension, time, state.data(), &guard, user_data) == 0 &&
                std::isfinite(guard);
        };
        transition.reset = [dimension, reset_callback, stable_reset_callback, user_data](
            double time,
            const std::vector<double>& pre_state,
            std::vector<double>& post_state) {
            if (pre_state.size() != dimension || !finite_values(pre_state)) return false;
            post_state.assign(dimension, 0.0);
            const int32_t status = reset_callback != nullptr
                ? reset_callback(
                    dimension,
                    time,
                    pre_state.data(),
                    post_state.data(),
                    user_data)
                : stable_reset_callback(
                    dimension,
                    time,
                    pre_state.data(),
                    pre_state.data(),
                    post_state.data(),
                    user_data);
            return status == 0 && finite_values(post_state);
        };
        if (stable_reset_callback != nullptr) {
            transition.stable_reset = [dimension, stable_reset_callback, user_data](
                double time,
                const std::vector<double>& stable_pre_state,
                const std::vector<double>& current_state,
                std::vector<double>& proposed_state) {
                if (stable_pre_state.size() != dimension ||
                    current_state.size() != dimension ||
                    !finite_values(stable_pre_state) || !finite_values(current_state)) {
                    return false;
                }
                proposed_state.assign(dimension, 0.0);
                return stable_reset_callback(
                           dimension,
                           time,
                           stable_pre_state.data(),
                           current_state.data(),
                           proposed_state.data(),
                           user_data) == 0 && finite_values(proposed_state);
            };
            transition.write_mask = source.write_mask;
        }
        problem.transitions.push_back(std::move(transition));
    }
    return problem;
}

smave::VerifiedHybridDaeSolveProblem hybrid_dae_service_problem(
    const HybridDaeSystem& system) {
    smave::VerifiedHybridDaeSolveProblem problem;
    problem.differential_mask = system.differential_mask;
    problem.initial_state = system.initial_state;
    problem.initial_derivative = system.initial_derivative;
    problem.initial_mode = system.initial_mode;
    problem.modes.reserve(system.modes.size());
    for (const auto& source : system.modes) {
        const size_t dimension = system.dimension;
        const smave_dae_residual_fn residual_callback = source.residual;
        const smave_dae_jacobian_fn jacobian_callback = source.jacobian;
        void* const user_data = source.user_data;
        smave::VerifiedHybridDaeMode mode;
        mode.residual = [dimension, residual_callback, user_data](
            double time,
            const std::vector<double>& state,
            const std::vector<double>& derivative,
            std::vector<double>& residual) {
            if (state.size() != dimension || derivative.size() != dimension ||
                !finite_values(state) || !finite_values(derivative)) return false;
            residual.assign(dimension, 0.0);
            return residual_callback(
                       dimension,
                       time,
                       state.data(),
                       derivative.data(),
                       residual.data(),
                       user_data) == 0 && finite_values(residual);
        };
        if (jacobian_callback != nullptr) {
            mode.jacobian = [dimension, jacobian_callback, user_data](
                double time,
                const std::vector<double>& state,
                const std::vector<double>& derivative,
                double derivative_scale,
                std::vector<std::vector<double>>& jacobian) {
                if (state.size() != dimension || derivative.size() != dimension ||
                    !finite_values(state) || !finite_values(derivative) ||
                    !std::isfinite(derivative_scale)) return false;
                std::vector<double> row_major(dimension * dimension);
                if (jacobian_callback(
                        dimension,
                        time,
                        state.data(),
                        derivative.data(),
                        derivative_scale,
                        row_major.data(),
                        user_data) != 0 || !finite_values(row_major)) return false;
                jacobian.assign(dimension, std::vector<double>(dimension));
                for (size_t row = 0; row < dimension; ++row) {
                    std::copy_n(
                        row_major.data() + row * dimension,
                        dimension,
                        jacobian[row].begin());
                }
                return true;
            };
        }
        problem.modes.push_back(std::move(mode));
    }
    problem.transitions.reserve(system.transitions.size());
    for (const auto& source : system.transitions) {
        const size_t dimension = system.dimension;
        const smave_dae_event_guard_fn guard_callback = source.guard;
        const smave_dae_event_reset_fn reset_callback = source.reset;
        const smave_hybrid_dae_stable_reset_fn stable_reset_callback =
            source.stable_reset;
        void* const user_data = source.user_data;
        smave::VerifiedHybridDaeTransition transition;
        transition.source_mode = source.source_mode;
        transition.target_mode = source.target_mode;
        transition.direction = source.direction;
        transition.priority = source.priority;
        transition.guard = [dimension, guard_callback, user_data](
            double time,
            const std::vector<double>& state,
            const std::vector<double>& derivative,
            double& guard) {
            return state.size() == dimension && derivative.size() == dimension &&
                finite_values(state) && finite_values(derivative) &&
                guard_callback(
                    dimension,
                    time,
                    state.data(),
                    derivative.data(),
                    &guard,
                    user_data) == 0 && std::isfinite(guard);
        };
        transition.reset = [dimension, reset_callback, stable_reset_callback, user_data](
            double time,
            const std::vector<double>& pre_state,
            const std::vector<double>& pre_derivative,
            std::vector<double>& post_state,
            std::vector<double>& post_derivative) {
            if (pre_state.size() != dimension ||
                pre_derivative.size() != dimension || !finite_values(pre_state) ||
                !finite_values(pre_derivative)) return false;
            post_state.assign(dimension, 0.0);
            post_derivative.assign(dimension, 0.0);
            const int32_t status = reset_callback != nullptr
                ? reset_callback(
                    dimension,
                    time,
                    pre_state.data(),
                    pre_derivative.data(),
                    post_state.data(),
                    post_derivative.data(),
                    user_data)
                : stable_reset_callback(
                    dimension,
                    time,
                    pre_state.data(),
                    pre_derivative.data(),
                    pre_state.data(),
                    pre_derivative.data(),
                    post_state.data(),
                    post_derivative.data(),
                    user_data);
            return status == 0 && finite_values(post_state) &&
                finite_values(post_derivative);
        };
        if (stable_reset_callback != nullptr) {
            transition.stable_reset = [dimension, stable_reset_callback, user_data](
                double time,
                const std::vector<double>& stable_pre_state,
                const std::vector<double>& stable_pre_derivative,
                const std::vector<double>& current_state,
                const std::vector<double>& current_derivative,
                std::vector<double>& proposed_state,
                std::vector<double>& proposed_derivative) {
                if (stable_pre_state.size() != dimension ||
                    stable_pre_derivative.size() != dimension ||
                    current_state.size() != dimension ||
                    current_derivative.size() != dimension ||
                    !finite_values(stable_pre_state) ||
                    !finite_values(stable_pre_derivative) ||
                    !finite_values(current_state) || !finite_values(current_derivative)) {
                    return false;
                }
                proposed_state.assign(dimension, 0.0);
                proposed_derivative.assign(dimension, 0.0);
                return stable_reset_callback(
                           dimension,
                           time,
                           stable_pre_state.data(),
                           stable_pre_derivative.data(),
                           current_state.data(),
                           current_derivative.data(),
                           proposed_state.data(),
                           proposed_derivative.data(),
                           user_data) == 0 && finite_values(proposed_state) &&
                    finite_values(proposed_derivative);
            };
            transition.state_write_mask = source.state_write_mask;
            transition.derivative_write_mask = source.derivative_write_mask;
        }
        problem.transitions.push_back(std::move(transition));
    }
    return problem;
}

}  // namespace

struct smave_library {
    smave_library(Allocator value, std::uint64_t identifier)
        : allocator(value), id(identifier) {}
    Allocator allocator;
    std::uint64_t id{};
    std::atomic<std::uint64_t> next_trace_id{1};
    std::atomic<size_t> children{};
};

struct smave_problem {
    smave_problem(smave_library* owner_value, smave::LinearSystem system_value)
        : owner(owner_value), kind(ProblemKind::linear), linear(std::move(system_value)) {}
    smave_problem(smave_library* owner_value, smave::ComplementarityIR system_value)
        : owner(owner_value), kind(ProblemKind::complementarity),
          complementarity(std::move(system_value)) {}
    smave_problem(smave_library* owner_value, BlockGraphSystem system_value)
        : owner(owner_value), kind(ProblemKind::block_graph),
          block_graph(std::move(system_value)) {}
    smave_problem(smave_library* owner_value, NonlinearSystem system_value)
        : owner(owner_value), kind(ProblemKind::nonlinear), nonlinear(std::move(system_value)) {}
    smave_problem(smave_library* owner_value, OdeSystem system_value)
        : owner(owner_value), kind(ProblemKind::ode), ode(std::move(system_value)) {}
    smave_problem(smave_library* owner_value, DaeSystem system_value)
        : owner(owner_value), kind(ProblemKind::dae), dae(std::move(system_value)) {}
    smave_problem(smave_library* owner_value, HybridSystem system_value)
        : owner(owner_value), kind(ProblemKind::hybrid), hybrid(std::move(system_value)) {}
    smave_problem(smave_library* owner_value, HybridDaeSystem system_value)
        : owner(owner_value),
          kind(ProblemKind::hybrid_dae),
          hybrid_dae(std::move(system_value)) {}
    smave_library* owner{};
    ProblemKind kind{ProblemKind::linear};
    smave::LinearSystem linear;
    smave::ComplementarityIR complementarity;
    BlockGraphSystem block_graph;
    NonlinearSystem nonlinear;
    OdeSystem ode;
    DaeSystem dae;
    HybridSystem hybrid;
    HybridDaeSystem hybrid_dae;
    std::atomic<size_t> children{};
    bool finalized{};
};

struct smave_solver {
    smave_solver(
        smave_problem* problem_value,
        double absolute_tolerance_value,
        double relative_tolerance_value,
        int maximum_iterations_value)
        : problem(problem_value),
          absolute_tolerance(absolute_tolerance_value),
          relative_tolerance(relative_tolerance_value),
          maximum_iterations(maximum_iterations_value) {}
    smave_problem* problem{};
    double absolute_tolerance{};
    double relative_tolerance{};
    int maximum_iterations{};
};

struct smave_cancel_token {
    explicit smave_cancel_token(smave_library* owner_value) : owner(owner_value) {}
    smave_library* owner{};
    std::atomic<bool> requested{};
    mutable std::atomic<size_t> active_solves{};
};

struct CancelTokenUse {
    explicit CancelTokenUse(const smave_cancel_token* value) : token(value) {
        if (token != nullptr) ++token->active_solves;
    }
    ~CancelTokenUse() {
        if (token != nullptr) --token->active_solves;
    }
    const smave_cancel_token* token{};
};

struct smave_result {
    explicit smave_result(smave_library* owner_value) : owner(owner_value) {}
    smave_library* owner{};
    std::vector<double> solution;
    std::vector<double> complementarity_gap;
    std::vector<size_t> block_output_offsets;
    std::vector<size_t> block_commit_order;
    std::string service_id;
    std::string backend;
    std::string equation_family;
    std::string plan_id;
    std::string diagnostic;
    smave_diagnostic_code diagnostic_code{SMAVE_DIAGNOSTIC_INVALID_CONTRACT};
    double residual_inf{};
    double backward_error{};
    double final_time{};
    size_t accepted_steps{};
    size_t rejected_steps{};
    size_t event_count{};
    double last_event_time{};
    size_t differentiation_index{1};
    size_t hidden_rank_checks{};
    double minimum_hidden_rank_margin{1.0};
    double maximum_hidden_residual_inf{};
    size_t final_mode{};
    size_t consistency_projection_count{};
    size_t complementarity_attempts{};
    double primal_violation{};
    double dual_violation{};
    double complementarity_violation{};
    double maximum_connection_error{};
    double maximum_original_gate_residual{};
    double maximum_fixed_point_residual{};
    size_t block_graph_ticks{};
    size_t block_graph_node_executions{};
    size_t block_graph_fallback_count{};
    size_t block_graph_fixed_point_components{};
    size_t block_graph_fixed_point_iterations{};
    size_t block_graph_node_count{};
    bool success{};
    bool used_fallback{};
};

namespace {

constexpr std::size_t error_stack_capacity = 8;
std::atomic<std::uint64_t> next_library_id{1};

struct ErrorRecord {
    std::uint64_t trace_id{};
    smave_status status{SMAVE_STATUS_OK};
    std::string operation;
    std::string message;
};

struct ThreadLibraryErrors {
    std::uint64_t library_id{};
    std::vector<ErrorRecord> records;
};

thread_local std::vector<ThreadLibraryErrors> thread_error_stacks;

ThreadLibraryErrors* error_stack(const smave_library* library, bool create) {
    if (library == nullptr) return nullptr;
    const auto found = std::find_if(
        thread_error_stacks.begin(), thread_error_stacks.end(),
        [library](const ThreadLibraryErrors& stack) {
            return stack.library_id == library->id;
        });
    if (found != thread_error_stacks.end()) return &*found;
    if (!create) return nullptr;
    thread_error_stacks.push_back({library->id, {}});
    return &thread_error_stacks.back();
}

smave_status record_error(
    smave_library* library,
    smave_status status,
    const char* operation,
    const char* message) noexcept {
    if (library == nullptr || status == SMAVE_STATUS_OK) return status;
    try {
        ThreadLibraryErrors* stack = error_stack(library, true);
        if (stack == nullptr) return status;
        if (stack->records.size() == error_stack_capacity) {
            stack->records.erase(stack->records.begin());
        }
        stack->records.push_back({
            library->next_trace_id.fetch_add(1, std::memory_order_relaxed),
            status,
            operation == nullptr ? "" : operation,
            message == nullptr ? "" : message,
        });
    } catch (...) {
        // Error reporting must never replace the original status or cross the C ABI.
    }
    return status;
}

}  // namespace

extern "C" {

uint32_t smave_abi_version(void) {
    return SMAVE_ABI_VERSION;
}

const char* smave_version_string(void) {
    return "0.1.0";
}

const char* smave_status_string(smave_status status) {
    switch (status) {
        case SMAVE_STATUS_OK: return "ok";
        case SMAVE_STATUS_INVALID_ARGUMENT: return "invalid argument";
        case SMAVE_STATUS_ABI_MISMATCH: return "ABI mismatch";
        case SMAVE_STATUS_INVALID_STATE: return "invalid state";
        case SMAVE_STATUS_UNSUPPORTED: return "unsupported";
        case SMAVE_STATUS_SOLVE_FAILED: return "solve failed";
        case SMAVE_STATUS_BUFFER_TOO_SMALL: return "buffer too small";
        case SMAVE_STATUS_INTERNAL_ERROR: return "internal error";
        case SMAVE_STATUS_CANCELLED: return "cancelled";
        case SMAVE_STATUS_DEADLINE_EXCEEDED: return "deadline exceeded";
    }
    return "unknown status";
}

smave_status smave_library_create(
    const smave_library_options* options,
    smave_library** library) {
    if (library == nullptr) return SMAVE_STATUS_INVALID_ARGUMENT;
    *library = nullptr;
    Allocator allocator{default_allocate, default_deallocate, nullptr};
    if (options != nullptr) {
        if (options->abi_version != SMAVE_ABI_VERSION) return SMAVE_STATUS_ABI_MISMATCH;
        if (options->struct_size < sizeof(smave_library_options)) {
            return SMAVE_STATUS_INVALID_ARGUMENT;
        }
        if ((options->allocate == nullptr) != (options->deallocate == nullptr)) {
            return SMAVE_STATUS_INVALID_ARGUMENT;
        }
        if (options->allocate != nullptr) {
            allocator = {options->allocate, options->deallocate, options->allocator_user_data};
        }
    }
    try {
        *library = allocate_object<smave_library>(
            allocator, allocator,
            next_library_id.fetch_add(1, std::memory_order_relaxed));
        return *library == nullptr ? SMAVE_STATUS_INTERNAL_ERROR : SMAVE_STATUS_OK;
    } catch (...) {
        return SMAVE_STATUS_INTERNAL_ERROR;
    }
}

smave_status smave_library_destroy(smave_library* library) {
    if (library == nullptr) return SMAVE_STATUS_OK;
    if (library->children.load() != 0) {
        return record_error(
            library, SMAVE_STATUS_INVALID_STATE, "smave_library_destroy",
            "library still owns live child handles");
    }
    const Allocator allocator = library->allocator;
    thread_error_stacks.erase(
        std::remove_if(
            thread_error_stacks.begin(), thread_error_stacks.end(),
            [library](const ThreadLibraryErrors& stack) {
                return stack.library_id == library->id;
            }),
        thread_error_stacks.end());
    deallocate_object(allocator, library);
    return SMAVE_STATUS_OK;
}

smave_status smave_library_has_capability(
    const smave_library* library,
    smave_capability capability,
    int32_t* available) {
    if (library == nullptr || available == nullptr) {
        if (library != nullptr) {
            return record_error(
                const_cast<smave_library*>(library), SMAVE_STATUS_INVALID_ARGUMENT,
                "smave_library_has_capability", "capability output is null");
        }
        return SMAVE_STATUS_INVALID_ARGUMENT;
    }
    *available = capability == SMAVE_CAPABILITY_LINEAR_DENSE ||
        capability == SMAVE_CAPABILITY_LINEAR_CSR ||
        capability == SMAVE_CAPABILITY_NONLINEAR ||
        capability == SMAVE_CAPABILITY_ODE ||
        capability == SMAVE_CAPABILITY_DAE ||
        capability == SMAVE_CAPABILITY_EVENTS ||
        capability == SMAVE_CAPABILITY_COMPLEMENTARITY ||
        capability == SMAVE_CAPABILITY_MULTIPHYSICS ||
        capability == SMAVE_CAPABILITY_HYBRID ||
        capability == SMAVE_CAPABILITY_HYBRID_DAE ||
        capability == SMAVE_CAPABILITY_INDEX_TWO_DAE ||
        capability == SMAVE_CAPABILITY_CANCELLATION ||
        capability == SMAVE_CAPABILITY_DEADLINE ||
        capability == SMAVE_CAPABILITY_EXTERNAL_LINEAR_FALLBACK ||
        capability == SMAVE_CAPABILITY_EXTERNAL_NONLINEAR_FALLBACK ||
        capability == SMAVE_CAPABILITY_EXTERNAL_ODE_STEPPER_FALLBACK ||
        capability == SMAVE_CAPABILITY_EXTERNAL_DAE_STEPPER_FALLBACK ||
        capability == SMAVE_CAPABILITY_ERROR_STACK;
    return SMAVE_STATUS_OK;
}

smave_status smave_library_get_error_count(
    const smave_library* library,
    size_t* count) {
    if (library == nullptr || count == nullptr) return SMAVE_STATUS_INVALID_ARGUMENT;
    const ThreadLibraryErrors* stack = error_stack(library, false);
    *count = stack == nullptr ? 0 : stack->records.size();
    return SMAVE_STATUS_OK;
}

smave_status smave_library_get_error(
    const smave_library* library,
    size_t newest_index,
    smave_error_info* info) {
    if (library == nullptr || info == nullptr) return SMAVE_STATUS_INVALID_ARGUMENT;
    if (info->abi_version != SMAVE_ABI_VERSION) return SMAVE_STATUS_ABI_MISMATCH;
    if (info->struct_size < sizeof(smave_error_info)) {
        return SMAVE_STATUS_INVALID_ARGUMENT;
    }
    const ThreadLibraryErrors* stack = error_stack(library, false);
    if (stack == nullptr || newest_index >= stack->records.size()) {
        return SMAVE_STATUS_INVALID_ARGUMENT;
    }
    const ErrorRecord& record =
        stack->records[stack->records.size() - 1 - newest_index];
    info->trace_id = record.trace_id;
    info->status = record.status;
    info->operation = record.operation.c_str();
    info->message = record.message.c_str();
    return SMAVE_STATUS_OK;
}

smave_status smave_library_clear_errors(smave_library* library) {
    if (library == nullptr) return SMAVE_STATUS_INVALID_ARGUMENT;
    if (ThreadLibraryErrors* stack = error_stack(library, false)) {
        stack->records.clear();
    }
    return SMAVE_STATUS_OK;
}

smave_status smave_dae_problem_create(
    smave_library* library,
    const smave_dae_problem_desc* descriptor,
    smave_problem** problem) {
    if (library == nullptr || descriptor == nullptr || problem == nullptr) {
        if (library != nullptr) {
            return record_error(
                library, SMAVE_STATUS_INVALID_ARGUMENT, "smave_dae_problem_create",
                "descriptor or output handle is null");
        }
        return SMAVE_STATUS_INVALID_ARGUMENT;
    }
    *problem = nullptr;
    if (descriptor->abi_version != SMAVE_ABI_VERSION) {
        return record_error(
            library, SMAVE_STATUS_ABI_MISMATCH, "smave_dae_problem_create",
            "descriptor ABI version is incompatible");
    }
    if (descriptor->struct_size < sizeof(smave_dae_problem_desc) ||
        descriptor->dimension == 0 || descriptor->initial_state == nullptr ||
        descriptor->initial_derivative == nullptr || descriptor->residual == nullptr ||
        !std::isfinite(descriptor->start_time) ||
        !std::isfinite(descriptor->end_time) ||
        !std::isfinite(descriptor->maximum_step) ||
        descriptor->end_time <= descriptor->start_time || descriptor->maximum_step <= 0.0) {
        return record_error(
            library, SMAVE_STATUS_INVALID_ARGUMENT, "smave_dae_problem_create",
            "descriptor shape, callbacks, or time interval is invalid");
    }
    try {
        DaeSystem system;
        system.dimension = descriptor->dimension;
        if (descriptor->differential_mask == nullptr) {
            return record_error(
                library, SMAVE_STATUS_INVALID_ARGUMENT, "smave_dae_problem_create",
                "differential mask is null");
        }
        system.differential_mask.assign(
            descriptor->differential_mask,
            descriptor->differential_mask + descriptor->dimension);
        if (std::none_of(
                system.differential_mask.begin(), system.differential_mask.end(),
                [](std::uint8_t value) { return value == 1; }) ||
            std::any_of(
                system.differential_mask.begin(), system.differential_mask.end(),
                [](std::uint8_t value) { return value > 1; })) {
            return record_error(
                library, SMAVE_STATUS_INVALID_ARGUMENT, "smave_dae_problem_create",
                "differential mask is invalid");
        }
        system.initial_state.assign(
            descriptor->initial_state,
            descriptor->initial_state + descriptor->dimension);
        system.initial_derivative.assign(
            descriptor->initial_derivative,
            descriptor->initial_derivative + descriptor->dimension);
        if (!finite_values(system.initial_state) || !finite_values(system.initial_derivative)) {
            return record_error(
                library, SMAVE_STATUS_INVALID_ARGUMENT, "smave_dae_problem_create",
                "initial state or derivative is not finite");
        }
        system.start_time = descriptor->start_time;
        system.end_time = descriptor->end_time;
        system.maximum_step = descriptor->maximum_step;
        system.residual = descriptor->residual;
        system.jacobian = descriptor->jacobian;
        system.user_data = descriptor->user_data;
        std::vector<double> initial_residual;
        const auto service_problem = dae_service_problem(system);
        if (!service_problem.residual(
                system.start_time,
                system.initial_state,
                system.initial_derivative,
                initial_residual) || initial_residual.size() != system.dimension) {
            return record_error(
                library, SMAVE_STATUS_INVALID_ARGUMENT, "smave_dae_problem_create",
                "initial residual callback failed");
        }
        smave_problem* created = allocate_object<smave_problem>(
            library->allocator, library, std::move(system));
        if (created == nullptr) {
            return record_error(
                library, SMAVE_STATUS_INTERNAL_ERROR, "smave_dae_problem_create",
                "allocator failed to create problem handle");
        }
        ++library->children;
        *problem = created;
        return SMAVE_STATUS_OK;
    } catch (...) {
        return record_error(
            library, SMAVE_STATUS_INTERNAL_ERROR, "smave_dae_problem_create",
            "unexpected exception while copying descriptor");
    }
}

smave_status smave_event_dae_problem_create(
    smave_library* library,
    const smave_event_dae_problem_desc* descriptor,
    smave_problem** problem) {
    if (library == nullptr || descriptor == nullptr || problem == nullptr) {
        return SMAVE_STATUS_INVALID_ARGUMENT;
    }
    *problem = nullptr;
    if (descriptor->abi_version != SMAVE_ABI_VERSION) return SMAVE_STATUS_ABI_MISMATCH;
    if (descriptor->struct_size < sizeof(smave_event_dae_problem_desc) ||
        descriptor->dimension == 0 || descriptor->differential_mask == nullptr ||
        descriptor->initial_state == nullptr || descriptor->initial_derivative == nullptr ||
        descriptor->residual == nullptr || descriptor->events == nullptr ||
        descriptor->event_count == 0 || !std::isfinite(descriptor->start_time) ||
        !std::isfinite(descriptor->end_time) ||
        !std::isfinite(descriptor->maximum_step) ||
        descriptor->end_time <= descriptor->start_time || descriptor->maximum_step <= 0.0) {
        return SMAVE_STATUS_INVALID_ARGUMENT;
    }
    try {
        DaeSystem system;
        system.dimension = descriptor->dimension;
        system.differential_mask.assign(
            descriptor->differential_mask,
            descriptor->differential_mask + descriptor->dimension);
        if (std::none_of(
                system.differential_mask.begin(), system.differential_mask.end(),
                [](std::uint8_t value) { return value == 1; }) ||
            std::any_of(
                system.differential_mask.begin(), system.differential_mask.end(),
                [](std::uint8_t value) { return value > 1; })) {
            return SMAVE_STATUS_INVALID_ARGUMENT;
        }
        system.initial_state.assign(
            descriptor->initial_state,
            descriptor->initial_state + descriptor->dimension);
        system.initial_derivative.assign(
            descriptor->initial_derivative,
            descriptor->initial_derivative + descriptor->dimension);
        if (!finite_values(system.initial_state) || !finite_values(system.initial_derivative)) {
            return SMAVE_STATUS_INVALID_ARGUMENT;
        }
        system.start_time = descriptor->start_time;
        system.end_time = descriptor->end_time;
        system.maximum_step = descriptor->maximum_step;
        system.residual = descriptor->residual;
        system.jacobian = descriptor->jacobian;
        system.user_data = descriptor->residual_user_data;
        system.events.reserve(descriptor->event_count);
        for (size_t index = 0; index < descriptor->event_count; ++index) {
            const auto& source = descriptor->events[index];
            if (source.abi_version != SMAVE_ABI_VERSION ||
                source.struct_size < sizeof(smave_dae_event_desc) ||
                source.direction < -1 || source.direction > 1 ||
                source.guard == nullptr || source.reset == nullptr) {
                return source.abi_version != SMAVE_ABI_VERSION
                    ? SMAVE_STATUS_ABI_MISMATCH : SMAVE_STATUS_INVALID_ARGUMENT;
            }
            system.events.push_back({
                .direction = source.direction,
                .priority = source.priority,
                .guard = source.guard,
                .reset = source.reset,
                .user_data = source.user_data,
            });
        }
        const auto service_problem = dae_service_problem(system);
        std::vector<double> initial_residual;
        if (!service_problem.residual(
                system.start_time,
                system.initial_state,
                system.initial_derivative,
                initial_residual) || initial_residual.size() != system.dimension) {
            return SMAVE_STATUS_INVALID_ARGUMENT;
        }
        for (const auto& event : service_problem.events) {
            double guard{};
            if (!event.guard(
                    system.start_time,
                    system.initial_state,
                    system.initial_derivative,
                    guard)) return SMAVE_STATUS_INVALID_ARGUMENT;
        }
        smave_problem* created = allocate_object<smave_problem>(
            library->allocator, library, std::move(system));
        if (created == nullptr) return SMAVE_STATUS_INTERNAL_ERROR;
        ++library->children;
        *problem = created;
        return SMAVE_STATUS_OK;
    } catch (...) {
        return SMAVE_STATUS_INTERNAL_ERROR;
    }
}

smave_status smave_hybrid_problem_create(
    smave_library* library,
    const smave_hybrid_problem_desc* descriptor,
    smave_problem** problem) {
    if (library == nullptr || descriptor == nullptr || problem == nullptr) {
        return SMAVE_STATUS_INVALID_ARGUMENT;
    }
    *problem = nullptr;
    if (descriptor->abi_version != SMAVE_ABI_VERSION) return SMAVE_STATUS_ABI_MISMATCH;
    if (descriptor->struct_size < sizeof(smave_hybrid_problem_desc) ||
        descriptor->dimension == 0 || descriptor->initial_state == nullptr ||
        descriptor->modes == nullptr || descriptor->mode_count == 0 ||
        descriptor->initial_mode >= descriptor->mode_count ||
        descriptor->transitions == nullptr || descriptor->transition_count == 0 ||
        !std::isfinite(descriptor->start_time) ||
        !std::isfinite(descriptor->end_time) ||
        !std::isfinite(descriptor->maximum_step) ||
        descriptor->end_time <= descriptor->start_time || descriptor->maximum_step <= 0.0) {
        return SMAVE_STATUS_INVALID_ARGUMENT;
    }
    try {
        HybridSystem system;
        system.dimension = descriptor->dimension;
        system.initial_state.assign(
            descriptor->initial_state,
            descriptor->initial_state + descriptor->dimension);
        if (!finite_values(system.initial_state)) return SMAVE_STATUS_INVALID_ARGUMENT;
        system.initial_mode = descriptor->initial_mode;
        system.start_time = descriptor->start_time;
        system.end_time = descriptor->end_time;
        system.maximum_step = descriptor->maximum_step;
        system.modes.reserve(descriptor->mode_count);
        for (size_t index = 0; index < descriptor->mode_count; ++index) {
            const auto& source = descriptor->modes[index];
            if (source.abi_version != SMAVE_ABI_VERSION ||
                source.struct_size < sizeof(smave_hybrid_mode_desc) ||
                source.right_hand_side == nullptr) {
                return source.abi_version != SMAVE_ABI_VERSION
                    ? SMAVE_STATUS_ABI_MISMATCH : SMAVE_STATUS_INVALID_ARGUMENT;
            }
            system.modes.push_back({source.right_hand_side, source.user_data});
        }
        system.transitions.reserve(descriptor->transition_count);
        constexpr size_t legacy_transition_size =
            offsetof(smave_hybrid_transition_desc, stable_reset);
        for (size_t index = 0; index < descriptor->transition_count; ++index) {
            const auto& source = descriptor->transitions[index];
            const bool extended = source.struct_size >= sizeof(smave_hybrid_transition_desc);
            const smave_hybrid_stable_reset_fn stable_reset =
                extended ? source.stable_reset : nullptr;
            const uint8_t* write_mask = extended ? source.write_mask : nullptr;
            if (source.abi_version != SMAVE_ABI_VERSION ||
                source.struct_size < legacy_transition_size ||
                source.source_mode >= descriptor->mode_count ||
                source.target_mode >= descriptor->mode_count ||
                source.direction < -1 || source.direction > 1 ||
                source.guard == nullptr ||
                (source.reset == nullptr && stable_reset == nullptr) ||
                ((stable_reset == nullptr) != (write_mask == nullptr))) {
                return source.abi_version != SMAVE_ABI_VERSION
                    ? SMAVE_STATUS_ABI_MISMATCH : SMAVE_STATUS_INVALID_ARGUMENT;
            }
            HybridSystem::Transition transition;
            transition.source_mode = source.source_mode;
            transition.target_mode = source.target_mode;
            transition.direction = source.direction;
            transition.priority = source.priority;
            transition.guard = source.guard;
            transition.reset = source.reset;
            transition.user_data = source.user_data;
            transition.stable_reset = stable_reset;
            if (write_mask != nullptr) {
                transition.write_mask.assign(
                    write_mask, write_mask + descriptor->dimension);
                if (std::any_of(
                        transition.write_mask.begin(),
                        transition.write_mask.end(),
                        [](std::uint8_t value) { return value > 1; })) {
                    return SMAVE_STATUS_INVALID_ARGUMENT;
                }
            }
            system.transitions.push_back(std::move(transition));
        }
        const auto service_problem = hybrid_service_problem(system);
        std::vector<double> derivative;
        if (!service_problem.modes[system.initial_mode].right_hand_side(
                system.start_time, system.initial_state, derivative)) {
            return SMAVE_STATUS_INVALID_ARGUMENT;
        }
        for (const auto& transition : service_problem.transitions) {
            if (transition.source_mode != system.initial_mode) continue;
            double guard{};
            if (!transition.guard(system.start_time, system.initial_state, guard)) {
                return SMAVE_STATUS_INVALID_ARGUMENT;
            }
        }
        smave_problem* created = allocate_object<smave_problem>(
            library->allocator, library, std::move(system));
        if (created == nullptr) return SMAVE_STATUS_INTERNAL_ERROR;
        ++library->children;
        *problem = created;
        return SMAVE_STATUS_OK;
    } catch (...) {
        return SMAVE_STATUS_INTERNAL_ERROR;
    }
}

smave_status smave_hybrid_dae_problem_create(
    smave_library* library,
    const smave_hybrid_dae_problem_desc* descriptor,
    smave_problem** problem) {
    if (library == nullptr || descriptor == nullptr || problem == nullptr) {
        return SMAVE_STATUS_INVALID_ARGUMENT;
    }
    *problem = nullptr;
    if (descriptor->abi_version != SMAVE_ABI_VERSION) return SMAVE_STATUS_ABI_MISMATCH;
    if (descriptor->struct_size < sizeof(smave_hybrid_dae_problem_desc) ||
        descriptor->dimension == 0 || descriptor->differential_mask == nullptr ||
        descriptor->initial_state == nullptr ||
        descriptor->initial_derivative == nullptr || descriptor->modes == nullptr ||
        descriptor->mode_count == 0 ||
        descriptor->initial_mode >= descriptor->mode_count ||
        descriptor->transitions == nullptr || descriptor->transition_count == 0 ||
        !std::isfinite(descriptor->start_time) ||
        !std::isfinite(descriptor->end_time) ||
        !std::isfinite(descriptor->maximum_step) ||
        descriptor->end_time <= descriptor->start_time ||
        descriptor->maximum_step <= 0.0) return SMAVE_STATUS_INVALID_ARGUMENT;
    try {
        HybridDaeSystem system;
        system.dimension = descriptor->dimension;
        system.differential_mask.assign(
            descriptor->differential_mask,
            descriptor->differential_mask + descriptor->dimension);
        if (std::none_of(
                system.differential_mask.begin(),
                system.differential_mask.end(),
                [](std::uint8_t value) { return value == 1; }) ||
            std::any_of(
                system.differential_mask.begin(),
                system.differential_mask.end(),
                [](std::uint8_t value) { return value > 1; })) {
            return SMAVE_STATUS_INVALID_ARGUMENT;
        }
        system.initial_state.assign(
            descriptor->initial_state,
            descriptor->initial_state + descriptor->dimension);
        system.initial_derivative.assign(
            descriptor->initial_derivative,
            descriptor->initial_derivative + descriptor->dimension);
        if (!finite_values(system.initial_state) ||
            !finite_values(system.initial_derivative)) return SMAVE_STATUS_INVALID_ARGUMENT;
        system.initial_mode = descriptor->initial_mode;
        system.start_time = descriptor->start_time;
        system.end_time = descriptor->end_time;
        system.maximum_step = descriptor->maximum_step;
        system.modes.reserve(descriptor->mode_count);
        for (size_t index = 0; index < descriptor->mode_count; ++index) {
            const auto& source = descriptor->modes[index];
            if (source.abi_version != SMAVE_ABI_VERSION ||
                source.struct_size < sizeof(smave_hybrid_dae_mode_desc) ||
                source.residual == nullptr) {
                return source.abi_version != SMAVE_ABI_VERSION
                    ? SMAVE_STATUS_ABI_MISMATCH : SMAVE_STATUS_INVALID_ARGUMENT;
            }
            system.modes.push_back({source.residual, source.jacobian, source.user_data});
        }
        system.transitions.reserve(descriptor->transition_count);
        constexpr size_t legacy_transition_size =
            offsetof(smave_hybrid_dae_transition_desc, stable_reset);
        for (size_t index = 0; index < descriptor->transition_count; ++index) {
            const auto& source = descriptor->transitions[index];
            const bool extended =
                source.struct_size >= sizeof(smave_hybrid_dae_transition_desc);
            const smave_hybrid_dae_stable_reset_fn stable_reset =
                extended ? source.stable_reset : nullptr;
            const uint8_t* state_write_mask =
                extended ? source.state_write_mask : nullptr;
            const uint8_t* derivative_write_mask =
                extended ? source.derivative_write_mask : nullptr;
            if (source.abi_version != SMAVE_ABI_VERSION ||
                source.struct_size < legacy_transition_size ||
                source.source_mode >= descriptor->mode_count ||
                source.target_mode >= descriptor->mode_count ||
                source.direction < -1 || source.direction > 1 ||
                source.guard == nullptr ||
                (source.reset == nullptr && stable_reset == nullptr) ||
                ((stable_reset == nullptr) != (state_write_mask == nullptr)) ||
                ((stable_reset == nullptr) != (derivative_write_mask == nullptr))) {
                return source.abi_version != SMAVE_ABI_VERSION
                    ? SMAVE_STATUS_ABI_MISMATCH : SMAVE_STATUS_INVALID_ARGUMENT;
            }
            HybridDaeSystem::Transition transition;
            transition.source_mode = source.source_mode;
            transition.target_mode = source.target_mode;
            transition.direction = source.direction;
            transition.priority = source.priority;
            transition.guard = source.guard;
            transition.reset = source.reset;
            transition.user_data = source.user_data;
            transition.stable_reset = stable_reset;
            if (stable_reset != nullptr) {
                transition.state_write_mask.assign(
                    state_write_mask, state_write_mask + descriptor->dimension);
                transition.derivative_write_mask.assign(
                    derivative_write_mask,
                    derivative_write_mask + descriptor->dimension);
                const auto invalid_mask = [](const std::vector<std::uint8_t>& mask) {
                    return std::any_of(mask.begin(), mask.end(), [](std::uint8_t value) {
                        return value > 1;
                    });
                };
                if (invalid_mask(transition.state_write_mask) ||
                    invalid_mask(transition.derivative_write_mask)) {
                    return SMAVE_STATUS_INVALID_ARGUMENT;
                }
            }
            system.transitions.push_back(std::move(transition));
        }
        const auto service_problem = hybrid_dae_service_problem(system);
        std::vector<double> residual;
        if (!service_problem.modes[system.initial_mode].residual(
                system.start_time,
                system.initial_state,
                system.initial_derivative,
                residual) || residual.size() != system.dimension) {
            return SMAVE_STATUS_INVALID_ARGUMENT;
        }
        for (const auto& transition : service_problem.transitions) {
            if (transition.source_mode != system.initial_mode) continue;
            double guard{};
            if (!transition.guard(
                    system.start_time,
                    system.initial_state,
                    system.initial_derivative,
                    guard)) return SMAVE_STATUS_INVALID_ARGUMENT;
        }
        smave_problem* created = allocate_object<smave_problem>(
            library->allocator, library, std::move(system));
        if (created == nullptr) return SMAVE_STATUS_INTERNAL_ERROR;
        ++library->children;
        *problem = created;
        return SMAVE_STATUS_OK;
    } catch (...) {
        return SMAVE_STATUS_INTERNAL_ERROR;
    }
}

smave_status smave_ode_problem_create(
    smave_library* library,
    const smave_ode_problem_desc* descriptor,
    smave_problem** problem) {
    if (library == nullptr || descriptor == nullptr || problem == nullptr) {
        if (library != nullptr) {
            return record_error(
                library, SMAVE_STATUS_INVALID_ARGUMENT, "smave_ode_problem_create",
                "descriptor or output handle is null");
        }
        return SMAVE_STATUS_INVALID_ARGUMENT;
    }
    *problem = nullptr;
    if (descriptor->abi_version != SMAVE_ABI_VERSION) {
        return record_error(
            library, SMAVE_STATUS_ABI_MISMATCH, "smave_ode_problem_create",
            "descriptor ABI version is incompatible");
    }
    if (descriptor->struct_size < sizeof(smave_ode_problem_desc) ||
        descriptor->dimension == 0 || descriptor->initial_state == nullptr ||
        descriptor->right_hand_side == nullptr ||
        !std::isfinite(descriptor->start_time) ||
        !std::isfinite(descriptor->end_time) ||
        !std::isfinite(descriptor->maximum_step) ||
        descriptor->end_time <= descriptor->start_time || descriptor->maximum_step <= 0.0) {
        return record_error(
            library, SMAVE_STATUS_INVALID_ARGUMENT, "smave_ode_problem_create",
            "descriptor shape, callback, or time interval is invalid");
    }
    try {
        OdeSystem system;
        system.dimension = descriptor->dimension;
        system.initial_state.assign(
            descriptor->initial_state,
            descriptor->initial_state + descriptor->dimension);
        if (!finite_values(system.initial_state)) {
            return record_error(
                library, SMAVE_STATUS_INVALID_ARGUMENT, "smave_ode_problem_create",
                "initial state is not finite");
        }
        system.start_time = descriptor->start_time;
        system.end_time = descriptor->end_time;
        system.maximum_step = descriptor->maximum_step;
        system.right_hand_side = descriptor->right_hand_side;
        system.user_data = descriptor->user_data;
        std::vector<double> initial_derivative;
        const auto service_problem = ode_service_problem(system);
        if (!service_problem.right_hand_side(
                system.start_time, system.initial_state, initial_derivative)) {
            return record_error(
                library, SMAVE_STATUS_INVALID_ARGUMENT, "smave_ode_problem_create",
                "initial RHS callback failed");
        }
        smave_problem* created = allocate_object<smave_problem>(
            library->allocator, library, std::move(system));
        if (created == nullptr) {
            return record_error(
                library, SMAVE_STATUS_INTERNAL_ERROR, "smave_ode_problem_create",
                "allocator failed to create problem handle");
        }
        ++library->children;
        *problem = created;
        return SMAVE_STATUS_OK;
    } catch (...) {
        return record_error(
            library, SMAVE_STATUS_INTERNAL_ERROR, "smave_ode_problem_create",
            "unexpected exception while copying descriptor");
    }
}

smave_status smave_event_ode_problem_create(
    smave_library* library,
    const smave_event_ode_problem_desc* descriptor,
    smave_problem** problem) {
    if (library == nullptr || descriptor == nullptr || problem == nullptr) {
        return SMAVE_STATUS_INVALID_ARGUMENT;
    }
    *problem = nullptr;
    if (descriptor->abi_version != SMAVE_ABI_VERSION) return SMAVE_STATUS_ABI_MISMATCH;
    if (descriptor->struct_size < sizeof(smave_event_ode_problem_desc) ||
        descriptor->dimension == 0 || descriptor->initial_state == nullptr ||
        descriptor->right_hand_side == nullptr || descriptor->events == nullptr ||
        descriptor->event_count == 0 || !std::isfinite(descriptor->start_time) ||
        !std::isfinite(descriptor->end_time) ||
        !std::isfinite(descriptor->maximum_step) ||
        descriptor->end_time <= descriptor->start_time || descriptor->maximum_step <= 0.0) {
        return SMAVE_STATUS_INVALID_ARGUMENT;
    }
    try {
        OdeSystem system;
        system.dimension = descriptor->dimension;
        system.initial_state.assign(
            descriptor->initial_state,
            descriptor->initial_state + descriptor->dimension);
        if (!finite_values(system.initial_state)) return SMAVE_STATUS_INVALID_ARGUMENT;
        system.start_time = descriptor->start_time;
        system.end_time = descriptor->end_time;
        system.maximum_step = descriptor->maximum_step;
        system.right_hand_side = descriptor->right_hand_side;
        system.user_data = descriptor->right_hand_side_user_data;
        system.events.reserve(descriptor->event_count);
        for (size_t index = 0; index < descriptor->event_count; ++index) {
            const auto& source = descriptor->events[index];
            if (source.abi_version != SMAVE_ABI_VERSION ||
                source.struct_size < sizeof(smave_event_desc) ||
                source.direction < -1 || source.direction > 1 ||
                source.guard == nullptr || source.reset == nullptr) {
                return source.abi_version != SMAVE_ABI_VERSION
                    ? SMAVE_STATUS_ABI_MISMATCH : SMAVE_STATUS_INVALID_ARGUMENT;
            }
            system.events.push_back({
                .direction = source.direction,
                .priority = source.priority,
                .guard = source.guard,
                .reset = source.reset,
                .user_data = source.user_data,
            });
        }
        const auto service_problem = ode_service_problem(system);
        std::vector<double> derivative;
        if (!service_problem.right_hand_side(
                system.start_time, system.initial_state, derivative)) {
            return SMAVE_STATUS_INVALID_ARGUMENT;
        }
        for (const auto& event : service_problem.events) {
            double guard{};
            if (!event.guard(system.start_time, system.initial_state, guard)) {
                return SMAVE_STATUS_INVALID_ARGUMENT;
            }
        }
        smave_problem* created = allocate_object<smave_problem>(
            library->allocator, library, std::move(system));
        if (created == nullptr) return SMAVE_STATUS_INTERNAL_ERROR;
        ++library->children;
        *problem = created;
        return SMAVE_STATUS_OK;
    } catch (...) {
        return SMAVE_STATUS_INTERNAL_ERROR;
    }
}

smave_status smave_nonlinear_problem_create(
    smave_library* library,
    const smave_nonlinear_problem_desc* descriptor,
    smave_problem** problem) {
    if (library == nullptr || descriptor == nullptr || problem == nullptr) {
        if (library != nullptr) {
            return record_error(
                library, SMAVE_STATUS_INVALID_ARGUMENT,
                "smave_nonlinear_problem_create", "descriptor or output handle is null");
        }
        return SMAVE_STATUS_INVALID_ARGUMENT;
    }
    *problem = nullptr;
    if (descriptor->abi_version != SMAVE_ABI_VERSION) {
        return record_error(
            library, SMAVE_STATUS_ABI_MISMATCH,
            "smave_nonlinear_problem_create", "descriptor ABI version is incompatible");
    }
    if (descriptor->struct_size < sizeof(smave_nonlinear_problem_desc) ||
        descriptor->dimension == 0 || descriptor->initial_state == nullptr ||
        descriptor->residual == nullptr) {
        return record_error(
            library, SMAVE_STATUS_INVALID_ARGUMENT,
            "smave_nonlinear_problem_create", "descriptor shape or residual callback is invalid");
    }
    try {
        NonlinearSystem system;
        system.dimension = descriptor->dimension;
        system.initial_state.assign(
            descriptor->initial_state,
            descriptor->initial_state + descriptor->dimension);
        if (!finite_values(system.initial_state)) {
            return record_error(
                library, SMAVE_STATUS_INVALID_ARGUMENT,
                "smave_nonlinear_problem_create", "initial state is not finite");
        }
        system.residual = descriptor->residual;
        system.jacobian = descriptor->jacobian;
        system.user_data = descriptor->user_data;
        std::vector<double> initial_residual;
        const auto service_problem = nonlinear_service_problem(system);
        if (!service_problem.residual(system.initial_state, initial_residual) ||
            initial_residual.size() != system.dimension || !finite_values(initial_residual)) {
            return record_error(
                library, SMAVE_STATUS_INVALID_ARGUMENT,
                "smave_nonlinear_problem_create", "initial residual callback failed");
        }
        smave_problem* created = allocate_object<smave_problem>(
            library->allocator, library, std::move(system));
        if (created == nullptr) {
            return record_error(
                library, SMAVE_STATUS_INTERNAL_ERROR,
                "smave_nonlinear_problem_create", "allocator failed to create problem handle");
        }
        ++library->children;
        *problem = created;
        return SMAVE_STATUS_OK;
    } catch (...) {
        return record_error(
            library, SMAVE_STATUS_INTERNAL_ERROR,
            "smave_nonlinear_problem_create", "unexpected exception while copying descriptor");
    }
}

smave_status smave_linear_problem_create(
    smave_library* library,
    const smave_linear_problem_desc* descriptor,
    smave_problem** problem) {
    if (library == nullptr || descriptor == nullptr || problem == nullptr) {
        return SMAVE_STATUS_INVALID_ARGUMENT;
    }
    *problem = nullptr;
    if (descriptor->abi_version != SMAVE_ABI_VERSION) return SMAVE_STATUS_ABI_MISMATCH;
    if (descriptor->struct_size < sizeof(smave_linear_problem_desc) ||
        descriptor->dimension == 0 || descriptor->right_hand_side == nullptr) {
        return SMAVE_STATUS_INVALID_ARGUMENT;
    }
    try {
        smave::LinearSystem system;
        const size_t dimension = descriptor->dimension;
        system.unknowns.reserve(dimension);
        for (size_t index = 0; index < dimension; ++index) {
            system.unknowns.push_back("x" + std::to_string(index));
        }
        system.right_hand_side.assign(
            descriptor->right_hand_side,
            descriptor->right_hand_side + dimension);
        if (!finite_values(system.right_hand_side)) return SMAVE_STATUS_INVALID_ARGUMENT;
        if (descriptor->storage == SMAVE_MATRIX_DENSE_ROW_MAJOR) {
            if (descriptor->dense_values == nullptr) return SMAVE_STATUS_INVALID_ARGUMENT;
            system.matrix.assign(dimension, std::vector<double>(dimension));
            for (size_t row = 0; row < dimension; ++row) {
                for (size_t column = 0; column < dimension; ++column) {
                    const double value = descriptor->dense_values[row * dimension + column];
                    if (!std::isfinite(value)) return SMAVE_STATUS_INVALID_ARGUMENT;
                    system.matrix[row][column] = value;
                }
            }
        } else if (descriptor->storage == SMAVE_MATRIX_CSR) {
            if (descriptor->row_offsets == nullptr || descriptor->column_indices == nullptr ||
                descriptor->sparse_values == nullptr || descriptor->nonzeros == 0 ||
                descriptor->row_offsets[0] != 0 ||
                descriptor->row_offsets[dimension] != descriptor->nonzeros) {
                return SMAVE_STATUS_INVALID_ARGUMENT;
            }
            system.sparsity.row_offsets.assign(
                descriptor->row_offsets, descriptor->row_offsets + dimension + 1);
            system.sparsity.row_count = dimension;
            system.sparsity.column_count = dimension;
            system.sparsity.column_indices.assign(
                descriptor->column_indices,
                descriptor->column_indices + descriptor->nonzeros);
            system.sparse_values.assign(
                descriptor->sparse_values,
                descriptor->sparse_values + descriptor->nonzeros);
            for (size_t row = 0; row < dimension; ++row) {
                if (system.sparsity.row_offsets[row] > system.sparsity.row_offsets[row + 1]) {
                    return SMAVE_STATUS_INVALID_ARGUMENT;
                }
                size_t previous = dimension;
                for (size_t offset = system.sparsity.row_offsets[row];
                     offset < system.sparsity.row_offsets[row + 1]; ++offset) {
                    const size_t column = system.sparsity.column_indices[offset];
                    if (previous != dimension && column <= previous) {
                        return SMAVE_STATUS_INVALID_ARGUMENT;
                    }
                    previous = column;
                }
            }
            for (size_t column : system.sparsity.column_indices) {
                if (column >= dimension) return SMAVE_STATUS_INVALID_ARGUMENT;
            }
            if (!finite_values(system.sparse_values)) return SMAVE_STATUS_INVALID_ARGUMENT;
        } else {
            return SMAVE_STATUS_UNSUPPORTED;
        }
        system.symmetric = (descriptor->flags & SMAVE_LINEAR_SYMMETRIC) != 0;
        system.positive_definite =
            (descriptor->flags & SMAVE_LINEAR_POSITIVE_DEFINITE) != 0;
        smave::classify_linear_system(system);
        smave_problem* created = allocate_object<smave_problem>(
            library->allocator, library, std::move(system));
        if (created == nullptr) return SMAVE_STATUS_INTERNAL_ERROR;
        ++library->children;
        *problem = created;
        return SMAVE_STATUS_OK;
    } catch (...) {
        return SMAVE_STATUS_INTERNAL_ERROR;
    }
}

smave_status smave_complementarity_problem_create(
    smave_library* library,
    const smave_complementarity_desc* descriptor,
    smave_problem** problem) {
    if (library == nullptr || descriptor == nullptr || problem == nullptr) {
        return SMAVE_STATUS_INVALID_ARGUMENT;
    }
    *problem = nullptr;
    if (descriptor->abi_version != SMAVE_ABI_VERSION) return SMAVE_STATUS_ABI_MISMATCH;
    if (descriptor->struct_size < sizeof(smave_complementarity_desc) ||
        descriptor->dimension == 0 || descriptor->offset == nullptr ||
        descriptor->flags != 0 ||
        descriptor->dimension >
            std::numeric_limits<size_t>::max() / descriptor->dimension) {
        return SMAVE_STATUS_INVALID_ARGUMENT;
    }
    try {
        const size_t dimension = descriptor->dimension;
        smave::ComplementarityIR model;
        model.model_id = "c-api-complementarity";
        model.variables.resize(dimension);
        model.gap_expressions.resize(dimension);
        model.matrix.assign(dimension, std::vector<double>(dimension));
        model.offset.assign(descriptor->offset, descriptor->offset + dimension);
        if (!finite_values(model.offset)) return SMAVE_STATUS_INVALID_ARGUMENT;
        for (size_t index = 0; index < dimension; ++index) {
            model.variables[index].name = "z" + std::to_string(index);
            model.variables[index].start = descriptor->initial_state == nullptr
                ? 0.0 : descriptor->initial_state[index];
            if (!std::isfinite(model.variables[index].start)) {
                return SMAVE_STATUS_INVALID_ARGUMENT;
            }
        }
        if (descriptor->storage == SMAVE_MATRIX_DENSE_ROW_MAJOR) {
            if (descriptor->dense_matrix == nullptr) return SMAVE_STATUS_INVALID_ARGUMENT;
            for (size_t row = 0; row < dimension; ++row) {
                for (size_t column = 0; column < dimension; ++column) {
                    const double value = descriptor->dense_matrix[row * dimension + column];
                    if (!std::isfinite(value)) return SMAVE_STATUS_INVALID_ARGUMENT;
                    model.matrix[row][column] = value;
                }
            }
        } else if (descriptor->storage == SMAVE_MATRIX_CSR) {
            if (descriptor->row_offsets == nullptr ||
                descriptor->column_indices == nullptr ||
                descriptor->sparse_values == nullptr || descriptor->nonzeros == 0 ||
                descriptor->row_offsets[0] != 0 ||
                descriptor->row_offsets[dimension] != descriptor->nonzeros) {
                return SMAVE_STATUS_INVALID_ARGUMENT;
            }
            for (size_t row = 0; row < dimension; ++row) {
                if (descriptor->row_offsets[row] > descriptor->row_offsets[row + 1]) {
                    return SMAVE_STATUS_INVALID_ARGUMENT;
                }
                size_t previous = dimension;
                for (size_t offset = descriptor->row_offsets[row];
                     offset < descriptor->row_offsets[row + 1]; ++offset) {
                    const size_t column = descriptor->column_indices[offset];
                    const double value = descriptor->sparse_values[offset];
                    if (column >= dimension || !std::isfinite(value) ||
                        (previous != dimension && column <= previous)) {
                        return SMAVE_STATUS_INVALID_ARGUMENT;
                    }
                    previous = column;
                    model.matrix[row][column] = value;
                }
            }
        } else {
            return SMAVE_STATUS_UNSUPPORTED;
        }
        for (size_t row = 0; row < dimension; ++row) {
            std::ostringstream expression;
            expression << std::setprecision(17) << model.offset[row];
            for (size_t column = 0; column < dimension; ++column) {
                const double coefficient = model.matrix[row][column];
                if (coefficient == 0.0) continue;
                if (coefficient > 0.0) expression << '+';
                expression << std::setprecision(17) << coefficient << '*'
                           << model.variables[column].name;
            }
            model.gap_expressions[row] = expression.str();
        }
        model.source_hash = complementarity_fingerprint(model);
        model.validate();
        smave_problem* created = allocate_object<smave_problem>(
            library->allocator, library, std::move(model));
        if (created == nullptr) return SMAVE_STATUS_INTERNAL_ERROR;
        ++library->children;
        *problem = created;
        return SMAVE_STATUS_OK;
    } catch (const std::invalid_argument&) {
        return SMAVE_STATUS_UNSUPPORTED;
    } catch (...) {
        return SMAVE_STATUS_INTERNAL_ERROR;
    }
}

smave_status smave_block_graph_problem_create(
    smave_library* library,
    const smave_block_graph_desc* descriptor,
    smave_problem** problem) {
    if (library == nullptr || descriptor == nullptr || problem == nullptr) {
        return SMAVE_STATUS_INVALID_ARGUMENT;
    }
    *problem = nullptr;
    if (descriptor->abi_version != SMAVE_ABI_VERSION) return SMAVE_STATUS_ABI_MISMATCH;
    if (descriptor->struct_size < sizeof(smave_block_graph_desc) ||
        descriptor->nodes == nullptr || descriptor->node_count == 0 ||
        (descriptor->connection_count > 0 && descriptor->connections == nullptr) ||
        !std::isfinite(descriptor->end_time) || !std::isfinite(descriptor->base_step) ||
        descriptor->end_time < 0.0 || descriptor->base_step <= 0.0) {
        return SMAVE_STATUS_INVALID_ARGUMENT;
    }
    try {
        BlockGraphSystem system;
        system.problem.end_time = descriptor->end_time;
        system.problem.base_step = descriptor->base_step;
        system.problem.nodes.reserve(descriptor->node_count);
        size_t total_outputs{};
        for (size_t index = 0; index < descriptor->node_count; ++index) {
            const smave_block_node_desc& source = descriptor->nodes[index];
            if (source.abi_version != SMAVE_ABI_VERSION) return SMAVE_STATUS_ABI_MISMATCH;
            if (source.struct_size < sizeof(smave_block_node_desc) || source.reserved != 0 ||
                source.output_count == 0 || !std::isfinite(source.sample_time) ||
                !std::isfinite(source.sample_offset) || !std::isfinite(source.parameter) ||
                !std::isfinite(source.initial_output) ||
                source.output_count > std::numeric_limits<size_t>::max() - total_outputs) {
                return SMAVE_STATUS_INVALID_ARGUMENT;
            }
            smave::VerifiedBlockGraphNode node;
            node.input_count = source.input_count;
            node.output_count = source.output_count;
            node.sample_time = source.sample_time;
            node.sample_offset = source.sample_offset;
            node.parameter = source.parameter;
            node.initial_output = source.initial_output;
            switch (source.kind) {
                case SMAVE_BLOCK_CONSTANT:
                    node.kind = smave::VerifiedBlockNodeKind::constant;
                    break;
                case SMAVE_BLOCK_GAIN:
                    node.kind = smave::VerifiedBlockNodeKind::gain;
                    break;
                case SMAVE_BLOCK_SUM:
                    node.kind = smave::VerifiedBlockNodeKind::sum;
                    if (source.input_count == 0 || source.sum_weights == nullptr) {
                        return SMAVE_STATUS_INVALID_ARGUMENT;
                    }
                    node.sum_weights.assign(
                        source.sum_weights, source.sum_weights + source.input_count);
                    if (!finite_values(node.sum_weights)) return SMAVE_STATUS_INVALID_ARGUMENT;
                    break;
                case SMAVE_BLOCK_UNIT_DELAY:
                    node.kind = smave::VerifiedBlockNodeKind::unit_delay;
                    break;
                case SMAVE_BLOCK_SWITCH_GT:
                    node.kind = smave::VerifiedBlockNodeKind::switch_gt;
                    break;
                case SMAVE_BLOCK_SWITCH_GE:
                    node.kind = smave::VerifiedBlockNodeKind::switch_ge;
                    break;
                case SMAVE_BLOCK_SWITCH_NE_ZERO:
                    node.kind = smave::VerifiedBlockNodeKind::switch_ne_zero;
                    break;
                case SMAVE_BLOCK_CALLBACK: {
                    if (source.evaluate == nullptr || source.gate == nullptr) {
                        return SMAVE_STATUS_INVALID_ARGUMENT;
                    }
                    node.kind = smave::VerifiedBlockNodeKind::callback;
                    const size_t input_count = source.input_count;
                    const size_t output_count = source.output_count;
                    const smave_block_gate_fn gate = source.gate;
                    void* const user_data = source.user_data;
                    const auto adapt = [input_count, output_count, gate, user_data](
                        smave_block_evaluate_fn callback) {
                        return [input_count, output_count, callback, gate, user_data](
                            double time,
                            const std::vector<double>& inputs,
                            std::vector<double>& outputs,
                            double& original_gate_residual) {
                            if (inputs.size() != input_count) return false;
                            outputs.assign(output_count, 0.0);
                            original_gate_residual = std::numeric_limits<double>::infinity();
                            const double* input_data = inputs.empty() ? nullptr : inputs.data();
                            if (callback(
                                    input_count, input_data, output_count, outputs.data(),
                                    time, user_data) != 0 || !finite_values(outputs)) {
                                return false;
                            }
                            return gate(
                                       input_count, input_data, output_count, outputs.data(),
                                       time, &original_gate_residual, user_data) == 0 &&
                                std::isfinite(original_gate_residual) &&
                                original_gate_residual >= 0.0;
                        };
                    };
                    node.evaluate = adapt(source.evaluate);
                    if (source.local_fallback != nullptr) {
                        node.fallback = adapt(source.local_fallback);
                    }
                    break;
                }
                default:
                    return SMAVE_STATUS_UNSUPPORTED;
            }
            total_outputs += source.output_count;
            system.problem.nodes.push_back(std::move(node));
        }
        system.problem.connections.reserve(descriptor->connection_count);
        for (size_t index = 0; index < descriptor->connection_count; ++index) {
            const smave_block_connection_desc& source = descriptor->connections[index];
            if (source.abi_version != SMAVE_ABI_VERSION) return SMAVE_STATUS_ABI_MISMATCH;
            if (source.struct_size < sizeof(smave_block_connection_desc)) {
                return SMAVE_STATUS_INVALID_ARGUMENT;
            }
            system.problem.connections.push_back({
                source.source_node,
                source.source_port,
                source.target_node,
                source.target_port});
        }
        auto validation_problem = system.problem;
        validation_problem.end_time = 0.0;
        for (auto& node : validation_problem.nodes) {
            if (node.kind != smave::VerifiedBlockNodeKind::callback) continue;
            const size_t output_count = node.output_count;
            node.evaluate = [output_count](
                double,
                const std::vector<double>&,
                std::vector<double>& outputs,
                double& residual) {
                outputs.assign(output_count, 0.0);
                residual = 0.0;
                return true;
            };
            node.fallback = {};
        }
        const auto validation = smave::verified_block_graph_solve(
            validation_problem, {.absolute_tolerance = 1.0e-12, .relative_tolerance = 1.0e-10});
        if (validation.diagnostic_code == smave::VerifiedSolveDiagnosticCode::invalid_contract) {
            return SMAVE_STATUS_INVALID_ARGUMENT;
        }
        smave_problem* created = allocate_object<smave_problem>(
            library->allocator, library, std::move(system));
        if (created == nullptr) return SMAVE_STATUS_INTERNAL_ERROR;
        ++library->children;
        *problem = created;
        return SMAVE_STATUS_OK;
    } catch (const std::bad_alloc&) {
        return SMAVE_STATUS_INTERNAL_ERROR;
    } catch (...) {
        return SMAVE_STATUS_INVALID_ARGUMENT;
    }
}

smave_status smave_problem_finalize(smave_problem* problem) {
    if (problem == nullptr) return SMAVE_STATUS_INVALID_ARGUMENT;
    if (problem->children.load() != 0) {
        return record_error(
            problem->owner, SMAVE_STATUS_INVALID_STATE, "smave_problem_finalize",
            "problem has live solver handles");
    }
    problem->finalized = true;
    return SMAVE_STATUS_OK;
}

smave_status smave_problem_destroy(smave_problem* problem) {
    if (problem == nullptr) return SMAVE_STATUS_OK;
    if (problem->children.load() != 0) {
        return record_error(
            problem->owner, SMAVE_STATUS_INVALID_STATE, "smave_problem_destroy",
            "problem still owns live solver handles");
    }
    smave_library* owner = problem->owner;
    const Allocator allocator = owner->allocator;
    deallocate_object(allocator, problem);
    --owner->children;
    return SMAVE_STATUS_OK;
}

smave_status smave_solver_create(
    smave_problem* problem,
    const smave_solver_options* options,
    smave_solver** solver) {
    if (problem == nullptr || solver == nullptr) return SMAVE_STATUS_INVALID_ARGUMENT;
    *solver = nullptr;
    if (!problem->finalized) {
        return record_error(
            problem->owner, SMAVE_STATUS_INVALID_STATE, "smave_solver_create",
            "problem must be finalized before solver creation");
    }
    double absolute_tolerance = 1.0e-12;
    double relative_tolerance = 1.0e-10;
    int maximum_iterations = 1000;
    if (options != nullptr) {
        if (options->abi_version != SMAVE_ABI_VERSION) {
            return record_error(
                problem->owner, SMAVE_STATUS_ABI_MISMATCH, "smave_solver_create",
                "solver options ABI version is incompatible");
        }
        if (options->struct_size < sizeof(smave_solver_options) ||
            !(options->absolute_tolerance >= 0.0) ||
            !(options->relative_tolerance >= 0.0) ||
            options->maximum_iterations <= 0) {
            return record_error(
                problem->owner, SMAVE_STATUS_INVALID_ARGUMENT, "smave_solver_create",
                "solver options are invalid");
        }
        absolute_tolerance = options->absolute_tolerance;
        relative_tolerance = options->relative_tolerance;
        maximum_iterations = options->maximum_iterations;
    }
    try {
        smave_solver* created = allocate_object<smave_solver>(
            problem->owner->allocator, problem, absolute_tolerance,
            relative_tolerance, maximum_iterations);
        if (created == nullptr) {
            return record_error(
                problem->owner, SMAVE_STATUS_INTERNAL_ERROR, "smave_solver_create",
                "allocator failed to create solver handle");
        }
        ++problem->children;
        *solver = created;
        return SMAVE_STATUS_OK;
    } catch (...) {
        return record_error(
            problem->owner, SMAVE_STATUS_INTERNAL_ERROR, "smave_solver_create",
            "unexpected exception while creating solver");
    }
}

smave_status smave_solver_destroy(smave_solver* solver) {
    if (solver == nullptr) return SMAVE_STATUS_OK;
    smave_problem* problem = solver->problem;
    const Allocator allocator = problem->owner->allocator;
    deallocate_object(allocator, solver);
    --problem->children;
    return SMAVE_STATUS_OK;
}

static smave_status solve_with_control(
    const smave_solver* solver,
    const smave_linear_fallback_desc* linear_fallback,
    const smave_nonlinear_fallback_desc* nonlinear_fallback,
    const smave_ode_dense_step_fallback_desc* ode_fallback,
    const smave_dae_step_fallback_desc* dae_fallback,
    const smave_cancel_token* token,
    uint64_t timeout_nanoseconds,
    smave_result** result);

smave_status smave_solver_solve(const smave_solver* solver, smave_result** result) {
    return smave_solver_solve_with_timeout(
        solver, nullptr, SMAVE_TIMEOUT_INFINITE, result);
}

smave_status smave_cancel_token_create(
    smave_library* library,
    smave_cancel_token** token) {
    if (library == nullptr || token == nullptr) {
        if (library != nullptr) {
            return record_error(
                library, SMAVE_STATUS_INVALID_ARGUMENT, "smave_cancel_token_create",
                "cancel token output is null");
        }
        return SMAVE_STATUS_INVALID_ARGUMENT;
    }
    *token = nullptr;
    try {
        smave_cancel_token* created = allocate_object<smave_cancel_token>(
            library->allocator, library);
        if (created == nullptr) {
            return record_error(
                library, SMAVE_STATUS_INTERNAL_ERROR, "smave_cancel_token_create",
                "allocator failed to create cancel token");
        }
        ++library->children;
        *token = created;
        return SMAVE_STATUS_OK;
    } catch (...) {
        return record_error(
            library, SMAVE_STATUS_INTERNAL_ERROR, "smave_cancel_token_create",
            "unexpected exception while creating cancel token");
    }
}

smave_status smave_cancel_token_request(smave_cancel_token* token) {
    if (token == nullptr) return SMAVE_STATUS_INVALID_ARGUMENT;
    token->requested.store(true, std::memory_order_release);
    return SMAVE_STATUS_OK;
}

smave_status smave_cancel_token_reset(smave_cancel_token* token) {
    if (token == nullptr) return SMAVE_STATUS_INVALID_ARGUMENT;
    if (token->active_solves.load(std::memory_order_acquire) != 0) {
        return record_error(
            token->owner, SMAVE_STATUS_INVALID_STATE, "smave_cancel_token_reset",
            "cancel token is in use by an active solve");
    }
    token->requested.store(false, std::memory_order_release);
    return SMAVE_STATUS_OK;
}

smave_status smave_cancel_token_destroy(smave_cancel_token* token) {
    if (token == nullptr) return SMAVE_STATUS_OK;
    if (token->active_solves.load(std::memory_order_acquire) != 0) {
        return record_error(
            token->owner, SMAVE_STATUS_INVALID_STATE, "smave_cancel_token_destroy",
            "cancel token is in use by an active solve");
    }
    smave_library* owner = token->owner;
    const Allocator allocator = owner->allocator;
    deallocate_object(allocator, token);
    --owner->children;
    return SMAVE_STATUS_OK;
}

smave_status smave_solver_solve_cancellable(
    const smave_solver* solver,
    const smave_cancel_token* token,
    smave_result** result) {
    return smave_solver_solve_with_timeout(
        solver, token, SMAVE_TIMEOUT_INFINITE, result);
}

smave_status smave_solver_solve_with_timeout(
    const smave_solver* solver,
    const smave_cancel_token* token,
    uint64_t timeout_nanoseconds,
    smave_result** result) {
    return solve_with_control(
        solver, nullptr, nullptr, nullptr, nullptr,
        token, timeout_nanoseconds, result);
}

smave_status smave_solver_solve_linear_with_fallback(
    const smave_solver* solver,
    const smave_linear_fallback_desc* fallback,
    const smave_cancel_token* token,
    uint64_t timeout_nanoseconds,
    smave_result** result) {
    return solve_with_control(
        solver, fallback, nullptr, nullptr, nullptr,
        token, timeout_nanoseconds, result);
}

smave_status smave_solver_solve_nonlinear_with_fallback(
    const smave_solver* solver,
    const smave_nonlinear_fallback_desc* fallback,
    const smave_cancel_token* token,
    uint64_t timeout_nanoseconds,
    smave_result** result) {
    return solve_with_control(
        solver, nullptr, fallback, nullptr, nullptr,
        token, timeout_nanoseconds, result);
}

smave_status smave_solver_solve_ode_with_fallback(
    const smave_solver* solver,
    const smave_ode_dense_step_fallback_desc* fallback,
    const smave_cancel_token* token,
    uint64_t timeout_nanoseconds,
    smave_result** result) {
    return solve_with_control(
        solver, nullptr, nullptr, fallback, nullptr,
        token, timeout_nanoseconds, result);
}

smave_status smave_solver_solve_dae_with_fallback(
    const smave_solver* solver,
    const smave_dae_step_fallback_desc* fallback,
    const smave_cancel_token* token,
    uint64_t timeout_nanoseconds,
    smave_result** result) {
    return solve_with_control(
        solver, nullptr, nullptr, nullptr, fallback,
        token, timeout_nanoseconds, result);
}

static smave_status solve_with_control(
    const smave_solver* solver,
    const smave_linear_fallback_desc* linear_fallback,
    const smave_nonlinear_fallback_desc* nonlinear_fallback,
    const smave_ode_dense_step_fallback_desc* ode_fallback,
    const smave_dae_step_fallback_desc* dae_fallback,
    const smave_cancel_token* token,
    uint64_t timeout_nanoseconds,
    smave_result** result) {
    if (result == nullptr) {
        if (solver != nullptr) {
            return record_error(
                solver->problem->owner, SMAVE_STATUS_INVALID_ARGUMENT,
                "smave_solver_solve_with_timeout", "result output is null");
        }
        return SMAVE_STATUS_INVALID_ARGUMENT;
    }
    *result = nullptr;
    if (solver == nullptr) return SMAVE_STATUS_INVALID_ARGUMENT;
    smave_library* const error_library = solver->problem->owner;
    if (linear_fallback != nullptr) {
        if (linear_fallback->abi_version != SMAVE_ABI_VERSION) {
            return record_error(
                error_library, SMAVE_STATUS_ABI_MISMATCH,
                "smave_solver_solve_with_fallback",
                "linear fallback ABI version is incompatible");
        }
        if (linear_fallback->struct_size < sizeof(smave_linear_fallback_desc) ||
            linear_fallback->solve == nullptr) {
            return record_error(
                error_library, SMAVE_STATUS_INVALID_ARGUMENT,
                "smave_solver_solve_with_fallback", "linear fallback descriptor is invalid");
        }
        if (solver->problem->kind != ProblemKind::linear) {
            return record_error(
                error_library, SMAVE_STATUS_UNSUPPORTED,
                "smave_solver_solve_with_fallback",
                "linear fallback cannot be used with this problem family");
        }
    }
    if (nonlinear_fallback != nullptr) {
        if (nonlinear_fallback->abi_version != SMAVE_ABI_VERSION) {
            return record_error(
                error_library, SMAVE_STATUS_ABI_MISMATCH,
                "smave_solver_solve_with_fallback",
                "nonlinear fallback ABI version is incompatible");
        }
        if (nonlinear_fallback->struct_size < sizeof(smave_nonlinear_fallback_desc) ||
            nonlinear_fallback->solve == nullptr) {
            return record_error(
                error_library, SMAVE_STATUS_INVALID_ARGUMENT,
                "smave_solver_solve_with_fallback", "nonlinear fallback descriptor is invalid");
        }
        if (solver->problem->kind != ProblemKind::nonlinear) {
            return record_error(
                error_library, SMAVE_STATUS_UNSUPPORTED,
                "smave_solver_solve_with_fallback",
                "nonlinear fallback cannot be used with this problem family");
        }
    }
    if (ode_fallback != nullptr) {
        if (ode_fallback->abi_version != SMAVE_ABI_VERSION) {
            return record_error(
                error_library, SMAVE_STATUS_ABI_MISMATCH,
                "smave_solver_solve_with_fallback",
                "ODE fallback ABI version is incompatible");
        }
        if (ode_fallback->struct_size < sizeof(smave_ode_dense_step_fallback_desc) ||
            ode_fallback->step == nullptr) {
            return record_error(
                error_library, SMAVE_STATUS_INVALID_ARGUMENT,
                "smave_solver_solve_with_fallback", "ODE fallback descriptor is invalid");
        }
        if (solver->problem->kind != ProblemKind::ode ||
            !solver->problem->ode.events.empty()) {
            return record_error(
                error_library, SMAVE_STATUS_UNSUPPORTED,
                "smave_solver_solve_with_fallback",
                "ODE fallback requires an event-free ODE problem");
        }
    }
    if (dae_fallback != nullptr) {
        if (dae_fallback->abi_version != SMAVE_ABI_VERSION) {
            return record_error(
                error_library, SMAVE_STATUS_ABI_MISMATCH,
                "smave_solver_solve_with_fallback",
                "DAE fallback ABI version is incompatible");
        }
        if (dae_fallback->struct_size < sizeof(smave_dae_step_fallback_desc) ||
            dae_fallback->step == nullptr) {
            return record_error(
                error_library, SMAVE_STATUS_INVALID_ARGUMENT,
                "smave_solver_solve_with_fallback", "DAE fallback descriptor is invalid");
        }
        if (solver->problem->kind != ProblemKind::dae ||
            !solver->problem->dae.events.empty()) {
            return record_error(
                error_library, SMAVE_STATUS_UNSUPPORTED,
                "smave_solver_solve_with_fallback",
                "DAE fallback requires an event-free DAE problem");
        }
    }
    if (token != nullptr && token->owner != solver->problem->owner) {
        return record_error(
            error_library, SMAVE_STATUS_INVALID_ARGUMENT,
            "smave_solver_solve_with_timeout", "cancel token belongs to another library");
    }
    const CancelTokenUse token_use(token);
    smave::LinearFallbackFunction external_linear_fallback;
    if (linear_fallback != nullptr) {
        const smave_linear_fallback_fn callback = linear_fallback->solve;
        void* const user_data = linear_fallback->user_data;
        external_linear_fallback = [callback, user_data](std::vector<double>& candidate) {
            return callback(candidate.size(), candidate.data(), user_data) == 0;
        };
    }
    smave::NonlinearFallbackFunction external_nonlinear_fallback;
    if (nonlinear_fallback != nullptr) {
        const smave_nonlinear_fallback_fn callback = nonlinear_fallback->solve;
        void* const user_data = nonlinear_fallback->user_data;
        external_nonlinear_fallback = [callback, user_data](
                                          std::vector<double>& candidate) {
            return callback(candidate.size(), candidate.data(), user_data) == 0;
        };
    }
    smave::OdeDenseStepFallbackFunction external_ode_fallback;
    if (ode_fallback != nullptr) {
        const smave_ode_dense_step_fallback_fn callback = ode_fallback->step;
        void* const user_data = ode_fallback->user_data;
        external_ode_fallback = [callback, user_data](
                                    double from_time,
                                    const std::vector<double>& previous_state,
                                    double to_time,
                                    std::vector<double>& quarter_state,
                                    std::vector<double>& midpoint_state,
                                    std::vector<double>& three_quarter_state,
                                    std::vector<double>& next_state) {
            return callback(
                previous_state.size(), from_time, previous_state.data(), to_time,
                quarter_state.data(), midpoint_state.data(),
                three_quarter_state.data(), next_state.data(), user_data) == 0;
        };
    }
    smave::DaeStepFallbackFunction external_dae_fallback;
    if (dae_fallback != nullptr) {
        const smave_dae_step_fallback_fn callback = dae_fallback->step;
        void* const user_data = dae_fallback->user_data;
        const DaeSystem* const system = &solver->problem->dae;
        external_dae_fallback = [callback, user_data, system](
                                    double from_time,
                                    const std::vector<double>& previous_state,
                                    const std::vector<double>& previous_derivative,
                                    double to_time,
                                    std::vector<double>& next_state,
                                    std::vector<double>& next_derivative) {
            return callback(
                previous_state.size(), system->differential_mask.data(), from_time,
                previous_state.data(), previous_derivative.data(), to_time,
                next_state.data(), next_derivative.data(), user_data) == 0;
        };
    }
    enum class StopReason { none, cancellation, deadline };
    std::atomic<StopReason> stop_reason{StopReason::none};
    const auto started = std::chrono::steady_clock::now();
    const auto cancelled = [token, timeout_nanoseconds, started, &stop_reason] {
        if (token != nullptr && token->requested.load(std::memory_order_acquire)) {
            StopReason expected = StopReason::none;
            stop_reason.compare_exchange_strong(
                expected, StopReason::cancellation, std::memory_order_acq_rel);
            return true;
        }
        if (timeout_nanoseconds != SMAVE_TIMEOUT_INFINITE) {
            const auto elapsed = std::chrono::steady_clock::now() - started;
            const auto elapsed_nanoseconds =
                std::chrono::duration<long double, std::nano>(elapsed).count();
            if (elapsed_nanoseconds >= static_cast<long double>(timeout_nanoseconds)) {
                StopReason expected = StopReason::none;
                stop_reason.compare_exchange_strong(
                    expected, StopReason::deadline, std::memory_order_acq_rel);
                return true;
            }
        }
        return stop_reason.load(std::memory_order_acquire) != StopReason::none;
    };
    const Allocator allocator = solver->problem->owner->allocator;
    smave_library* owner = solver->problem->owner;
    smave_result* solved = nullptr;
    try {
        solved = allocate_object<smave_result>(allocator, owner);
        if (solved == nullptr) {
            return record_error(
                error_library, SMAVE_STATUS_INTERNAL_ERROR,
                "smave_solver_solve_with_timeout",
                "allocator failed to create result handle");
        }
        ++owner->children;
        const auto solve_status = [&stop_reason, solved](
                                      bool success,
                                      smave::VerifiedSolveDiagnosticCode diagnostic_code) {
            if (success) return SMAVE_STATUS_OK;
            if (diagnostic_code != smave::VerifiedSolveDiagnosticCode::cancelled) {
                return SMAVE_STATUS_SOLVE_FAILED;
            }
            if (stop_reason.load(std::memory_order_acquire) == StopReason::deadline) {
                solved->diagnostic_code = SMAVE_DIAGNOSTIC_DEADLINE_EXCEEDED;
                solved->diagnostic = "deadline exceeded at cooperative solve boundary";
                return SMAVE_STATUS_DEADLINE_EXCEEDED;
            }
            return SMAVE_STATUS_CANCELLED;
        };
        if (solver->problem->kind == ProblemKind::complementarity) {
            const auto service_result = smave::verified_complementarity_solve(
                solver->problem->complementarity,
                {.absolute = solver->absolute_tolerance,
                 .relative = solver->relative_tolerance,
                 .maximum_pgs_iterations = solver->maximum_iterations,
                 .maximum_newton_iterations = solver->maximum_iterations,
                 .cancellation_requested = cancelled});
            solved->solution = service_result.solution;
            solved->complementarity_gap = service_result.gap;
            solved->service_id = smave::verified_complementarity_solve_service_v1;
            solved->backend = service_result.backend;
            solved->equation_family = service_result.equation_family;
            solved->plan_id = service_result.plan_id;
            solved->diagnostic = service_result.diagnostic;
            solved->diagnostic_code = c_diagnostic_code(service_result.diagnostic_code);
            solved->residual_inf = service_result.residual_inf;
            solved->backward_error = std::max({
                service_result.residual_inf,
                service_result.primal_violation,
                service_result.dual_violation,
                service_result.complementarity_violation});
            solved->used_fallback = service_result.used_fallback;
            solved->success = service_result.success;
            solved->complementarity_attempts = service_result.attempts;
            solved->primal_violation = service_result.primal_violation;
            solved->dual_violation = service_result.dual_violation;
            solved->complementarity_violation =
                service_result.complementarity_violation;
            *result = solved;
            return solve_status(service_result.success, service_result.diagnostic_code);
        }
        if (solver->problem->kind == ProblemKind::block_graph) {
            const auto service_result = smave::verified_block_graph_solve(
                solver->problem->block_graph.problem,
                {.absolute_tolerance = solver->absolute_tolerance,
                 .relative_tolerance = solver->relative_tolerance,
                 .maximum_fixed_point_iterations = solver->maximum_iterations,
                 .cancellation_requested = cancelled});
            solved->solution = service_result.outputs;
            solved->block_output_offsets = service_result.output_offsets;
            solved->block_commit_order = service_result.commit_order;
            solved->service_id = smave::verified_block_graph_solve_service_v1;
            solved->backend = service_result.backend;
            solved->equation_family = service_result.equation_family;
            solved->plan_id = service_result.plan_id;
            solved->diagnostic = service_result.diagnostic;
            solved->diagnostic_code = c_diagnostic_code(service_result.diagnostic_code);
            solved->residual_inf = service_result.maximum_original_gate_residual;
            solved->backward_error = std::max(
                service_result.maximum_original_gate_residual,
                service_result.maximum_connection_error);
            solved->used_fallback = service_result.used_fallback;
            solved->success = service_result.success;
            solved->final_time = service_result.final_time;
            solved->maximum_connection_error = service_result.maximum_connection_error;
            solved->maximum_original_gate_residual =
                service_result.maximum_original_gate_residual;
            solved->maximum_fixed_point_residual =
                service_result.maximum_fixed_point_residual;
            solved->block_graph_ticks = service_result.ticks;
            solved->block_graph_node_executions = service_result.node_executions;
            solved->block_graph_fallback_count = service_result.fallback_count;
            solved->block_graph_fixed_point_components =
                service_result.fixed_point_components;
            solved->block_graph_fixed_point_iterations =
                service_result.fixed_point_iterations;
            solved->block_graph_node_count = solver->problem->block_graph.problem.nodes.size();
            *result = solved;
            return solve_status(service_result.success, service_result.diagnostic_code);
        }
        if (solver->problem->kind == ProblemKind::nonlinear) {
            const NonlinearSystem& system = solver->problem->nonlinear;
            const auto service_result = smave::verified_nonlinear_solve(
                nonlinear_service_problem(system),
                {.absolute_tolerance = solver->absolute_tolerance,
                 .relative_tolerance = solver->relative_tolerance,
                 .maximum_iterations = solver->maximum_iterations,
                 .cancellation_requested = cancelled,
                 .external_fallback = external_nonlinear_fallback});
            solved->solution = service_result.solution;
            solved->service_id = smave::verified_nonlinear_solve_service_v1;
            solved->backend = service_result.backend;
            solved->equation_family = service_result.equation_family;
            solved->plan_id = service_result.plan_id;
            solved->diagnostic = service_result.diagnostic;
            solved->diagnostic_code = c_diagnostic_code(service_result.diagnostic_code);
            solved->residual_inf = service_result.residual_inf;
            solved->backward_error = service_result.backward_error;
            solved->used_fallback = service_result.used_fallback;
            solved->success = service_result.success;
            *result = solved;
            return solve_status(service_result.success, service_result.diagnostic_code);
        }
        if (solver->problem->kind == ProblemKind::ode) {
            const OdeSystem& system = solver->problem->ode;
            const auto service_result = smave::verified_ode_solve(
                ode_service_problem(system),
                {.start_time = system.start_time,
                 .end_time = system.end_time,
                 .maximum_step = system.maximum_step,
                 .absolute_tolerance = solver->absolute_tolerance,
                 .relative_tolerance = solver->relative_tolerance,
                 .maximum_steps = solver->maximum_iterations,
                 .cancellation_requested = cancelled,
                 .external_step_fallback = external_ode_fallback});
            solved->solution = service_result.solution;
            solved->service_id = smave::verified_ode_solve_service_v1;
            solved->backend = service_result.backend;
            solved->equation_family = service_result.equation_family;
            solved->plan_id = service_result.plan_id;
            solved->diagnostic = service_result.diagnostic;
            solved->diagnostic_code = c_diagnostic_code(service_result.diagnostic_code);
            solved->residual_inf = service_result.maximum_scaled_local_error;
            solved->backward_error = service_result.maximum_scaled_local_error;
            solved->used_fallback = service_result.used_fallback;
            solved->success = service_result.success;
            solved->final_time = service_result.final_time;
            solved->accepted_steps = service_result.accepted_steps;
            solved->rejected_steps = service_result.rejected_steps;
            solved->event_count = service_result.event_count;
            solved->last_event_time = service_result.last_event_time;
            *result = solved;
            return solve_status(service_result.success, service_result.diagnostic_code);
        }
        if (solver->problem->kind == ProblemKind::dae) {
            const DaeSystem& system = solver->problem->dae;
            const auto service_result = smave::verified_dae_solve(
                dae_service_problem(system),
                {.start_time = system.start_time,
                 .end_time = system.end_time,
                 .maximum_step = system.maximum_step,
                 .absolute_tolerance = solver->absolute_tolerance,
                 .relative_tolerance = solver->relative_tolerance,
                 .maximum_iterations = solver->maximum_iterations,
                 .maximum_newton_iterations = solver->maximum_iterations,
                 .cancellation_requested = cancelled,
                 .external_step_fallback = external_dae_fallback});
            solved->solution = service_result.solution;
            solved->service_id = smave::verified_dae_solve_service_v1;
            solved->backend = service_result.backend;
            solved->equation_family = service_result.equation_family;
            solved->plan_id = service_result.plan_id;
            solved->diagnostic = service_result.diagnostic;
            solved->diagnostic_code = c_diagnostic_code(service_result.diagnostic_code);
            solved->residual_inf = service_result.maximum_residual_inf;
            solved->backward_error = service_result.maximum_residual_inf;
            solved->used_fallback = service_result.used_fallback;
            solved->success = service_result.success;
            solved->final_time = service_result.final_time;
            solved->accepted_steps = service_result.accepted_steps;
            solved->rejected_steps = service_result.rejected_steps;
            solved->event_count = service_result.event_count;
            solved->last_event_time = service_result.last_event_time;
            solved->differentiation_index = service_result.differentiation_index;
            solved->hidden_rank_checks = service_result.hidden_rank_checks;
            solved->minimum_hidden_rank_margin =
                service_result.minimum_hidden_rank_margin;
            solved->maximum_hidden_residual_inf =
                service_result.maximum_hidden_residual_inf;
            *result = solved;
            return solve_status(service_result.success, service_result.diagnostic_code);
        }
        if (solver->problem->kind == ProblemKind::hybrid) {
            const HybridSystem& system = solver->problem->hybrid;
            const auto service_result = smave::verified_hybrid_solve(
                hybrid_service_problem(system),
                {.start_time = system.start_time,
                 .end_time = system.end_time,
                 .maximum_step = system.maximum_step,
                 .absolute_tolerance = solver->absolute_tolerance,
                 .relative_tolerance = solver->relative_tolerance,
                 .maximum_steps = solver->maximum_iterations,
                 .cancellation_requested = cancelled});
            solved->solution = service_result.solution;
            solved->service_id = smave::verified_hybrid_solve_service_v1;
            solved->backend = service_result.backend;
            solved->equation_family = service_result.equation_family;
            solved->plan_id = service_result.plan_id;
            solved->diagnostic = service_result.diagnostic;
            solved->diagnostic_code = c_diagnostic_code(service_result.diagnostic_code);
            solved->residual_inf = service_result.maximum_scaled_local_error;
            solved->backward_error = service_result.maximum_scaled_local_error;
            solved->used_fallback = service_result.used_fallback;
            solved->success = service_result.success;
            solved->final_time = service_result.final_time;
            solved->accepted_steps = service_result.accepted_steps;
            solved->rejected_steps = service_result.rejected_steps;
            solved->event_count = service_result.event_count;
            solved->last_event_time = service_result.last_event_time;
            solved->final_mode = service_result.final_mode;
            *result = solved;
            return solve_status(service_result.success, service_result.diagnostic_code);
        }
        if (solver->problem->kind == ProblemKind::hybrid_dae) {
            const HybridDaeSystem& system = solver->problem->hybrid_dae;
            const auto service_result = smave::verified_hybrid_dae_solve(
                hybrid_dae_service_problem(system),
                {.start_time = system.start_time,
                 .end_time = system.end_time,
                 .maximum_step = system.maximum_step,
                 .absolute_tolerance = solver->absolute_tolerance,
                 .relative_tolerance = solver->relative_tolerance,
                 .maximum_iterations = solver->maximum_iterations,
                 .maximum_newton_iterations = solver->maximum_iterations,
                 .cancellation_requested = cancelled});
            solved->solution = service_result.solution;
            solved->service_id = smave::verified_hybrid_dae_solve_service_v1;
            solved->backend = service_result.backend;
            solved->equation_family = service_result.equation_family;
            solved->plan_id = service_result.plan_id;
            solved->diagnostic = service_result.diagnostic;
            solved->diagnostic_code = c_diagnostic_code(service_result.diagnostic_code);
            solved->residual_inf = service_result.maximum_residual_inf;
            solved->backward_error = service_result.maximum_residual_inf;
            solved->used_fallback = service_result.used_fallback;
            solved->success = service_result.success;
            solved->final_time = service_result.final_time;
            solved->accepted_steps = service_result.accepted_steps;
            solved->rejected_steps = service_result.rejected_steps;
            solved->event_count = service_result.event_count;
            solved->last_event_time = service_result.last_event_time;
            solved->final_mode = service_result.final_mode;
            solved->consistency_projection_count =
                service_result.consistency_projection_count;
            *result = solved;
            return solve_status(service_result.success, service_result.diagnostic_code);
        }

        const auto service_result = smave::verified_linear_solve(
            solver->problem->linear,
            {.absolute_tolerance = solver->absolute_tolerance,
             .relative_tolerance = solver->relative_tolerance,
             .maximum_work_iterations = solver->maximum_iterations,
             .cancellation_requested = cancelled,
             .external_fallback = external_linear_fallback});
        solved->solution = service_result.solution;
        solved->service_id = smave::verified_linear_solve_service_v1;
        solved->backend = service_result.backend;
        solved->equation_family = service_result.equation_family;
        solved->plan_id = service_result.plan_id;
        solved->diagnostic = service_result.diagnostic;
        solved->diagnostic_code = c_diagnostic_code(service_result.diagnostic_code);
        solved->residual_inf = service_result.residual_inf;
        solved->backward_error = service_result.backward_error;
        solved->success = service_result.success;
        solved->used_fallback = service_result.used_fallback;
        *result = solved;
        return solve_status(service_result.success, service_result.diagnostic_code);
    } catch (...) {
        deallocate_object(allocator, solved);
        if (solved != nullptr) --owner->children;
        return record_error(
            error_library, SMAVE_STATUS_INTERNAL_ERROR,
            "smave_solver_solve_with_timeout",
            "unexpected exception while solving problem");
    }
}

smave_status smave_result_get_info(const smave_result* result, smave_result_info* info) {
    if (result == nullptr || info == nullptr) return SMAVE_STATUS_INVALID_ARGUMENT;
    if (info->abi_version != SMAVE_ABI_VERSION) return SMAVE_STATUS_ABI_MISMATCH;
    if (info->struct_size < sizeof(smave_result_info)) return SMAVE_STATUS_INVALID_ARGUMENT;
    info->success = result->success;
    info->used_fallback = result->used_fallback;
    info->dimension = result->solution.size();
    info->residual_inf = result->residual_inf;
    info->backward_error = result->backward_error;
    info->backend = result->backend.c_str();
    info->diagnostic = result->diagnostic.c_str();
    return SMAVE_STATUS_OK;
}

smave_status smave_result_get_provenance(
    const smave_result* result,
    const char** service_id,
    const char** plan_id,
    const char** equation_family) {
    if (result == nullptr || service_id == nullptr || plan_id == nullptr ||
        equation_family == nullptr) return SMAVE_STATUS_INVALID_ARGUMENT;
    *service_id = result->service_id.c_str();
    *plan_id = result->plan_id.c_str();
    *equation_family = result->equation_family.c_str();
    return SMAVE_STATUS_OK;
}

smave_status smave_result_get_diagnostic_code(
    const smave_result* result,
    smave_diagnostic_code* diagnostic_code) {
    if (result == nullptr || diagnostic_code == nullptr) {
        return SMAVE_STATUS_INVALID_ARGUMENT;
    }
    *diagnostic_code = result->diagnostic_code;
    return SMAVE_STATUS_OK;
}

smave_status smave_result_get_ode_info(
    const smave_result* result,
    smave_ode_result_info* info) {
    if (result == nullptr || info == nullptr) return SMAVE_STATUS_INVALID_ARGUMENT;
    if (info->abi_version != SMAVE_ABI_VERSION) return SMAVE_STATUS_ABI_MISMATCH;
    constexpr size_t ode_v1_prefix_size = offsetof(smave_ode_result_info, event_count);
    if (info->struct_size < ode_v1_prefix_size) {
        return SMAVE_STATUS_INVALID_ARGUMENT;
    }
    if (result->service_id != smave::verified_ode_solve_service_v1) {
        return SMAVE_STATUS_UNSUPPORTED;
    }
    info->final_time = result->final_time;
    info->maximum_scaled_local_error = result->residual_inf;
    info->accepted_steps = result->accepted_steps;
    info->rejected_steps = result->rejected_steps;
    if (info->struct_size >= sizeof(smave_ode_result_info)) {
        info->event_count = result->event_count;
        info->last_event_time = result->last_event_time;
    }
    return SMAVE_STATUS_OK;
}

smave_status smave_result_get_dae_info(
    const smave_result* result,
    smave_dae_result_info* info) {
    if (result == nullptr || info == nullptr) return SMAVE_STATUS_INVALID_ARGUMENT;
    if (info->abi_version != SMAVE_ABI_VERSION) return SMAVE_STATUS_ABI_MISMATCH;
    const size_t legacy_prefix_size = offsetof(smave_dae_result_info, event_count);
    if (info->struct_size < legacy_prefix_size) {
        return SMAVE_STATUS_INVALID_ARGUMENT;
    }
    if (result->service_id != smave::verified_dae_solve_service_v1) {
        return SMAVE_STATUS_UNSUPPORTED;
    }
    info->final_time = result->final_time;
    info->maximum_residual_inf = result->residual_inf;
    info->accepted_steps = result->accepted_steps;
    info->rejected_steps = result->rejected_steps;
    const size_t event_prefix_size = offsetof(
        smave_dae_result_info, differentiation_index);
    if (info->struct_size >= event_prefix_size) {
        info->event_count = result->event_count;
        info->last_event_time = result->last_event_time;
    }
    if (info->struct_size >= sizeof(smave_dae_result_info)) {
        info->differentiation_index = result->differentiation_index;
        info->hidden_rank_checks = result->hidden_rank_checks;
        info->minimum_hidden_rank_margin = result->minimum_hidden_rank_margin;
        info->maximum_hidden_residual_inf = result->maximum_hidden_residual_inf;
    }
    return SMAVE_STATUS_OK;
}

smave_status smave_result_get_hybrid_info(
    const smave_result* result,
    smave_hybrid_result_info* info) {
    if (result == nullptr || info == nullptr) return SMAVE_STATUS_INVALID_ARGUMENT;
    if (info->abi_version != SMAVE_ABI_VERSION) return SMAVE_STATUS_ABI_MISMATCH;
    if (info->struct_size < sizeof(smave_hybrid_result_info)) {
        return SMAVE_STATUS_INVALID_ARGUMENT;
    }
    if (result->service_id != smave::verified_hybrid_solve_service_v1) {
        return SMAVE_STATUS_UNSUPPORTED;
    }
    info->final_time = result->final_time;
    info->maximum_scaled_local_error = result->residual_inf;
    info->accepted_steps = result->accepted_steps;
    info->rejected_steps = result->rejected_steps;
    info->event_count = result->event_count;
    info->last_event_time = result->last_event_time;
    info->final_mode = result->final_mode;
    return SMAVE_STATUS_OK;
}

smave_status smave_result_get_hybrid_dae_info(
    const smave_result* result,
    smave_hybrid_dae_result_info* info) {
    if (result == nullptr || info == nullptr) return SMAVE_STATUS_INVALID_ARGUMENT;
    if (info->abi_version != SMAVE_ABI_VERSION) return SMAVE_STATUS_ABI_MISMATCH;
    if (info->struct_size < sizeof(smave_hybrid_dae_result_info)) {
        return SMAVE_STATUS_INVALID_ARGUMENT;
    }
    if (result->service_id != smave::verified_hybrid_dae_solve_service_v1) {
        return SMAVE_STATUS_UNSUPPORTED;
    }
    info->final_time = result->final_time;
    info->maximum_residual_inf = result->residual_inf;
    info->accepted_steps = result->accepted_steps;
    info->rejected_steps = result->rejected_steps;
    info->event_count = result->event_count;
    info->last_event_time = result->last_event_time;
    info->final_mode = result->final_mode;
    info->consistency_projection_count = result->consistency_projection_count;
    return SMAVE_STATUS_OK;
}

smave_status smave_result_get_complementarity_info(
    const smave_result* result,
    smave_complementarity_result_info* info) {
    if (result == nullptr || info == nullptr) return SMAVE_STATUS_INVALID_ARGUMENT;
    if (info->abi_version != SMAVE_ABI_VERSION) return SMAVE_STATUS_ABI_MISMATCH;
    if (info->struct_size < sizeof(smave_complementarity_result_info)) {
        return SMAVE_STATUS_INVALID_ARGUMENT;
    }
    if (result->service_id != smave::verified_complementarity_solve_service_v1) {
        return SMAVE_STATUS_UNSUPPORTED;
    }
    info->primal_violation = result->primal_violation;
    info->dual_violation = result->dual_violation;
    info->complementarity_violation = result->complementarity_violation;
    info->attempts = result->complementarity_attempts;
    return SMAVE_STATUS_OK;
}

smave_status smave_result_get_block_graph_info(
    const smave_result* result,
    smave_block_graph_result_info* info) {
    if (result == nullptr || info == nullptr) return SMAVE_STATUS_INVALID_ARGUMENT;
    if (info->abi_version != SMAVE_ABI_VERSION) return SMAVE_STATUS_ABI_MISMATCH;
    if (info->struct_size < sizeof(smave_block_graph_result_info)) {
        return SMAVE_STATUS_INVALID_ARGUMENT;
    }
    if (result->service_id != smave::verified_block_graph_solve_service_v1) {
        return SMAVE_STATUS_UNSUPPORTED;
    }
    info->final_time = result->final_time;
    info->maximum_connection_error = result->maximum_connection_error;
    info->maximum_original_gate_residual = result->maximum_original_gate_residual;
    info->ticks = result->block_graph_ticks;
    info->node_executions = result->block_graph_node_executions;
    info->fallback_count = result->block_graph_fallback_count;
    info->node_count = result->block_graph_node_count;
    info->maximum_fixed_point_residual = result->maximum_fixed_point_residual;
    info->fixed_point_components = result->block_graph_fixed_point_components;
    info->fixed_point_iterations = result->block_graph_fixed_point_iterations;
    return SMAVE_STATUS_OK;
}

smave_status smave_result_copy_solution(
    const smave_result* result,
    double* values,
    size_t capacity,
    size_t* required) {
    if (result == nullptr || required == nullptr) return SMAVE_STATUS_INVALID_ARGUMENT;
    *required = result->solution.size();
    if (capacity < result->solution.size() || values == nullptr) {
        return SMAVE_STATUS_BUFFER_TOO_SMALL;
    }
    std::copy(result->solution.begin(), result->solution.end(), values);
    return SMAVE_STATUS_OK;
}

smave_status smave_result_copy_complementarity_gap(
    const smave_result* result,
    double* values,
    size_t capacity,
    size_t* required) {
    if (result == nullptr || required == nullptr) return SMAVE_STATUS_INVALID_ARGUMENT;
    if (result->service_id != smave::verified_complementarity_solve_service_v1) {
        return SMAVE_STATUS_UNSUPPORTED;
    }
    *required = result->complementarity_gap.size();
    if (capacity < result->complementarity_gap.size() || values == nullptr) {
        return SMAVE_STATUS_BUFFER_TOO_SMALL;
    }
    std::copy(
        result->complementarity_gap.begin(),
        result->complementarity_gap.end(), values);
    return SMAVE_STATUS_OK;
}

smave_status smave_result_copy_block_output_offsets(
    const smave_result* result,
    size_t* values,
    size_t capacity,
    size_t* required) {
    if (result == nullptr || required == nullptr) return SMAVE_STATUS_INVALID_ARGUMENT;
    if (result->service_id != smave::verified_block_graph_solve_service_v1) {
        return SMAVE_STATUS_UNSUPPORTED;
    }
    *required = result->block_output_offsets.size();
    if (capacity < result->block_output_offsets.size() || values == nullptr) {
        return SMAVE_STATUS_BUFFER_TOO_SMALL;
    }
    std::copy(result->block_output_offsets.begin(), result->block_output_offsets.end(), values);
    return SMAVE_STATUS_OK;
}

smave_status smave_result_copy_block_commit_order(
    const smave_result* result,
    size_t* values,
    size_t capacity,
    size_t* required) {
    if (result == nullptr || required == nullptr) return SMAVE_STATUS_INVALID_ARGUMENT;
    if (result->service_id != smave::verified_block_graph_solve_service_v1) {
        return SMAVE_STATUS_UNSUPPORTED;
    }
    *required = result->block_commit_order.size();
    if (capacity < result->block_commit_order.size() || values == nullptr) {
        return SMAVE_STATUS_BUFFER_TOO_SMALL;
    }
    std::copy(result->block_commit_order.begin(), result->block_commit_order.end(), values);
    return SMAVE_STATUS_OK;
}

smave_status smave_result_destroy(smave_result* result) {
    if (result == nullptr) return SMAVE_STATUS_OK;
    smave_library* owner = result->owner;
    const Allocator allocator = owner->allocator;
    deallocate_object(allocator, result);
    --owner->children;
    return SMAVE_STATUS_OK;
}

}  // extern "C"
