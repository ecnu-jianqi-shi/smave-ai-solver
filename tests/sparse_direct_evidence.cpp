#include "smave/linear.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: smave_sparse_direct_evidence OUTPUT\n";
        return 2;
    }
    constexpr double scale = 1.0e-16;
    smave::LinearSystem system;
    system.unknowns = {"x1", "x2", "x3", "x4", "x5"};
    system.matrix = {
        {1 * scale, 2 * scale, 0, 0, 0},
        {1 * scale, 0, 3 * scale, 0, 0},
        {1 * scale, 0, 0, 4 * scale, 0},
        {1 * scale, 0, 0, 0, 5 * scale},
        {0, 6 * scale, 7 * scale, 0, 0},
    };
    system.right_hand_side = {
        3 * scale, 4 * scale, 5 * scale, 6 * scale, 13 * scale};
    const auto result = smave::sparse_ordered_threshold_pivot_solve(system);
    const auto industrial = smave::industrial_sparse_direct_solve(system);
    double maximum_error = 0.0;
    for (const double value : result.solution) {
        maximum_error = std::max(maximum_error, std::abs(value - 1.0));
    }
    const bool nonidentity = result.column_order !=
        std::vector<std::size_t>({0, 1, 2, 3, 4});
    auto singular = system;
    singular.matrix.back() = singular.matrix.front();
    singular.right_hand_side.back() = singular.right_hand_side.front();
    const bool singular_rejected =
        !smave::sparse_ordered_threshold_pivot_solve(singular).solved;
    const auto industrial_singular = smave::industrial_sparse_direct_solve(singular);
    const bool invalid_threshold_rejected =
        !smave::sparse_ordered_threshold_pivot_solve(system, 0.0).solved &&
        !smave::sparse_ordered_threshold_pivot_solve(system, 1.1).solved;
    const bool passed = result.solved && maximum_error <= 1.0e-12 && nonidentity &&
        result.ordering_fill_edges < result.natural_fill_edges &&
        result.minimum_scaled_pivot > 0.0 && singular_rejected &&
        invalid_threshold_rejected &&
        (!smave::industrial_sparse_direct_available() ||
         (industrial.solved && industrial.residual_inf <= 1.0e-25 &&
          !industrial_singular.solved));
    std::ofstream output(argv[1]);
    if (!output) throw std::runtime_error("failed to open sparse evidence output");
    output << std::setprecision(17)
           << "SMAVE_SPARSE_DIRECT_EVIDENCE 1\n"
           << "SUCCESS " << passed << '\n'
           << "SCALE " << scale << '\n'
           << "MAXIMUM_SOLUTION_ERROR " << maximum_error << '\n'
           << "INITIAL_NONZEROS " << result.initial_nonzeros << '\n'
           << "UPPER_NONZEROS " << result.upper_nonzeros << '\n'
           << "ORDERING_FILL_EDGES " << result.ordering_fill_edges << '\n'
           << "NATURAL_FILL_EDGES " << result.natural_fill_edges << '\n'
           << "ROW_SWAPS " << result.row_swaps << '\n'
           << "MINIMUM_SCALED_PIVOT " << result.minimum_scaled_pivot << '\n'
           << "SINGULAR_REJECTED " << singular_rejected << '\n'
           << "INVALID_THRESHOLD_REJECTED " << invalid_threshold_rejected << '\n'
           << "INDUSTRIAL_AVAILABLE " << industrial.available << '\n'
           << "INDUSTRIAL_BACKEND " << std::quoted(industrial.backend) << '\n'
           << "INDUSTRIAL_SOLVED " << industrial.solved << '\n'
           << "INDUSTRIAL_RESIDUAL " << industrial.residual_inf << '\n'
           << "INDUSTRIAL_REASON " << std::quoted(industrial.reason) << '\n'
           << "INDUSTRIAL_SINGULAR_REJECTED " << !industrial_singular.solved << '\n'
           << "COLUMN_ORDER";
    for (const auto column : result.column_order) output << ' ' << column;
    output << '\n';
    return passed ? 0 : 1;
}
