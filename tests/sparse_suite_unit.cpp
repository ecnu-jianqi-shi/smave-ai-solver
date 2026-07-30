#include "smave/benchmark/sparse_suite.hpp"
#include "smave/linear.hpp"
#include "smave/routing.hpp"
#include "smave/solve_service.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 4) {
            throw std::invalid_argument(
                "usage: smave_sparse_suite_unit SUITESPARSE_ROOT OUTPUT_DIRECTORY EXPECTED_CASES");
        }
        const std::filesystem::path root(argv[1]);
        const std::filesystem::path output(argv[2]);
        const auto expected_cases = static_cast<std::size_t>(std::stoul(argv[3]));
        std::filesystem::remove_all(output);
        std::filesystem::create_directories(output);
        const auto cases = smave::benchmark::discover_suite_sparse_cases(root);
        require(cases.size() == expected_cases,
                "SuiteSparse system matrix discovery count changed");
        const auto matrix = smave::benchmark::read_matrix_market(
            root / "small/west0479/west0479.mtx");
        require(matrix.rows == 479 && matrix.columns == 479 &&
                    matrix.nonzeros() == 1888,
                "Matrix Market general parser produced unexpected west0479 shape");
        const auto symmetric = smave::benchmark::read_matrix_market(
            root / "small/nasa2910/nasa2910.mtx");
        require(symmetric.rows == 2910 && symmetric.declared_symmetric &&
                    symmetric.nonzeros() == 174296,
                "Matrix Market symmetric expansion produced unexpected nasa2910 shape");
        const auto singular_consistent = smave::benchmark::read_matrix_market(
            root / "final-heldout-v1/laser/laser.mtx");
        smave::LinearSystem singular_system;
        singular_system.sparsity.row_count = singular_consistent.rows;
        singular_system.sparsity.column_count = singular_consistent.columns;
        singular_system.sparsity.row_offsets = singular_consistent.row_offsets;
        singular_system.sparsity.column_indices = singular_consistent.column_indices;
        singular_system.sparse_values = singular_consistent.values;
        singular_system.right_hand_side = singular_consistent.multiply(
            smave::benchmark::deterministic_reference_solution(
                singular_consistent.columns));
        smave::classify_linear_system(singular_system);
        const auto singular_solve = smave::verified_linear_solve(
            singular_system,
            {.absolute_tolerance = 1.0e-12,
             .relative_tolerance = 1.0e-10,
             .maximum_work_iterations = 250,
             .restart_dimension = 40,
             .built_in_sparse_direct_row_limit = 512});
        require(
            singular_solve.success && singular_solve.used_fallback &&
                singular_solve.backend == "gmres-identity-cpu-v1" &&
                singular_solve.backward_error <= 1.0e-10 &&
                !singular_solve.attempts.empty() &&
                singular_solve.attempts.back().backend ==
                    "gmres-identity-cpu-v1" &&
                singular_solve.attempts.back().status == "accepted",
            "consistent singular SuiteSparse system did not reach verified identity GMRES");
        const auto structurally_singular = smave::benchmark::read_matrix_market(
            root / "final-heldout-v2/M10PI_n1/M10PI_n1.mtx");
        smave::LinearSystem structurally_singular_system;
        structurally_singular_system.sparsity.row_count = structurally_singular.rows;
        structurally_singular_system.sparsity.column_count = structurally_singular.columns;
        structurally_singular_system.sparsity.row_offsets = structurally_singular.row_offsets;
        structurally_singular_system.sparsity.column_indices =
            structurally_singular.column_indices;
        structurally_singular_system.sparse_values = structurally_singular.values;
        structurally_singular_system.right_hand_side = structurally_singular.multiply(
            smave::benchmark::deterministic_reference_solution(
                structurally_singular.columns));
        smave::classify_linear_system(structurally_singular_system);
        if (smave::superlu_sparse_direct_available()) {
            const auto structural_rejection = smave::superlu_sparse_direct_solve(
                structurally_singular_system);
            require(
                !structural_rejection.solved &&
                    structural_rejection.reason.find("structural-rank gate") !=
                        std::string::npos &&
                    structural_rejection.reason.find("deficiency 3") !=
                        std::string::npos,
                "structurally singular SuiteSparse matrix reached SuperLU factorization");
        }
        const auto structurally_singular_solve = smave::verified_linear_solve(
            structurally_singular_system,
            {.absolute_tolerance = 1.0e-12,
             .relative_tolerance = 1.0e-10,
             .maximum_work_iterations = 250,
             .restart_dimension = 40,
             .built_in_sparse_direct_row_limit = 512});
        require(
            structurally_singular_solve.success &&
                structurally_singular_solve.backend == "lsqr-identity-cpu-v1" &&
                structurally_singular_solve.backward_error <= 1.0e-10 &&
                !structurally_singular_solve.attempts.empty() &&
                structurally_singular_solve.attempts.back().backend ==
                    "lsqr-identity-cpu-v1" &&
                structurally_singular_solve.attempts.back().status == "accepted",
            "deficiency-3 SuiteSparse system did not reach verified LSQR");

        const auto larger_structurally_singular = smave::benchmark::read_matrix_market(
            root / "final-heldout-v2/TS/TS.mtx");
        smave::LinearSystem larger_structurally_singular_system;
        larger_structurally_singular_system.sparsity.row_count =
            larger_structurally_singular.rows;
        larger_structurally_singular_system.sparsity.column_count =
            larger_structurally_singular.columns;
        larger_structurally_singular_system.sparsity.row_offsets =
            larger_structurally_singular.row_offsets;
        larger_structurally_singular_system.sparsity.column_indices =
            larger_structurally_singular.column_indices;
        larger_structurally_singular_system.sparse_values =
            larger_structurally_singular.values;
        larger_structurally_singular_system.right_hand_side =
            larger_structurally_singular.multiply(
                smave::benchmark::deterministic_reference_solution(
                    larger_structurally_singular.columns));
        smave::classify_linear_system(larger_structurally_singular_system);
        const auto larger_structurally_singular_solve = smave::verified_linear_solve(
            larger_structurally_singular_system,
            {.absolute_tolerance = 1.0e-12,
             .relative_tolerance = 1.0e-10,
             .maximum_work_iterations = 250,
             .restart_dimension = 40,
             .built_in_sparse_direct_row_limit = 512});
        require(
            larger_structurally_singular_solve.success &&
                larger_structurally_singular_solve.backend ==
                    "lsqr-identity-cpu-v1" &&
                larger_structurally_singular_solve.backward_error <= 1.0e-10 &&
                !larger_structurally_singular_solve.attempts.empty() &&
                larger_structurally_singular_solve.attempts.back().backend ==
                    "lsqr-identity-cpu-v1" &&
                larger_structurally_singular_solve.attempts.back().status == "accepted",
            "deficiency-2 SuiteSparse system did not reach verified LSQR");
        const auto duplicate_path = output / "duplicates.mtx";
        std::ofstream(duplicate_path)
            << "%%MatrixMarket matrix coordinate real general\n"
            << "2 2 4\n"
            << "1 1 2\n"
            << "1 1 3\n"
            << "1 2 1\n"
            << "1 2 -1\n";
        const auto duplicate = smave::benchmark::read_matrix_market(duplicate_path);
        require(duplicate.nonzeros() == 1 && duplicate.values.front() == 5.0,
                "Matrix Market duplicate coordinates were not merged");
        const auto routed_spd = smave::route_sparse_linear_system({
            .fingerprint = "unit-spd",
            .rows = 100000,
            .columns = 100000,
            .nonzeros = 500000,
            .structurally_symmetric = true,
            .numerically_symmetric = true,
            .numerically_positive_definite = true,
            .diagonal_condition_estimate = 12.0,
        });
        require(!routed_spd.steps.empty() &&
                    routed_spd.steps.front().expert_version == "pcg-ic0-cpu-v1" &&
                    routed_spd.terminal_fallback ==
                        "terminal-numerical-linear-cascade-v1",
                "sparse SPD profile did not route through PCG with mandatory fallback");
        const auto routed_nonsymmetric = smave::route_sparse_linear_system({
            .fingerprint = "unit-nonsymmetric",
            .rows = 100000,
            .columns = 100000,
            .nonzeros = 500000,
            .structurally_symmetric = false,
            .numerically_symmetric = false,
            .numerically_positive_definite = false,
            .diagonal_condition_estimate = 30.0,
        });
        require(!routed_nonsymmetric.steps.empty() &&
                    routed_nonsymmetric.steps.front().expert_version ==
                        "gmres-ilu0-cpu-v1",
                "large nonsymmetric profile did not route through bounded-memory GMRES");
        const auto routed_apple_tensor = smave::route_sparse_linear_system({
            .fingerprint = "unit-apple-tensor-grid",
            .rows = 4096,
            .columns = 4096,
            .nonzeros = 20480,
            .structurally_symmetric = true,
            .numerically_symmetric = true,
            .numerically_positive_definite = true,
            .diagonal_condition_estimate = 4.0,
            .regular_grid = true,
            .grid_dimension = 2,
            .batch_size = 40,
            .expected_reuses = 100,
            .apple_accelerate_available = true,
            .metal_available = true,
            .learned_expert_available = true,
            .learned_expert_resident = true,
        });
        require(routed_apple_tensor.steps.size() >= 3 &&
                    routed_apple_tensor.steps[0].expert_version ==
                        "learned-tensor-operator-resident-v1" &&
                    routed_apple_tensor.steps[1].expert_version ==
                        "accelerate-vdsp-spectral-plan-fp64-v1" &&
                    routed_apple_tensor.steps[2].expert_version ==
                        "metal-gpu-batched-stencil-v1" &&
                    routed_apple_tensor.steps[0].backend_chain.back() ==
                        "original-solver-fallback",
                "Apple tensor grid profile did not route through learned, spectral, and Metal backends");
        const auto expected = smave::benchmark::deterministic_reference_solution(
            matrix.columns);
        const auto right_hand_side = matrix.multiply(expected);
        require(smave::benchmark::relative_residual(
                    matrix, expected, right_hand_side) == 0.0,
                "manufactured sparse right-hand side is inconsistent");
        smave::benchmark::SparseCaseResult result;
        result.test_case = cases.front();
        result.rows = matrix.rows;
        result.columns = matrix.columns;
        result.nonzeros = matrix.nonzeros();
        result.value_kind = "real";
        result.symmetry = "general";
        result.right_hand_side_kind = "manufactured";
        result.equation_family = "linear-sparse-nonsymmetric";
        result.solve_plan_id = "unit-plan";
        result.backend_chain = {"unit-backend", "unit-fallback"};
        result.smave = {
            .backend = "smave-test",
            .status = "converged",
            .relative_residual = 1.0e-12,
            .relative_solution_error = 2.0e-12,
        };
        result.references.push_back({
            .backend = "reference-test",
            .status = "converged",
            .relative_residual = 1.0e-13,
            .relative_solution_error = 2.0e-13,
        });
        result.correctness_agreement = true;
        const auto checkpoint = output / "checkpoint.txt";
        smave::benchmark::write_sparse_case_result(result, checkpoint);
        const auto restored = smave::benchmark::read_sparse_case_result(checkpoint);
        require(restored.test_case.name == result.test_case.name &&
                    restored.solve_plan_id == result.solve_plan_id &&
                    restored.backend_chain == result.backend_chain &&
                    restored.references.size() == 1 &&
                    restored.correctness_agreement,
                "sparse checkpoint roundtrip failed");
        std::cout << "sparse suite unit passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "sparse suite unit failure: " << error.what() << '\n';
        return 1;
    }
}
