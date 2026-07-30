#include "smave/linear.hpp"
#include "smave/device.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

double residual_inf(
    const std::vector<double>& solution,
    const std::vector<double>& right_hand_side,
    double diagonal,
    double off_diagonal) {
    double maximum{};
    for (std::size_t index = 0; index < solution.size(); ++index) {
        const auto left = index == 0 ? solution.size() - 1 : index - 1;
        const auto right = index + 1 == solution.size() ? 0 : index + 1;
        maximum = std::max(maximum, std::abs(
            diagonal * solution[index] +
            off_diagonal * (solution[left] + solution[right]) -
            right_hand_side[index]));
    }
    return maximum;
}

}  // namespace

int main() {
    try {
        constexpr std::size_t size = 64;
        constexpr double diagonal = 2.5;
        constexpr double off_diagonal = -0.75;
        smave::PeriodicTridiagonalFactorization factorization(
            size, diagonal, off_diagonal);
        require(factorization.valid(), "valid periodic factorization was rejected");
        require(factorization.size() == size, "periodic factorization lost its size");

        std::vector<double> first(size);
        std::vector<double> second(size);
        for (std::size_t index = 0; index < size; ++index) {
            first[index] = std::sin(0.17 * static_cast<double>(index)) + 0.25;
            second[index] = std::cos(0.11 * static_cast<double>(index)) - 0.5;
        }
        std::vector<double> first_solution;
        require(factorization.solve(first, first_solution), "single periodic solve failed");
        require(residual_inf(first_solution, first, diagonal, off_diagonal) <= 1.0e-12,
                "single periodic solve failed the original residual gate");

        std::vector<double> packed = first;
        packed.insert(packed.end(), second.begin(), second.end());
        std::vector<double> batch_solutions;
        require(factorization.solve_batch(packed, 2, batch_solutions),
                "batched periodic solve failed");
        require(std::equal(first_solution.begin(), first_solution.end(),
                           batch_solutions.begin()),
                "single and batched periodic solutions differ");
        const std::vector<double> second_solution(
            batch_solutions.begin() + static_cast<std::ptrdiff_t>(size),
            batch_solutions.end());
        require(residual_inf(second_solution, second, diagonal, off_diagonal) <= 1.0e-12,
                "batched periodic solve failed the original residual gate");
        require(!factorization.solve_batch(packed, 3, batch_solutions),
                "invalid periodic batch shape was accepted");

        const smave::PeriodicTridiagonalFactorization invalid(2, diagonal, off_diagonal);
        require(!invalid.valid(), "undersized periodic factorization was accepted");

        constexpr std::size_t width = 8;
        constexpr double diffusion_number = 0.125;
        std::vector<double> exact(width * width);
        std::vector<double> right_hand_side(width * width);
        for (std::size_t row = 0; row < width; ++row) {
            for (std::size_t column = 0; column < width; ++column) {
                const auto index = row * width + column;
                exact[index] = std::sin(0.25 * static_cast<double>(row)) +
                    std::cos(0.37 * static_cast<double>(column));
            }
        }
        for (std::size_t row = 0; row < width; ++row) {
            const auto south = row == 0 ? width - 1 : row - 1;
            const auto north = row + 1 == width ? 0 : row + 1;
            for (std::size_t column = 0; column < width; ++column) {
                const auto west = column == 0 ? width - 1 : column - 1;
                const auto east = column + 1 == width ? 0 : column + 1;
                const auto index = row * width + column;
                right_hand_side[index] =
                    (1.0 + 4.0 * diffusion_number) * exact[index] -
                    diffusion_number * (exact[row * width + west] +
                                        exact[row * width + east] +
                                        exact[south * width + column] +
                                        exact[north * width + column]);
            }
        }
        const auto spectral = smave::accelerate_periodic_helmholtz_2d_batch(
            right_hand_side, 1, width, diffusion_number);
        smave::AcceleratePeriodicHelmholtz2DPlan reusable_plan(
            width, diffusion_number);
        if (!spectral.available) {
            require(!spectral.executed && !spectral.verified &&
                        spectral.output.empty() && !spectral.reason.empty(),
                    "unavailable Accelerate solve did not fail closed");
            require(!reusable_plan.available() && !reusable_plan.reason().empty(),
                    "unavailable reusable Accelerate plan was not reported");
            std::vector<double> unavailable_solution;
            require(!reusable_plan.solve(right_hand_side, unavailable_solution),
                    "unavailable reusable Accelerate plan executed");
        } else {
            require(spectral.executed && spectral.verified,
                    "available Accelerate spectral solve did not execute");
            double spectral_error{};
            for (std::size_t index = 0; index < exact.size(); ++index) {
                spectral_error = std::max(
                    spectral_error, std::abs(spectral.output[index] - exact[index]));
            }
            require(spectral_error <= 1.0e-12,
                    "Accelerate spectral solve failed the manufactured solution");
            require(reusable_plan.available(),
                    "reusable Accelerate FFT plan unavailable");
            std::vector<double> reusable_solution;
            double reusable_kernel_us{};
            require(reusable_plan.solve(
                        right_hand_side, reusable_solution, &reusable_kernel_us),
                    "reusable Accelerate FFT plan solve failed");
            require(reusable_kernel_us > 0.0,
                    "reusable Accelerate FFT plan did not report kernel time");
            double reusable_error{};
            for (std::size_t index = 0; index < exact.size(); ++index) {
                reusable_error = std::max(reusable_error,
                    std::abs(reusable_solution[index] - exact[index]));
            }
            require(reusable_error <= 1.0e-12,
                    "reusable Accelerate FFT plan failed manufactured solution");
            std::vector<double> reusable_batch_right_hand_side;
            reusable_batch_right_hand_side.insert(
                reusable_batch_right_hand_side.end(),
                right_hand_side.begin(), right_hand_side.end());
            reusable_batch_right_hand_side.insert(
                reusable_batch_right_hand_side.end(),
                right_hand_side.begin(), right_hand_side.end());
            std::vector<double> reusable_batch_solution;
            double reusable_batch_kernel_us{};
            require(reusable_plan.solve_batch(
                        reusable_batch_right_hand_side, 2,
                        reusable_batch_solution, &reusable_batch_kernel_us),
                    "reusable Accelerate FFT batch plan solve failed");
            require(reusable_batch_kernel_us > 0.0,
                    "reusable Accelerate FFT batch plan did not report kernel time");
            for (std::size_t index = 0; index < reusable_batch_solution.size(); ++index) {
                require(std::abs(
                            reusable_batch_solution[index] -
                            exact[index % exact.size()]) <= 1.0e-12,
                        "reusable Accelerate FFT batch plan failed manufactured solution");
            }
        }

        std::vector<double> lower(size);
        std::vector<double> variable_diagonal(size);
        std::vector<double> upper(size);
        std::vector<double> variable_exact(size);
        for (std::size_t index = 0; index < size; ++index) {
            lower[index] = index == 0 ? 0.0 : -0.2 - 0.001 * index;
            variable_diagonal[index] = 2.0 + 0.01 * index;
            upper[index] = index + 1 == size ? 0.0 : -0.1 + 0.0005 * index;
            variable_exact[index] = std::sin(0.07 * static_cast<double>(index));
        }
        std::vector<double> variable_right_hand_side(size);
        for (std::size_t index = 0; index < size; ++index) {
            variable_right_hand_side[index] =
                variable_diagonal[index] * variable_exact[index];
            if (index > 0) {
                variable_right_hand_side[index] +=
                    lower[index] * variable_exact[index - 1];
            }
            if (index + 1 < size) {
                variable_right_hand_side[index] +=
                    upper[index] * variable_exact[index + 1];
            }
        }
        std::vector<double> variable_solution;
        require(smave::tridiagonal_direct_solve(
                    lower, variable_diagonal, upper,
                    variable_right_hand_side, variable_solution),
                "variable tridiagonal solve failed");
        double variable_error{};
        for (std::size_t index = 0; index < size; ++index) {
            variable_error = std::max(variable_error,
                std::abs(variable_solution[index] - variable_exact[index]));
        }
        require(variable_error <= 1.0e-13,
                "variable tridiagonal solve failed manufactured solution");

        constexpr double top_right = -0.15;
        constexpr double bottom_left = -0.08;
        auto cyclic_right_hand_side = variable_right_hand_side;
        cyclic_right_hand_side.front() += top_right * variable_exact.back();
        cyclic_right_hand_side.back() += bottom_left * variable_exact.front();
        std::vector<double> cyclic_solution;
        require(smave::cyclic_tridiagonal_direct_solve(
                    lower, variable_diagonal, upper, top_right, bottom_left,
                    cyclic_right_hand_side, cyclic_solution),
                "variable cyclic tridiagonal solve failed");
        double cyclic_error{};
        for (std::size_t index = 0; index < size; ++index) {
            cyclic_error = std::max(cyclic_error,
                std::abs(cyclic_solution[index] - variable_exact[index]));
        }
        require(cyclic_error <= 1.0e-13,
                "variable cyclic tridiagonal solve failed manufactured solution");

        smave::VariableTridiagonalWorkspace variable_workspace(size);
        std::vector<double> workspace_solution;
        require(variable_workspace.solve(
                    lower, variable_diagonal, upper,
                    variable_right_hand_side, workspace_solution) &&
                    variable_workspace.size() == size,
                "variable tridiagonal workspace solve failed");
        require(variable_workspace.solve_cyclic(
                    lower, variable_diagonal, upper, top_right, bottom_left,
                    cyclic_right_hand_side, workspace_solution),
                "variable cyclic workspace solve failed");
        require(variable_workspace.solve_cyclic_constant_diagonal(
                    lower, variable_diagonal.front(), upper,
                    top_right, bottom_left, cyclic_right_hand_side,
                    workspace_solution),
                "constant-diagonal cyclic workspace solve failed");
        constexpr double affine_diagonal = 4.0;
        constexpr double lower_offset = -0.3;
        constexpr double lower_state_scale = 0.02;
        constexpr double upper_offset = -0.2;
        constexpr double upper_state_scale = -0.01;
        constexpr std::size_t batch = 4;
        std::vector<double> affine_state(size);
        std::vector<double> affine_lower(size);
        std::vector<double> affine_upper(size);
        std::vector<double> affine_right(size);
        for (std::size_t index = 0; index < size; ++index) {
            affine_state[index] = 0.5 + static_cast<double>(index) * 0.25;
            affine_lower[index] =
                lower_offset + lower_state_scale * affine_state[index];
            affine_upper[index] =
                upper_offset + upper_state_scale * affine_state[index];
        }
        for (std::size_t index = 0; index < size; ++index) {
            const auto previous = index == 0 ? size - 1 : index - 1;
            const auto next = index + 1 == size ? 0 : index + 1;
            affine_right[index] = affine_diagonal * variable_exact[index] +
                affine_lower[index] * variable_exact[previous] +
                affine_upper[index] * variable_exact[next];
        }
        require(variable_workspace.
                    solve_cyclic_constant_diagonal_affine_off_diagonal(
                        affine_diagonal, lower_offset, lower_state_scale,
                        upper_offset, upper_state_scale, affine_state,
                        affine_right, workspace_solution),
                "affine off-diagonal cyclic workspace solve failed");
        double affine_error{};
        for (std::size_t index = 0; index < size; ++index) {
            affine_error = std::max(
                affine_error,
                std::abs(workspace_solution[index] - variable_exact[index]));
        }
        require(affine_error <= 1.0e-13,
                "affine off-diagonal cyclic manufactured solve failed");
        smave::BatchedVariableTridiagonalWorkspace affine_batched_workspace(
            size, batch);
        std::vector<double> affine_batched_state(size * batch);
        std::vector<double> affine_batched_right(size * batch);
        for (std::size_t index = 0; index < size; ++index) {
            for (std::size_t lane = 0; lane < batch; ++lane) {
                const auto position = index * batch + lane;
                affine_batched_state[position] = affine_state[index] +
                    0.1 * static_cast<double>(lane);
                const auto previous = index == 0 ? size - 1 : index - 1;
                const auto next = index + 1 == size ? 0 : index + 1;
                const auto lower_value = lower_offset + lower_state_scale *
                    affine_batched_state[position];
                const auto upper_value = upper_offset + upper_state_scale *
                    affine_batched_state[position];
                affine_batched_right[position] = affine_diagonal *
                    variable_exact[index] + lower_value * variable_exact[previous] +
                    upper_value * variable_exact[next];
            }
        }
        std::vector<double> affine_batched_solution;
        require(affine_batched_workspace.
                    solve_cyclic_constant_diagonal_affine_off_diagonal_interleaved(
                        affine_diagonal, lower_offset, lower_state_scale,
                        upper_offset, upper_state_scale, affine_batched_state,
                        affine_batched_right, affine_batched_solution),
                "batched affine off-diagonal cyclic solve failed");
        double affine_batched_error{};
        for (std::size_t index = 0; index < size; ++index) {
            for (std::size_t lane = 0; lane < batch; ++lane) {
                affine_batched_error = std::max(
                    affine_batched_error,
                    std::abs(affine_batched_solution[index * batch + lane] -
                             variable_exact[index]));
            }
        }
        require(affine_batched_error <= 1.0e-13,
                "batched affine off-diagonal cyclic manufactured solve failed");
        constexpr std::size_t neon_batch = 3;
        smave::BatchedVariableTridiagonalWorkspace affine_neon_workspace(
            size, neon_batch);
        std::vector<double> affine_neon_state(size * neon_batch);
        std::vector<double> affine_neon_right(size * neon_batch);
        for (std::size_t index = 0; index < size; ++index) {
            const auto previous = index == 0 ? size - 1 : index - 1;
            const auto next = index + 1 == size ? 0 : index + 1;
            for (std::size_t lane = 0; lane < neon_batch; ++lane) {
                const auto position = index * neon_batch + lane;
                affine_neon_state[position] = affine_state[index] +
                    0.1 * static_cast<double>(lane);
                const auto lower_value = lower_offset + lower_state_scale *
                    affine_neon_state[position];
                const auto upper_value = upper_offset + upper_state_scale *
                    affine_neon_state[position];
                affine_neon_right[position] = affine_diagonal *
                    variable_exact[index] + lower_value * variable_exact[previous] +
                    upper_value * variable_exact[next];
            }
        }
        std::vector<double> affine_neon_solution;
        require(affine_neon_workspace.
                    solve_cyclic_constant_diagonal_affine_off_diagonal_interleaved(
                        affine_diagonal, lower_offset, lower_state_scale,
                        upper_offset, upper_state_scale, affine_neon_state,
                        affine_neon_right, affine_neon_solution),
                "three-lane affine cyclic solve failed");
        double affine_neon_error{};
        for (std::size_t index = 0; index < size; ++index) {
            for (std::size_t lane = 0; lane < neon_batch; ++lane) {
                affine_neon_error = std::max(
                    affine_neon_error,
                    std::abs(affine_neon_solution[index * neon_batch + lane] -
                             variable_exact[index]));
            }
        }
        require(affine_neon_error <= 1.0e-13,
                "three-lane affine cyclic manufactured solve failed");
        std::vector<double> m_matrix_diagonal(size, 4.0);
        std::vector<double> m_matrix_lower(size, -1.0);
        std::vector<double> m_matrix_upper(size, -1.0);
        m_matrix_lower.front() = 0.0;
        m_matrix_upper.back() = 0.0;
        std::vector<double> m_matrix_right(size, 2.0);
        require(variable_workspace.solve_strictly_diagonally_dominant_m_matrix(
                    m_matrix_lower, m_matrix_diagonal, m_matrix_upper,
                    m_matrix_right, workspace_solution),
                "strictly diagonally dominant M-matrix workspace solve failed");
        require(variable_workspace.
                    solve_strictly_diagonally_dominant_m_matrix_constant_off_diagonal(
                        -1.0, m_matrix_diagonal, -1.0,
                        m_matrix_right, workspace_solution),
                "constant off-diagonal M-matrix workspace solve failed");
        require(!variable_workspace.
                    solve_strictly_diagonally_dominant_m_matrix_constant_off_diagonal(
                        1.0, m_matrix_diagonal, -1.0,
                        m_matrix_right, workspace_solution),
                "positive lower off-diagonal was not rejected");

        smave::BatchedVariableTridiagonalWorkspace batched_workspace(size, batch);
        std::vector<double> batched_lower(size * batch);
        std::vector<double> batched_diagonal(size * batch);
        std::vector<double> batched_upper(size * batch);
        std::vector<double> batched_right(size * batch);
        for (std::size_t index = 0; index < size; ++index) {
            for (std::size_t lane = 0; lane < batch; ++lane) {
                const auto position = index * batch + lane;
                batched_lower[position] = index > 0 ? -1.0 : 0.0;
                batched_diagonal[position] = 4.0 + 0.25 * lane;
                batched_upper[position] = index + 1 < size ? -1.0 : 0.0;
                batched_right[position] = 1.0 + index + 0.5 * lane;
            }
        }
        std::vector<double> batched_solution;
        require(batched_workspace.
                    solve_strictly_diagonally_dominant_m_matrix_interleaved(
                        batched_lower, batched_diagonal, batched_upper,
                        batched_right, batched_solution),
                "batched M-matrix workspace solve failed");
        double batched_residual{};
        for (std::size_t index = 0; index < size; ++index) {
            for (std::size_t lane = 0; lane < batch; ++lane) {
                const auto position = index * batch + lane;
                auto product = batched_diagonal[position] *
                    batched_solution[position];
                if (index > 0) {
                    product += batched_lower[position] *
                        batched_solution[position - batch];
                }
                if (index + 1 < size) {
                    product += batched_upper[position] *
                        batched_solution[position + batch];
                }
                batched_residual = std::max(
                    batched_residual,
                    std::abs(product - batched_right[position]));
            }
        }
        require(batched_residual <= 1.0e-13,
                "batched M-matrix solve failed original residual gate");
        std::vector<double> batched_constant_solution;
        require(batched_workspace.
                    solve_strictly_diagonally_dominant_m_matrix_constant_off_diagonal_interleaved(
                        -1.0, batched_diagonal, -1.0,
                        batched_right, batched_constant_solution),
                "batched constant off-diagonal M-matrix solve failed");
        double batched_constant_residual{};
        for (std::size_t index = 0; index < size; ++index) {
            for (std::size_t lane = 0; lane < batch; ++lane) {
                const auto position = index * batch + lane;
                auto product = batched_diagonal[position] *
                    batched_constant_solution[position];
                if (index > 0) {
                    product -= batched_constant_solution[position - batch];
                }
                if (index + 1 < size) {
                    product -= batched_constant_solution[position + batch];
                }
                batched_constant_residual = std::max(
                    batched_constant_residual,
                    std::abs(product - batched_right[position]));
            }
        }
        require(batched_constant_residual <= 1.0e-13,
                "batched constant off-diagonal solve failed residual gate");

        constexpr std::size_t five_point_width = 10;
        constexpr std::size_t five_point_batch = 3;
        const auto five_point_points = five_point_width * five_point_width;
        const auto five_point_values = five_point_points * five_point_batch;
        std::vector<double> five_point_west(five_point_values);
        std::vector<double> five_point_east(five_point_values);
        std::vector<double> five_point_south(five_point_values);
        std::vector<double> five_point_north(five_point_values);
        std::vector<double> five_point_diagonal(five_point_values);
        std::vector<double> five_point_exact(five_point_values);
        std::vector<double> five_point_right(five_point_values);
        for (std::size_t row = 0; row < five_point_width; ++row) {
            for (std::size_t column = 0; column < five_point_width; ++column) {
                const auto point = row * five_point_width + column;
                for (std::size_t lane = 0; lane < five_point_batch; ++lane) {
                    const auto position = point * five_point_batch + lane;
                    const auto horizontal =
                        0.65 + 0.03 * static_cast<double>(column + lane);
                    const auto vertical =
                        0.75 + 0.02 * static_cast<double>(row + 2 * lane);
                    five_point_west[position] = column > 0 ? horizontal : 0.0;
                    five_point_east[position] =
                        column + 1 < five_point_width ? horizontal : 0.0;
                    five_point_south[position] = row > 0 ? vertical : 0.0;
                    five_point_north[position] =
                        row + 1 < five_point_width ? vertical : 0.0;
                    five_point_diagonal[position] = 1.0 +
                        five_point_west[position] + five_point_east[position] +
                        five_point_south[position] + five_point_north[position];
                    five_point_exact[position] =
                        std::sin(0.13 * static_cast<double>(point + lane)) +
                        0.2 * static_cast<double>(lane);
                }
            }
        }
        for (std::size_t row = 0; row < five_point_width; ++row) {
            for (std::size_t column = 0; column < five_point_width; ++column) {
                const auto point = row * five_point_width + column;
                for (std::size_t lane = 0; lane < five_point_batch; ++lane) {
                    const auto position = point * five_point_batch + lane;
                    auto product = five_point_diagonal[position] *
                        five_point_exact[position];
                    if (column > 0) product -= five_point_west[position] *
                        five_point_exact[(point - 1) * five_point_batch + lane];
                    if (column + 1 < five_point_width) {
                        product -= five_point_east[position] *
                            five_point_exact[(point + 1) * five_point_batch + lane];
                    }
                    if (row > 0) product -= five_point_south[position] *
                        five_point_exact[(point - five_point_width) *
                            five_point_batch + lane];
                    if (row + 1 < five_point_width) {
                        product -= five_point_north[position] *
                            five_point_exact[(point + five_point_width) *
                                five_point_batch + lane];
                    }
                    five_point_right[position] = product;
                }
            }
        }
        smave::BatchedFivePointSsorPcgWorkspace five_point_workspace(
            five_point_width, five_point_batch);
        const auto five_point_result = five_point_workspace.solve_interleaved(
            five_point_west, five_point_east, five_point_south, five_point_north,
            five_point_diagonal, five_point_right,
            1.4, 1.0e-12, 1.0e-10, 200);
        require(five_point_result.valid &&
                    std::all_of(five_point_result.converged.begin(),
                                five_point_result.converged.end(),
                                [](bool value) { return value; }),
                "batched five-point SSOR-PCG failed manufactured lanes");
        double five_point_error{};
        for (std::size_t position = 0; position < five_point_values; ++position) {
            five_point_error = std::max(
                five_point_error,
                std::abs(five_point_result.solution[position] -
                         five_point_exact[position]));
        }
        require(five_point_error <= 2.0e-9 &&
                    *std::max_element(five_point_result.residual_inf.begin(),
                                      five_point_result.residual_inf.end()) <= 1.0e-9,
                "batched five-point SSOR-PCG failed exact or true residual gate");

        smave::LinearSystem spd;
        spd.unknowns = {"x0", "x1", "x2", "x3"};
        spd.matrix = {
            {4.0, -1.0, 0.0, 0.0},
            {-1.0, 4.0, -1.0, 0.0},
            {0.0, -1.0, 4.0, -1.0},
            {0.0, 0.0, -1.0, 3.0},
        };
        spd.right_hand_side = {1.0, 2.0, 3.0, 4.0};
        smave::classify_linear_system(spd);
        const auto ssor = smave::symmetric_gauss_seidel_preconditioner(spd, 1.0);
        require(static_cast<bool>(ssor), "valid SSOR preconditioner was rejected");
        const auto pcg = smave::preconditioned_conjugate_gradient(
            spd, std::vector<double>(4), ssor, 1.0e-13, 1.0e-13, 20);
        require(pcg.converged && !pcg.breakdown && pcg.iterations <= 4,
                "SSOR-PCG failed the SPD manufactured system");
        const auto sparse_cholesky =
            smave::accelerate_sparse_spd_direct_solve(spd);
        if (sparse_cholesky.available) {
            require(sparse_cholesky.solved &&
                        sparse_cholesky.residual_inf <= 1.0e-12,
                    "Accelerate sparse Cholesky failed the SPD manufactured system");
        } else {
            require(!sparse_cholesky.solved &&
                        sparse_cholesky.backend == "unavailable" &&
                        !sparse_cholesky.reason.empty() &&
                        sparse_cholesky.solution.empty(),
                    "unavailable Accelerate sparse Cholesky did not fail closed");
        }
        const auto band_cholesky =
            smave::accelerate_spd_band_direct_solve(spd, 1);
        if (band_cholesky.available) {
            require(band_cholesky.solved &&
                        band_cholesky.residual_inf <= 1.0e-12,
                    "Accelerate band Cholesky failed the SPD manufactured system");
        } else {
            require(!band_cholesky.solved &&
                        band_cholesky.backend == "unavailable" &&
                        !band_cholesky.reason.empty() &&
                        band_cholesky.solution.empty(),
                    "unavailable Accelerate band Cholesky did not fail closed");
        }
        constexpr std::size_t stencil_width = 3;
        constexpr std::size_t stencil_size = stencil_width * stencil_width;
        std::vector<double> stencil_west(stencil_size, 1.0);
        std::vector<double> stencil_east(stencil_size, 1.0);
        std::vector<double> stencil_south(stencil_size, 1.0);
        std::vector<double> stencil_north(stencil_size, 1.0);
        std::vector<double> stencil_diagonal(stencil_size, 6.0);
        std::vector<double> stencil_exact(stencil_size);
        for (std::size_t index = 0; index < stencil_size; ++index) {
            stencil_exact[index] = 0.25 + static_cast<double>(index);
        }
        std::vector<double> stencil_right(stencil_size);
        for (std::size_t row = 0; row < stencil_width; ++row) {
            for (std::size_t column = 0; column < stencil_width; ++column) {
                const auto index = row * stencil_width + column;
                auto value = stencil_diagonal[index] * stencil_exact[index];
                if (column > 0) value -= stencil_west[index] * stencil_exact[index - 1];
                if (column + 1 < stencil_width) {
                    value -= stencil_east[index] * stencil_exact[index + 1];
                }
                if (row > 0) {
                    value -= stencil_south[index] * stencil_exact[index - stencil_width];
                }
                if (row + 1 < stencil_width) {
                    value -= stencil_north[index] * stencil_exact[index + stencil_width];
                }
                stencil_right[index] = value;
            }
        }
        const auto five_point_cholesky =
            smave::accelerate_five_point_spd_direct_solve(
                stencil_width, stencil_west, stencil_east, stencil_south,
                stencil_north, stencil_diagonal, stencil_right);
        double five_point_cholesky_error{};
        if (five_point_cholesky.solution.size() == stencil_exact.size()) {
            for (std::size_t index = 0; index < stencil_size; ++index) {
                five_point_cholesky_error = std::max(
                    five_point_cholesky_error,
                    std::abs(five_point_cholesky.solution[index] -
                             stencil_exact[index]));
            }
        } else {
            five_point_cholesky_error =
                std::numeric_limits<double>::infinity();
        }
        if (five_point_cholesky.available) {
            require(five_point_cholesky.solved &&
                        five_point_cholesky.residual_inf <= 1.0e-12 &&
                        five_point_cholesky_error <= 1.0e-12,
                    "Accelerate five-point Cholesky failed manufactured system");
        } else {
            require(!five_point_cholesky.solved &&
                        five_point_cholesky.backend == "unavailable" &&
                        !five_point_cholesky.reason.empty() &&
                        five_point_cholesky.solution.empty(),
                    "unavailable Accelerate five-point Cholesky did not fail closed");
        }

        constexpr double bidiagonal_diagonal = 1.7;
        constexpr double bidiagonal_lower = -0.7;
        smave::PeriodicLowerBidiagonalFactorization bidiagonal(
            size, bidiagonal_diagonal, bidiagonal_lower);
        require(bidiagonal.valid(), "periodic bidiagonal factorization failed");
        std::vector<double> bidiagonal_solution;
        require(bidiagonal.solve(first, bidiagonal_solution),
                "periodic bidiagonal solve failed");
        double bidiagonal_residual{};
        for (std::size_t index = 0; index < size; ++index) {
            const auto previous = index == 0 ? size - 1 : index - 1;
            bidiagonal_residual = std::max(bidiagonal_residual, std::abs(
                bidiagonal_diagonal * bidiagonal_solution[index] +
                bidiagonal_lower * bidiagonal_solution[previous] - first[index]));
        }
        require(bidiagonal_residual <= 1.0e-13,
                "periodic bidiagonal solve failed original residual gate");
        std::vector<double> bidiagonal_batch(size * batch);
        for (std::size_t index = 0; index < size; ++index) {
            for (std::size_t lane = 0; lane < batch; ++lane) {
                bidiagonal_batch[index * batch + lane] = first[index] + lane;
            }
        }
        std::vector<double> bidiagonal_batch_solution;
        require(bidiagonal.solve_interleaved(
                    bidiagonal_batch, batch, bidiagonal_batch_solution),
                "periodic bidiagonal batch solve failed");
        double bidiagonal_batch_residual{};
        for (std::size_t index = 0; index < size; ++index) {
            const auto previous = index == 0 ? size - 1 : index - 1;
            for (std::size_t lane = 0; lane < batch; ++lane) {
                const auto position = index * batch + lane;
                bidiagonal_batch_residual = std::max(
                    bidiagonal_batch_residual,
                    std::abs(bidiagonal_diagonal *
                                 bidiagonal_batch_solution[position] +
                             bidiagonal_lower * bidiagonal_batch_solution[
                                 previous * batch + lane] -
                             bidiagonal_batch[position]));
            }
        }
        require(bidiagonal_batch_residual <= 1.0e-13,
                "periodic bidiagonal batch failed original residual gate");
        std::vector<double> bidiagonal_relative_residuals;
        require(smave::periodic_lower_bidiagonal_relative_residual_interleaved(
                    bidiagonal_batch, bidiagonal_batch_solution, batch,
                    bidiagonal_diagonal, bidiagonal_lower,
                    bidiagonal_relative_residuals) &&
                    *std::max_element(bidiagonal_relative_residuals.begin(),
                                      bidiagonal_relative_residuals.end()) <= 1.0e-13,
                "periodic bidiagonal vectorized residual gate failed");
        std::vector<double> burgers_relative_residuals;
        constexpr double burgers_diffusion = 0.15;
        constexpr double burgers_convection = 0.025;
        require(smave::frozen_burgers_relative_residual_interleaved(
                    bidiagonal_batch, bidiagonal_batch_solution, batch,
                    burgers_diffusion, burgers_convection,
                    burgers_relative_residuals),
                "vectorized frozen Burgers residual gate failed");
        std::vector<double> burgers_reference_residuals(batch);
        for (std::size_t lane = 0; lane < batch; ++lane) {
            double residual_squared{};
            double state_squared{};
            for (std::size_t index = 0; index < size; ++index) {
                const auto previous = index == 0 ? size - 1 : index - 1;
                const auto next = index + 1 == size ? 0 : index + 1;
                const auto position = index * batch + lane;
                const auto state = bidiagonal_batch[position];
                const auto convection = burgers_convection * state;
                const auto product =
                    (1.0 + 2.0 * burgers_diffusion) *
                        bidiagonal_batch_solution[position] +
                    (-burgers_diffusion - convection) *
                        bidiagonal_batch_solution[previous * batch + lane] +
                    (-burgers_diffusion + convection) *
                        bidiagonal_batch_solution[next * batch + lane];
                const auto residual = state - product;
                residual_squared += residual * residual;
                state_squared += state * state;
            }
            burgers_reference_residuals[lane] = std::sqrt(residual_squared) /
                std::max(1.0, std::sqrt(state_squared));
            require(std::abs(burgers_relative_residuals[lane] -
                             burgers_reference_residuals[lane]) <= 1.0e-14,
                    "vectorized Burgers residual disagreed with scalar gate");
        }

        constexpr std::size_t multigrid_width = 8;
        const auto multigrid_size = multigrid_width * multigrid_width;
        smave::LinearSystem poisson;
        poisson.unknowns.resize(multigrid_size);
        poisson.matrix.assign(
            multigrid_size, std::vector<double>(multigrid_size));
        poisson.right_hand_side.assign(multigrid_size, 1.0);
        for (std::size_t row = 0; row < multigrid_width; ++row) {
            for (std::size_t column = 0; column < multigrid_width; ++column) {
                const auto index = row * multigrid_width + column;
                poisson.matrix[index][index] = 4.0;
                if (column > 0) poisson.matrix[index][index - 1] = -1.0;
                if (column + 1 < multigrid_width) {
                    poisson.matrix[index][index + 1] = -1.0;
                }
                if (row > 0) {
                    poisson.matrix[index][index - multigrid_width] = -1.0;
                }
                if (row + 1 < multigrid_width) {
                    poisson.matrix[index][index + multigrid_width] = -1.0;
                }
            }
        }
        smave::classify_linear_system(poisson);
        smave::AggregationMultigrid2D multigrid(
            poisson, multigrid_width, 4, 2, 2, 2.0 / 3.0);
        require(multigrid.valid() && multigrid.levels() >= 3 &&
                    multigrid.storage_bytes() > 0,
                "aggregation multigrid hierarchy construction failed");
        const smave::Preconditioner multigrid_preconditioner =
            [&multigrid](const std::vector<double>& residual,
                         std::vector<double>& correction) {
                return multigrid.apply(residual, correction);
            };
        const auto multigrid_pcg = smave::preconditioned_conjugate_gradient(
            poisson, std::vector<double>(multigrid_size),
            multigrid_preconditioner, 1.0e-11, 1.0e-11, 100);
        require(multigrid_pcg.converged && !multigrid_pcg.breakdown &&
                    multigrid_pcg.iterations < 30,
                "aggregation multigrid PCG failed Poisson manufactured system");
        std::vector<double> geometric_west(multigrid_size);
        std::vector<double> geometric_east(multigrid_size);
        std::vector<double> geometric_south(multigrid_size);
        std::vector<double> geometric_north(multigrid_size);
        std::vector<double> geometric_diagonal(multigrid_size, 4.0);
        for (std::size_t row = 0; row < multigrid_width; ++row) {
            for (std::size_t column = 0; column < multigrid_width; ++column) {
                const auto index = row * multigrid_width + column;
                geometric_west[index] = column > 0 ? 1.0 : 0.0;
                geometric_east[index] =
                    column + 1 < multigrid_width ? 1.0 : 0.0;
                geometric_south[index] = row > 0 ? 1.0 : 0.0;
                geometric_north[index] =
                    row + 1 < multigrid_width ? 1.0 : 0.0;
            }
        }
        smave::GeometricFivePointMultigrid2D geometric_multigrid(
            multigrid_width, geometric_west, geometric_east,
            geometric_south, geometric_north, geometric_diagonal,
            4, 2, 2, 2.0 / 3.0);
        require(geometric_multigrid.valid() &&
                    geometric_multigrid.levels() >= 3 &&
                    geometric_multigrid.storage_bytes() > 0,
                "geometric five-point multigrid hierarchy construction failed");
        const smave::Preconditioner geometric_multigrid_preconditioner =
            [&geometric_multigrid](const std::vector<double>& residual,
                                   std::vector<double>& correction) {
                return geometric_multigrid.apply(residual, correction);
            };
        const auto geometric_multigrid_pcg =
            smave::preconditioned_conjugate_gradient(
                poisson, std::vector<double>(multigrid_size),
                geometric_multigrid_preconditioner,
                1.0e-11, 1.0e-11, 100);
        require(geometric_multigrid_pcg.converged &&
                    !geometric_multigrid_pcg.breakdown &&
                    geometric_multigrid_pcg.iterations < 30,
                "geometric five-point multigrid PCG failed Poisson system");
        std::cout << "periodic linear unit passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "periodic linear unit failure: " << error.what() << '\n';
        return 1;
    }
}
