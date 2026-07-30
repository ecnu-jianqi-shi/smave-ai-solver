#include "smave/benchmark/sparse_suite.hpp"
#include "smave/linear.hpp"
#include "smave/routing.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <sys/resource.h>
#include <vector>

#if defined(SMAVE_HAVE_BENCH_SUPERLU)
extern "C" {
#include <slu_ddefs.h>
}
#endif
#if defined(SMAVE_HAVE_BENCH_PETSC)
#include <petscksp.h>
#endif

namespace {

using Clock = std::chrono::steady_clock;

double elapsed_seconds(Clock::time_point started) {
    return std::chrono::duration<double>(Clock::now() - started).count();
}

std::size_t peak_resident_bytes() {
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) return 0;
#if defined(__APPLE__)
    return static_cast<std::size_t>(usage.ru_maxrss);
#else
    return static_cast<std::size_t>(usage.ru_maxrss) * 1024U;
#endif
}

std::vector<double> diagonal_inverse(
    const smave::benchmark::SparseMatrix& matrix) {
    std::vector<double> inverse(matrix.rows, 1.0);
    for (std::size_t row = 0; row < matrix.rows; ++row) {
        double diagonal{};
        for (std::size_t offset = matrix.row_offsets[row];
             offset < matrix.row_offsets[row + 1]; ++offset) {
            if (matrix.column_indices[offset] == row) {
                diagonal += matrix.values[offset];
            }
        }
        if (std::abs(diagonal) > 1.0e-14 && std::isfinite(diagonal)) {
            inverse[row] = 1.0 / diagonal;
        }
    }
    return inverse;
}

bool plausible_spd(const smave::benchmark::SparseMatrix& matrix) {
    if (!matrix.declared_symmetric || matrix.rows != matrix.columns) return false;
    for (std::size_t row = 0; row < matrix.rows; ++row) {
        double diagonal{};
        for (std::size_t offset = matrix.row_offsets[row];
             offset < matrix.row_offsets[row + 1]; ++offset) {
            if (matrix.column_indices[offset] == row) diagonal += matrix.values[offset];
        }
        if (!(diagonal > 0.0) || !std::isfinite(diagonal)) return false;
    }
    return true;
}

bool has_nonzero_diagonal(const smave::benchmark::SparseMatrix& matrix) {
    for (std::size_t row = 0; row < matrix.rows; ++row) {
        bool found{};
        for (std::size_t offset = matrix.row_offsets[row];
             offset < matrix.row_offsets[row + 1]; ++offset) {
            if (matrix.column_indices[offset] == row &&
                std::abs(matrix.values[offset]) > 1.0e-14) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

double diagonal_condition_estimate(const smave::benchmark::SparseMatrix& matrix) {
    double minimum = std::numeric_limits<double>::infinity();
    double maximum{};
    for (std::size_t row = 0; row < matrix.rows; ++row) {
        double diagonal{};
        for (std::size_t offset = matrix.row_offsets[row];
             offset < matrix.row_offsets[row + 1]; ++offset) {
            if (matrix.column_indices[offset] == row) diagonal += matrix.values[offset];
        }
        const auto magnitude = std::abs(diagonal);
        if (magnitude > 0.0 && std::isfinite(magnitude)) {
            minimum = std::min(minimum, magnitude);
            maximum = std::max(maximum, magnitude);
        }
    }
    return std::isfinite(minimum) && minimum > 0.0 ? maximum / minimum
                                                    : std::numeric_limits<double>::infinity();
}

struct SolveResult {
    smave::benchmark::SolverObservation observation;
    std::vector<double> solution;
};

constexpr double kRelativeResidualTolerance = 1.0e-8;
constexpr double kRelativeSolutionErrorTolerance = 1.0e-6;
constexpr double kBackwardStableResidualTolerance = 1.0e-12;

bool backward_stable_forward_error_exception(
    const smave::benchmark::SolverObservation& observation,
    const std::vector<double>* expected) {
    return expected != nullptr &&
        observation.relative_residual <= kBackwardStableResidualTolerance &&
        std::isfinite(observation.relative_solution_error);
}

bool passes_correctness_gate(
    const smave::benchmark::SolverObservation& observation,
    const std::vector<double>* expected) {
    return observation.relative_residual <= kRelativeResidualTolerance &&
        (expected == nullptr ||
         observation.relative_solution_error <= kRelativeSolutionErrorTolerance ||
         backward_stable_forward_error_exception(observation, expected));
}

void append_correctness_gate_diagnostic(
    smave::benchmark::SolverObservation& observation,
    const std::vector<double>* expected) {
    if (observation.status == "converged" &&
        backward_stable_forward_error_exception(observation, expected) &&
        observation.relative_solution_error > kRelativeSolutionErrorTolerance) {
        observation.reason +=
            "; strict backward-stability exception: residual <= 1e-12; "
            "manufactured forward error retained as conditioning diagnostic";
    } else if (observation.status != "converged" && expected != nullptr &&
               observation.relative_solution_error >
                   kRelativeSolutionErrorTolerance) {
        observation.reason += "; relative solution error gate failed";
    }
}

SolveResult solve_smave(
    const smave::benchmark::SparseMatrix& matrix,
    const std::vector<double>& right_hand_side,
    const std::vector<double>* expected,
    const smave::SolvePlan& plan,
    int maximum_iterations,
    int restart_dimension,
    std::size_t direct_limit) {
    SolveResult result;
    const auto setup_started = Clock::now();
    const auto operation = [&matrix](
        const std::vector<double>& input, std::vector<double>& output) {
        output = matrix.multiply(input);
        return true;
    };
    const auto jacobi = diagonal_inverse(matrix);
    const smave::Preconditioner jacobi_preconditioner =
        [jacobi](const std::vector<double>& residual, std::vector<double>& output) {
            if (residual.size() != jacobi.size()) return false;
            output.resize(residual.size());
            for (std::size_t index = 0; index < residual.size(); ++index) {
                output[index] = jacobi[index] * residual[index];
            }
            return true;
        };
    auto finalize_krylov = [&](const smave::KrylovResult& krylov,
                               const std::string& backend) {
        result.observation.backend = backend;
        result.observation.iterations = static_cast<std::size_t>(krylov.iterations);
        result.observation.reason = krylov.reason;
        result.solution = krylov.solution;
        if (!result.solution.empty()) {
            result.observation.relative_residual = smave::benchmark::relative_residual(
                matrix, result.solution, right_hand_side);
            result.observation.relative_solution_error = expected != nullptr
                ? smave::benchmark::relative_error(result.solution, *expected) : -1.0;
        } else {
            result.observation.relative_residual = std::numeric_limits<double>::infinity();
            result.observation.relative_solution_error = -1.0;
        }
        result.observation.status = krylov.converged &&
                passes_correctness_gate(result.observation, expected)
            ? "converged" : "failed";
        append_correctness_gate_diagnostic(result.observation, expected);
        return result.observation.status == "converged";
    };
    auto make_sparse_system = [&]() {
        smave::LinearSystem direct_system;
        direct_system.unknowns.resize(matrix.rows);
        direct_system.sparsity.row_count = matrix.rows;
        direct_system.sparsity.column_count = matrix.columns;
        direct_system.sparsity.row_offsets = matrix.row_offsets;
        direct_system.sparsity.column_indices = matrix.column_indices;
        direct_system.sparse_values = matrix.values;
        direct_system.right_hand_side = right_hand_side;
        return direct_system;
    };
    std::string failure_chain;
    auto append_failure = [&](const std::string& failure) {
        if (!failure_chain.empty()) failure_chain += "; ";
        failure_chain += failure;
    };
    for (const auto& step : plan.steps) {
        if (step.expert_version == "pcg-ic0-cpu-v1" ||
            step.expert_version == "pcg-jacobi-cpu-v1") {
            smave::Preconditioner preconditioner = jacobi_preconditioner;
            if (step.expert_version == "pcg-ic0-cpu-v1" &&
                matrix.nonzeros() <= 20000000U) {
                auto system = make_sparse_system();
                const auto incomplete = smave::incomplete_cholesky_zero_preconditioner(
                    system, system.sparsity);
                if (incomplete) preconditioner = incomplete;
            }
            result.observation.setup_seconds = elapsed_seconds(setup_started);
            const auto solve_started = Clock::now();
            const auto krylov = smave::preconditioned_conjugate_gradient(
                matrix.rows, operation, right_hand_side,
                std::vector<double>(matrix.columns), preconditioner,
                1.0e-12, 1.0e-8, maximum_iterations);
            result.observation.solve_seconds += elapsed_seconds(solve_started);
            if (finalize_krylov(krylov, step.expert_version)) {
                result.observation.reason = "SolvePlan " + plan.plan_id + "; " +
                    result.observation.reason;
                result.observation.peak_resident_bytes = peak_resident_bytes();
                return result;
            }
            append_failure(step.expert_version + " failed");
            continue;
        }
        if (step.expert_version == "gmres-ilu0-cpu-v1" ||
            step.expert_version == "gmres-ilut-cpu-v1") {
            smave::Preconditioner preconditioner = jacobi_preconditioner;
            if (matrix.nonzeros() <= 20000000U) {
                auto system = make_sparse_system();
                const auto incomplete = step.expert_version == "gmres-ilut-cpu-v1"
                    ? smave::incomplete_lu_threshold_preconditioner(system, 1.0e-3, 40)
                    : smave::incomplete_lu_zero_preconditioner(system, system.sparsity);
                if (incomplete) preconditioner = incomplete;
            }
            result.observation.setup_seconds = elapsed_seconds(setup_started);
            const auto solve_started = Clock::now();
            const auto krylov = smave::restarted_gmres(
                matrix.rows, operation, right_hand_side,
                std::vector<double>(matrix.columns), preconditioner,
                1.0e-12, 1.0e-8, maximum_iterations, restart_dimension);
            result.observation.solve_seconds += elapsed_seconds(solve_started);
            if (finalize_krylov(krylov, step.expert_version)) {
                result.observation.reason = "SolvePlan " + plan.plan_id + "; " +
                    result.observation.reason;
                result.observation.peak_resident_bytes = peak_resident_bytes();
                return result;
            }
            append_failure(step.expert_version + " failed");
            continue;
        }
        if (step.expert_version == smave::industrial_sparse_direct_backend() &&
            matrix.rows <= direct_limit) {
            auto system = make_sparse_system();
            result.observation.setup_seconds = elapsed_seconds(setup_started);
            const auto solve_started = Clock::now();
            const auto direct = smave::industrial_sparse_direct_solve(system);
            result.observation.solve_seconds += elapsed_seconds(solve_started);
            result.observation.backend = direct.backend;
            result.observation.reason = direct.reason;
            result.solution = direct.solution;
            if (direct.solved) {
                result.observation.relative_residual = smave::benchmark::relative_residual(
                    matrix, result.solution, right_hand_side);
                result.observation.relative_solution_error = expected != nullptr
                    ? smave::benchmark::relative_error(result.solution, *expected) : -1.0;
                result.observation.status = passes_correctness_gate(
                    result.observation, expected) ? "converged" : "failed";
                append_correctness_gate_diagnostic(result.observation, expected);
                if (result.observation.status == "converged") {
                    result.observation.reason = "SolvePlan " + plan.plan_id +
                        (failure_chain.empty() ? "; " :
                            "; prior attempts: " + failure_chain + "; ") +
                        result.observation.reason;
                    result.observation.peak_resident_bytes = peak_resident_bytes();
                    return result;
                }
            }
            append_failure(step.expert_version + " failed");
            continue;
        }
        if (step.expert_version == smave::superlu_sparse_direct_backend() &&
            matrix.rows <= direct_limit) {
            auto system = make_sparse_system();
            result.observation.setup_seconds = elapsed_seconds(setup_started);
            const auto solve_started = Clock::now();
            const auto direct = smave::superlu_sparse_direct_solve(system);
            result.observation.solve_seconds += elapsed_seconds(solve_started);
            result.observation.backend = direct.backend;
            result.observation.reason = direct.reason;
            result.solution = direct.solution;
            if (direct.solved) {
                result.observation.relative_residual = smave::benchmark::relative_residual(
                    matrix, result.solution, right_hand_side);
                result.observation.relative_solution_error = expected != nullptr
                    ? smave::benchmark::relative_error(result.solution, *expected) : -1.0;
                result.observation.status = passes_correctness_gate(
                    result.observation, expected) ? "converged" : "failed";
                append_correctness_gate_diagnostic(result.observation, expected);
                if (result.observation.status == "converged") {
                    result.observation.reason = "SolvePlan " + plan.plan_id +
                        (failure_chain.empty() ? "; " :
                            "; prior attempts: " + failure_chain + "; ") +
                        result.observation.reason;
                    result.observation.peak_resident_bytes = peak_resident_bytes();
                    return result;
                }
            }
            append_failure(step.expert_version + " failed");
            continue;
        }
        if (step.expert_version == "sparse-ordered-threshold-pivot-cpu-v2" &&
            matrix.rows <= std::min<std::size_t>(direct_limit, 5000U)) {
            auto system = make_sparse_system();
            result.observation.setup_seconds = elapsed_seconds(setup_started);
            const auto solve_started = Clock::now();
            const auto direct = smave::sparse_ordered_threshold_pivot_solve(system);
            result.observation.solve_seconds += elapsed_seconds(solve_started);
            result.observation.backend = step.expert_version;
            result.observation.reason = direct.solved
                ? "ordered threshold-pivot solve completed"
                : "ordered threshold-pivot solve failed";
            result.solution = direct.solution;
            if (direct.solved) {
                result.observation.relative_residual = smave::benchmark::relative_residual(
                    matrix, result.solution, right_hand_side);
                result.observation.relative_solution_error = expected != nullptr
                    ? smave::benchmark::relative_error(result.solution, *expected) : -1.0;
                result.observation.status = passes_correctness_gate(
                    result.observation, expected) ? "converged" : "failed";
                append_correctness_gate_diagnostic(result.observation, expected);
                if (result.observation.status == "converged") {
                    result.observation.reason = "SolvePlan " + plan.plan_id +
                        (failure_chain.empty() ? "; " :
                            "; prior attempts: " + failure_chain + "; ") +
                        result.observation.reason;
                    result.observation.peak_resident_bytes = peak_resident_bytes();
                    return result;
                }
            }
            append_failure(step.expert_version + " failed");
        }
    }
    const auto fallback_started = Clock::now();
    const auto fallback = smave::restarted_gmres(
        matrix.rows, operation, right_hand_side,
        std::vector<double>(matrix.columns), jacobi_preconditioner,
        1.0e-12, 1.0e-8, maximum_iterations, restart_dimension);
    result.observation.solve_seconds += elapsed_seconds(fallback_started);
    finalize_krylov(fallback, plan.terminal_fallback);
    result.observation.reason = "SolvePlan " + plan.plan_id + "; " + failure_chain +
        "; terminal fallback: " + result.observation.reason;
    result.observation.peak_resident_bytes = peak_resident_bytes();
    return result;
}

SolveResult solve_superlu(
    const smave::benchmark::SparseMatrix& matrix,
    const std::vector<double>& right_hand_side,
    const std::vector<double>* expected,
    std::size_t direct_limit) {
    SolveResult result;
    result.observation.backend = "superlu-dgssv-7.0.1";
#if !defined(SMAVE_HAVE_BENCH_SUPERLU)
    (void)matrix;
    (void)right_hand_side;
    (void)expected;
    (void)direct_limit;
    result.observation.status = "unavailable";
    result.observation.reason = "SuperLU benchmark library was not configured";
    return result;
#else
    if (matrix.rows > direct_limit) {
        result.observation.status = "skipped-resource-limit";
        result.observation.reason = "matrix exceeds configured direct factorization limit";
        return result;
    }
    const auto setup_started = Clock::now();
    std::vector<int_t> column_counts(matrix.columns);
    for (const auto column : matrix.column_indices) ++column_counts[column];
    std::vector<int_t> column_offsets(matrix.columns + 1);
    for (std::size_t column = 0; column < matrix.columns; ++column) {
        column_offsets[column + 1] = column_offsets[column] + column_counts[column];
    }
    std::vector<int_t> positions = column_offsets;
    auto* values = doubleMalloc(static_cast<int_t>(matrix.nonzeros()));
    auto* row_indices = intMalloc(static_cast<int_t>(matrix.nonzeros()));
    auto* offsets = intMalloc(static_cast<int_t>(matrix.columns + 1));
    if (values == nullptr || row_indices == nullptr || offsets == nullptr) {
        throw std::bad_alloc();
    }
    for (std::size_t column = 0; column <= matrix.columns; ++column) {
        offsets[column] = column_offsets[column];
    }
    for (std::size_t row = 0; row < matrix.rows; ++row) {
        for (std::size_t offset = matrix.row_offsets[row];
             offset < matrix.row_offsets[row + 1]; ++offset) {
            const auto column = matrix.column_indices[offset];
            const auto destination = positions[column]++;
            values[destination] = matrix.values[offset];
            row_indices[destination] = static_cast<int_t>(row);
        }
    }
    auto* rhs = doubleMalloc(static_cast<int_t>(right_hand_side.size()));
    if (rhs == nullptr) throw std::bad_alloc();
    std::copy(right_hand_side.begin(), right_hand_side.end(), rhs);
    SuperMatrix a{};
    SuperMatrix b{};
    SuperMatrix lower{};
    SuperMatrix upper{};
    dCreate_CompCol_Matrix(
        &a, static_cast<int_t>(matrix.rows), static_cast<int_t>(matrix.columns),
        static_cast<int_t>(matrix.nonzeros()), values, row_indices, offsets,
        SLU_NC, SLU_D, SLU_GE);
    dCreate_Dense_Matrix(
        &b, static_cast<int_t>(matrix.rows), 1, rhs,
        static_cast<int_t>(matrix.rows), SLU_DN, SLU_D, SLU_GE);
    std::vector<int> permutation_columns(matrix.columns);
    std::vector<int> permutation_rows(matrix.rows);
    superlu_options_t options{};
    set_default_options(&options);
    options.PrintStat = NO;
    SuperLUStat_t statistics{};
    StatInit(&statistics);
    result.observation.setup_seconds = elapsed_seconds(setup_started);
    const auto solve_started = Clock::now();
    int_t information{};
    dgssv(
        &options, &a, permutation_columns.data(), permutation_rows.data(),
        &lower, &upper, &b, &statistics, &information);
    result.observation.solve_seconds = elapsed_seconds(solve_started);
    result.observation.reason = "dgssv info=" + std::to_string(information);
    if (information == 0) {
        result.solution.assign(rhs, rhs + right_hand_side.size());
        result.observation.relative_residual = smave::benchmark::relative_residual(
            matrix, result.solution, right_hand_side);
        result.observation.relative_solution_error = expected != nullptr
            ? smave::benchmark::relative_error(result.solution, *expected)
            : -1.0;
        result.observation.status = passes_correctness_gate(result.observation, expected)
            ? "converged" : "failed";
        append_correctness_gate_diagnostic(result.observation, expected);
    } else {
        result.observation.status = "failed";
        result.observation.relative_residual = std::numeric_limits<double>::infinity();
        result.observation.relative_solution_error = -1.0;
    }
    result.observation.peak_resident_bytes = peak_resident_bytes();
    StatFree(&statistics);
    Destroy_CompCol_Matrix(&a);
    Destroy_SuperMatrix_Store(&b);
    if (information <= static_cast<int_t>(matrix.columns)) {
        Destroy_SuperNode_Matrix(&lower);
        Destroy_CompCol_Matrix(&upper);
    }
    return result;
#endif
}

SolveResult solve_petsc(
    const smave::benchmark::SparseMatrix& matrix,
    const std::vector<double>& right_hand_side,
    const std::vector<double>* expected,
    int maximum_iterations,
    int restart_dimension) {
    SolveResult result;
    result.observation.backend = "petsc-ksp-3.25.3";
#if !defined(SMAVE_HAVE_BENCH_PETSC)
    (void)matrix;
    (void)right_hand_side;
    (void)expected;
    (void)maximum_iterations;
    (void)restart_dimension;
    result.observation.status = "unavailable";
    result.observation.reason = "PETSc benchmark library was not configured";
    return result;
#else
    if (matrix.rows > static_cast<std::size_t>(std::numeric_limits<PetscInt>::max()) ||
        matrix.nonzeros() > static_cast<std::size_t>(std::numeric_limits<PetscInt>::max())) {
        result.observation.status = "skipped-index-limit";
        result.observation.reason = "matrix exceeds PETSc integer index range";
        return result;
    }
    const auto setup_started = Clock::now();
    std::vector<PetscInt> row_offsets(matrix.row_offsets.begin(), matrix.row_offsets.end());
    std::vector<PetscInt> column_indices(
        matrix.column_indices.begin(), matrix.column_indices.end());
    Mat petsc_matrix{};
    Vec right{};
    Vec solution{};
    KSP solver{};
    PetscCallAbort(PETSC_COMM_SELF, MatCreateSeqAIJWithArrays(
        PETSC_COMM_SELF,
        static_cast<PetscInt>(matrix.rows),
        static_cast<PetscInt>(matrix.columns),
        row_offsets.data(), column_indices.data(),
        const_cast<double*>(matrix.values.data()),
        &petsc_matrix));
    PetscCallAbort(PETSC_COMM_SELF, VecCreateSeqWithArray(
        PETSC_COMM_SELF, 1, static_cast<PetscInt>(right_hand_side.size()),
        right_hand_side.data(), &right));
    PetscCallAbort(PETSC_COMM_SELF, VecDuplicate(right, &solution));
    PetscCallAbort(PETSC_COMM_SELF, VecSet(solution, 0.0));
    PetscCallAbort(PETSC_COMM_SELF, KSPCreate(PETSC_COMM_SELF, &solver));
    PetscCallAbort(PETSC_COMM_SELF, KSPSetOperators(solver, petsc_matrix, petsc_matrix));
    PC preconditioner{};
    PetscCallAbort(PETSC_COMM_SELF, KSPGetPC(solver, &preconditioner));
    if (plausible_spd(matrix)) {
        PetscCallAbort(PETSC_COMM_SELF, KSPSetType(solver, KSPCG));
        PetscCallAbort(PETSC_COMM_SELF, PCSetType(preconditioner, PCICC));
    } else {
        PetscCallAbort(PETSC_COMM_SELF, KSPSetType(solver, KSPGMRES));
        PetscCallAbort(PETSC_COMM_SELF, KSPGMRESSetRestart(
            solver, static_cast<PetscInt>(restart_dimension)));
        PetscCallAbort(PETSC_COMM_SELF, PCSetType(
            preconditioner, has_nonzero_diagonal(matrix) ? PCILU : PCNONE));
    }
    PetscCallAbort(PETSC_COMM_SELF, KSPSetTolerances(
        solver, 1.0e-8, 1.0e-12, PETSC_DEFAULT,
        static_cast<PetscInt>(maximum_iterations)));
    result.observation.setup_seconds = elapsed_seconds(setup_started);
    const auto solve_started = Clock::now();
    const auto solve_error = KSPSolve(solver, right, solution);
    result.observation.solve_seconds = elapsed_seconds(solve_started);
    KSPConvergedReason reason{};
    PetscInt iterations{};
    PetscCallAbort(PETSC_COMM_SELF, KSPGetConvergedReason(solver, &reason));
    PetscCallAbort(PETSC_COMM_SELF, KSPGetIterationNumber(solver, &iterations));
    result.observation.iterations = static_cast<std::size_t>(iterations);
    result.observation.reason = "KSP reason=" + std::to_string(static_cast<int>(reason));
    if (solve_error == 0 && reason > 0) {
        const PetscScalar* data{};
        PetscCallAbort(PETSC_COMM_SELF, VecGetArrayRead(solution, &data));
        result.solution.assign(data, data + matrix.rows);
        PetscCallAbort(PETSC_COMM_SELF, VecRestoreArrayRead(solution, &data));
        result.observation.relative_residual = smave::benchmark::relative_residual(
            matrix, result.solution, right_hand_side);
        result.observation.relative_solution_error = expected != nullptr
            ? smave::benchmark::relative_error(result.solution, *expected)
            : -1.0;
        result.observation.status = passes_correctness_gate(result.observation, expected)
            ? "converged" : "failed";
        append_correctness_gate_diagnostic(result.observation, expected);
    } else {
        result.observation.status = "failed";
        result.observation.relative_residual = std::numeric_limits<double>::infinity();
        result.observation.relative_solution_error = -1.0;
    }
    result.observation.peak_resident_bytes = peak_resident_bytes();
    PetscCallAbort(PETSC_COMM_SELF, KSPDestroy(&solver));
    PetscCallAbort(PETSC_COMM_SELF, VecDestroy(&solution));
    PetscCallAbort(PETSC_COMM_SELF, VecDestroy(&right));
    PetscCallAbort(PETSC_COMM_SELF, MatDestroy(&petsc_matrix));
    return result;
#endif
}

std::string option(int argc, char** argv, const std::string& name) {
    for (int index = 1; index + 1 < argc; ++index) {
        if (argv[index] == name) return argv[index + 1];
    }
    return {};
}

}  // namespace

int main(int argc, char** argv) {
    try {
#if defined(SMAVE_HAVE_BENCH_PETSC)
        PetscCallAbort(PETSC_COMM_SELF, PetscInitializeNoArguments());
        PetscCallAbort(PETSC_COMM_SELF, PetscPushErrorHandler(
            PetscReturnErrorHandler, nullptr));
#endif
        if (argc < 5) {
            throw std::invalid_argument(
                "usage: smave_sparse_case_benchmark --matrix FILE --output FILE "
                "[--rhs FILE] [--max-iterations N] [--restart N] [--direct-limit N]");
        }
        const auto matrix_path = option(argc, argv, "--matrix");
        const auto output_path = option(argc, argv, "--output");
        if (matrix_path.empty() || output_path.empty()) {
            throw std::invalid_argument("matrix and output are required");
        }
        const int maximum_iterations = option(argc, argv, "--max-iterations").empty()
            ? 500 : std::stoi(option(argc, argv, "--max-iterations"));
        const int restart = option(argc, argv, "--restart").empty()
            ? 20 : std::stoi(option(argc, argv, "--restart"));
        const std::size_t direct_limit = option(argc, argv, "--direct-limit").empty()
            ? 30000U : std::stoull(option(argc, argv, "--direct-limit"));
        const auto matrix = smave::benchmark::read_matrix_market(matrix_path);
        if (matrix.rows != matrix.columns) {
            throw std::invalid_argument("sparse solve benchmark requires a square matrix");
        }
        std::vector<double> expected;
        std::vector<double> right_hand_side;
        const auto rhs_path = option(argc, argv, "--rhs");
        std::string rhs_kind;
        if (!rhs_path.empty()) {
            right_hand_side = smave::benchmark::read_matrix_market_vector(
                rhs_path, matrix.rows);
            rhs_kind = "suite-provided";
        } else {
            expected = smave::benchmark::deterministic_reference_solution(matrix.columns);
            right_hand_side = matrix.multiply(expected);
            rhs_kind = "manufactured-known-solution";
        }
        const auto* expected_pointer = expected.empty() ? nullptr : &expected;
        const auto spd = plausible_spd(matrix);
        const auto plan = smave::route_sparse_linear_system({
            .fingerprint = std::filesystem::path(matrix_path).stem().string() + "-" +
                std::to_string(matrix.rows) + "-" + std::to_string(matrix.nonzeros()),
            .rows = matrix.rows,
            .columns = matrix.columns,
            .nonzeros = matrix.nonzeros(),
            .structurally_symmetric = matrix.declared_symmetric,
            .numerically_symmetric = matrix.declared_symmetric,
            .numerically_positive_definite = spd,
            .diagonal_condition_estimate = diagonal_condition_estimate(matrix),
        });
        auto smave_result = solve_smave(
            matrix, right_hand_side, expected_pointer,
            plan,
            maximum_iterations, restart, direct_limit);
        auto superlu_result = solve_superlu(
            matrix, right_hand_side, expected_pointer, direct_limit);
        auto petsc_result = solve_petsc(
            matrix, right_hand_side, expected_pointer,
            maximum_iterations, restart);
        smave::benchmark::SparseCaseResult result;
        result.test_case.name = std::filesystem::path(matrix_path).stem().string();
        result.test_case.matrix_path = matrix_path;
        result.test_case.right_hand_side_path = rhs_path;
        result.rows = matrix.rows;
        result.columns = matrix.columns;
        result.nonzeros = matrix.nonzeros();
        result.value_kind = matrix.pattern ? "pattern" : "numeric";
        result.symmetry = matrix.declared_symmetric ? "symmetric" : "general";
        result.right_hand_side_kind = rhs_kind;
        result.equation_family = plan.assessment.equation_family;
        result.solve_plan_id = plan.plan_id;
        for (const auto& step : plan.steps) {
            result.backend_chain.push_back(step.expert_version);
        }
        result.backend_chain.push_back(plan.terminal_fallback);
        result.smave = smave_result.observation;
        result.references.push_back(superlu_result.observation);
        result.references.push_back(petsc_result.observation);
        result.correctness_agreement = result.smave.status == "converged" &&
            (expected_pointer != nullptr
                ? (superlu_result.observation.status == "converged" ||
                   petsc_result.observation.status == "converged")
                : ((superlu_result.observation.status == "converged" &&
                    smave::benchmark::relative_error(
                        smave_result.solution, superlu_result.solution) <=
                        kRelativeSolutionErrorTolerance) ||
                   (petsc_result.observation.status == "converged" &&
                    smave::benchmark::relative_error(
                        smave_result.solution, petsc_result.solution) <=
                        kRelativeSolutionErrorTolerance)));
        smave::benchmark::write_sparse_case_result(result, output_path);
        std::cout << result.test_case.name
                  << " smave=" << result.smave.status
                  << " superlu=" << superlu_result.observation.status
                  << " petsc=" << petsc_result.observation.status
                  << " agreement=" << result.correctness_agreement << '\n';
#if defined(SMAVE_HAVE_BENCH_PETSC)
        PetscCallAbort(PETSC_COMM_SELF, PetscFinalize());
#endif
        return result.smave.status == "converged" ? 0 : 4;
    } catch (const std::exception& error) {
        std::cerr << "sparse case benchmark failure: " << error.what() << '\n';
        return 2;
    }
}
