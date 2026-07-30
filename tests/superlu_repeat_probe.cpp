#include "smave/benchmark/sparse_suite.hpp"
#include "smave/linear.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

int main(int argc, char** argv) {
    try {
        if (argc != 3 && argc != 4) {
            throw std::invalid_argument(
                "usage: superlu-repeat-probe MATRIX_PATH REPETITIONS "
                "[superlu|qr-superlu|superlu-qr]");
        }
        const auto matrix = smave::benchmark::read_matrix_market(argv[1]);
        smave::LinearSystem system;
        system.sparsity.row_count = matrix.rows;
        system.sparsity.column_count = matrix.columns;
        system.sparsity.row_offsets = matrix.row_offsets;
        system.sparsity.column_indices = matrix.column_indices;
        system.sparse_values = matrix.values;
        system.right_hand_side = matrix.multiply(
            smave::benchmark::deterministic_reference_solution(matrix.columns));
        smave::classify_linear_system(system);
        const int repetitions = std::stoi(argv[2]);
        const std::string sequence = argc == 4 ? argv[3] : "superlu";
        if (sequence != "superlu" && sequence != "qr-superlu" &&
            sequence != "superlu-qr") {
            throw std::invalid_argument("unknown sparse backend sequence");
        }
        for (int repetition = 0; repetition < repetitions; ++repetition) {
            std::cout << "begin=" << repetition << std::endl;
            if (sequence == "qr-superlu") {
                const auto qr = smave::industrial_sparse_direct_solve(system);
                std::cout << "qr-before=" << repetition
                          << " solved=" << (qr.solved ? 1 : 0)
                          << " residual=" << qr.residual_inf
                          << " reason=" << qr.reason << std::endl;
            }
            const auto result = smave::superlu_sparse_direct_solve(system);
            std::cout << "end=" << repetition
                      << " available=" << (result.available ? 1 : 0)
                      << " solved=" << (result.solved ? 1 : 0)
                      << " residual=" << result.residual_inf
                      << " reason=" << result.reason << std::endl;
            if (sequence == "superlu-qr") {
                const auto qr = smave::industrial_sparse_direct_solve(system);
                std::cout << "qr-after=" << repetition
                          << " solved=" << (qr.solved ? 1 : 0)
                          << " residual=" << qr.residual_inf
                          << " reason=" << qr.reason << std::endl;
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
