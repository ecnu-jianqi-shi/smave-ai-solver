#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace smave::benchmark {

struct SparseMatrix {
    std::size_t rows{};
    std::size_t columns{};
    std::vector<std::size_t> row_offsets;
    std::vector<std::size_t> column_indices;
    std::vector<double> values;
    bool declared_symmetric{false};
    bool pattern{false};

    [[nodiscard]] std::size_t nonzeros() const;
    [[nodiscard]] std::vector<double> multiply(
        const std::vector<double>& input) const;
};

struct SparseCase {
    std::string name;
    std::filesystem::path matrix_path;
    std::filesystem::path right_hand_side_path;
};

struct SolverObservation {
    std::string backend;
    std::string status;
    std::string reason;
    std::size_t iterations{};
    double setup_seconds{};
    double solve_seconds{};
    double relative_residual{};
    double relative_solution_error{};
    std::size_t peak_resident_bytes{};
};

struct SparseCaseResult {
    SparseCase test_case;
    std::size_t rows{};
    std::size_t columns{};
    std::size_t nonzeros{};
    std::string value_kind;
    std::string symmetry;
    std::string right_hand_side_kind;
    std::string equation_family;
    std::string solve_plan_id;
    std::vector<std::string> backend_chain;
    SolverObservation smave;
    std::vector<SolverObservation> references;
    bool correctness_agreement{false};
};

[[nodiscard]] SparseMatrix read_matrix_market(
    const std::filesystem::path& path);
[[nodiscard]] std::vector<double> read_matrix_market_vector(
    const std::filesystem::path& path,
    std::size_t expected_size);
[[nodiscard]] std::vector<SparseCase> discover_suite_sparse_cases(
    const std::filesystem::path& root);
[[nodiscard]] std::vector<double> deterministic_reference_solution(
    std::size_t size);
[[nodiscard]] double relative_residual(
    const SparseMatrix& matrix,
    const std::vector<double>& solution,
    const std::vector<double>& right_hand_side);
[[nodiscard]] double relative_error(
    const std::vector<double>& actual,
    const std::vector<double>& expected);

void write_sparse_case_result(
    const SparseCaseResult& result,
    const std::filesystem::path& path);
[[nodiscard]] SparseCaseResult read_sparse_case_result(
    const std::filesystem::path& path);

}  // namespace smave::benchmark
