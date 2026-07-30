#include "smave/benchmark/sparse_suite.hpp"
#include "smave/linear.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr double symmetry_relative_tolerance = 1.0e-12;

const std::set<std::string> development_matrix_names{
    "msc00726", "crystm01", "bfwb398", "saylr4", "impcol_e", "west0479",
    "nasa2910", "G10", "ck400", "dw256A", "nasa4704", "G26", "rdb450",
    "fs_541_1", "plbuckle", "Si2", "rotor2", "ex10", "laser",
    "Chebyshev2", "Trefethen_700", "spaceShuttleEntry_1", "M10PI_n1",
    "Chem97ZtZ", "c-18", "TS"};

struct MatrixAudit {
    std::string name;
    std::filesystem::path relative_path;
    std::size_t rows{};
    std::size_t columns{};
    std::size_t nonzeros{};
    bool declared_symmetric{};
    bool structurally_symmetric{};
    bool numerically_symmetric{};
    bool conservative_positive_definite{};
    bool cholesky_available{};
    bool cholesky_solved{};
    double cholesky_residual_inf{};
    std::string cholesky_reason;
    std::string numeric_class;
    bool used_in_development{};
};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::string tsv_text(std::string value) {
    std::replace(value.begin(), value.end(), '\t', ' ');
    std::replace(value.begin(), value.end(), '\n', ' ');
    std::replace(value.begin(), value.end(), '\r', ' ');
    return value;
}

double coefficient(
    const smave::benchmark::SparseMatrix& matrix,
    std::size_t row,
    std::size_t column) {
    const auto begin = matrix.row_offsets[row];
    const auto end = matrix.row_offsets[row + 1];
    const auto iterator = std::lower_bound(
        matrix.column_indices.begin() + static_cast<std::ptrdiff_t>(begin),
        matrix.column_indices.begin() + static_cast<std::ptrdiff_t>(end),
        column);
    return iterator != matrix.column_indices.begin() +
            static_cast<std::ptrdiff_t>(end) &&
            *iterator == column
        ? matrix.values[static_cast<std::size_t>(
              iterator - matrix.column_indices.begin())]
        : 0.0;
}

bool structurally_symmetric(const smave::benchmark::SparseMatrix& matrix) {
    if (matrix.rows != matrix.columns) return false;
    for (std::size_t row = 0; row < matrix.rows; ++row) {
        for (std::size_t offset = matrix.row_offsets[row];
             offset < matrix.row_offsets[row + 1]; ++offset) {
            const auto column = matrix.column_indices[offset];
            if (coefficient(matrix, column, row) == 0.0) return false;
        }
    }
    return true;
}

bool numerically_symmetric(const smave::benchmark::SparseMatrix& matrix) {
    if (!structurally_symmetric(matrix)) return false;
    for (std::size_t row = 0; row < matrix.rows; ++row) {
        for (std::size_t offset = matrix.row_offsets[row];
             offset < matrix.row_offsets[row + 1]; ++offset) {
            const auto column = matrix.column_indices[offset];
            const auto left = matrix.values[offset];
            const auto right = coefficient(matrix, column, row);
            const double scale = 1.0 + std::max(std::abs(left), std::abs(right));
            if (std::abs(left - right) > symmetry_relative_tolerance * scale) {
                return false;
            }
        }
    }
    return true;
}

smave::LinearSystem make_system(
    const smave::benchmark::SparseMatrix& matrix,
    const std::vector<double>& right_hand_side) {
    smave::LinearSystem system;
    system.sparsity.row_count = matrix.rows;
    system.sparsity.column_count = matrix.columns;
    system.sparsity.row_offsets = matrix.row_offsets;
    system.sparsity.column_indices = matrix.column_indices;
    system.sparse_values = matrix.values;
    system.right_hand_side = right_hand_side;
    system.sparsity.validate();
    return system;
}

MatrixAudit audit_case(
    const std::filesystem::path& root,
    const smave::benchmark::SparseCase& test_case,
    std::size_t row_limit) {
    const auto matrix = smave::benchmark::read_matrix_market(test_case.matrix_path);
    MatrixAudit result;
    result.name = test_case.name;
    result.relative_path = test_case.matrix_path.lexically_relative(root);
    result.rows = matrix.rows;
    result.columns = matrix.columns;
    result.nonzeros = matrix.nonzeros();
    result.declared_symmetric = matrix.declared_symmetric;
    result.structurally_symmetric = structurally_symmetric(matrix);
    result.numerically_symmetric = numerically_symmetric(matrix);
    result.used_in_development = development_matrix_names.contains(result.name);

    if (result.numerically_symmetric && result.rows <= row_limit) {
        const auto expected = smave::benchmark::deterministic_reference_solution(
            matrix.columns);
        auto classified = make_system(matrix, matrix.multiply(expected));
        smave::classify_linear_system(classified);
        result.conservative_positive_definite = classified.positive_definite;

        auto probe = classified;
        probe.symmetric = true;
        probe.positive_definite = true;
        const auto cholesky = smave::accelerate_sparse_spd_direct_solve(probe);
        result.cholesky_available = cholesky.available;
        result.cholesky_solved = cholesky.solved;
        result.cholesky_residual_inf = cholesky.residual_inf;
        result.cholesky_reason = cholesky.reason;
    } else if (result.numerically_symmetric) {
        result.cholesky_reason = "row limit exceeded; probe not run";
    } else {
        result.cholesky_reason = "not numerically symmetric; probe not applicable";
    }

    if (!result.numerically_symmetric) {
        result.numeric_class = "nonsymmetric";
    } else if (result.cholesky_solved) {
        result.numeric_class = "spd";
    } else if (result.cholesky_available) {
        result.numeric_class = "symmetric-non-spd";
    } else {
        result.numeric_class = "symmetric-unresolved";
    }
    return result;
}

void write_report(
    const std::filesystem::path& output_directory,
    const std::filesystem::path& root,
    std::size_t row_limit,
    const std::vector<MatrixAudit>& audits) {
    std::filesystem::create_directories(output_directory);
    std::ofstream table(output_directory / "structure-audit.tsv");
    require(static_cast<bool>(table), "cannot write structure audit table");
    table << "name\trelative_path\trows\tcolumns\tnonzeros\tdeclared_symmetric"
          << "\tstructurally_symmetric\tnumerically_symmetric"
          << "\tconservative_positive_definite\tcholesky_available"
          << "\tcholesky_solved\tcholesky_residual_inf\tcholesky_reason"
          << "\tnumeric_class\tused_in_development\n";
    table << std::setprecision(17);
    for (const auto& audit : audits) {
        table << audit.name << '\t' << audit.relative_path.string() << '\t'
              << audit.rows << '\t' << audit.columns << '\t' << audit.nonzeros
              << '\t' << (audit.declared_symmetric ? 1 : 0) << '\t'
              << (audit.structurally_symmetric ? 1 : 0) << '\t'
              << (audit.numerically_symmetric ? 1 : 0) << '\t'
              << (audit.conservative_positive_definite ? 1 : 0) << '\t'
              << (audit.cholesky_available ? 1 : 0) << '\t'
              << (audit.cholesky_solved ? 1 : 0) << '\t'
              << audit.cholesky_residual_inf << '\t'
              << tsv_text(audit.cholesky_reason) << '\t'
              << audit.numeric_class << '\t'
              << (audit.used_in_development ? 1 : 0) << '\n';
    }

    std::size_t spd_count{};
    std::size_t symmetric_non_spd_count{};
    std::size_t symmetric_unresolved_count{};
    std::size_t nonsymmetric_count{};
    for (const auto& audit : audits) {
        if (audit.numeric_class == "spd") {
            ++spd_count;
        } else if (audit.numeric_class == "symmetric-non-spd") {
            ++symmetric_non_spd_count;
        } else if (audit.numeric_class == "symmetric-unresolved") {
            ++symmetric_unresolved_count;
        } else {
            ++nonsymmetric_count;
        }
    }
    std::ofstream evidence(output_directory / "evidence.txt");
    require(static_cast<bool>(evidence), "cannot write structure audit evidence");
    evidence << "SMAVE_SUITESPARSE_STRUCTURE_AUDIT 1\n"
             << "snapshot_date=2026-07-27\n"
             << "matrix_root=" << root.string() << '\n'
             << "matrix_row_limit=" << row_limit << '\n'
             << "candidate_count=" << audits.size() << '\n'
             << "spd_count=" << spd_count << '\n'
             << "symmetric_non_spd_count=" << symmetric_non_spd_count << '\n'
             << "symmetric_unresolved_count=" << symmetric_unresolved_count << '\n'
             << "nonsymmetric_count=" << nonsymmetric_count << '\n'
             << "development_matrix_count="
             << std::count_if(audits.begin(), audits.end(), [](const auto& audit) {
                    return audit.used_in_development;
                })
             << '\n'
             << "classification_rule=numeric-symmetry-and-conservative-cholesky-probe-v1\n"
             << "performance_measurements=0\n"
             << "structure_audit_table=structure-audit.tsv\n"
             << "END\n";
}

}  // namespace

int main(int argc, char** argv) {
    try {
        require(argc == 3 || argc == 4,
                "usage: suitesparse-structure-audit MATRIX_ROOT OUTPUT_DIRECTORY [ROW_LIMIT]");
        const std::filesystem::path root = argv[1];
        const std::filesystem::path output_directory = argv[2];
        const std::size_t row_limit = argc == 4
            ? static_cast<std::size_t>(std::stoull(argv[3]))
            : 50000;
        const auto cases = smave::benchmark::discover_suite_sparse_cases(root);
        std::vector<MatrixAudit> audits;
        for (const auto& test_case : cases) {
            const auto audit = audit_case(root, test_case, row_limit);
            if (audit.rows <= row_limit) audits.push_back(audit);
        }
        std::sort(audits.begin(), audits.end(), [](const auto& left, const auto& right) {
            if (left.rows != right.rows) return left.rows < right.rows;
            return left.name < right.name;
        });
        write_report(output_directory, root, row_limit, audits);
        std::cout << "SMAVE_SUITESPARSE_STRUCTURE_AUDIT 1\n"
                  << "audited=" << audits.size() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
