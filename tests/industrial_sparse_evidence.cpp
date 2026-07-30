#include "smave/linear.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

double infinity_norm(const std::vector<double>& values) {
    double result{};
    for (const auto value : values) result = std::max(result, std::abs(value));
    return result;
}

smave::LinearSystem read_matrix_market(const std::filesystem::path& path) {
    std::ifstream input(path);
    require(static_cast<bool>(input), "failed to open " + path.string());
    std::string line;
    std::getline(input, line);
    require(line == "%%MatrixMarket matrix coordinate real general",
            "unsupported Matrix Market header in " + path.string());
    do {
        require(static_cast<bool>(std::getline(input, line)),
                "missing Matrix Market dimensions");
    } while (line.empty() || line.front() == '%');
    std::size_t rows{};
    std::size_t columns{};
    std::size_t entries{};
    {
        std::istringstream dimensions(line);
        dimensions >> rows >> columns >> entries;
        require(dimensions && rows == columns && rows > 0,
                "industrial evidence requires a nonempty square matrix");
    }
    std::vector<std::map<std::size_t, double>> matrix_rows(rows);
    for (std::size_t entry = 0; entry < entries; ++entry) {
        std::size_t row{};
        std::size_t column{};
        double value{};
        require(static_cast<bool>(input >> row >> column >> value),
                "invalid Matrix Market coordinate entry");
        require(row > 0 && row <= rows && column > 0 && column <= columns &&
                    std::isfinite(value),
                "out-of-range or non-finite Matrix Market entry");
        matrix_rows[row - 1][column - 1] += value;
    }
    smave::LinearSystem system;
    system.unknowns.resize(rows);
    system.sparsity.row_count = rows;
    system.sparsity.column_count = columns;
    system.sparsity.row_offsets.reserve(rows + 1);
    system.sparsity.row_offsets.push_back(0);
    for (std::size_t row = 0; row < rows; ++row) {
        system.unknowns[row] = "x" + std::to_string(row + 1);
        for (const auto& [column, value] : matrix_rows[row]) {
            if (value == 0.0) continue;
            system.sparsity.column_indices.push_back(column);
            system.sparse_values.push_back(value);
        }
        system.sparsity.row_offsets.push_back(system.sparse_values.size());
    }
    system.sparsity.validate();
    return system;
}

struct MatrixEvidence {
    std::string name;
    std::size_t dimension{};
    std::size_t nonzeros{};
    double relative_solution_error{};
    double relative_residual{};
};

MatrixEvidence solve_matrix(const std::filesystem::path& path) {
    auto system = read_matrix_market(path);
    std::vector<double> expected(system.size());
    for (std::size_t index = 0; index < expected.size(); ++index) {
        expected[index] = 1.0 +
            static_cast<double>((index * 17U + 3U) % 13U) / 13.0;
    }
    system.right_hand_side = system.multiply(expected);
    const auto result = smave::industrial_sparse_direct_solve(system);
    require(result.available && result.backend == "accelerate-sparse-qr-cpu-v1",
            "expected Apple Accelerate sparse QR backend");
    require(result.solved && result.solution.size() == expected.size(),
            path.filename().string() + ": " + result.reason);
    std::vector<double> error(expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index) {
        error[index] = result.solution[index] - expected[index];
    }
    const auto product = system.multiply(result.solution);
    std::vector<double> residual(expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index) {
        residual[index] = product[index] - system.right_hand_side[index];
    }
    const double solution_error = infinity_norm(error) /
        std::max(1.0, infinity_norm(expected));
    const double residual_error = infinity_norm(residual) /
        std::max(1.0, infinity_norm(system.right_hand_side));
    require(solution_error <= 1.0e-8 && residual_error <= 1.0e-10,
            path.filename().string() + ": industrial solution accuracy gate failed");
    return {
        path.parent_path().filename().string(), system.size(), system.nonzeros(),
        solution_error, residual_error};
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 3) {
            std::cerr << "usage: smave_industrial_sparse_evidence SUITESPARSE_ROOT OUTPUT\n";
            return 2;
        }
        const std::filesystem::path root(argv[1]);
        const auto west = solve_matrix(root / "small/west0479/west0479.mtx");
        const auto waveguide = solve_matrix(root / "large/dw1024/dw1024.mtx");

        auto singular = read_matrix_market(root / "small/west0479/west0479.mtx");
        singular.right_hand_side.assign(singular.size(), 0.0);
        const auto row_begin = singular.sparsity.row_offsets.back() -
            (singular.sparsity.row_offsets.back() -
             singular.sparsity.row_offsets[singular.size() - 1]);
        for (std::size_t offset = row_begin;
             offset < singular.sparsity.row_offsets.back(); ++offset) {
            singular.sparse_values[offset] = 0.0;
        }
        const auto singular_result = smave::industrial_sparse_direct_solve(singular);
        require(!singular_result.solved,
                "rank-deficient modified SuiteSparse matrix was accepted");

        std::ofstream output(argv[2]);
        require(static_cast<bool>(output), "failed to open industrial evidence output");
        output << std::setprecision(17)
               << "SMAVE_INDUSTRIAL_SPARSE_EVIDENCE 1\n"
               << "SUCCESS 1\n"
               << "BACKEND \"" << smave::industrial_sparse_direct_backend() << "\"\n";
        for (const auto& evidence : {west, waveguide}) {
            output << "MATRIX \"" << evidence.name << "\" DIMENSION "
                   << evidence.dimension << " NONZEROS " << evidence.nonzeros
                   << " RELATIVE_SOLUTION_ERROR " << evidence.relative_solution_error
                   << " RELATIVE_RESIDUAL " << evidence.relative_residual << '\n';
        }
        output << "RANK_DEFICIENT_REJECTED 1\n"
               << "RANK_DEFICIENT_REASON \"" << singular_result.reason << "\"\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
