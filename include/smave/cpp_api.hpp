#pragma once

#include "smave/c_api.h"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace smave::sdk {

class Error final : public std::runtime_error {
public:
    explicit Error(smave_status status)
        : std::runtime_error(smave_status_string(status)), status_(status) {}

    [[nodiscard]] smave_status status() const noexcept { return status_; }

private:
    smave_status status_{};
};

inline void check(smave_status status) {
    if (status != SMAVE_STATUS_OK) throw Error(status);
}

namespace detail {

struct LibraryDeleter {
    void operator()(smave_library* value) const noexcept {
        if (value != nullptr) (void)smave_library_destroy(value);
    }
};

struct ProblemDeleter {
    void operator()(smave_problem* value) const noexcept {
        if (value != nullptr) (void)smave_problem_destroy(value);
    }
};

struct SolverDeleter {
    void operator()(smave_solver* value) const noexcept {
        if (value != nullptr) (void)smave_solver_destroy(value);
    }
};

struct ResultDeleter {
    void operator()(smave_result* value) const noexcept {
        if (value != nullptr) (void)smave_result_destroy(value);
    }
};

struct CancelTokenDeleter {
    void operator()(smave_cancel_token* value) const noexcept {
        if (value != nullptr) (void)smave_cancel_token_destroy(value);
    }
};

struct LibraryState {
    explicit LibraryState(smave_library* value) : handle(value) {}
    std::unique_ptr<smave_library, LibraryDeleter> handle;
};

struct ProblemState {
    ProblemState(std::shared_ptr<LibraryState> owner, smave_problem* value)
        : library(std::move(owner)), handle(value) {}
    std::shared_ptr<LibraryState> library;
    std::unique_ptr<smave_problem, ProblemDeleter> handle;
};

struct SolverState {
    SolverState(std::shared_ptr<ProblemState> owner, smave_solver* value)
        : problem(std::move(owner)), handle(value) {}
    std::shared_ptr<ProblemState> problem;
    std::unique_ptr<smave_solver, SolverDeleter> handle;
};

inline bool outcome_status(smave_status status) noexcept {
    return status == SMAVE_STATUS_OK || status == SMAVE_STATUS_SOLVE_FAILED ||
        status == SMAVE_STATUS_CANCELLED || status == SMAVE_STATUS_DEADLINE_EXCEEDED;
}

}

class Problem;
class Solver;
class CancelToken;

class Result {
public:
    struct Info {
        bool success{};
        bool used_fallback{};
        std::size_t dimension{};
        double residual_inf{};
        double backward_error{};
        std::string backend;
        std::string diagnostic;
    };

    Result() = default;
    Result(Result&&) noexcept = default;
    Result& operator=(Result&&) noexcept = default;
    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;

    [[nodiscard]] explicit operator bool() const noexcept {
        return handle_ != nullptr;
    }

    [[nodiscard]] smave_result* get() const noexcept { return handle_.get(); }

    [[nodiscard]] Info info() const {
        smave_result_info value{};
        value.struct_size = sizeof(value);
        value.abi_version = SMAVE_ABI_VERSION;
        check(smave_result_get_info(handle_.get(), &value));
        return {
            value.success != 0,
            value.used_fallback != 0,
            value.dimension,
            value.residual_inf,
            value.backward_error,
            value.backend == nullptr ? "" : value.backend,
            value.diagnostic == nullptr ? "" : value.diagnostic,
        };
    }

    [[nodiscard]] smave_diagnostic_code diagnostic_code() const {
        smave_diagnostic_code value{SMAVE_DIAGNOSTIC_INVALID_CONTRACT};
        check(smave_result_get_diagnostic_code(handle_.get(), &value));
        return value;
    }

    [[nodiscard]] std::vector<double> solution() const {
        const auto common = info();
        std::vector<double> values(common.dimension);
        std::size_t required{};
        check(smave_result_copy_solution(
            handle_.get(), values.data(), values.size(), &required));
        values.resize(required);
        return values;
    }

    struct Provenance {
        std::string service_id;
        std::string plan_id;
        std::string equation_family;
    };

    [[nodiscard]] Provenance provenance() const {
        const char* service_id{};
        const char* plan_id{};
        const char* equation_family{};
        check(smave_result_get_provenance(
            handle_.get(), &service_id, &plan_id, &equation_family));
        return {
            service_id == nullptr ? "" : service_id,
            plan_id == nullptr ? "" : plan_id,
            equation_family == nullptr ? "" : equation_family,
        };
    }

private:
    friend class Solver;

    Result(std::shared_ptr<detail::LibraryState> owner, smave_result* value)
        : library_(std::move(owner)), handle_(value) {}

    std::shared_ptr<detail::LibraryState> library_;
    std::unique_ptr<smave_result, detail::ResultDeleter> handle_;
};

struct SolveOutcome {
    smave_status status{SMAVE_STATUS_INTERNAL_ERROR};
    Result result;

    [[nodiscard]] bool success() const { return result.info().success; }
};

class Library {
public:
    struct ErrorRecord {
        std::uint64_t trace_id{};
        smave_status status{SMAVE_STATUS_OK};
        std::string operation;
        std::string message;
    };

    explicit Library(const smave_library_options* options = nullptr) {
        smave_library* raw{};
        check(smave_library_create(options, &raw));
        std::unique_ptr<smave_library, detail::LibraryDeleter> guard(raw);
        state_ = std::make_shared<detail::LibraryState>(raw);
        guard.release();
    }

    Library(Library&&) noexcept = default;
    Library& operator=(Library&&) noexcept = default;
    Library(const Library&) = delete;
    Library& operator=(const Library&) = delete;

    [[nodiscard]] smave_library* get() const noexcept {
        return state_ == nullptr ? nullptr : state_->handle.get();
    }

    [[nodiscard]] bool has(smave_capability capability) const {
        std::int32_t available{};
        check(smave_library_has_capability(get(), capability, &available));
        return available != 0;
    }

    [[nodiscard]] std::vector<ErrorRecord> errors() const {
        std::size_t count{};
        check(smave_library_get_error_count(get(), &count));
        std::vector<ErrorRecord> records;
        records.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            smave_error_info info{};
            info.struct_size = sizeof(info);
            info.abi_version = SMAVE_ABI_VERSION;
            check(smave_library_get_error(get(), index, &info));
            records.push_back({
                info.trace_id,
                info.status,
                info.operation == nullptr ? "" : info.operation,
                info.message == nullptr ? "" : info.message,
            });
        }
        return records;
    }

    void clear_errors() const { check(smave_library_clear_errors(get())); }

    [[nodiscard]] Problem linear(const smave_linear_problem_desc& descriptor) const;
    [[nodiscard]] Problem complementarity(
        const smave_complementarity_desc& descriptor) const;
    [[nodiscard]] Problem block_graph(const smave_block_graph_desc& descriptor) const;
    [[nodiscard]] Problem nonlinear(const smave_nonlinear_problem_desc& descriptor) const;
    [[nodiscard]] Problem ode(const smave_ode_problem_desc& descriptor) const;
    [[nodiscard]] Problem event_ode(const smave_event_ode_problem_desc& descriptor) const;
    [[nodiscard]] Problem dae(const smave_dae_problem_desc& descriptor) const;
    [[nodiscard]] Problem event_dae(const smave_event_dae_problem_desc& descriptor) const;
    [[nodiscard]] Problem hybrid(const smave_hybrid_problem_desc& descriptor) const;
    [[nodiscard]] Problem hybrid_dae(const smave_hybrid_dae_problem_desc& descriptor) const;
    [[nodiscard]] CancelToken cancel_token() const;

private:
    template <typename Descriptor>
    [[nodiscard]] Problem create_problem(
        const Descriptor& descriptor,
        smave_status (*create)(smave_library*, const Descriptor*, smave_problem**)) const;

    std::shared_ptr<detail::LibraryState> state_;
};

class Problem {
public:
    Problem(Problem&&) noexcept = default;
    Problem& operator=(Problem&&) noexcept = default;
    Problem(const Problem&) = delete;
    Problem& operator=(const Problem&) = delete;

    [[nodiscard]] smave_problem* get() const noexcept {
        return state_ == nullptr ? nullptr : state_->handle.get();
    }

    void finalize() const { check(smave_problem_finalize(get())); }

    [[nodiscard]] Solver solver(const smave_solver_options* options = nullptr) const;

private:
    friend class Library;

    explicit Problem(std::shared_ptr<detail::ProblemState> state)
        : state_(std::move(state)) {}

    std::shared_ptr<detail::ProblemState> state_;
};

class CancelToken {
public:
    CancelToken(CancelToken&&) noexcept = default;
    CancelToken& operator=(CancelToken&&) noexcept = default;
    CancelToken(const CancelToken&) = delete;
    CancelToken& operator=(const CancelToken&) = delete;

    [[nodiscard]] smave_cancel_token* get() const noexcept { return handle_.get(); }

    void request() const { check(smave_cancel_token_request(handle_.get())); }
    void reset() const { check(smave_cancel_token_reset(handle_.get())); }

private:
    friend class Library;

    CancelToken(std::shared_ptr<detail::LibraryState> owner, smave_cancel_token* value)
        : library_(std::move(owner)), handle_(value) {}

    std::shared_ptr<detail::LibraryState> library_;
    std::unique_ptr<smave_cancel_token, detail::CancelTokenDeleter> handle_;
};

class Solver {
public:
    Solver(Solver&&) noexcept = default;
    Solver& operator=(Solver&&) noexcept = default;
    Solver(const Solver&) = delete;
    Solver& operator=(const Solver&) = delete;

    [[nodiscard]] smave_solver* get() const noexcept {
        return state_ == nullptr ? nullptr : state_->handle.get();
    }

    [[nodiscard]] SolveOutcome solve() const {
        return invoke([&](smave_result** result) {
            return smave_solver_solve(get(), result);
        });
    }

    [[nodiscard]] SolveOutcome solve(const CancelToken& token) const {
        return invoke([&](smave_result** result) {
            return smave_solver_solve_cancellable(get(), token.get(), result);
        });
    }

    [[nodiscard]] SolveOutcome solve_for(
        std::uint64_t timeout_nanoseconds,
        const CancelToken* token = nullptr) const {
        return invoke([&](smave_result** result) {
            return smave_solver_solve_with_timeout(
                get(), token == nullptr ? nullptr : token->get(),
                timeout_nanoseconds, result);
        });
    }

    [[nodiscard]] SolveOutcome solve_linear(
        const smave_linear_fallback_desc& fallback,
        const CancelToken* token = nullptr,
        std::uint64_t timeout_nanoseconds = SMAVE_TIMEOUT_INFINITE) const {
        return invoke([&](smave_result** result) {
            return smave_solver_solve_linear_with_fallback(
                get(), &fallback, token == nullptr ? nullptr : token->get(),
                timeout_nanoseconds, result);
        });
    }

    [[nodiscard]] SolveOutcome solve_nonlinear(
        const smave_nonlinear_fallback_desc& fallback,
        const CancelToken* token = nullptr,
        std::uint64_t timeout_nanoseconds = SMAVE_TIMEOUT_INFINITE) const {
        return invoke([&](smave_result** result) {
            return smave_solver_solve_nonlinear_with_fallback(
                get(), &fallback, token == nullptr ? nullptr : token->get(),
                timeout_nanoseconds, result);
        });
    }

    [[nodiscard]] SolveOutcome solve_ode(
        const smave_ode_dense_step_fallback_desc& fallback,
        const CancelToken* token = nullptr,
        std::uint64_t timeout_nanoseconds = SMAVE_TIMEOUT_INFINITE) const {
        return invoke([&](smave_result** result) {
            return smave_solver_solve_ode_with_fallback(
                get(), &fallback, token == nullptr ? nullptr : token->get(),
                timeout_nanoseconds, result);
        });
    }

    [[nodiscard]] SolveOutcome solve_dae(
        const smave_dae_step_fallback_desc& fallback,
        const CancelToken* token = nullptr,
        std::uint64_t timeout_nanoseconds = SMAVE_TIMEOUT_INFINITE) const {
        return invoke([&](smave_result** result) {
            return smave_solver_solve_dae_with_fallback(
                get(), &fallback, token == nullptr ? nullptr : token->get(),
                timeout_nanoseconds, result);
        });
    }

private:
    friend class Problem;

    explicit Solver(std::shared_ptr<detail::SolverState> state)
        : state_(std::move(state)) {}

    template <typename Function>
    [[nodiscard]] SolveOutcome invoke(Function&& function) const {
        smave_result* raw{};
        const smave_status status = std::forward<Function>(function)(&raw);
        if (!detail::outcome_status(status)) {
            if (raw != nullptr) (void)smave_result_destroy(raw);
            throw Error(status);
        }
        if (raw == nullptr) throw Error(SMAVE_STATUS_INTERNAL_ERROR);
        return {
            status,
            Result(state_->problem->library, raw),
        };
    }

    std::shared_ptr<detail::SolverState> state_;
};

template <typename Descriptor>
Problem Library::create_problem(
    const Descriptor& descriptor,
    smave_status (*create)(smave_library*, const Descriptor*, smave_problem**)) const {
    smave_problem* raw{};
    check(create(get(), &descriptor, &raw));
    std::unique_ptr<smave_problem, detail::ProblemDeleter> guard(raw);
    auto state = std::make_shared<detail::ProblemState>(state_, raw);
    guard.release();
    return Problem(std::move(state));
}

inline Problem Library::linear(const smave_linear_problem_desc& descriptor) const {
    return create_problem(descriptor, smave_linear_problem_create);
}

inline Problem Library::complementarity(
    const smave_complementarity_desc& descriptor) const {
    return create_problem(descriptor, smave_complementarity_problem_create);
}

inline Problem Library::block_graph(const smave_block_graph_desc& descriptor) const {
    return create_problem(descriptor, smave_block_graph_problem_create);
}

inline Problem Library::nonlinear(const smave_nonlinear_problem_desc& descriptor) const {
    return create_problem(descriptor, smave_nonlinear_problem_create);
}

inline Problem Library::ode(const smave_ode_problem_desc& descriptor) const {
    return create_problem(descriptor, smave_ode_problem_create);
}

inline Problem Library::event_ode(const smave_event_ode_problem_desc& descriptor) const {
    return create_problem(descriptor, smave_event_ode_problem_create);
}

inline Problem Library::dae(const smave_dae_problem_desc& descriptor) const {
    return create_problem(descriptor, smave_dae_problem_create);
}

inline Problem Library::event_dae(const smave_event_dae_problem_desc& descriptor) const {
    return create_problem(descriptor, smave_event_dae_problem_create);
}

inline Problem Library::hybrid(const smave_hybrid_problem_desc& descriptor) const {
    return create_problem(descriptor, smave_hybrid_problem_create);
}

inline Problem Library::hybrid_dae(const smave_hybrid_dae_problem_desc& descriptor) const {
    return create_problem(descriptor, smave_hybrid_dae_problem_create);
}

inline CancelToken Library::cancel_token() const {
    smave_cancel_token* raw{};
    check(smave_cancel_token_create(get(), &raw));
    return CancelToken(state_, raw);
}

inline Solver Problem::solver(const smave_solver_options* options) const {
    smave_solver* raw{};
    check(smave_solver_create(get(), options, &raw));
    std::unique_ptr<smave_solver, detail::SolverDeleter> guard(raw);
    auto state = std::make_shared<detail::SolverState>(state_, raw);
    guard.release();
    return Solver(std::move(state));
}

}
