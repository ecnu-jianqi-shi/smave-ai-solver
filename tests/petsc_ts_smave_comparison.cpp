#include "smave/linear.hpp"

#include <petscts.h>
#include <petscdmda.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Ex5Field {
    PetscScalar Ts, Ta;
    PetscScalar u, v;
    PetscScalar p;
};

struct Ex5Context {
    DM da;
    PetscScalar csoil;
    PetscScalar dzlay;
    PetscScalar emma;
    PetscScalar wind;
    PetscScalar dewtemp;
    PetscScalar pressure1;
    PetscScalar airtemp;
    PetscScalar Ts;
    PetscScalar fract;
    PetscScalar Tc;
    PetscScalar lat;
    PetscScalar init;
    PetscScalar deep_grnd_temp;
};

struct Ex5Input {
    double surface_temperature{};
    double dew_point_temperature{};
    double air_temperature{};
    double cloud_temperature{};
    double cloud_fraction{};
    double wind_speed{};
    double precipitable_water{};
    double runtime_hours{};
    double initialization{};
};

extern "C" {
PetscScalar emission(PetscScalar);
PetscScalar fahr_to_cel(PetscScalar);
PetscErrorCode FormInitialSolution(DM, Vec, void*);
PetscErrorCode RhsFunc(TS, PetscReal, Vec, Vec, void*);
}

PetscErrorCode rhs(TS, PetscReal, Vec input, Vec output, void*) {
    const PetscScalar* x{};
    PetscScalar* y{};
    PetscCall(VecGetArrayRead(input, &x));
    PetscCall(VecGetArrayWrite(output, &y));
    y[0] = 2.0 * x[0] + x[1];
    y[1] = x[0] + 2.0 * x[1] + x[2];
    y[2] = x[1] + 2.0 * x[2];
    PetscCall(VecRestoreArrayRead(input, &x));
    PetscCall(VecRestoreArrayWrite(output, &y));
    return PETSC_SUCCESS;
}

PetscErrorCode jacobian(TS, PetscReal, Vec, Mat matrix, Mat preconditioner, void*) {
    const PetscInt rows[] = {0, 1, 2};
    const PetscScalar values[] = {2, 1, 0, 1, 2, 1, 0, 1, 2};
    PetscCall(MatSetValues(preconditioner, 3, rows, 3, rows, values, INSERT_VALUES));
    PetscCall(MatAssemblyBegin(preconditioner, MAT_FINAL_ASSEMBLY));
    PetscCall(MatAssemblyEnd(preconditioner, MAT_FINAL_ASSEMBLY));
    if (matrix != preconditioner) {
        PetscCall(MatAssemblyBegin(matrix, MAT_FINAL_ASSEMBLY));
        PetscCall(MatAssemblyEnd(matrix, MAT_FINAL_ASSEMBLY));
    }
    return PETSC_SUCCESS;
}

PetscErrorCode dae_reduced_rhs(TS, PetscReal, Vec input, Vec output, void*) {
    const PetscScalar* state{};
    PetscScalar* derivative{};
    PetscCall(VecGetArrayRead(input, &state));
    PetscCall(VecGetArrayWrite(output, &derivative));
    derivative[0] = 2.0 * state[0];
    PetscCall(VecRestoreArrayRead(input, &state));
    PetscCall(VecRestoreArrayWrite(output, &derivative));
    return PETSC_SUCCESS;
}

PetscErrorCode polynomial_chain_rhs(
    TS, PetscReal time, Vec input, Vec output, void*) {
    PetscInt size{};
    const PetscScalar* state{};
    PetscScalar* derivative{};
    PetscCall(VecGetLocalSize(input, &size));
    PetscCall(VecGetArrayRead(input, &state));
    PetscCall(VecGetArrayWrite(output, &derivative));
    if (size > 0) derivative[0] = 1.0;
    for (PetscInt index = 1; index < size; ++index) {
        derivative[index] = (index + 1) *
            (state[index - 1] + std::pow(time, index)) / 2.0;
    }
    PetscCall(VecRestoreArrayRead(input, &state));
    PetscCall(VecRestoreArrayWrite(output, &derivative));
    return PETSC_SUCCESS;
}

struct ConvectionDiffusionContext {
    PetscInt width{9};
    PetscInt height{9};
    double spacing_x{0.1};
    double spacing_y{0.1};
    double advection{1.0};
    double diffusion{0.1};
};

struct FemHeatContext {
    Mat mass{};
    Mat stiffness{};
    KSP mass_solver{};
    Vec work{};
};

struct BratuContext {
    PetscInt width{17};
    PetscInt height{17};
    double lambda{6.0};
};

struct BgkContext {
    int cells{200};
    double lower{-10.0};
    double upper{10.0};
};

struct ParticleLandauContext {
    int dimension{2};
    int particles{3};
    double extent{1.0};
    double spacing{2.0 / std::sqrt(3.0)};
    double epsilon{0.64 * std::pow(2.0 / std::sqrt(3.0), 1.98)};
};

double landau_gaussian(const ParticleLandauContext& context,
                       const double* mean, const double* point) {
    double squared_distance = 0.0;
    for (int axis = 0; axis < context.dimension; ++axis) {
        const double difference = point[axis] - mean[axis];
        squared_distance += difference * difference;
    }
    return std::pow(2.0 * PETSC_PI * context.epsilon,
                    -context.dimension / 2.0) *
        std::exp(-squared_distance / (2.0 * context.epsilon));
}

std::array<double, 2> landau_entropy_gradient(
    const ParticleLandauContext& context,
    const std::vector<double>& velocity, int particle) {
    std::array<double, 2> gradient{};
    const int cells = static_cast<int>(std::lround(
        2.0 * context.extent / context.spacing));
    const double initial = 0.5 * context.spacing - context.extent;
    for (int row = 0; row < cells; ++row) {
        for (int column = 0; column < cells; ++column) {
            const double center[] = {
                initial + column * context.spacing,
                initial + row * context.spacing};
            double density = 0.0;
            for (int other = 0; other < context.particles; ++other)
                density += landau_gaussian(
                    context, &velocity[other * context.dimension], center);
            const double log_density = std::log(density);
            for (int axis = 0; axis < context.dimension; ++axis) {
                gradient[axis] += -std::abs(
                    velocity[particle * context.dimension + axis] - center[axis]) /
                    context.epsilon * landau_gaussian(
                        context, &velocity[particle * context.dimension], center) *
                    log_density;
            }
        }
    }
    return gradient;
}

void particle_landau_rhs_values(const ParticleLandauContext& context,
                                const std::vector<double>& state,
                                std::vector<double>& derivative) {
    derivative.assign(state.size(), 0.0);
    std::vector<std::array<double, 2>> gradients(context.particles);
    for (int particle = 0; particle < context.particles; ++particle)
        gradients[particle] = landau_entropy_gradient(context, state, particle);
    for (int particle = 0; particle < context.particles; ++particle) {
        for (int other = 0; other < context.particles; ++other) {
            if (particle == other) continue;
            const double difference[] = {
                state[particle * 2] - state[other * 2],
                state[particle * 2 + 1] - state[other * 2 + 1]};
            const double squared_norm = difference[0] * difference[0] +
                difference[1] * difference[1];
            const double norm = std::sqrt(squared_norm);
            const double gradient_difference[] = {
                gradients[particle][0] - gradients[other][0],
                gradients[particle][1] - gradients[other][1]};
            for (int row = 0; row < 2; ++row) {
                for (int column = 0; column < 2; ++column) {
                    const double tensor = (row == column ? 1.0 / norm : 0.0) -
                        difference[row] * difference[column] /
                        (squared_norm * norm);
                    derivative[particle * 2 + row] +=
                        tensor * gradient_difference[column];
                }
            }
        }
    }
}

PetscErrorCode particle_landau_rhs(
    TS, PetscReal, Vec input, Vec output, void* raw_context) {
    const auto* context = static_cast<const ParticleLandauContext*>(raw_context);
    const PetscScalar* values{};
    PetscScalar* rates{};
    PetscInt size{};
    PetscCall(VecGetLocalSize(input, &size));
    PetscCall(VecGetArrayRead(input, &values));
    std::vector<double> state(values, values + size);
    std::vector<double> derivative;
    particle_landau_rhs_values(*context, state, derivative);
    PetscCall(VecGetArrayWrite(output, &rates));
    std::copy(derivative.begin(), derivative.end(), rates);
    PetscCall(VecRestoreArrayRead(input, &values));
    PetscCall(VecRestoreArrayWrite(output, &rates));
    return PETSC_SUCCESS;
}

void bgk_rhs_values(const BgkContext& context,
                    const std::vector<double>& state,
                    std::vector<double>& derivative) {
    const double spacing = (context.upper - context.lower) / context.cells;
    double mass = 0.0;
    double momentum = 0.0;
    double energy = 0.0;
    for (int cell = 0; cell < context.cells; ++cell) {
        const double velocity = context.lower + (cell + 0.5) * spacing;
        mass += state[cell];
        momentum += state[cell] * velocity;
        energy += state[cell] * velocity * velocity;
    }
    const double mean_velocity = momentum / mass;
    const double temperature = energy / mass - mean_velocity * mean_velocity;
    const double alpha = std::sqrt(0.5 / temperature);
    derivative.resize(state.size());
    for (int cell = 0; cell < context.cells; ++cell) {
        const double left = context.lower + cell * spacing;
        const double right = left + spacing;
        const double equilibrium = 0.5 * mass *
            (std::erf(alpha * (right - mean_velocity)) -
             std::erf(alpha * (left - mean_velocity)));
        derivative[cell] = equilibrium - state[cell];
    }
}

PetscErrorCode bgk_rhs(TS, PetscReal, Vec input, Vec output, void* raw_context) {
    const auto* context = static_cast<const BgkContext*>(raw_context);
    const PetscScalar* values{};
    PetscScalar* rates{};
    PetscInt size{};
    PetscCall(VecGetLocalSize(input, &size));
    PetscCall(VecGetArrayRead(input, &values));
    std::vector<double> state(values, values + size);
    std::vector<double> derivative;
    bgk_rhs_values(*context, state, derivative);
    PetscCall(VecGetArrayWrite(output, &rates));
    std::copy(derivative.begin(), derivative.end(), rates);
    PetscCall(VecRestoreArrayRead(input, &values));
    PetscCall(VecRestoreArrayWrite(output, &rates));
    return PETSC_SUCCESS;
}

PetscErrorCode bratu_residual(
    TS, PetscReal, Vec state, Vec derivative, Vec residual, void* raw_context) {
    const auto* context = static_cast<const BratuContext*>(raw_context);
    const PetscScalar* values{};
    const PetscScalar* rates{};
    PetscScalar* output{};
    PetscCall(VecGetArrayRead(state, &values));
    PetscCall(VecGetArrayRead(derivative, &rates));
    PetscCall(VecGetArrayWrite(residual, &output));
    const double spacing_x = 1.0 / (context->width - 1);
    const double spacing_y = 1.0 / (context->height - 1);
    for (PetscInt row = 0; row < context->height; ++row) {
        for (PetscInt column = 0; column < context->width; ++column) {
            const auto index = row * context->width + column;
            if (row == 0 || column == 0 || row + 1 == context->height ||
                column + 1 == context->width) {
                output[index] = values[index];
            } else {
                const auto laplacian_x =
                    (values[index - 1] - 2.0 * values[index] + values[index + 1]) /
                    (spacing_x * spacing_x);
                const auto laplacian_y =
                    (values[index - context->width] - 2.0 * values[index] +
                     values[index + context->width]) / (spacing_y * spacing_y);
                output[index] = rates[index] - laplacian_x - laplacian_y -
                    context->lambda * std::exp(values[index]);
            }
        }
    }
    PetscCall(VecRestoreArrayRead(state, &values));
    PetscCall(VecRestoreArrayRead(derivative, &rates));
    PetscCall(VecRestoreArrayWrite(residual, &output));
    return PETSC_SUCCESS;
}

PetscErrorCode bratu_jacobian(
    TS, PetscReal, Vec state, Vec, PetscReal shift,
    Mat matrix, Mat preconditioner, void* raw_context) {
    const auto* context = static_cast<const BratuContext*>(raw_context);
    const PetscScalar* values{};
    PetscCall(VecGetArrayRead(state, &values));
    const double inverse_x = (context->width - 1.0) * (context->width - 1.0);
    const double inverse_y = (context->height - 1.0) * (context->height - 1.0);
    PetscCall(MatZeroEntries(preconditioner));
    for (PetscInt row = 0; row < context->height; ++row) {
        for (PetscInt column = 0; column < context->width; ++column) {
            const PetscInt index = row * context->width + column;
            if (row == 0 || column == 0 || row + 1 == context->height ||
                column + 1 == context->width) {
                PetscCall(MatSetValue(preconditioner, index, index, 1.0, INSERT_VALUES));
            } else {
                const PetscInt columns[] = {
                    index - context->width, index - 1, index,
                    index + 1, index + context->width};
                const PetscScalar entries[] = {
                    -inverse_y, -inverse_x,
                    shift + 2.0 * inverse_x + 2.0 * inverse_y -
                        context->lambda * std::exp(values[index]),
                    -inverse_x, -inverse_y};
                PetscCall(MatSetValues(
                    preconditioner, 1, &index, 5, columns, entries, INSERT_VALUES));
            }
        }
    }
    PetscCall(MatAssemblyBegin(preconditioner, MAT_FINAL_ASSEMBLY));
    PetscCall(MatAssemblyEnd(preconditioner, MAT_FINAL_ASSEMBLY));
    if (matrix != preconditioner) {
        PetscCall(MatAssemblyBegin(matrix, MAT_FINAL_ASSEMBLY));
        PetscCall(MatAssemblyEnd(matrix, MAT_FINAL_ASSEMBLY));
    }
    PetscCall(VecRestoreArrayRead(state, &values));
    return PETSC_SUCCESS;
}

PetscErrorCode fem_heat_rhs(
    TS, PetscReal, Vec input, Vec output, void* raw_context) {
    auto* context = static_cast<FemHeatContext*>(raw_context);
    PetscCall(MatMult(context->stiffness, input, context->work));
    PetscCall(KSPSolve(context->mass_solver, context->work, output));
    return PETSC_SUCCESS;
}

std::vector<double> convection_diffusion_derivative(
    const std::vector<double>& input,
    const ConvectionDiffusionContext& context) {
    const auto width = static_cast<std::size_t>(context.width);
    const auto height = static_cast<std::size_t>(context.height);
    const auto size = width * height;
    std::vector<double> output(size);
    const double center = -2.0 * context.diffusion *
        (1.0 / (context.spacing_x * context.spacing_x) +
         1.0 / (context.spacing_y * context.spacing_y));
    const double left = 0.5 * context.advection / context.spacing_x +
        context.diffusion / (context.spacing_x * context.spacing_x);
    const double right = -0.5 * context.advection / context.spacing_x +
        context.diffusion / (context.spacing_x * context.spacing_x);
    const double lower = 0.5 * context.advection / context.spacing_y +
        context.diffusion / (context.spacing_y * context.spacing_y);
    const double upper = -0.5 * context.advection / context.spacing_y +
        context.diffusion / (context.spacing_y * context.spacing_y);
    output[0] = center * input[0] + right * input[1] + upper * input[width];
    output[width - 1] = 2.0 * left * input[width - 2] +
        center * input[width - 1] + upper * input[2 * width - 1];
    for (std::size_t column = 1; column + 1 < width; ++column) {
        output[column] = center * input[column] + left * input[column - 1] +
            right * input[column + 1] + upper * input[column + width];
    }
    for (std::size_t row = 1; row + 1 < height; ++row) {
        const auto offset = row * width;
        output[offset] = center * input[offset] + right * input[offset + 1] +
            lower * input[offset - width] + upper * input[offset + width];
        output[offset + width - 1] =
            center * input[offset + width - 1] +
            2.0 * left * input[offset + width - 2] +
            lower * input[offset - 1] + upper * input[offset + 2 * width - 1];
        for (std::size_t column = 1; column + 1 < width; ++column) {
            const auto index = offset + column;
            output[index] = center * input[index] + left * input[index - 1] +
                right * input[index + 1] + lower * input[index - width] +
                upper * input[index + width];
        }
    }
    const auto last_row = size - width;
    output[last_row] = center * input[last_row] + right * input[last_row + 1] +
        2.0 * lower * input[last_row - width];
    output[size - 1] = 2.0 * left * input[size - 2] + center * input[size - 1] +
        2.0 * lower * input[size - width - 1];
    for (std::size_t column = 1; column + 1 < width; ++column) {
        const auto index = last_row + column;
        output[index] = center * input[index] + left * input[index - 1] +
            right * input[index + 1] + 2.0 * lower * input[index - width];
    }
    return output;
}

PetscErrorCode convection_diffusion_rhs(
    TS, PetscReal, Vec input, Vec output, void* raw_context) {
    const auto* context = static_cast<const ConvectionDiffusionContext*>(raw_context);
    const PetscScalar* values{};
    PetscScalar* derivative{};
    PetscInt size{};
    PetscCall(VecGetLocalSize(input, &size));
    PetscCall(VecGetArrayRead(input, &values));
    const std::vector<double> state(values, values + size);
    PetscCall(VecRestoreArrayRead(input, &values));
    const auto result = convection_diffusion_derivative(state, *context);
    PetscCall(VecGetArrayWrite(output, &derivative));
    std::copy(result.begin(), result.end(), derivative);
    PetscCall(VecRestoreArrayWrite(output, &derivative));
    return PETSC_SUCCESS;
}

PetscErrorCode reaction_residual(
    TS, PetscReal, Vec state, Vec derivative, Vec residual, void*) {
    const PetscScalar* values{};
    const PetscScalar* rates{};
    PetscScalar* output{};
    PetscCall(VecGetArrayRead(state, &values));
    PetscCall(VecGetArrayRead(derivative, &rates));
    PetscCall(VecGetArrayWrite(residual, &output));
    output[0] = rates[0] + values[0];
    output[1] = rates[1] - values[0];
    PetscCall(VecRestoreArrayRead(state, &values));
    PetscCall(VecRestoreArrayRead(derivative, &rates));
    PetscCall(VecRestoreArrayWrite(residual, &output));
    return PETSC_SUCCESS;
}

PetscErrorCode reaction_jacobian(
    TS, PetscReal, Vec, Vec, PetscReal shift,
    Mat matrix, Mat preconditioner, void*) {
    const PetscInt rows[] = {0, 1};
    const PetscScalar values[] = {shift + 1.0, 0.0, -1.0, shift};
    PetscCall(MatSetValues(
        preconditioner, 2, rows, 2, rows, values, INSERT_VALUES));
    PetscCall(MatAssemblyBegin(preconditioner, MAT_FINAL_ASSEMBLY));
    PetscCall(MatAssemblyEnd(preconditioner, MAT_FINAL_ASSEMBLY));
    if (matrix != preconditioner) {
        PetscCall(MatAssemblyBegin(matrix, MAT_FINAL_ASSEMBLY));
        PetscCall(MatAssemblyEnd(matrix, MAT_FINAL_ASSEMBLY));
    }
    return PETSC_SUCCESS;
}

PetscErrorCode mass_ode_residual(
    TS, PetscReal, Vec, Vec derivative, Vec residual, void*) {
    const PetscScalar* rate{};
    PetscScalar* output{};
    PetscCall(VecGetArrayRead(derivative, &rate));
    PetscCall(VecGetArrayWrite(residual, &output));
    output[0] = 2.0 * rate[0] - 1.0;
    PetscCall(VecRestoreArrayRead(derivative, &rate));
    PetscCall(VecRestoreArrayWrite(residual, &output));
    return PETSC_SUCCESS;
}

PetscErrorCode mass_ode_jacobian(
    TS, PetscReal, Vec, Vec, PetscReal shift,
    Mat matrix, Mat preconditioner, void*) {
    PetscCall(MatSetValue(preconditioner, 0, 0, 2.0 * shift, INSERT_VALUES));
    PetscCall(MatAssemblyBegin(preconditioner, MAT_FINAL_ASSEMBLY));
    PetscCall(MatAssemblyEnd(preconditioner, MAT_FINAL_ASSEMBLY));
    if (matrix != preconditioner) {
        PetscCall(MatAssemblyBegin(matrix, MAT_FINAL_ASSEMBLY));
        PetscCall(MatAssemblyEnd(matrix, MAT_FINAL_ASSEMBLY));
    }
    return PETSC_SUCCESS;
}

PetscErrorCode mass_dae_residual(
    TS, PetscReal, Vec state, Vec derivative, Vec residual, void*) {
    const PetscScalar* values{};
    const PetscScalar* rates{};
    PetscScalar* output{};
    PetscCall(VecGetArrayRead(state, &values));
    PetscCall(VecGetArrayRead(derivative, &rates));
    PetscCall(VecGetArrayWrite(residual, &output));
    output[0] = values[0] * rates[0] - values[0] * values[1];
    output[1] = values[0] - values[1];
    PetscCall(VecRestoreArrayRead(state, &values));
    PetscCall(VecRestoreArrayRead(derivative, &rates));
    PetscCall(VecRestoreArrayWrite(residual, &output));
    return PETSC_SUCCESS;
}

PetscErrorCode mass_dae_jacobian(
    TS, PetscReal, Vec state, Vec derivative, PetscReal shift,
    Mat matrix, Mat preconditioner, void*) {
    const PetscScalar* values{};
    const PetscScalar* rates{};
    PetscCall(VecGetArrayRead(state, &values));
    PetscCall(VecGetArrayRead(derivative, &rates));
    const PetscInt rows[] = {0, 1};
    const PetscScalar entries[] = {
        rates[0] - values[1] + shift * values[0], -values[0],
        1.0, -1.0,
    };
    PetscCall(MatSetValues(
        preconditioner, 2, rows, 2, rows, entries, INSERT_VALUES));
    PetscCall(MatAssemblyBegin(preconditioner, MAT_FINAL_ASSEMBLY));
    PetscCall(MatAssemblyEnd(preconditioner, MAT_FINAL_ASSEMBLY));
    if (matrix != preconditioner) {
        PetscCall(MatAssemblyBegin(matrix, MAT_FINAL_ASSEMBLY));
        PetscCall(MatAssemblyEnd(matrix, MAT_FINAL_ASSEMBLY));
    }
    PetscCall(VecRestoreArrayRead(state, &values));
    PetscCall(VecRestoreArrayRead(derivative, &rates));
    return PETSC_SUCCESS;
}

struct DaeComparison {
    double petsc_state{};
    double smave_state{};
    double smave_algebraic{};
    double relative_error{};
    double constraint_residual{};
    std::size_t smave_iterations{};
    double petsc_seconds{};
    double smave_seconds{};
};

double relative_error(
    const std::vector<double>& left, const std::vector<double>& right);

struct MotionContext {
    double rate{};
};

struct DiffusionContext {
    int size{};
    double spacing{};
};

PetscErrorCode nonlinear_diffusion_rhs(
    TS, PetscReal time, Vec input, Vec output, void* context) {
    const auto* diffusion = static_cast<const DiffusionContext*>(context);
    const PetscScalar* state{};
    PetscScalar* derivative{};
    PetscCall(VecGetArrayRead(input, &state));
    PetscCall(VecGetArrayWrite(output, &derivative));
    derivative[0] = 1.0;
    derivative[diffusion->size - 1] = 2.0;
    const auto scale = 1.0 /
        (2.0 * diffusion->spacing * diffusion->spacing *
         (1.0 + time) * (1.0 + time));
    for (int index = 1; index + 1 < diffusion->size; ++index) {
        derivative[index] = state[index] * scale *
            (state[index + 1] + state[index - 1] - 2.0 * state[index]);
    }
    PetscCall(VecRestoreArrayRead(input, &state));
    PetscCall(VecRestoreArrayWrite(output, &derivative));
    return PETSC_SUCCESS;
}

PetscErrorCode nonlinear_diffusion_jacobian(
    TS, PetscReal time, Vec input, Mat matrix, Mat preconditioner,
    void* context) {
    const auto* diffusion = static_cast<const DiffusionContext*>(context);
    const PetscScalar* state{};
    PetscCall(VecGetArrayRead(input, &state));
    PetscCall(MatZeroEntries(preconditioner));
    const auto scale = 1.0 /
        (2.0 * diffusion->spacing * diffusion->spacing *
         (1.0 + time) * (1.0 + time));
    for (int index = 1; index + 1 < diffusion->size; ++index) {
        const PetscInt row = index;
        const PetscInt columns[] = {index - 1, index, index + 1};
        const PetscScalar values[] = {
            scale * state[index],
            scale * (state[index + 1] + state[index - 1] - 4.0 * state[index]),
            scale * state[index],
        };
        PetscCall(MatSetValues(
            preconditioner, 1, &row, 3, columns, values, INSERT_VALUES));
    }
    PetscCall(MatAssemblyBegin(preconditioner, MAT_FINAL_ASSEMBLY));
    PetscCall(MatAssemblyEnd(preconditioner, MAT_FINAL_ASSEMBLY));
    if (matrix != preconditioner) {
        PetscCall(MatAssemblyBegin(matrix, MAT_FINAL_ASSEMBLY));
        PetscCall(MatAssemblyEnd(matrix, MAT_FINAL_ASSEMBLY));
    }
    PetscCall(VecRestoreArrayRead(input, &state));
    return PETSC_SUCCESS;
}

PetscErrorCode velocity_residual(
    TS, PetscReal, Vec, Vec velocity, Vec residual, void* context) {
    const auto* motion = static_cast<const MotionContext*>(context);
    const PetscScalar* values{};
    PetscScalar* output{};
    PetscCall(VecGetArrayRead(velocity, &values));
    PetscCall(VecGetArrayWrite(residual, &output));
    output[0] = values[0] - motion->rate;
    PetscCall(VecRestoreArrayRead(velocity, &values));
    PetscCall(VecRestoreArrayWrite(residual, &output));
    return PETSC_SUCCESS;
}

PetscErrorCode velocity_jacobian(
    TS, PetscReal, Vec, Vec, PetscReal shift, Mat matrix, Mat preconditioner,
    void*) {
    PetscCall(MatSetValue(preconditioner, 0, 0, shift, INSERT_VALUES));
    PetscCall(MatAssemblyBegin(preconditioner, MAT_FINAL_ASSEMBLY));
    PetscCall(MatAssemblyEnd(preconditioner, MAT_FINAL_ASSEMBLY));
    if (matrix != preconditioner) {
        PetscCall(MatAssemblyBegin(matrix, MAT_FINAL_ASSEMBLY));
        PetscCall(MatAssemblyEnd(matrix, MAT_FINAL_ASSEMBLY));
    }
    return PETSC_SUCCESS;
}

PetscErrorCode acceleration_residual(
    TS, PetscReal, Vec, Vec, Vec acceleration, Vec residual, void* context) {
    const auto* motion = static_cast<const MotionContext*>(context);
    const PetscScalar* values{};
    PetscScalar* output{};
    PetscCall(VecGetArrayRead(acceleration, &values));
    PetscCall(VecGetArrayWrite(residual, &output));
    output[0] = values[0] - motion->rate;
    PetscCall(VecRestoreArrayRead(acceleration, &values));
    PetscCall(VecRestoreArrayWrite(residual, &output));
    return PETSC_SUCCESS;
}

PetscErrorCode acceleration_jacobian(
    TS, PetscReal, Vec, Vec, Vec, PetscReal, PetscReal shift,
    Mat matrix, Mat preconditioner, void*) {
    PetscCall(MatSetValue(preconditioner, 0, 0, shift, INSERT_VALUES));
    PetscCall(MatAssemblyBegin(preconditioner, MAT_FINAL_ASSEMBLY));
    PetscCall(MatAssemblyEnd(preconditioner, MAT_FINAL_ASSEMBLY));
    if (matrix != preconditioner) {
        PetscCall(MatAssemblyBegin(matrix, MAT_FINAL_ASSEMBLY));
        PetscCall(MatAssemblyEnd(matrix, MAT_FINAL_ASSEMBLY));
    }
    return PETSC_SUCCESS;
}

struct MotionComparison {
    std::vector<double> petsc;
    std::vector<double> smave;
    double relative_error{};
    std::size_t smave_iterations{};
    double petsc_seconds{};
    double smave_seconds{};
};

struct Ex5Comparison {
    double relative_error{};
    std::size_t smave_iterations{};
    double petsc_seconds{};
    double smave_seconds{};
    double petsc_norm{};
    double smave_norm{};
};

double ex5_control_value(const std::vector<std::string>& lines,
                         const std::string& label) {
    static const std::regex number_at_end(
        R"(([+-]?[0-9]+(?:\.[0-9]+)?)\s*$)");
    for (const auto& line : lines) {
        if (line.find(label) == std::string::npos) continue;
        std::smatch match;
        if (std::regex_search(line, match, number_at_end))
            return std::stod(match[1].str());
    }
    throw std::runtime_error("missing ex5 control value: " + label);
}

Ex5Input read_ex5_control(const std::string& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot open ex5 control file: " + path);
    std::vector<std::string> lines;
    for (std::string line; std::getline(input, line);) lines.push_back(line);
    return {
        ex5_control_value(lines, "Surface Temperature"),
        ex5_control_value(lines, "Dew Point Temperature"),
        ex5_control_value(lines, "Air Temperature"),
        ex5_control_value(lines, "Temperature at Cloud base"),
        ex5_control_value(lines, "Fraction of sky covered"),
        ex5_control_value(lines, "Wind speed"),
        ex5_control_value(lines, "Precipitable Water"),
        ex5_control_value(lines, "RUNTIME"),
        ex5_control_value(lines, "Initiation specifier")};
}

Ex5Context make_ex5_context(DM da, const Ex5Input& input) {
    Ex5Context context{};
    context.da = da;
    context.Ts = fahr_to_cel(input.surface_temperature) + 273.0;
    context.fract = input.cloud_fraction;
    context.dewtemp = fahr_to_cel(input.dew_point_temperature) + 273.0;
    context.csoil = 2000000.0;
    context.dzlay = 0.08;
    context.emma = emission(3.0);
    context.wind = input.wind_speed;
    context.pressure1 = 101300.0;
    context.airtemp = fahr_to_cel(input.air_temperature) + 273.0;
    context.Tc = fahr_to_cel(input.cloud_temperature) + 273.0;
    context.init = input.initialization;
    context.lat = 70.0 * 0.0174532;
    context.deep_grnd_temp = fahr_to_cel(input.surface_temperature - 10.0) + 273.0;
    return context;
}

std::vector<double> ex5_initial_state(Ex5Context& context, Vec state) {
    PetscCallAbort(PETSC_COMM_SELF, FormInitialSolution(
        context.da, state, &context));
    const PetscScalar* values{};
    PetscInt size{};
    PetscCallAbort(PETSC_COMM_SELF, VecGetLocalSize(state, &size));
    PetscCallAbort(PETSC_COMM_SELF, VecGetArrayRead(state, &values));
    std::vector<double> result(values, values + size);
    PetscCallAbort(PETSC_COMM_SELF, VecRestoreArrayRead(state, &values));
    return result;
}

void ex5_rhs(Ex5Context& context, Vec input, Vec output,
             const std::vector<double>& values,
             std::vector<double>& result) {
    PetscCallAbort(PETSC_COMM_SELF, VecPlaceArray(input, values.data()));
    PetscCallAbort(PETSC_COMM_SELF, VecPlaceArray(output, result.data()));
    PetscCallAbort(PETSC_COMM_SELF, RhsFunc(nullptr, 0.0, input, output, &context));
    PetscCallAbort(PETSC_COMM_SELF, VecResetArray(output));
    PetscCallAbort(PETSC_COMM_SELF, VecResetArray(input));
}

Ex5Comparison compare_ex5(const std::string& control_path) {
    constexpr PetscInt width = 20;
    constexpr PetscInt height = 20;
    constexpr PetscInt dof = 5;
    constexpr int steps = 130;
    constexpr double delta_time = 1.0;
    constexpr double final_time = 43200.0;
    DM da{};
    PetscCallAbort(PETSC_COMM_SELF, DMDACreate2d(
        PETSC_COMM_SELF, DM_BOUNDARY_PERIODIC, DM_BOUNDARY_PERIODIC,
        DMDA_STENCIL_STAR, width, height, PETSC_DECIDE, PETSC_DECIDE,
        dof, 1, nullptr, nullptr, &da));
    PetscCallAbort(PETSC_COMM_SELF, DMSetUp(da));
    const auto control = read_ex5_control(control_path);
    Ex5Context context = make_ex5_context(da, control);
    Vec state{};
    PetscCallAbort(PETSC_COMM_SELF, DMCreateGlobalVector(da, &state));
    const auto initial = ex5_initial_state(context, state);

    Vec rhs_vector{};
    Mat jacobian_matrix{};
    TS solver{};
    PetscCallAbort(PETSC_COMM_SELF, VecDuplicate(state, &rhs_vector));
    PetscCallAbort(PETSC_COMM_SELF, DMSetMatType(da, MATAIJ));
    PetscCallAbort(PETSC_COMM_SELF, DMCreateMatrix(da, &jacobian_matrix));
    ISColoring coloring{};
    MatFDColoring matrix_coloring{};
    PetscCallAbort(PETSC_COMM_SELF, DMCreateColoring(
        da, IS_COLORING_GLOBAL, &coloring));
    PetscCallAbort(PETSC_COMM_SELF, MatFDColoringCreate(
        jacobian_matrix, coloring, &matrix_coloring));
    PetscCallAbort(PETSC_COMM_SELF, MatFDColoringSetUp(
        jacobian_matrix, coloring, matrix_coloring));
    PetscCallAbort(PETSC_COMM_SELF, ISColoringDestroy(&coloring));
    PetscCallAbort(PETSC_COMM_SELF, TSCreate(PETSC_COMM_SELF, &solver));
    PetscCallAbort(PETSC_COMM_SELF, TSSetProblemType(solver, TS_NONLINEAR));
    PetscCallAbort(PETSC_COMM_SELF, TSSetType(solver, TSBEULER));
    PetscCallAbort(PETSC_COMM_SELF, TSSetRHSFunction(
        solver, rhs_vector, RhsFunc, &context));
    PetscCallAbort(PETSC_COMM_SELF, TSSetIJacobian(
        solver, jacobian_matrix, jacobian_matrix,
        TSComputeIJacobianDefaultColor, matrix_coloring));
    PetscCallAbort(PETSC_COMM_SELF, TSSetTimeStep(solver, delta_time));
    PetscCallAbort(PETSC_COMM_SELF, TSSetMaxSteps(solver, steps));
    PetscCallAbort(PETSC_COMM_SELF, TSSetMaxTime(solver, final_time));
    PetscCallAbort(PETSC_COMM_SELF, TSSetExactFinalTime(
        solver, TS_EXACTFINALTIME_STEPOVER));
    PetscCallAbort(PETSC_COMM_SELF, TSSetSolution(solver, state));
    PetscCallAbort(PETSC_COMM_SELF, TSSetDM(solver, da));
    const auto petsc_started = std::chrono::steady_clock::now();
    PetscCallAbort(PETSC_COMM_SELF, TSSolve(solver, state));
    const auto petsc_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - petsc_started).count();
    const PetscScalar* petsc_values{};
    PetscInt size{};
    PetscCallAbort(PETSC_COMM_SELF, VecGetLocalSize(state, &size));
    PetscCallAbort(PETSC_COMM_SELF, VecGetArrayRead(state, &petsc_values));
    const std::vector<double> petsc_state(petsc_values, petsc_values + size);
    PetscCallAbort(PETSC_COMM_SELF, VecRestoreArrayRead(state, &petsc_values));

    Vec input{};
    Vec output{};
    PetscCallAbort(PETSC_COMM_SELF, VecCreateSeq(PETSC_COMM_SELF, size, &input));
    PetscCallAbort(PETSC_COMM_SELF, VecDuplicate(input, &output));
    std::vector<double> smave_state = initial;
    std::vector<double> rhs_value(size);
    std::vector<double> perturbed(size);
    std::size_t total_iterations{};
    const auto smave_started = std::chrono::steady_clock::now();
    for (int step = 0; step < steps; ++step) {
        const auto previous = smave_state;
        std::vector<double> candidate = previous;
        std::vector<double> candidate_rhs(size);
        std::vector<double> residual(size);
        double residual_norm = 0.0;
        for (int newton = 0; newton < 12; ++newton) {
            ex5_rhs(context, input, output, candidate, candidate_rhs);
            residual_norm = 0.0;
            for (PetscInt index = 0; index < size; ++index) {
                residual[index] = candidate[index] - previous[index] -
                    delta_time * candidate_rhs[index];
                residual_norm = std::max(residual_norm, std::abs(residual[index]));
            }
            if (residual_norm <= 1.0e-9) break;
            const smave::LinearOperator jacobian =
                [&](const std::vector<double>& direction,
                    std::vector<double>& result) {
                    double direction_norm = 0.0;
                    double candidate_norm = 0.0;
                    for (PetscInt index = 0; index < size; ++index) {
                        direction_norm = std::max(direction_norm, std::abs(direction[index]));
                        candidate_norm = std::max(candidate_norm, std::abs(candidate[index]));
                    }
                    if (direction_norm == 0.0) {
                        result.assign(size, 0.0);
                        return true;
                    }
                    const double epsilon = 1.0e-7 *
                        (1.0 + candidate_norm) / direction_norm;
                    for (PetscInt index = 0; index < size; ++index)
                        perturbed[index] = candidate[index] + epsilon * direction[index];
                    ex5_rhs(context, input, output, perturbed, rhs_value);
                    result.resize(size);
                    for (PetscInt index = 0; index < size; ++index)
                        result[index] = direction[index] - delta_time *
                            (rhs_value[index] - candidate_rhs[index]) / epsilon;
                    return true;
                };
            std::vector<double> negative_residual(size);
            for (PetscInt index = 0; index < size; ++index)
                negative_residual[index] = -residual[index];
            const auto correction = smave::restarted_gmres(
                size, jacobian, negative_residual, std::vector<double>(size, 0.0),
                [](const std::vector<double>& input, std::vector<double>& result) {
                    result = input;
                    return true;
                }, 1.0e-10, 1.0e-8, 80, 30);
            if (!correction.converged)
                throw std::runtime_error("SMAVE ex5 Newton-Krylov failed: " + correction.reason);
            total_iterations += static_cast<std::size_t>(correction.iterations);
            double scale = 1.0;
            bool accepted = false;
            while (scale >= 1.0 / 128.0) {
                std::vector<double> trial = candidate;
                for (PetscInt index = 0; index < size; ++index)
                    trial[index] += scale * correction.solution[index];
                std::vector<double> trial_rhs(size);
                ex5_rhs(context, input, output, trial, trial_rhs);
                double trial_norm = 0.0;
                for (PetscInt index = 0; index < size; ++index)
                    trial_norm = std::max(trial_norm, std::abs(
                        trial[index] - previous[index] - delta_time * trial_rhs[index]));
                if (trial_norm < residual_norm) {
                    candidate = std::move(trial);
                    accepted = true;
                    break;
                }
                scale *= 0.5;
            }
            if (!accepted)
                throw std::runtime_error("SMAVE ex5 line search failed");
        }
        if (residual_norm > 1.0e-7)
            throw std::runtime_error("SMAVE ex5 Newton residual exceeded tolerance");
        smave_state = std::move(candidate);
    }
    const auto smave_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - smave_started).count();
    const auto petsc_norm = *std::max_element(petsc_state.begin(), petsc_state.end(),
        [](double left, double right) { return std::abs(left) < std::abs(right); });
    const auto smave_norm = *std::max_element(smave_state.begin(), smave_state.end(),
        [](double left, double right) { return std::abs(left) < std::abs(right); });
    const auto error = relative_error(smave_state, petsc_state);
    PetscCallAbort(PETSC_COMM_SELF, VecDestroy(&output));
    PetscCallAbort(PETSC_COMM_SELF, VecDestroy(&input));
    PetscCallAbort(PETSC_COMM_SELF, MatFDColoringDestroy(&matrix_coloring));
    PetscCallAbort(PETSC_COMM_SELF, TSDestroy(&solver));
    PetscCallAbort(PETSC_COMM_SELF, MatDestroy(&jacobian_matrix));
    PetscCallAbort(PETSC_COMM_SELF, VecDestroy(&rhs_vector));
    PetscCallAbort(PETSC_COMM_SELF, VecDestroy(&state));
    PetscCallAbort(PETSC_COMM_SELF, DMDestroy(&da));
    return {error, total_iterations, petsc_seconds, smave_seconds,
            petsc_norm, smave_norm};
}

MotionComparison compare_bgk() {
    constexpr double delta_time = 0.01;
    constexpr int steps = 1000;
    BgkContext context;
    const double spacing = (context.upper - context.lower) / context.cells;
    std::vector<double> initial(context.cells);
    for (int cell = 0; cell < context.cells; ++cell) {
        const double velocity = context.lower + (cell + 0.5) * spacing;
        initial[cell] = velocity >= -1.0 && velocity <= 1.0 ? 1.0 : 0.0;
    }
    Vec state{};
    TS solver{};
    PetscCallAbort(PETSC_COMM_SELF, VecCreateSeq(
        PETSC_COMM_SELF, context.cells, &state));
    PetscScalar* values{};
    PetscCallAbort(PETSC_COMM_SELF, VecGetArrayWrite(state, &values));
    std::copy(initial.begin(), initial.end(), values);
    PetscCallAbort(PETSC_COMM_SELF, VecRestoreArrayWrite(state, &values));
    PetscCallAbort(PETSC_COMM_SELF, TSCreate(PETSC_COMM_SELF, &solver));
    PetscCallAbort(PETSC_COMM_SELF, TSSetType(solver, TSEULER));
    PetscCallAbort(PETSC_COMM_SELF, TSSetRHSFunction(
        solver, nullptr, bgk_rhs, &context));
    PetscCallAbort(PETSC_COMM_SELF, TSSetTimeStep(solver, delta_time));
    PetscCallAbort(PETSC_COMM_SELF, TSSetMaxSteps(solver, steps));
    PetscCallAbort(PETSC_COMM_SELF, TSSetMaxTime(solver, steps * delta_time));
    PetscCallAbort(PETSC_COMM_SELF, TSSetExactFinalTime(
        solver, TS_EXACTFINALTIME_MATCHSTEP));
    PetscCallAbort(PETSC_COMM_SELF, TSSetSolution(solver, state));
    const auto petsc_started = std::chrono::steady_clock::now();
    PetscCallAbort(PETSC_COMM_SELF, TSSolve(solver, state));
    const auto petsc_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - petsc_started).count();
    const PetscScalar* petsc_values{};
    PetscCallAbort(PETSC_COMM_SELF, VecGetArrayRead(state, &petsc_values));
    const std::vector<double> petsc_state(
        petsc_values, petsc_values + context.cells);
    PetscCallAbort(PETSC_COMM_SELF, VecRestoreArrayRead(state, &petsc_values));

    std::vector<double> smave_state = initial;
    std::vector<double> derivative;
    const auto smave_started = std::chrono::steady_clock::now();
    for (int step = 0; step < steps; ++step) {
        bgk_rhs_values(context, smave_state, derivative);
        for (int cell = 0; cell < context.cells; ++cell)
            smave_state[cell] += delta_time * derivative[cell];
    }
    const auto smave_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - smave_started).count();
    MotionComparison comparison;
    comparison.petsc = petsc_state;
    comparison.smave = smave_state;
    comparison.relative_error = relative_error(smave_state, petsc_state);
    comparison.smave_iterations = steps;
    comparison.petsc_seconds = petsc_seconds;
    comparison.smave_seconds = smave_seconds;
    PetscCallAbort(PETSC_COMM_SELF, TSDestroy(&solver));
    PetscCallAbort(PETSC_COMM_SELF, VecDestroy(&state));
    return comparison;
}

MotionComparison compare_particle_landau() {
    constexpr double delta_time = 0.1;
    constexpr double theta = 0.5;
    ParticleLandauContext context;
    const std::vector<double> initial = {
        -0.7, -0.2,
         0.3, -0.6,
         0.5,  0.8};
    Vec state{};
    TS solver{};
    PetscCallAbort(PETSC_COMM_SELF, VecCreateSeq(PETSC_COMM_SELF, 6, &state));
    PetscScalar* values{};
    PetscCallAbort(PETSC_COMM_SELF, VecGetArrayWrite(state, &values));
    std::copy(initial.begin(), initial.end(), values);
    PetscCallAbort(PETSC_COMM_SELF, VecRestoreArrayWrite(state, &values));
    PetscCallAbort(PETSC_COMM_SELF, TSCreate(PETSC_COMM_SELF, &solver));
    PetscCallAbort(PETSC_COMM_SELF, TSSetType(solver, TSTHETA));
    PetscCallAbort(PETSC_COMM_SELF, TSThetaSetTheta(solver, theta));
    PetscCallAbort(PETSC_COMM_SELF, TSSetRHSFunction(
        solver, nullptr, particle_landau_rhs, &context));
    PetscCallAbort(PETSC_COMM_SELF, TSSetTimeStep(solver, delta_time));
    PetscCallAbort(PETSC_COMM_SELF, TSSetMaxSteps(solver, 1));
    PetscCallAbort(PETSC_COMM_SELF, TSSetMaxTime(solver, delta_time));
    PetscCallAbort(PETSC_COMM_SELF, TSSetExactFinalTime(
        solver, TS_EXACTFINALTIME_MATCHSTEP));
    PetscCallAbort(PETSC_COMM_SELF, TSSetSolution(solver, state));
    SNES nonlinear_solver{};
    PetscCallAbort(PETSC_COMM_SELF, TSGetSNES(solver, &nonlinear_solver));
    PetscCallAbort(PETSC_COMM_SELF, SNESSetTolerances(
        nonlinear_solver, 1.0e-13, 1.0e-13, PETSC_CURRENT,
        PETSC_CURRENT, PETSC_CURRENT));
    const auto petsc_started = std::chrono::steady_clock::now();
    PetscCallAbort(PETSC_COMM_SELF, TSSolve(solver, state));
    const auto petsc_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - petsc_started).count();
    const PetscScalar* petsc_values{};
    PetscCallAbort(PETSC_COMM_SELF, VecGetArrayRead(state, &petsc_values));
    const std::vector<double> petsc_state(petsc_values, petsc_values + 6);
    PetscCallAbort(PETSC_COMM_SELF, VecRestoreArrayRead(state, &petsc_values));

    std::vector<double> previous_rhs;
    particle_landau_rhs_values(context, initial, previous_rhs);
    std::vector<double> candidate = initial;
    std::size_t total_iterations{};
    const auto smave_started = std::chrono::steady_clock::now();
    double residual_norm = 0.0;
    for (int newton = 0; newton < 12; ++newton) {
        std::vector<double> candidate_rhs;
        particle_landau_rhs_values(context, candidate, candidate_rhs);
        std::vector<double> residual(6);
        residual_norm = 0.0;
        for (int index = 0; index < 6; ++index) {
            residual[index] = candidate[index] - initial[index] - delta_time *
                ((1.0 - theta) * previous_rhs[index] + theta * candidate_rhs[index]);
            residual_norm = std::max(residual_norm, std::abs(residual[index]));
        }
        if (residual_norm <= 1.0e-12) break;
        const smave::LinearOperator jacobian =
            [&](const std::vector<double>& direction,
                std::vector<double>& result) {
                double direction_norm = 0.0;
                double candidate_norm = 0.0;
                for (int index = 0; index < 6; ++index) {
                    direction_norm = std::max(direction_norm, std::abs(direction[index]));
                    candidate_norm = std::max(candidate_norm, std::abs(candidate[index]));
                }
                if (direction_norm == 0.0) {
                    result.assign(6, 0.0);
                    return true;
                }
                const double epsilon = 1.0e-7 *
                    (1.0 + candidate_norm) / direction_norm;
                std::vector<double> perturbed = candidate;
                for (int index = 0; index < 6; ++index)
                    perturbed[index] += epsilon * direction[index];
                std::vector<double> perturbed_rhs;
                particle_landau_rhs_values(context, perturbed, perturbed_rhs);
                result.resize(6);
                for (int index = 0; index < 6; ++index)
                    result[index] = direction[index] - delta_time * theta *
                        (perturbed_rhs[index] - candidate_rhs[index]) / epsilon;
                return true;
            };
        std::vector<double> right_hand_side(6);
        for (int index = 0; index < 6; ++index)
            right_hand_side[index] = -residual[index];
        const auto correction = smave::restarted_gmres(
            6, jacobian, right_hand_side, std::vector<double>(6),
            [](const std::vector<double>& input, std::vector<double>& output) {
                output = input;
                return true;
            }, 1.0e-13, 1.0e-11, 40, 6);
        if (!correction.converged)
            throw std::runtime_error("SMAVE ex27 Newton-Krylov failed: " + correction.reason);
        total_iterations += static_cast<std::size_t>(correction.iterations);
        for (int index = 0; index < 6; ++index)
            candidate[index] += correction.solution[index];
    }
    if (residual_norm > 1.0e-10)
        throw std::runtime_error("SMAVE ex27 residual exceeded tolerance");
    const auto smave_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - smave_started).count();
    MotionComparison comparison;
    comparison.petsc = petsc_state;
    comparison.smave = candidate;
    comparison.relative_error = relative_error(candidate, petsc_state);
    comparison.smave_iterations = total_iterations;
    comparison.petsc_seconds = petsc_seconds;
    comparison.smave_seconds = smave_seconds;
    PetscCallAbort(PETSC_COMM_SELF, TSDestroy(&solver));
    PetscCallAbort(PETSC_COMM_SELF, VecDestroy(&state));
    return comparison;
}

smave::KrylovResult solve_smave_linear(
    std::size_t size, const smave::LinearOperator& operation,
    const std::vector<double>& right_hand_side,
    const std::vector<double>& initial,
    const std::vector<double>& diagonal) {
    const smave::Preconditioner preconditioner = [diagonal](
        const std::vector<double>& residual, std::vector<double>& output) {
        if (residual.size() != diagonal.size()) return false;
        output.resize(residual.size());
        for (std::size_t index = 0; index < residual.size(); ++index) {
            output[index] = residual[index] / diagonal[index];
        }
        return true;
    };
    auto result = smave::restarted_gmres(
        size, operation, right_hand_side, initial, preconditioner,
        1.0e-14, 1.0e-12,
        std::max(200, static_cast<int>(size) * 4),
        std::min(80, static_cast<int>(size)));
    if (!result.converged) {
        throw std::runtime_error("SMAVE linear backend failed: " + result.reason);
    }
    return result;
}

MotionComparison compare_constant_velocity() {
    constexpr double final_time = 3.0;
    constexpr int steps = 8;
    constexpr double delta_time = final_time / steps;
    MotionContext context{.rate = 1.0};
    Vec residual{};
    Vec state{};
    Mat matrix{};
    TS solver{};
    PetscCallAbort(PETSC_COMM_SELF, TSCreate(PETSC_COMM_SELF, &solver));
    PetscCallAbort(PETSC_COMM_SELF, TSSetType(solver, TSALPHA));
    PetscCallAbort(PETSC_COMM_SELF, TSSetMaxTime(solver, final_time));
    PetscCallAbort(PETSC_COMM_SELF, TSSetExactFinalTime(
        solver, TS_EXACTFINALTIME_MATCHSTEP));
    PetscCallAbort(PETSC_COMM_SELF, TSSetTimeStep(solver, delta_time));
    PetscCallAbort(PETSC_COMM_SELF, TSAlphaSetRadius(solver, 0.0));
    PetscCallAbort(PETSC_COMM_SELF, VecCreateSeq(PETSC_COMM_SELF, 1, &residual));
    PetscCallAbort(PETSC_COMM_SELF, VecDuplicate(residual, &state));
    PetscCallAbort(PETSC_COMM_SELF, VecSet(state, 1.0));
    PetscCallAbort(PETSC_COMM_SELF, MatCreateSeqDense(
        PETSC_COMM_SELF, 1, 1, nullptr, &matrix));
    PetscCallAbort(PETSC_COMM_SELF, TSSetIFunction(
        solver, residual, velocity_residual, &context));
    PetscCallAbort(PETSC_COMM_SELF, TSSetIJacobian(
        solver, matrix, matrix, velocity_jacobian, &context));
    PetscCallAbort(PETSC_COMM_SELF, TSSetSolution(solver, state));
    const auto petsc_started = std::chrono::steady_clock::now();
    PetscCallAbort(PETSC_COMM_SELF, TSSolve(solver, nullptr));
    const double petsc_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - petsc_started).count();
    const PetscScalar* values{};
    PetscCallAbort(PETSC_COMM_SELF, VecGetArrayRead(state, &values));
    const std::vector<double> petsc{values[0]};
    PetscCallAbort(PETSC_COMM_SELF, VecRestoreArrayRead(state, &values));
    double smave_state = 1.0;
    std::size_t smave_iterations{};
    const auto smave_started = std::chrono::steady_clock::now();
    for (int step = 0; step < steps; ++step) smave_state += delta_time * context.rate;
    for (int step = 0; step < steps; ++step) {
        const smave::LinearOperator identity = [](
            const std::vector<double>& input, std::vector<double>& output) {
            output = input;
            return true;
        };
        const auto velocity = solve_smave_linear(
            1, identity, {context.rate}, {0.0}, {1.0});
        smave_iterations += static_cast<std::size_t>(velocity.iterations);
        if (step == 0) smave_state = 1.0;
        smave_state += delta_time * velocity.solution[0];
    }
    const double smave_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - smave_started).count();
    const std::vector<double> smave{smave_state};
    PetscCallAbort(PETSC_COMM_SELF, TSDestroy(&solver));
    PetscCallAbort(PETSC_COMM_SELF, MatDestroy(&matrix));
    PetscCallAbort(PETSC_COMM_SELF, VecDestroy(&state));
    PetscCallAbort(PETSC_COMM_SELF, VecDestroy(&residual));
    return {petsc, smave, relative_error(smave, petsc), smave_iterations,
            petsc_seconds, smave_seconds};
}

MotionComparison compare_constant_acceleration() {
    constexpr double final_time = 3.0;
    constexpr int steps = 8;
    constexpr double delta_time = final_time / steps;
    MotionContext context{.rate = 1.0};
    Vec residual{};
    Vec displacement{};
    Vec velocity{};
    Mat matrix{};
    TS solver{};
    PetscCallAbort(PETSC_COMM_SELF, TSCreate(PETSC_COMM_SELF, &solver));
    PetscCallAbort(PETSC_COMM_SELF, TSSetType(solver, TSALPHA2));
    PetscCallAbort(PETSC_COMM_SELF, TSSetMaxTime(solver, final_time));
    PetscCallAbort(PETSC_COMM_SELF, TSSetExactFinalTime(
        solver, TS_EXACTFINALTIME_MATCHSTEP));
    PetscCallAbort(PETSC_COMM_SELF, TSSetTimeStep(solver, delta_time));
    PetscCallAbort(PETSC_COMM_SELF, TSAlpha2SetRadius(solver, 0.0));
    PetscCallAbort(PETSC_COMM_SELF, VecCreateSeq(PETSC_COMM_SELF, 1, &residual));
    PetscCallAbort(PETSC_COMM_SELF, VecDuplicate(residual, &displacement));
    PetscCallAbort(PETSC_COMM_SELF, VecDuplicate(residual, &velocity));
    PetscCallAbort(PETSC_COMM_SELF, VecSet(displacement, 1.0));
    PetscCallAbort(PETSC_COMM_SELF, VecSet(velocity, 0.0));
    PetscCallAbort(PETSC_COMM_SELF, MatCreateSeqDense(
        PETSC_COMM_SELF, 1, 1, nullptr, &matrix));
    PetscCallAbort(PETSC_COMM_SELF, TSSetI2Function(
        solver, residual, acceleration_residual, &context));
    PetscCallAbort(PETSC_COMM_SELF, TSSetI2Jacobian(
        solver, matrix, matrix, acceleration_jacobian, &context));
    PetscCallAbort(PETSC_COMM_SELF, TS2SetSolution(
        solver, displacement, velocity));
    const auto petsc_started = std::chrono::steady_clock::now();
    PetscCallAbort(PETSC_COMM_SELF, TSSolve(solver, nullptr));
    const double petsc_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - petsc_started).count();
    const PetscScalar* displacement_values{};
    const PetscScalar* velocity_values{};
    PetscCallAbort(PETSC_COMM_SELF, VecGetArrayRead(
        displacement, &displacement_values));
    PetscCallAbort(PETSC_COMM_SELF, VecGetArrayRead(velocity, &velocity_values));
    const std::vector<double> petsc{
        displacement_values[0], velocity_values[0]};
    PetscCallAbort(PETSC_COMM_SELF, VecRestoreArrayRead(
        displacement, &displacement_values));
    PetscCallAbort(PETSC_COMM_SELF, VecRestoreArrayRead(
        velocity, &velocity_values));
    double smave_displacement = 1.0;
    double smave_velocity = 0.0;
    std::size_t smave_iterations{};
    const auto smave_started = std::chrono::steady_clock::now();
    for (int step = 0; step < steps; ++step) {
        const smave::LinearOperator identity = [](
            const std::vector<double>& input, std::vector<double>& output) {
            output = input;
            return true;
        };
        const auto acceleration = solve_smave_linear(
            1, identity, {context.rate}, {0.0}, {1.0});
        smave_iterations += static_cast<std::size_t>(acceleration.iterations);
        smave_displacement += delta_time * smave_velocity +
            0.5 * acceleration.solution[0] * delta_time * delta_time;
        smave_velocity += acceleration.solution[0] * delta_time;
    }
    const double smave_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - smave_started).count();
    const std::vector<double> smave{smave_displacement, smave_velocity};
    PetscCallAbort(PETSC_COMM_SELF, TSDestroy(&solver));
    PetscCallAbort(PETSC_COMM_SELF, MatDestroy(&matrix));
    PetscCallAbort(PETSC_COMM_SELF, VecDestroy(&velocity));
    PetscCallAbort(PETSC_COMM_SELF, VecDestroy(&displacement));
    PetscCallAbort(PETSC_COMM_SELF, VecDestroy(&residual));
    return {petsc, smave, relative_error(smave, petsc), smave_iterations,
            petsc_seconds, smave_seconds};
}

std::vector<double> polynomial_chain_derivative(
    double time, const std::vector<double>& state) {
    std::vector<double> derivative(state.size());
    if (!state.empty()) derivative[0] = 1.0;
    for (std::size_t index = 1; index < state.size(); ++index) {
        derivative[index] = static_cast<double>(index + 1) *
            (state[index - 1] + std::pow(time, static_cast<int>(index))) / 2.0;
    }
    return derivative;
}

MotionComparison compare_rk4_polynomial_chain() {
    constexpr std::size_t size = 9;
    constexpr double final_time = 1.0;
    constexpr int steps = 8;
    constexpr double delta_time = final_time / steps;
    Vec state{};
    TS solver{};
    PetscCallAbort(PETSC_COMM_SELF, VecCreateSeq(
        PETSC_COMM_SELF, static_cast<PetscInt>(size), &state));
    PetscCallAbort(PETSC_COMM_SELF, VecSet(state, 0.0));
    PetscCallAbort(PETSC_COMM_SELF, TSCreate(PETSC_COMM_SELF, &solver));
    PetscCallAbort(PETSC_COMM_SELF, TSSetType(solver, TSRK));
    PetscCallAbort(PETSC_COMM_SELF, TSRKSetType(solver, TSRK4));
    PetscCallAbort(PETSC_COMM_SELF, TSSetRHSFunction(
        solver, nullptr, polynomial_chain_rhs, nullptr));
    PetscCallAbort(PETSC_COMM_SELF, TSSetTimeStep(solver, delta_time));
    PetscCallAbort(PETSC_COMM_SELF, TSSetMaxSteps(solver, steps));
    PetscCallAbort(PETSC_COMM_SELF, TSSetMaxTime(solver, final_time));
    PetscCallAbort(PETSC_COMM_SELF, TSSetExactFinalTime(
        solver, TS_EXACTFINALTIME_MATCHSTEP));
    PetscCallAbort(PETSC_COMM_SELF, TSSetSolution(solver, state));
    const auto petsc_started = std::chrono::steady_clock::now();
    PetscCallAbort(PETSC_COMM_SELF, TSSolve(solver, state));
    const double petsc_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - petsc_started).count();
    const PetscScalar* values{};
    PetscCallAbort(PETSC_COMM_SELF, VecGetArrayRead(state, &values));
    const std::vector<double> petsc(values, values + size);
    PetscCallAbort(PETSC_COMM_SELF, VecRestoreArrayRead(state, &values));

    std::vector<double> smave(size);
    double time{};
    const auto smave_started = std::chrono::steady_clock::now();
    for (int step = 0; step < steps; ++step) {
        const auto first = polynomial_chain_derivative(time, smave);
        auto stage = smave;
        for (std::size_t index = 0; index < size; ++index) {
            stage[index] += 0.5 * delta_time * first[index];
        }
        const auto second = polynomial_chain_derivative(time + 0.5 * delta_time, stage);
        stage = smave;
        for (std::size_t index = 0; index < size; ++index) {
            stage[index] += 0.5 * delta_time * second[index];
        }
        const auto third = polynomial_chain_derivative(time + 0.5 * delta_time, stage);
        stage = smave;
        for (std::size_t index = 0; index < size; ++index) {
            stage[index] += delta_time * third[index];
        }
        const auto fourth = polynomial_chain_derivative(time + delta_time, stage);
        for (std::size_t index = 0; index < size; ++index) {
            smave[index] += delta_time *
                (first[index] + 2.0 * second[index] + 2.0 * third[index] +
                 fourth[index]) / 6.0;
        }
        time += delta_time;
    }
    const double smave_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - smave_started).count();
    PetscCallAbort(PETSC_COMM_SELF, TSDestroy(&solver));
    PetscCallAbort(PETSC_COMM_SELF, VecDestroy(&state));
    return {petsc, smave, relative_error(smave, petsc), 0,
            petsc_seconds, smave_seconds};
}

MotionComparison compare_euler_convection_diffusion() {
    ConvectionDiffusionContext context;
    constexpr int steps = 10;
    constexpr double delta_time = 0.1;
    const auto size = static_cast<std::size_t>(context.width * context.height);
    std::vector<double> initial(size);
    for (PetscInt row = 0; row < context.height; ++row) {
        for (PetscInt column = 0; column < context.width; ++column) {
            const double x = context.spacing_x * (column + 1);
            const double y = context.spacing_y * (row + 1);
            initial[static_cast<std::size_t>(row * context.width + column)] =
                std::exp(-20.0 * ((x - 0.5) * (x - 0.5) +
                                  (y - 0.5) * (y - 0.5)));
        }
    }
    Vec state{};
    TS solver{};
    PetscCallAbort(PETSC_COMM_SELF, VecCreateSeq(
        PETSC_COMM_SELF, static_cast<PetscInt>(size), &state));
    PetscScalar* values{};
    PetscCallAbort(PETSC_COMM_SELF, VecGetArrayWrite(state, &values));
    std::copy(initial.begin(), initial.end(), values);
    PetscCallAbort(PETSC_COMM_SELF, VecRestoreArrayWrite(state, &values));
    PetscCallAbort(PETSC_COMM_SELF, TSCreate(PETSC_COMM_SELF, &solver));
    PetscCallAbort(PETSC_COMM_SELF, TSSetType(solver, TSEULER));
    PetscCallAbort(PETSC_COMM_SELF, TSSetRHSFunction(
        solver, nullptr, convection_diffusion_rhs, &context));
    PetscCallAbort(PETSC_COMM_SELF, TSSetTimeStep(solver, delta_time));
    PetscCallAbort(PETSC_COMM_SELF, TSSetMaxSteps(solver, steps));
    PetscCallAbort(PETSC_COMM_SELF, TSSetMaxTime(solver, steps * delta_time));
    PetscCallAbort(PETSC_COMM_SELF, TSSetExactFinalTime(
        solver, TS_EXACTFINALTIME_MATCHSTEP));
    PetscCallAbort(PETSC_COMM_SELF, TSSetSolution(solver, state));
    const auto petsc_started = std::chrono::steady_clock::now();
    PetscCallAbort(PETSC_COMM_SELF, TSSolve(solver, state));
    const double petsc_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - petsc_started).count();
    const PetscScalar* final_values{};
    PetscCallAbort(PETSC_COMM_SELF, VecGetArrayRead(state, &final_values));
    const std::vector<double> petsc(final_values, final_values + size);
    PetscCallAbort(PETSC_COMM_SELF, VecRestoreArrayRead(state, &final_values));

    auto smave = initial;
    const auto smave_started = std::chrono::steady_clock::now();
    for (int step = 0; step < steps; ++step) {
        const auto derivative = convection_diffusion_derivative(smave, context);
        for (std::size_t index = 0; index < size; ++index) {
            smave[index] += delta_time * derivative[index];
        }
    }
    const double smave_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - smave_started).count();
    PetscCallAbort(PETSC_COMM_SELF, TSDestroy(&solver));
    PetscCallAbort(PETSC_COMM_SELF, VecDestroy(&state));
    return {petsc, smave, relative_error(smave, petsc), 0,
            petsc_seconds, smave_seconds};
}

MotionComparison compare_cn_fem_heat() {
    constexpr PetscInt grid_points = 10;
    constexpr PetscInt size = grid_points - 2;
    constexpr double spacing = 1.0 / (grid_points - 1);
    constexpr double delta_time = 1.0 / (2.0 * (grid_points - 1) *
                                         (grid_points - 1));
    constexpr double requested_final_time = 0.014;
    constexpr int steps = 3;
    std::vector<double> initial(size);
    const double pi = std::acos(-1.0);
    for (PetscInt index = 0; index < size; ++index) {
        const double coordinate = spacing * (index + 1);
        initial[static_cast<std::size_t>(index)] =
            std::sin(6.0 * pi * coordinate) +
            3.0 * std::sin(2.0 * pi * coordinate);
    }

    FemHeatContext context;
    Vec state{};
    TS solver{};
    PetscCallAbort(PETSC_COMM_SELF, MatCreateSeqAIJ(
        PETSC_COMM_SELF, size, size, 3, nullptr, &context.mass));
    PetscCallAbort(PETSC_COMM_SELF, MatCreateSeqAIJ(
        PETSC_COMM_SELF, size, size, 3, nullptr, &context.stiffness));
    for (PetscInt row = 0; row < size; ++row) {
        PetscCallAbort(PETSC_COMM_SELF, MatSetValue(
            context.mass, row, row, 2.0 * spacing / 3.0, INSERT_VALUES));
        PetscCallAbort(PETSC_COMM_SELF, MatSetValue(
            context.stiffness, row, row, -2.0 / spacing, INSERT_VALUES));
        if (row > 0) {
            PetscCallAbort(PETSC_COMM_SELF, MatSetValue(
                context.mass, row, row - 1, spacing / 6.0, INSERT_VALUES));
            PetscCallAbort(PETSC_COMM_SELF, MatSetValue(
                context.stiffness, row, row - 1, 1.0 / spacing, INSERT_VALUES));
        }
        if (row + 1 < size) {
            PetscCallAbort(PETSC_COMM_SELF, MatSetValue(
                context.mass, row, row + 1, spacing / 6.0, INSERT_VALUES));
            PetscCallAbort(PETSC_COMM_SELF, MatSetValue(
                context.stiffness, row, row + 1, 1.0 / spacing, INSERT_VALUES));
        }
    }
    PetscCallAbort(PETSC_COMM_SELF, MatAssemblyBegin(context.mass, MAT_FINAL_ASSEMBLY));
    PetscCallAbort(PETSC_COMM_SELF, MatAssemblyEnd(context.mass, MAT_FINAL_ASSEMBLY));
    PetscCallAbort(PETSC_COMM_SELF, MatAssemblyBegin(
        context.stiffness, MAT_FINAL_ASSEMBLY));
    PetscCallAbort(PETSC_COMM_SELF, MatAssemblyEnd(
        context.stiffness, MAT_FINAL_ASSEMBLY));
    PetscCallAbort(PETSC_COMM_SELF, VecCreateSeq(PETSC_COMM_SELF, size, &state));
    PetscScalar* state_values{};
    PetscCallAbort(PETSC_COMM_SELF, VecGetArrayWrite(state, &state_values));
    std::copy(initial.begin(), initial.end(), state_values);
    PetscCallAbort(PETSC_COMM_SELF, VecRestoreArrayWrite(state, &state_values));
    PetscCallAbort(PETSC_COMM_SELF, VecDuplicate(state, &context.work));
    PetscCallAbort(PETSC_COMM_SELF, KSPCreate(PETSC_COMM_SELF, &context.mass_solver));
    PetscCallAbort(PETSC_COMM_SELF, KSPSetOperators(
        context.mass_solver, context.mass, context.mass));
    PetscCallAbort(PETSC_COMM_SELF, KSPSetType(context.mass_solver, KSPPREONLY));
    PC preconditioner{};
    PetscCallAbort(PETSC_COMM_SELF, KSPGetPC(context.mass_solver, &preconditioner));
    PetscCallAbort(PETSC_COMM_SELF, PCSetType(preconditioner, PCLU));
    PetscCallAbort(PETSC_COMM_SELF, KSPSetUp(context.mass_solver));
    PetscCallAbort(PETSC_COMM_SELF, TSCreate(PETSC_COMM_SELF, &solver));
    PetscCallAbort(PETSC_COMM_SELF, TSSetType(solver, TSCN));
    PetscCallAbort(PETSC_COMM_SELF, TSSetRHSFunction(
        solver, nullptr, fem_heat_rhs, &context));
    PetscCallAbort(PETSC_COMM_SELF, TSSetTimeStep(solver, delta_time));
    PetscCallAbort(PETSC_COMM_SELF, TSSetMaxSteps(solver, 10000));
    PetscCallAbort(PETSC_COMM_SELF, TSSetMaxTime(solver, requested_final_time));
    PetscCallAbort(PETSC_COMM_SELF, TSSetExactFinalTime(
        solver, TS_EXACTFINALTIME_STEPOVER));
    PetscCallAbort(PETSC_COMM_SELF, TSSetSolution(solver, state));
    TSAdapt adapt{};
    PetscCallAbort(PETSC_COMM_SELF, TSGetAdapt(solver, &adapt));
    PetscCallAbort(PETSC_COMM_SELF, TSAdaptSetType(adapt, TSADAPTNONE));
    const auto petsc_started = std::chrono::steady_clock::now();
    PetscCallAbort(PETSC_COMM_SELF, TSSolve(solver, state));
    const double petsc_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - petsc_started).count();
    PetscInt petsc_steps{};
    PetscCallAbort(PETSC_COMM_SELF, TSGetStepNumber(solver, &petsc_steps));
    if (petsc_steps != steps) {
        throw std::runtime_error("PETSc ex3 step-over contract changed");
    }
    const PetscScalar* final_values{};
    PetscCallAbort(PETSC_COMM_SELF, VecGetArrayRead(state, &final_values));
    const std::vector<double> petsc(final_values, final_values + size);
    PetscCallAbort(PETSC_COMM_SELF, VecRestoreArrayRead(state, &final_values));

    auto mass_apply = [](const std::vector<double>& input) {
        std::vector<double> output(input.size());
        for (std::size_t row = 0; row < input.size(); ++row) {
            output[row] = 2.0 * spacing / 3.0 * input[row];
            if (row > 0) output[row] += spacing / 6.0 * input[row - 1];
            if (row + 1 < input.size()) {
                output[row] += spacing / 6.0 * input[row + 1];
            }
        }
        return output;
    };
    auto stiffness_apply = [](const std::vector<double>& input) {
        std::vector<double> output(input.size());
        for (std::size_t row = 0; row < input.size(); ++row) {
            output[row] = -2.0 / spacing * input[row];
            if (row > 0) output[row] += input[row - 1] / spacing;
            if (row + 1 < input.size()) output[row] += input[row + 1] / spacing;
        }
        return output;
    };
    std::vector<double> smave = initial;
    std::size_t smave_iterations{};
    const auto smave_started = std::chrono::steady_clock::now();
    for (int step = 0; step < steps; ++step) {
        const auto mass = mass_apply(smave);
        const auto stiffness = stiffness_apply(smave);
        std::vector<double> right_hand_side(size);
        for (std::size_t row = 0; row < right_hand_side.size(); ++row) {
            right_hand_side[row] = mass[row] +
                0.5 * delta_time * stiffness[row];
        }
        const smave::LinearOperator operation = [mass_apply, stiffness_apply](
            const std::vector<double>& input, std::vector<double>& output) {
            output = mass_apply(input);
            const auto stiffness = stiffness_apply(input);
            for (std::size_t row = 0; row < output.size(); ++row) {
                output[row] -= 0.5 * delta_time * stiffness[row];
            }
            return true;
        };
        std::vector<double> diagonal(size,
            2.0 * spacing / 3.0 + delta_time / spacing);
        const auto solved = solve_smave_linear(
            size, operation, right_hand_side, smave, diagonal);
        smave_iterations += static_cast<std::size_t>(solved.iterations);
        smave = solved.solution;
    }
    const double smave_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - smave_started).count();
    PetscCallAbort(PETSC_COMM_SELF, TSDestroy(&solver));
    PetscCallAbort(PETSC_COMM_SELF, KSPDestroy(&context.mass_solver));
    PetscCallAbort(PETSC_COMM_SELF, VecDestroy(&context.work));
    PetscCallAbort(PETSC_COMM_SELF, VecDestroy(&state));
    PetscCallAbort(PETSC_COMM_SELF, MatDestroy(&context.stiffness));
    PetscCallAbort(PETSC_COMM_SELF, MatDestroy(&context.mass));
    return {petsc, smave, relative_error(smave, petsc), smave_iterations,
            petsc_seconds, smave_seconds};
}

MotionComparison compare_bdf1_bratu() {
    BratuContext context;
    constexpr int steps = 5;
    constexpr double delta_time = 1.0e-3;
    const auto size = static_cast<std::size_t>(context.width * context.height);
    Vec state{};
    Vec residual{};
    Mat matrix{};
    TS solver{};
    PetscCallAbort(PETSC_COMM_SELF, VecCreateSeq(
        PETSC_COMM_SELF, static_cast<PetscInt>(size), &state));
    PetscCallAbort(PETSC_COMM_SELF, VecDuplicate(state, &residual));
    PetscCallAbort(PETSC_COMM_SELF, VecSet(state, 0.0));
    PetscCallAbort(PETSC_COMM_SELF, MatCreateSeqAIJ(
        PETSC_COMM_SELF, static_cast<PetscInt>(size),
        static_cast<PetscInt>(size), 5, nullptr, &matrix));
    PetscCallAbort(PETSC_COMM_SELF, TSCreate(PETSC_COMM_SELF, &solver));
    PetscCallAbort(PETSC_COMM_SELF, TSSetProblemType(solver, TS_NONLINEAR));
    PetscCallAbort(PETSC_COMM_SELF, TSSetType(solver, TSBDF));
    PetscCallAbort(PETSC_COMM_SELF, TSBDFSetOrder(solver, 1));
    TSAdapt adapt{};
    PetscCallAbort(PETSC_COMM_SELF, TSGetAdapt(solver, &adapt));
    PetscCallAbort(PETSC_COMM_SELF, TSAdaptSetType(adapt, TSADAPTNONE));
    PetscCallAbort(PETSC_COMM_SELF, TSSetIFunction(
        solver, residual, bratu_residual, &context));
    PetscCallAbort(PETSC_COMM_SELF, TSSetIJacobian(
        solver, matrix, matrix, bratu_jacobian, &context));
    PetscCallAbort(PETSC_COMM_SELF, TSSetTimeStep(solver, delta_time));
    PetscCallAbort(PETSC_COMM_SELF, TSSetMaxSteps(solver, steps));
    PetscCallAbort(PETSC_COMM_SELF, TSSetMaxTime(solver, steps * delta_time));
    PetscCallAbort(PETSC_COMM_SELF, TSSetExactFinalTime(
        solver, TS_EXACTFINALTIME_MATCHSTEP));
    PetscCallAbort(PETSC_COMM_SELF, TSSetSolution(solver, state));
    SNES nonlinear_solver{};
    PetscCallAbort(PETSC_COMM_SELF, TSGetSNES(solver, &nonlinear_solver));
    PetscCallAbort(PETSC_COMM_SELF, SNESSetTolerances(
        nonlinear_solver, 1.0e-10, 1.0e-10, PETSC_CURRENT,
        PETSC_CURRENT, PETSC_CURRENT));
    const auto petsc_started = std::chrono::steady_clock::now();
    PetscCallAbort(PETSC_COMM_SELF, TSSolve(solver, state));
    const double petsc_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - petsc_started).count();
    const PetscScalar* values{};
    PetscCallAbort(PETSC_COMM_SELF, VecGetArrayRead(state, &values));
    const std::vector<double> petsc(values, values + size);
    PetscCallAbort(PETSC_COMM_SELF, VecRestoreArrayRead(state, &values));

    const double inverse_x = (context.width - 1.0) * (context.width - 1.0);
    const double inverse_y = (context.height - 1.0) * (context.height - 1.0);
    std::vector<double> smave(size);
    std::size_t smave_iterations{};
    const auto smave_started = std::chrono::steady_clock::now();
    for (int step = 0; step < steps; ++step) {
        auto candidate = smave;
        bool converged{};
        for (int newton = 0; newton < 12; ++newton) {
            std::vector<double> nonlinear_residual(size);
            std::vector<double> diagonal(size, 1.0);
            double residual_norm{};
            for (PetscInt row = 0; row < context.height; ++row) {
                for (PetscInt column = 0; column < context.width; ++column) {
                    const auto index = static_cast<std::size_t>(
                        row * context.width + column);
                    if (row == 0 || column == 0 || row + 1 == context.height ||
                        column + 1 == context.width) {
                        nonlinear_residual[index] = candidate[index];
                    } else {
                        const auto laplacian = inverse_x *
                                (candidate[index - 1] - 2.0 * candidate[index] +
                                 candidate[index + 1]) +
                            inverse_y *
                                (candidate[index - context.width] -
                                 2.0 * candidate[index] +
                                 candidate[index + context.width]);
                        nonlinear_residual[index] = candidate[index] - smave[index] -
                            delta_time *
                                (laplacian + context.lambda * std::exp(candidate[index]));
                        diagonal[index] = 1.0 +
                            2.0 * delta_time * (inverse_x + inverse_y) -
                            delta_time * context.lambda * std::exp(candidate[index]);
                    }
                    residual_norm = std::max(
                        residual_norm, std::abs(nonlinear_residual[index]));
                }
            }
            if (residual_norm <= 1.0e-10) {
                converged = true;
                break;
            }
            const smave::LinearOperator jacobian = [&](const std::vector<double>& input,
                                                        std::vector<double>& output) {
                output.resize(size);
                for (PetscInt row = 0; row < context.height; ++row) {
                    for (PetscInt column = 0; column < context.width; ++column) {
                        const auto index = static_cast<std::size_t>(
                            row * context.width + column);
                        if (row == 0 || column == 0 ||
                            row + 1 == context.height || column + 1 == context.width) {
                            output[index] = input[index];
                        } else {
                            output[index] = diagonal[index] * input[index] -
                                delta_time * inverse_x *
                                    (input[index - 1] + input[index + 1]) -
                                delta_time * inverse_y *
                                    (input[index - context.width] +
                                     input[index + context.width]);
                        }
                    }
                }
                return true;
            };
            std::vector<double> right_hand_side(size);
            for (std::size_t index = 0; index < size; ++index) {
                right_hand_side[index] = -nonlinear_residual[index];
            }
            const auto correction = solve_smave_linear(
                size, jacobian, right_hand_side, std::vector<double>(size), diagonal);
            smave_iterations += static_cast<std::size_t>(correction.iterations);
            for (std::size_t index = 0; index < size; ++index) {
                candidate[index] += correction.solution[index];
            }
        }
        if (!converged) throw std::runtime_error("SMAVE Bratu Newton failed");
        smave = std::move(candidate);
    }
    const double smave_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - smave_started).count();
    PetscCallAbort(PETSC_COMM_SELF, TSDestroy(&solver));
    PetscCallAbort(PETSC_COMM_SELF, MatDestroy(&matrix));
    PetscCallAbort(PETSC_COMM_SELF, VecDestroy(&residual));
    PetscCallAbort(PETSC_COMM_SELF, VecDestroy(&state));
    return {petsc, smave, relative_error(smave, petsc), smave_iterations,
            petsc_seconds, smave_seconds};
}

MotionComparison compare_bdf1_reaction() {
    constexpr double final_time = 1.0;
    constexpr int steps = 100;
    constexpr double delta_time = final_time / steps;
    Vec state{};
    Vec residual{};
    Mat matrix{};
    TS solver{};
    PetscCallAbort(PETSC_COMM_SELF, VecCreateSeq(PETSC_COMM_SELF, 2, &state));
    PetscCallAbort(PETSC_COMM_SELF, VecDuplicate(state, &residual));
    PetscScalar initial[] = {2.0, 1.0};
    PetscCallAbort(PETSC_COMM_SELF, VecSetValues(
        state, 2, std::array<PetscInt, 2>{0, 1}.data(), initial, INSERT_VALUES));
    PetscCallAbort(PETSC_COMM_SELF, VecAssemblyBegin(state));
    PetscCallAbort(PETSC_COMM_SELF, VecAssemblyEnd(state));
    PetscCallAbort(PETSC_COMM_SELF, MatCreateSeqDense(
        PETSC_COMM_SELF, 2, 2, nullptr, &matrix));
    PetscCallAbort(PETSC_COMM_SELF, TSCreate(PETSC_COMM_SELF, &solver));
    PetscCallAbort(PETSC_COMM_SELF, TSSetType(solver, TSBDF));
    PetscCallAbort(PETSC_COMM_SELF, TSBDFSetOrder(solver, 1));
    TSAdapt adapt{};
    PetscCallAbort(PETSC_COMM_SELF, TSGetAdapt(solver, &adapt));
    PetscCallAbort(PETSC_COMM_SELF, TSAdaptSetType(adapt, TSADAPTNONE));
    PetscCallAbort(PETSC_COMM_SELF, TSSetIFunction(
        solver, residual, reaction_residual, nullptr));
    PetscCallAbort(PETSC_COMM_SELF, TSSetIJacobian(
        solver, matrix, matrix, reaction_jacobian, nullptr));
    PetscCallAbort(PETSC_COMM_SELF, TSSetTimeStep(solver, delta_time));
    PetscCallAbort(PETSC_COMM_SELF, TSSetMaxSteps(solver, steps));
    PetscCallAbort(PETSC_COMM_SELF, TSSetMaxTime(solver, final_time));
    PetscCallAbort(PETSC_COMM_SELF, TSSetExactFinalTime(
        solver, TS_EXACTFINALTIME_MATCHSTEP));
    PetscCallAbort(PETSC_COMM_SELF, TSSetSolution(solver, state));
    const auto petsc_started = std::chrono::steady_clock::now();
    PetscCallAbort(PETSC_COMM_SELF, TSSolve(solver, state));
    const double petsc_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - petsc_started).count();
    const PetscScalar* values{};
    PetscCallAbort(PETSC_COMM_SELF, VecGetArrayRead(state, &values));
    const std::vector<double> petsc(values, values + 2);
    PetscCallAbort(PETSC_COMM_SELF, VecRestoreArrayRead(state, &values));
    std::vector<double> smave{2.0, 1.0};
    std::size_t smave_iterations{};
    const smave::LinearOperator operation = [delta_time](
        const std::vector<double>& input, std::vector<double>& output) {
        output = {
            (1.0 + delta_time) * input[0],
            -delta_time * input[0] + input[1],
        };
        return true;
    };
    const auto smave_started = std::chrono::steady_clock::now();
    for (int step = 0; step < steps; ++step) {
        const auto solved = solve_smave_linear(
            2, operation, smave, smave, {1.0 + delta_time, 1.0});
        smave_iterations += static_cast<std::size_t>(solved.iterations);
        smave = solved.solution;
    }
    const double smave_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - smave_started).count();
    PetscCallAbort(PETSC_COMM_SELF, TSDestroy(&solver));
    PetscCallAbort(PETSC_COMM_SELF, MatDestroy(&matrix));
    PetscCallAbort(PETSC_COMM_SELF, VecDestroy(&residual));
    PetscCallAbort(PETSC_COMM_SELF, VecDestroy(&state));
    return {petsc, smave, relative_error(smave, petsc), smave_iterations,
            petsc_seconds, smave_seconds};
}

MotionComparison compare_nonlinear_diffusion() {
    constexpr int size = 60;
    constexpr int steps = 100;
    const double spacing = 1.0 / static_cast<double>(size - 1);
    const double delta_time = spacing / 2.0;
    DiffusionContext context{size, spacing};
    Vec state{};
    Mat matrix{};
    TS solver{};
    PetscCallAbort(PETSC_COMM_SELF, VecCreateSeq(PETSC_COMM_SELF, size, &state));
    std::vector<PetscInt> indices(size);
    std::vector<PetscScalar> initial(size);
    for (int index = 0; index < size; ++index) {
        indices[index] = index;
        const auto coordinate = spacing * index;
        initial[index] = 1.0 + coordinate * coordinate;
    }
    PetscCallAbort(PETSC_COMM_SELF, VecSetValues(
        state, size, indices.data(), initial.data(), INSERT_VALUES));
    PetscCallAbort(PETSC_COMM_SELF, VecAssemblyBegin(state));
    PetscCallAbort(PETSC_COMM_SELF, VecAssemblyEnd(state));
    PetscCallAbort(PETSC_COMM_SELF, MatCreateSeqAIJ(
        PETSC_COMM_SELF, size, size, 3, nullptr, &matrix));
    PetscCallAbort(PETSC_COMM_SELF, TSCreate(PETSC_COMM_SELF, &solver));
    PetscCallAbort(PETSC_COMM_SELF, TSSetType(solver, TSBEULER));
    TSAdapt adapt{};
    PetscCallAbort(PETSC_COMM_SELF, TSGetAdapt(solver, &adapt));
    PetscCallAbort(PETSC_COMM_SELF, TSAdaptSetType(adapt, TSADAPTNONE));
    PetscCallAbort(PETSC_COMM_SELF, TSSetRHSFunction(
        solver, nullptr, nonlinear_diffusion_rhs, &context));
    PetscCallAbort(PETSC_COMM_SELF, TSSetRHSJacobian(
        solver, matrix, matrix, nonlinear_diffusion_jacobian, &context));
    PetscCallAbort(PETSC_COMM_SELF, TSSetTimeStep(solver, delta_time));
    PetscCallAbort(PETSC_COMM_SELF, TSSetMaxSteps(solver, steps));
    PetscCallAbort(PETSC_COMM_SELF, TSSetExactFinalTime(
        solver, TS_EXACTFINALTIME_MATCHSTEP));
    PetscCallAbort(PETSC_COMM_SELF, TSSetSolution(solver, state));
    const auto petsc_started = std::chrono::steady_clock::now();
    PetscCallAbort(PETSC_COMM_SELF, TSSolve(solver, state));
    const double petsc_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - petsc_started).count();
    const PetscScalar* values{};
    PetscCallAbort(PETSC_COMM_SELF, VecGetArrayRead(state, &values));
    const std::vector<double> petsc(values, values + size);
    PetscCallAbort(PETSC_COMM_SELF, VecRestoreArrayRead(state, &values));

    std::vector<double> smave(initial.begin(), initial.end());
    std::size_t smave_iterations{};
    double time{};
    const auto smave_started = std::chrono::steady_clock::now();
    for (int step = 0; step < steps; ++step) {
        const auto next_time = time + delta_time;
        auto candidate = smave;
        const auto predictor_scale = (1.0 + next_time) / (1.0 + time);
        for (int index = 0; index < size; ++index) {
            candidate[index] *= predictor_scale;
            if (index > 0 && index + 1 < size) {
                candidate[index] += 1.0e-6 * std::sin(static_cast<double>(index));
            }
        }
        bool newton_converged{};
        for (int newton = 0; newton < 12; ++newton) {
            const auto scale = 1.0 /
                (2.0 * spacing * spacing *
                 (1.0 + next_time) * (1.0 + next_time));
            std::vector<double> residual(size);
            residual.front() = candidate.front() - smave.front() - delta_time;
            residual.back() = candidate.back() - smave.back() - 2.0 * delta_time;
            double residual_norm = std::max(
                std::abs(residual.front()), std::abs(residual.back()));
            std::vector<double> diagonal(size, 1.0);
            for (int index = 1; index + 1 < size; ++index) {
                const auto laplacian = candidate[index + 1] + candidate[index - 1] -
                    2.0 * candidate[index];
                residual[index] = candidate[index] - smave[index] -
                    delta_time * candidate[index] * scale * laplacian;
                diagonal[index] = 1.0 - delta_time * scale *
                    (laplacian - 2.0 * candidate[index]);
                residual_norm = std::max(residual_norm, std::abs(residual[index]));
            }
            if (residual_norm <= 1.0e-10) {
                newton_converged = true;
                break;
            }
            const smave::LinearOperator jacobian = [&, scale](
                const std::vector<double>& input, std::vector<double>& output) {
                output.resize(size);
                output.front() = input.front();
                output.back() = input.back();
                for (int index = 1; index + 1 < size; ++index) {
                    const auto laplacian = candidate[index + 1] +
                        candidate[index - 1] - 2.0 * candidate[index];
                    output[index] =
                        (1.0 - delta_time * scale *
                            (laplacian - 2.0 * candidate[index])) * input[index] -
                        delta_time * scale * candidate[index] *
                            (input[index - 1] + input[index + 1]);
                }
                return true;
            };
            std::vector<double> right_hand_side(size);
            for (int index = 0; index < size; ++index) {
                right_hand_side[index] = -residual[index];
            }
            const auto correction = solve_smave_linear(
                size, jacobian, right_hand_side, std::vector<double>(size), diagonal);
            smave_iterations += static_cast<std::size_t>(correction.iterations);
            for (int index = 0; index < size; ++index) {
                candidate[index] += correction.solution[index];
            }
        }
        if (!newton_converged) {
            throw std::runtime_error("SMAVE nonlinear diffusion Newton failed");
        }
        smave = std::move(candidate);
        time = next_time;
    }
    const double smave_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - smave_started).count();
    PetscCallAbort(PETSC_COMM_SELF, TSDestroy(&solver));
    PetscCallAbort(PETSC_COMM_SELF, MatDestroy(&matrix));
    PetscCallAbort(PETSC_COMM_SELF, VecDestroy(&state));
    return {petsc, smave, relative_error(smave, petsc), smave_iterations,
            petsc_seconds, smave_seconds};
}

MotionComparison compare_mass_ode() {
    constexpr int steps = 3;
    constexpr double delta_time = 1.0;
    Vec state{};
    Vec residual{};
    Mat matrix{};
    TS solver{};
    PetscCallAbort(PETSC_COMM_SELF, VecCreateSeq(PETSC_COMM_SELF, 1, &state));
    PetscCallAbort(PETSC_COMM_SELF, VecDuplicate(state, &residual));
    PetscCallAbort(PETSC_COMM_SELF, VecSet(state, 0.0));
    PetscCallAbort(PETSC_COMM_SELF, MatCreateSeqDense(
        PETSC_COMM_SELF, 1, 1, nullptr, &matrix));
    PetscCallAbort(PETSC_COMM_SELF, TSCreate(PETSC_COMM_SELF, &solver));
    PetscCallAbort(PETSC_COMM_SELF, TSSetType(solver, TSBEULER));
    PetscCallAbort(PETSC_COMM_SELF, TSSetIFunction(
        solver, residual, mass_ode_residual, nullptr));
    PetscCallAbort(PETSC_COMM_SELF, TSSetIJacobian(
        solver, matrix, matrix, mass_ode_jacobian, nullptr));
    PetscCallAbort(PETSC_COMM_SELF, TSSetTimeStep(solver, delta_time));
    PetscCallAbort(PETSC_COMM_SELF, TSSetMaxSteps(solver, steps));
    PetscCallAbort(PETSC_COMM_SELF, TSSetExactFinalTime(
        solver, TS_EXACTFINALTIME_MATCHSTEP));
    PetscCallAbort(PETSC_COMM_SELF, TSSetSolution(solver, state));
    const auto petsc_started = std::chrono::steady_clock::now();
    PetscCallAbort(PETSC_COMM_SELF, TSSolve(solver, state));
    const double petsc_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - petsc_started).count();
    const PetscScalar* values{};
    PetscCallAbort(PETSC_COMM_SELF, VecGetArrayRead(state, &values));
    const std::vector<double> petsc{values[0]};
    PetscCallAbort(PETSC_COMM_SELF, VecRestoreArrayRead(state, &values));
    std::vector<double> smave{0.0};
    std::size_t smave_iterations{};
    const smave::LinearOperator operation = [](
        const std::vector<double>& input, std::vector<double>& output) {
        output = {2.0 * input[0]};
        return true;
    };
    const auto smave_started = std::chrono::steady_clock::now();
    for (int step = 0; step < steps; ++step) {
        const auto solved = solve_smave_linear(
            1, operation, {2.0 * smave[0] + delta_time}, smave, {2.0});
        smave_iterations += static_cast<std::size_t>(solved.iterations);
        smave = solved.solution;
    }
    const double smave_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - smave_started).count();
    PetscCallAbort(PETSC_COMM_SELF, TSDestroy(&solver));
    PetscCallAbort(PETSC_COMM_SELF, MatDestroy(&matrix));
    PetscCallAbort(PETSC_COMM_SELF, VecDestroy(&residual));
    PetscCallAbort(PETSC_COMM_SELF, VecDestroy(&state));
    return {petsc, smave, relative_error(smave, petsc), smave_iterations,
            petsc_seconds, smave_seconds};
}

MotionComparison compare_mass_dae() {
    constexpr int steps = 16;
    constexpr double delta_time = 1.0 / 16.0;
    Vec state{};
    Vec residual{};
    Mat matrix{};
    TS solver{};
    PetscCallAbort(PETSC_COMM_SELF, VecCreateSeq(PETSC_COMM_SELF, 2, &state));
    PetscCallAbort(PETSC_COMM_SELF, VecDuplicate(state, &residual));
    PetscCallAbort(PETSC_COMM_SELF, VecSet(state, 0.5));
    PetscCallAbort(PETSC_COMM_SELF, MatCreateSeqDense(
        PETSC_COMM_SELF, 2, 2, nullptr, &matrix));
    PetscCallAbort(PETSC_COMM_SELF, TSCreate(PETSC_COMM_SELF, &solver));
    PetscCallAbort(PETSC_COMM_SELF, TSSetType(solver, TSBEULER));
    PetscCallAbort(PETSC_COMM_SELF, TSSetIFunction(
        solver, residual, mass_dae_residual, nullptr));
    PetscCallAbort(PETSC_COMM_SELF, TSSetIJacobian(
        solver, matrix, matrix, mass_dae_jacobian, nullptr));
    PetscCallAbort(PETSC_COMM_SELF, TSSetTimeStep(solver, delta_time));
    PetscCallAbort(PETSC_COMM_SELF, TSSetMaxSteps(solver, steps));
    PetscCallAbort(PETSC_COMM_SELF, TSSetMaxTime(solver, 1.0));
    PetscCallAbort(PETSC_COMM_SELF, TSSetExactFinalTime(
        solver, TS_EXACTFINALTIME_MATCHSTEP));
    PetscCallAbort(PETSC_COMM_SELF, TSSetSolution(solver, state));
    const auto petsc_started = std::chrono::steady_clock::now();
    PetscCallAbort(PETSC_COMM_SELF, TSSolve(solver, state));
    const double petsc_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - petsc_started).count();
    const PetscScalar* values{};
    PetscCallAbort(PETSC_COMM_SELF, VecGetArrayRead(state, &values));
    const std::vector<double> petsc{values[0], values[1]};
    PetscCallAbort(PETSC_COMM_SELF, VecRestoreArrayRead(state, &values));
    std::vector<double> smave{0.5, 0.5};
    std::size_t smave_iterations{};
    const smave::LinearOperator operation = [delta_time](
        const std::vector<double>& input, std::vector<double>& output) {
        output = {
            input[0] / delta_time - input[1],
            input[0] - input[1],
        };
        return true;
    };
    const auto smave_started = std::chrono::steady_clock::now();
    for (int step = 0; step < steps; ++step) {
        const auto solved = solve_smave_linear(
            2, operation, {smave[0] / delta_time, 0.0}, smave,
            {1.0 / delta_time, -1.0});
        smave_iterations += static_cast<std::size_t>(solved.iterations);
        smave = solved.solution;
    }
    const double smave_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - smave_started).count();
    PetscCallAbort(PETSC_COMM_SELF, TSDestroy(&solver));
    PetscCallAbort(PETSC_COMM_SELF, MatDestroy(&matrix));
    PetscCallAbort(PETSC_COMM_SELF, VecDestroy(&residual));
    PetscCallAbort(PETSC_COMM_SELF, VecDestroy(&state));
    return {petsc, smave, relative_error(smave, petsc), smave_iterations,
            petsc_seconds, smave_seconds};
}

DaeComparison compare_euler_dae(double delta_time, int steps) {
    Vec state{};
    TS solver{};
    PetscCallAbort(PETSC_COMM_SELF, VecCreateSeq(PETSC_COMM_SELF, 1, &state));
    PetscCallAbort(PETSC_COMM_SELF, VecSet(state, 1.0));
    PetscCallAbort(PETSC_COMM_SELF, TSCreate(PETSC_COMM_SELF, &solver));
    PetscCallAbort(PETSC_COMM_SELF, TSSetType(solver, TSEULER));
    PetscCallAbort(PETSC_COMM_SELF, TSSetRHSFunction(
        solver, nullptr, dae_reduced_rhs, nullptr));
    PetscCallAbort(PETSC_COMM_SELF, TSSetTimeStep(solver, delta_time));
    PetscCallAbort(PETSC_COMM_SELF, TSSetMaxSteps(solver, steps));
    PetscCallAbort(PETSC_COMM_SELF, TSSetExactFinalTime(
        solver, TS_EXACTFINALTIME_MATCHSTEP));
    PetscCallAbort(PETSC_COMM_SELF, TSSetSolution(solver, state));
    const auto petsc_started = std::chrono::steady_clock::now();
    PetscCallAbort(PETSC_COMM_SELF, TSSolve(solver, state));
    const double petsc_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - petsc_started).count();
    const PetscScalar* values{};
    PetscCallAbort(PETSC_COMM_SELF, VecGetArrayRead(state, &values));
    const double petsc_state = values[0];
    PetscCallAbort(PETSC_COMM_SELF, VecRestoreArrayRead(state, &values));
    PetscCallAbort(PETSC_COMM_SELF, TSDestroy(&solver));
    PetscCallAbort(PETSC_COMM_SELF, VecDestroy(&state));

    double smave_state = 1.0;
    double algebraic = 1.0;
    std::size_t smave_iterations{};
    const auto smave_started = std::chrono::steady_clock::now();
    for (int step = 0; step < steps; ++step) {
        const smave::LinearOperator constraint = [](
            const std::vector<double>& input, std::vector<double>& output) {
            output = input;
            return true;
        };
        const auto projected = solve_smave_linear(
            1, constraint, {smave_state}, {algebraic}, {1.0});
        smave_iterations += static_cast<std::size_t>(projected.iterations);
        algebraic = projected.solution[0];
        smave_state += delta_time * (smave_state + algebraic);
    }
    const smave::LinearOperator constraint = [](
        const std::vector<double>& input, std::vector<double>& output) {
        output = input;
        return true;
    };
    const auto projected = solve_smave_linear(
        1, constraint, {smave_state}, {algebraic}, {1.0});
    smave_iterations += static_cast<std::size_t>(projected.iterations);
    algebraic = projected.solution[0];
    const double smave_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - smave_started).count();
    return {
        .petsc_state = petsc_state,
        .smave_state = smave_state,
        .smave_algebraic = algebraic,
        .relative_error = std::abs(smave_state - petsc_state) /
            std::max(1.0, std::abs(petsc_state)),
        .constraint_residual = std::abs(smave_state - algebraic),
        .smave_iterations = smave_iterations,
        .petsc_seconds = petsc_seconds,
        .smave_seconds = smave_seconds,
    };
}

double relative_error(const std::vector<double>& left, const std::vector<double>& right) {
    double numerator{};
    double denominator{1.0};
    for (std::size_t index = 0; index < left.size(); ++index) {
        numerator = std::max(numerator, std::abs(left[index] - right[index]));
        denominator = std::max(denominator, std::abs(right[index]));
    }
    return numerator / denominator;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        std::cerr << "usage: smave_petsc_ts_comparison OUTPUT [EX5_CONTROL]\n";
        return 2;
    }
    PetscCallAbort(PETSC_COMM_SELF, PetscInitializeNoArguments());
    try {
        constexpr double delta_time = 0.001;
        constexpr int steps = 100;
        Vec state{};
        Mat matrix{};
        TS solver{};
        PetscCallAbort(PETSC_COMM_SELF, VecCreateSeq(PETSC_COMM_SELF, 3, &state));
        PetscCallAbort(PETSC_COMM_SELF, VecSet(state, 1.0));
        PetscCallAbort(PETSC_COMM_SELF, MatCreateSeqAIJ(PETSC_COMM_SELF, 3, 3, 3, nullptr, &matrix));
        PetscCallAbort(PETSC_COMM_SELF, TSCreate(PETSC_COMM_SELF, &solver));
        PetscCallAbort(PETSC_COMM_SELF, TSSetType(solver, TSBEULER));
        PetscCallAbort(PETSC_COMM_SELF, TSSetRHSFunction(solver, nullptr, rhs, nullptr));
        PetscCallAbort(PETSC_COMM_SELF, TSSetRHSJacobian(solver, matrix, matrix, jacobian, nullptr));
        PetscCallAbort(PETSC_COMM_SELF, TSSetTimeStep(solver, delta_time));
        PetscCallAbort(PETSC_COMM_SELF, TSSetMaxSteps(solver, steps));
        PetscCallAbort(PETSC_COMM_SELF, TSSetExactFinalTime(solver, TS_EXACTFINALTIME_MATCHSTEP));
        PetscCallAbort(PETSC_COMM_SELF, TSSetSolution(solver, state));
        const auto ex2_petsc_started = std::chrono::steady_clock::now();
        PetscCallAbort(PETSC_COMM_SELF, TSSolve(solver, state));
        const double ex2_petsc_seconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - ex2_petsc_started).count();
        const PetscScalar* values{};
        PetscCallAbort(PETSC_COMM_SELF, VecGetArrayRead(state, &values));
        const std::vector<double> petsc(values, values + 3);
        PetscCallAbort(PETSC_COMM_SELF, VecRestoreArrayRead(state, &values));

        std::vector<double> smave_state(3, 1.0);
        std::size_t total_iterations{};
        const smave::LinearOperator operation = [](const std::vector<double>& input,
                                                    std::vector<double>& output) {
            output = {
                (1.0 - 2.0 * delta_time) * input[0] - delta_time * input[1],
                -delta_time * input[0] + (1.0 - 2.0 * delta_time) * input[1] - delta_time * input[2],
                -delta_time * input[1] + (1.0 - 2.0 * delta_time) * input[2],
            };
            return true;
        };
        const smave::Preconditioner preconditioner = [](const std::vector<double>& residual,
                                                         std::vector<double>& output) {
            output.resize(residual.size());
            for (std::size_t index = 0; index < residual.size(); ++index) {
                output[index] = residual[index] / (1.0 - 2.0 * delta_time);
            }
            return true;
        };
        const auto ex2_smave_started = std::chrono::steady_clock::now();
        for (int step = 0; step < steps; ++step) {
            const auto result = smave::restarted_gmres(
                3, operation, smave_state, smave_state, preconditioner,
                1.0e-14, 1.0e-12, 20, 3);
            if (!result.converged) throw std::runtime_error("SMAVE BEuler linear solve failed");
            total_iterations += static_cast<std::size_t>(result.iterations);
            smave_state = result.solution;
        }
        const double ex2_smave_seconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - ex2_smave_started).count();
        const auto error = relative_error(smave_state, petsc);
        const auto ex6 = compare_euler_dae(0.01, 100);
        const auto ex7 = compare_euler_dae(0.01, 100);
        const auto ex80 = compare_constant_acceleration();
        const auto ex81 = compare_constant_velocity();
        const auto ex14 = compare_rk4_polynomial_chain();
        const auto ex15 = compare_bdf1_reaction();
        const auto ex12 = compare_nonlinear_diffusion();
        const auto ex18 = compare_mass_dae();
        const auto ex26 = compare_mass_ode();
        const auto ex4 = compare_euler_convection_diffusion();
        const auto ex3 = compare_cn_fem_heat();
        const auto ex21 = compare_bdf1_bratu();
        const auto ex5 = compare_ex5(
            argc > 2 ? argv[2] : "benchmark/petsc-ts/ex5_control.txt");
        const auto ex28 = compare_bgk();
        const auto ex27 = compare_particle_landau();
        const bool ex2_agreement = error <= 1.0e-10;
        const bool ex6_agreement = ex6.relative_error <= 1.0e-12 &&
            ex6.constraint_residual <= 1.0e-12;
        const bool ex7_agreement = ex7.relative_error <= 1.0e-12 &&
            ex7.constraint_residual <= 1.0e-12;
        const bool ex80_agreement = ex80.relative_error <= 1.0e-12 &&
            ex80.smave_iterations > 0;
        const bool ex81_agreement = ex81.relative_error <= 1.0e-12 &&
            ex81.smave_iterations > 0;
        const bool ex14_agreement = ex14.relative_error <= 1.0e-12;
        const auto ex15_conservation = std::abs(ex15.smave[0] + ex15.smave[1] - 3.0);
        const bool ex15_agreement = ex15.relative_error <= 1.0e-12 &&
            ex15_conservation <= 1.0e-12;
        std::vector<double> ex12_exact(ex12.smave.size());
        const double ex12_time = 100.0 / (2.0 * 59.0);
        for (std::size_t index = 0; index < ex12_exact.size(); ++index) {
            const auto coordinate = static_cast<double>(index) / 59.0;
            ex12_exact[index] = (1.0 + ex12_time) *
                (1.0 + coordinate * coordinate);
        }
        const auto ex12_exact_error = relative_error(ex12.smave, ex12_exact);
        const bool ex12_agreement = ex12.relative_error <= 1.0e-9 &&
            ex12_exact_error <= 1.0e-9;
        const bool ex18_agreement = ex18.relative_error <= 1.0e-8;
        const bool ex26_agreement = ex26.relative_error <= 1.0e-12;
        const bool ex4_agreement = ex4.relative_error <= 1.0e-12;
        const bool ex3_agreement = ex3.relative_error <= 1.0e-8 &&
            ex3.smave_iterations > 0;
        const bool ex21_agreement = ex21.relative_error <= 1.0e-8 &&
            ex21.smave_iterations > 0;
        const bool ex5_agreement = ex5.relative_error <= 1.0e-6 &&
            ex5.smave_iterations > 0;
        const bool ex28_agreement = ex28.relative_error <= 1.0e-12;
        const bool ex27_agreement = ex27.relative_error <= 1.0e-7 &&
            ex27.smave_iterations > 0;
        const int agreements = static_cast<int>(ex2_agreement) +
            static_cast<int>(ex6_agreement) + static_cast<int>(ex7_agreement) +
            static_cast<int>(ex80_agreement) + static_cast<int>(ex81_agreement) +
            static_cast<int>(ex14_agreement) + static_cast<int>(ex15_agreement);
        const int total_agreements = agreements + static_cast<int>(ex12_agreement) +
            static_cast<int>(ex18_agreement) + static_cast<int>(ex26_agreement) +
            static_cast<int>(ex4_agreement) + static_cast<int>(ex3_agreement);
        const int all_agreements = total_agreements +
            static_cast<int>(ex21_agreement) + static_cast<int>(ex5_agreement) +
            static_cast<int>(ex28_agreement) + static_cast<int>(ex27_agreement);
        std::filesystem::create_directories(std::filesystem::path(argv[1]).parent_path());
        std::ofstream output(argv[1]);
        output << std::setprecision(17)
               << "SMAVE_PETSC_TS_COMPARISON 6\n"
               << "CASES 16\n"
               << "AGREEMENTS " << all_agreements << "\n"
               << "CASE \"ex2\" METHOD \"BEuler\" AGREEMENT "
               << ex2_agreement << " ERROR " << error
               << " PETSC_SECONDS " << ex2_petsc_seconds
               << " SMAVE_SECONDS " << ex2_smave_seconds
               << " SMAVE_VS_PETSC_SPEEDUP "
               << ex2_petsc_seconds / ex2_smave_seconds << "\n"
               << "CASE \"ex6\" METHOD \"Euler+algebraic-projection\" AGREEMENT "
               << ex6_agreement << " ERROR " << ex6.relative_error
               << " CONSTRAINT_RESIDUAL " << ex6.constraint_residual
               << " SMAVE_ITERATIONS " << ex6.smave_iterations
               << " PETSC_SECONDS " << ex6.petsc_seconds
               << " SMAVE_SECONDS " << ex6.smave_seconds
               << " SMAVE_VS_PETSC_SPEEDUP "
               << ex6.petsc_seconds / ex6.smave_seconds
               << " PETSC_FINAL " << ex6.petsc_state
               << " SMAVE_FINAL " << ex6.smave_state << ' ' << ex6.smave_algebraic << "\n"
               << "CASE \"ex7\" METHOD \"Euler+combined-layout-projection\" AGREEMENT "
               << ex7_agreement << " ERROR " << ex7.relative_error
               << " CONSTRAINT_RESIDUAL " << ex7.constraint_residual
               << " SMAVE_ITERATIONS " << ex7.smave_iterations
               << " PETSC_SECONDS " << ex7.petsc_seconds
               << " SMAVE_SECONDS " << ex7.smave_seconds
               << " SMAVE_VS_PETSC_SPEEDUP "
               << ex7.petsc_seconds / ex7.smave_seconds
               << " PETSC_FINAL " << ex7.petsc_state
               << " SMAVE_FINAL " << ex7.smave_state << ' ' << ex7.smave_algebraic << "\n"
               << "CASE \"ex80\" METHOD \"Alpha2+constant-acceleration\" AGREEMENT "
               << ex80_agreement << " ERROR " << ex80.relative_error
               << " SMAVE_ITERATIONS " << ex80.smave_iterations
               << " PETSC_SECONDS " << ex80.petsc_seconds
               << " SMAVE_SECONDS " << ex80.smave_seconds
               << " SMAVE_VS_PETSC_SPEEDUP "
               << ex80.petsc_seconds / ex80.smave_seconds
               << " PETSC_FINAL " << ex80.petsc[0] << ' ' << ex80.petsc[1]
               << " SMAVE_FINAL " << ex80.smave[0] << ' ' << ex80.smave[1] << "\n"
               << "CASE \"ex81\" METHOD \"Alpha+constant-velocity\" AGREEMENT "
               << ex81_agreement << " ERROR " << ex81.relative_error
               << " SMAVE_ITERATIONS " << ex81.smave_iterations
               << " PETSC_SECONDS " << ex81.petsc_seconds
               << " SMAVE_SECONDS " << ex81.smave_seconds
               << " SMAVE_VS_PETSC_SPEEDUP "
               << ex81.petsc_seconds / ex81.smave_seconds
               << " PETSC_FINAL " << ex81.petsc[0]
               << " SMAVE_FINAL " << ex81.smave[0] << "\n"
               << "CASE \"ex14\" METHOD \"RK4-polynomial-chain\" AGREEMENT "
               << ex14_agreement << " ERROR " << ex14.relative_error
               << " PETSC_SECONDS " << ex14.petsc_seconds
               << " SMAVE_SECONDS " << ex14.smave_seconds
               << " SMAVE_VS_PETSC_SPEEDUP "
               << ex14.petsc_seconds / ex14.smave_seconds
               << " PETSC_FINAL_NORM " << *std::max_element(
                    ex14.petsc.begin(), ex14.petsc.end())
               << " SMAVE_FINAL_NORM " << *std::max_element(
                    ex14.smave.begin(), ex14.smave.end()) << "\n"
               << "CASE \"ex15\" METHOD \"BDF1-conservative-reaction\" AGREEMENT "
               << ex15_agreement << " ERROR " << ex15.relative_error
               << " SMAVE_ITERATIONS " << ex15.smave_iterations
               << " PETSC_SECONDS " << ex15.petsc_seconds
               << " SMAVE_SECONDS " << ex15.smave_seconds
               << " SMAVE_VS_PETSC_SPEEDUP "
               << ex15.petsc_seconds / ex15.smave_seconds
               << " CONSERVATION_RESIDUAL " << ex15_conservation
               << " PETSC_FINAL " << ex15.petsc[0] << ' ' << ex15.petsc[1]
               << " SMAVE_FINAL " << ex15.smave[0] << ' ' << ex15.smave[1] << "\n"
               << "CASE \"ex12\" METHOD \"BEuler-nonlinear-diffusion\" AGREEMENT "
               << ex12_agreement << " ERROR " << ex12.relative_error
               << " EXACT_ERROR " << ex12_exact_error
               << " SMAVE_ITERATIONS " << ex12.smave_iterations
               << " PETSC_SECONDS " << ex12.petsc_seconds
               << " SMAVE_SECONDS " << ex12.smave_seconds
               << " SMAVE_VS_PETSC_SPEEDUP "
               << ex12.petsc_seconds / ex12.smave_seconds << "\n"
               << "CASE \"ex18\" METHOD \"BEuler-nontrivial-mass-DAE\" AGREEMENT "
               << ex18_agreement << " ERROR " << ex18.relative_error
               << " SMAVE_ITERATIONS " << ex18.smave_iterations
               << " PETSC_SECONDS " << ex18.petsc_seconds
               << " SMAVE_SECONDS " << ex18.smave_seconds
               << " SMAVE_VS_PETSC_SPEEDUP "
               << ex18.petsc_seconds / ex18.smave_seconds
               << " PETSC_FINAL " << ex18.petsc[0] << ' ' << ex18.petsc[1]
               << " SMAVE_FINAL " << ex18.smave[0] << ' ' << ex18.smave[1] << "\n"
               << "CASE \"ex26\" METHOD \"BEuler-mass-ODE\" AGREEMENT "
               << ex26_agreement << " ERROR " << ex26.relative_error
               << " SMAVE_ITERATIONS " << ex26.smave_iterations
               << " PETSC_SECONDS " << ex26.petsc_seconds
               << " SMAVE_SECONDS " << ex26.smave_seconds
               << " SMAVE_VS_PETSC_SPEEDUP "
               << ex26.petsc_seconds / ex26.smave_seconds
               << " PETSC_FINAL " << ex26.petsc[0]
               << " SMAVE_FINAL " << ex26.smave[0] << "\n"
               << "CASE \"ex4\" METHOD \"Euler-2D-convection-diffusion\" AGREEMENT "
               << ex4_agreement << " ERROR " << ex4.relative_error
               << " PETSC_SECONDS " << ex4.petsc_seconds
               << " SMAVE_SECONDS " << ex4.smave_seconds
               << " SMAVE_VS_PETSC_SPEEDUP "
               << ex4.petsc_seconds / ex4.smave_seconds
               << " GRID 9 9 STEPS 10\n"
               << "CASE \"ex3\" METHOD \"CN-FEM-heat-mass-matrix\" AGREEMENT "
               << ex3_agreement << " ERROR " << ex3.relative_error
               << " SMAVE_ITERATIONS " << ex3.smave_iterations
               << " PETSC_SECONDS " << ex3.petsc_seconds
               << " SMAVE_SECONDS " << ex3.smave_seconds
               << " SMAVE_VS_PETSC_SPEEDUP "
               << ex3.petsc_seconds / ex3.smave_seconds
               << " GRID_POINTS 10 REQUESTED_FINAL_TIME 0.014 STEPS 3\n"
               << "CASE \"ex21\" METHOD \"BDF1-2D-Bratu\" AGREEMENT "
               << ex21_agreement << " ERROR " << ex21.relative_error
               << " SMAVE_ITERATIONS " << ex21.smave_iterations
               << " PETSC_SECONDS " << ex21.petsc_seconds
               << " SMAVE_SECONDS " << ex21.smave_seconds
               << " SMAVE_VS_PETSC_SPEEDUP "
               << ex21.petsc_seconds / ex21.smave_seconds
               << " GRID 17 17 LAMBDA 6 STEPS 5\n"
               << "CASE \"ex5\" METHOD \"BEuler-JFNK\" AGREEMENT "
               << ex5_agreement << " ERROR " << ex5.relative_error
               << " SMAVE_ITERATIONS " << ex5.smave_iterations
               << " PETSC_SECONDS " << ex5.petsc_seconds
               << " SMAVE_SECONDS " << ex5.smave_seconds
               << " SMAVE_VS_PETSC_SPEEDUP "
               << ex5.petsc_seconds / ex5.smave_seconds
               << " GRID 20 20 DOF 5 STEPS 130 CONTROL_INPUT \""
               << (argc > 2 ? argv[2] : "benchmark/petsc-ts/ex5_control.txt")
               << "\"\n"
               << "CASE \"ex28\" METHOD \"Euler-BGK\" AGREEMENT "
               << ex28_agreement << " ERROR " << ex28.relative_error
               << " PETSC_SECONDS " << ex28.petsc_seconds
               << " SMAVE_SECONDS " << ex28.smave_seconds
               << " SMAVE_VS_PETSC_SPEEDUP "
               << ex28.petsc_seconds / ex28.smave_seconds
               << " VELOCITY_CELLS 200 STEPS 1000\n"
               << "CASE \"ex27\" METHOD \"Theta0.5-particle-Landau\" AGREEMENT "
               << ex27_agreement << " ERROR " << ex27.relative_error
               << " SMAVE_ITERATIONS " << ex27.smave_iterations
               << " PETSC_SECONDS " << ex27.petsc_seconds
               << " SMAVE_SECONDS " << ex27.smave_seconds
               << " SMAVE_VS_PETSC_SPEEDUP "
               << ex27.petsc_seconds / ex27.smave_seconds
               << " TOLERANCE 1e-7 PARTICLES 3 DIMENSION 2 STEPS 1\n"
               << "METHOD \"BEuler\"\n"
               << "TIME_STEP " << delta_time << "\n"
               << "STEPS " << steps << "\n"
               << "SMAVE_TOTAL_LINEAR_ITERATIONS " << total_iterations << "\n"
               << "PETSC_FINAL " << petsc[0] << ' ' << petsc[1] << ' ' << petsc[2] << "\n"
               << "SMAVE_FINAL " << smave_state[0] << ' ' << smave_state[1] << ' ' << smave_state[2] << "\n"
               << "RELATIVE_INF_ERROR " << error << "\n"
               << "AGREEMENT " << (all_agreements == 16) << "\nEND\n";
        PetscCallAbort(PETSC_COMM_SELF, TSDestroy(&solver));
        PetscCallAbort(PETSC_COMM_SELF, MatDestroy(&matrix));
        PetscCallAbort(PETSC_COMM_SELF, VecDestroy(&state));
        PetscCallAbort(PETSC_COMM_SELF, PetscFinalize());
        std::cout << "PETSc TS/SMAVE BEuler agreement error=" << error << '\n';
        return all_agreements == 16 ? 0 : 4;
    } catch (const std::exception& error) {
        std::cerr << "PETSc TS comparison failure: " << error.what() << '\n';
        PetscFinalize();
        return 2;
    }
}
