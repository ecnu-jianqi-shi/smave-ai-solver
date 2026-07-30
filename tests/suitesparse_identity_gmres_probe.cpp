#include "smave/benchmark/sparse_suite.hpp"
#include "smave/linear.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::uint64_t splitmix64(std::uint64_t value) {
    value += UINT64_C(0x9e3779b97f4a7c15);
    value = (value ^ (value >> 30U)) * UINT64_C(0xbf58476d1ce4e5b9);
    value = (value ^ (value >> 27U)) * UINT64_C(0x94d049bb133111eb);
    return value ^ (value >> 31U);
}

std::vector<double> reference_solution(
    std::size_t size,
    const std::string& kind,
    std::uint64_t seed) {
    std::vector<double> result(size);
    if (kind == "smooth") {
        for (std::size_t index = 0; index < size; ++index) {
            const double phase = 2.0 * std::acos(-1.0) *
                static_cast<double>(index + 1) / static_cast<double>(size + 1);
            result[index] = 1.0 + 0.25 * std::sin(phase);
        }
    } else if (kind == "oscillatory") {
        for (std::size_t index = 0; index < size; ++index) {
            const double phase = 18.0 * std::acos(-1.0) *
                static_cast<double>(index + 1) / static_cast<double>(size + 1);
            const double sign = index % 2 == 0 ? 1.0 : -1.0;
            result[index] = sign * (0.75 + 0.25 * std::sin(phase));
        }
    } else if (kind == "sparse") {
        const std::size_t stride = std::max<std::size_t>(3, size / 23);
        for (std::size_t index = 0; index < size; index += stride) {
            result[index] = 0.5 + static_cast<double>((index / stride) % 7) / 7.0;
        }
    } else if (kind == "random-like") {
        for (std::size_t index = 0; index < size; ++index) {
            const auto bits = splitmix64(seed ^ static_cast<std::uint64_t>(index));
            const double unit = static_cast<double>(bits >> 11U) /
                static_cast<double>(UINT64_C(1) << 53U);
            result[index] = 2.0 * unit - 1.0;
        }
    } else {
        throw std::invalid_argument("unknown request kind");
    }
    return result;
}

smave::LinearSystem make_system(
    const smave::benchmark::SparseMatrix& matrix,
    std::vector<double> right_hand_side) {
    smave::LinearSystem system;
    system.sparsity.row_count = matrix.rows;
    system.sparsity.column_count = matrix.columns;
    system.sparsity.row_offsets = matrix.row_offsets;
    system.sparsity.column_indices = matrix.column_indices;
    system.sparse_values = matrix.values;
    system.right_hand_side = std::move(right_hand_side);
    smave::classify_linear_system(system);
    return system;
}

double relative_residual(
    const smave::LinearSystem& system,
    const std::vector<double>& solution) {
    const auto product = system.multiply(solution);
    double residual_inf{};
    double matrix_inf{};
    double solution_inf{};
    double right_inf{};
    for (std::size_t row = 0; row < system.size(); ++row) {
        residual_inf = std::max(
            residual_inf, std::abs(product[row] - system.right_hand_side[row]));
        solution_inf = std::max(solution_inf, std::abs(solution[row]));
        right_inf = std::max(right_inf, std::abs(system.right_hand_side[row]));
        double row_sum{};
        for (std::size_t offset = system.sparsity.row_offsets[row];
             offset < system.sparsity.row_offsets[row + 1]; ++offset) {
            row_sum += std::abs(system.sparse_values[offset]);
        }
        matrix_inf = std::max(matrix_inf, row_sum);
    }
    return residual_inf /
        std::max(std::numeric_limits<double>::min(),
                 right_inf + matrix_inf * solution_inf);
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2) {
            throw std::invalid_argument(
                "usage: suitesparse-identity-gmres-probe MATRIX_PATH");
        }
        const auto matrix = smave::benchmark::read_matrix_market(argv[1]);
        const smave::Preconditioner identity = [](
            const std::vector<double>& residual,
            std::vector<double>& result) {
            result = residual;
            return std::all_of(result.begin(), result.end(), [](double value) {
                return std::isfinite(value);
            });
        };
        std::uint64_t seed = UINT64_C(0x5a17e5d3c92b4f81) + 14U * 8U;
        std::cout << std::setprecision(17)
                  << "kind\ttolerance\tconverged\tbreakdown\tstagnated"
                  << "\titerations\tbackward_error\treason\n";
        for (const std::string kind : {
                 "smooth", "oscillatory", "sparse", "random-like"}) {
            for (const double tolerance : {1.0e-6, 1.0e-10}) {
                const auto expected = reference_solution(matrix.columns, kind, seed++);
                const auto system = make_system(matrix, matrix.multiply(expected));
                const auto result = smave::restarted_gmres(
                    system, std::vector<double>(system.size()), identity,
                    1.0e-12, tolerance, 250, 40);
                const double error = result.solution.size() == system.size()
                    ? relative_residual(system, result.solution)
                    : std::numeric_limits<double>::infinity();
                std::cout << kind << '\t' << tolerance << '\t'
                          << (result.converged ? 1 : 0) << '\t'
                          << (result.breakdown ? 1 : 0) << '\t'
                          << (result.stagnated ? 1 : 0) << '\t'
                          << result.iterations << '\t' << error << '\t'
                          << result.reason << '\n';
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
