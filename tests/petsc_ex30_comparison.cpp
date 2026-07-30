#include "smave/linear.hpp"

#include <petsclandau.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

extern "C" int smave_petsc_ex30_original_main(int, char**);

namespace {

struct Ex30Evidence {
    std::vector<double> initial;
    std::vector<double> smave;
    std::size_t iterations{};
    double smave_seconds{};
    double smave_residual{};
};

Ex30Evidence evidence;

std::vector<double> read_vector(Vec vector) {
    PetscInt size{};
    const PetscScalar* values{};
    PetscCallAbort(PETSC_COMM_SELF, VecGetLocalSize(vector, &size));
    PetscCallAbort(PETSC_COMM_SELF, VecGetArrayRead(vector, &values));
    std::vector<double> result(values, values + size);
    PetscCallAbort(PETSC_COMM_SELF, VecRestoreArrayRead(vector, &values));
    return result;
}

double relative_error(const std::vector<double>& left,
                      const std::vector<double>& right) {
    double numerator{};
    double denominator{};
    for (std::size_t index = 0; index < left.size(); ++index) {
        numerator = std::max(numerator, std::abs(left[index] - right[index]));
        denominator = std::max(denominator, std::abs(right[index]));
    }
    return numerator / std::max(1.0, denominator);
}

void invalidate_landau_operator(TS solver) {
    Mat jacobian{}, preconditioner{};
    PetscCallAbort(PETSC_COMM_SELF, TSGetIJacobian(
        solver, &jacobian, &preconditioner, nullptr, nullptr));
    PetscCallAbort(PETSC_COMM_SELF, MatZeroEntries(jacobian));
}

double implicit_residual_norm(TS solver, Vec template_vector,
                              const std::vector<double>& initial,
                              const std::vector<double>& state) {
    Vec state_vector{}, derivative_vector{}, residual_vector{};
    PetscCallAbort(PETSC_COMM_SELF, VecDuplicate(template_vector, &state_vector));
    PetscCallAbort(PETSC_COMM_SELF, VecDuplicate(template_vector, &derivative_vector));
    PetscCallAbort(PETSC_COMM_SELF, VecDuplicate(template_vector, &residual_vector));
    std::vector<double> derivative(state.size());
    std::vector<double> residual(state.size());
    for (std::size_t index = 0; index < state.size(); ++index)
        derivative[index] = (state[index] - initial[index]) / 0.1;
    PetscCallAbort(PETSC_COMM_SELF, VecPlaceArray(state_vector, state.data()));
    PetscCallAbort(PETSC_COMM_SELF, VecPlaceArray(derivative_vector, derivative.data()));
    PetscCallAbort(PETSC_COMM_SELF, VecPlaceArray(residual_vector, residual.data()));
    invalidate_landau_operator(solver);
    PetscCallAbort(PETSC_COMM_SELF, DMPlexLandauIFunction(
        solver, 0.1, state_vector, derivative_vector, residual_vector, nullptr));
    PetscCallAbort(PETSC_COMM_SELF, VecResetArray(residual_vector));
    PetscCallAbort(PETSC_COMM_SELF, VecResetArray(derivative_vector));
    PetscCallAbort(PETSC_COMM_SELF, VecResetArray(state_vector));
    double norm{};
    for (double value : residual) norm = std::max(norm, std::abs(value));
    PetscCallAbort(PETSC_COMM_SELF, VecDestroy(&residual_vector));
    PetscCallAbort(PETSC_COMM_SELF, VecDestroy(&derivative_vector));
    PetscCallAbort(PETSC_COMM_SELF, VecDestroy(&state_vector));
    return norm;
}

}  // namespace

extern "C" PetscErrorCode SmaveEx30BeforeSolve(TS solver, Vec state) {
    PetscFunctionBeginUser;
    evidence.initial = read_vector(state);
    evidence.smave = evidence.initial;
    const PetscInt size = static_cast<PetscInt>(evidence.initial.size());
    Vec candidate_vector{}, derivative_vector{}, residual_vector{};
    PetscCall(VecDuplicate(state, &candidate_vector));
    PetscCall(VecDuplicate(state, &derivative_vector));
    PetscCall(VecDuplicate(state, &residual_vector));
    std::vector<double> derivative(size);
    std::vector<double> residual(size);
    std::vector<double> perturbed(size);
    std::vector<double> perturbed_residual(size);
    constexpr double delta_time = 0.1;
    const auto started = std::chrono::steady_clock::now();
    double residual_norm{};
    for (int newton = 0; newton < 12; ++newton) {
        for (PetscInt index = 0; index < size; ++index)
            derivative[index] = (evidence.smave[index] - evidence.initial[index]) /
                delta_time;
        PetscCall(VecPlaceArray(candidate_vector, evidence.smave.data()));
        PetscCall(VecPlaceArray(derivative_vector, derivative.data()));
        PetscCall(VecPlaceArray(residual_vector, residual.data()));
        invalidate_landau_operator(solver);
        PetscCall(DMPlexLandauIFunction(
            solver, delta_time, candidate_vector, derivative_vector,
            residual_vector, nullptr));
        PetscCall(VecResetArray(residual_vector));
        PetscCall(VecResetArray(derivative_vector));
        PetscCall(VecResetArray(candidate_vector));
        residual_norm = 0.0;
        for (double value : residual)
            residual_norm = std::max(residual_norm, std::abs(value));
        if (residual_norm <= 1.0e-10) break;
        const auto base_state = evidence.smave;
        const auto base_residual = residual;
        const smave::LinearOperator jacobian =
            [&](const std::vector<double>& direction,
                std::vector<double>& output) {
                double direction_norm{};
                double state_norm{};
                for (PetscInt index = 0; index < size; ++index) {
                    direction_norm = std::max(direction_norm, std::abs(direction[index]));
                    state_norm = std::max(state_norm, std::abs(base_state[index]));
                }
                if (direction_norm == 0.0) {
                    output.assign(size, 0.0);
                    return true;
                }
                const double epsilon = 1.0e-7 * (1.0 + state_norm) / direction_norm;
                for (PetscInt index = 0; index < size; ++index) {
                    perturbed[index] = base_state[index] + epsilon * direction[index];
                    derivative[index] = (perturbed[index] - evidence.initial[index]) /
                        delta_time;
                }
                PetscCallAbort(PETSC_COMM_SELF, VecPlaceArray(
                    candidate_vector, perturbed.data()));
                PetscCallAbort(PETSC_COMM_SELF, VecPlaceArray(
                    derivative_vector, derivative.data()));
                PetscCallAbort(PETSC_COMM_SELF, VecPlaceArray(
                    residual_vector, perturbed_residual.data()));
                invalidate_landau_operator(solver);
                PetscCallAbort(PETSC_COMM_SELF, DMPlexLandauIFunction(
                    solver, delta_time, candidate_vector, derivative_vector,
                    residual_vector, nullptr));
                PetscCallAbort(PETSC_COMM_SELF, VecResetArray(residual_vector));
                PetscCallAbort(PETSC_COMM_SELF, VecResetArray(derivative_vector));
                PetscCallAbort(PETSC_COMM_SELF, VecResetArray(candidate_vector));
                output.resize(size);
                for (PetscInt index = 0; index < size; ++index)
                    output[index] = (perturbed_residual[index] - base_residual[index]) /
                        epsilon;
                return true;
            };
        std::vector<double> right_hand_side(size);
        for (PetscInt index = 0; index < size; ++index)
            right_hand_side[index] = -residual[index];
        const auto correction = smave::restarted_gmres(
            size, jacobian, right_hand_side, std::vector<double>(size),
            [](const std::vector<double>& input, std::vector<double>& output) {
                output = input;
                return true;
            }, 1.0e-11, 1.0e-9, 200, std::min<PetscInt>(28, size));
        if (!correction.converged)
            SETERRQ(PETSC_COMM_SELF, PETSC_ERR_NOT_CONVERGED,
                    "SMAVE ex30 JFNK failed: %s", correction.reason.c_str());
        evidence.iterations += static_cast<std::size_t>(correction.iterations);
        for (PetscInt index = 0; index < size; ++index)
            evidence.smave[index] += correction.solution[index];
    }
    evidence.smave_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - started).count();
    evidence.smave_residual = residual_norm;
    PetscCheck(residual_norm <= 1.0e-8, PETSC_COMM_SELF,
               PETSC_ERR_NOT_CONVERGED, "SMAVE ex30 residual %g", residual_norm);
    PetscCall(VecDestroy(&residual_vector));
    PetscCall(VecDestroy(&derivative_vector));
    PetscCall(VecDestroy(&candidate_vector));
    PetscFunctionReturn(PETSC_SUCCESS);
}

extern "C" PetscErrorCode SmaveEx30AfterSolve(
    TS solver, Vec state, PetscLogDouble petsc_seconds) {
    PetscFunctionBeginUser;
    const auto petsc = read_vector(state);
    const double error = relative_error(evidence.smave, petsc);
    const double petsc_residual = implicit_residual_norm(
        solver, state, evidence.initial, petsc);
    const double smave_residual = implicit_residual_norm(
        solver, state, evidence.initial, evidence.smave);
    const bool agreement = error <= 1.0e-4 &&
        petsc_residual <= 1.0e-6 && smave_residual <= 1.0e-6 &&
        evidence.iterations > 0;
    const char* output_path = std::getenv("SMAVE_EX30_OUTPUT");
    PetscCheck(output_path, PETSC_COMM_SELF, PETSC_ERR_ARG_NULL,
               "SMAVE_EX30_OUTPUT is required");
    std::ofstream output(output_path);
    output << std::setprecision(17)
           << "SMAVE_PETSC_EX30_COMPARISON 1\n"
           << "AGREEMENT " << agreement << "\n"
           << "ERROR " << error << "\n"
           << "TOLERANCE 1e-4\n"
           << "PETSC_RESIDUAL_INF " << petsc_residual << "\n"
           << "SMAVE_RESIDUAL_INF " << smave_residual << "\n"
           << "UNKNOWNS " << petsc.size() << "\n"
           << "SMAVE_ITERATIONS " << evidence.iterations << "\n"
           << "PETSC_SECONDS " << petsc_seconds << "\n"
           << "SMAVE_SECONDS " << evidence.smave_seconds << "\n"
           << "SMAVE_VS_PETSC_SPEEDUP "
           << petsc_seconds / evidence.smave_seconds << "\nEND\n";
    PetscCheck(agreement, PETSC_COMM_SELF, PETSC_ERR_NOT_CONVERGED,
               "SMAVE ex30 error %g", error);
    PetscFunctionReturn(PETSC_SUCCESS);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: smave_petsc_ex30_comparison OUTPUT [PETSc options]\n";
        return 2;
    }
    setenv("SMAVE_EX30_OUTPUT", argv[1], 1);
    for (int index = 1; index + 1 < argc; ++index) argv[index] = argv[index + 1];
    --argc;
    return smave_petsc_ex30_original_main(argc, argv);
}
