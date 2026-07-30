#include "smave/dae.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

}

int main(int argc, char** argv) {
    try {
        if (argc != 2) {
            std::cerr << "usage: smave_large_dae_evidence OUTPUT_DIRECTORY\n";
            return 2;
        }
        const std::filesystem::path directory(argv[1]);
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory);
        constexpr std::size_t width = 33;
        constexpr std::size_t state_count = width * width;
        constexpr std::size_t joint_count = state_count + 1;
        const auto source = directory / "LargeIndexOneDae33.mo";
        std::ofstream model(source);
        model << "model LargeIndexOneDae33\n";
        for (std::size_t index = 0; index < state_count; ++index) {
            model << "  Real x" << index + 1 << "(start=0, nominal=1);\n";
        }
        model << "  Real z(start=0, nominal=1);\n"
              << "equation\n";
        for (std::size_t row = 0; row < width; ++row) {
            for (std::size_t column = 0; column < width; ++column) {
                const auto index = row * width + column;
                model << "  der(x" << index + 1 << ") = -4*x" << index + 1
                      << " - 0.01*x" << index + 1 << "^3 + 1";
                if (row > 0) model << " + x" << index + 1 - width;
                if (row + 1 < width) model << " + x" << index + 1 + width;
                if (column > 0) model << " + x" << index;
                if (column + 1 < width) model << " + x" << index + 2;
                model << ";\n";
            }
        }
        model << "  z = 0;\n"
              << "end LargeIndexOneDae33;\n";
        model.close();

        const auto compile_started = std::chrono::steady_clock::now();
        const auto ir = smave::compile_index_one_dae(source, "LargeIndexOneDae33");
        const auto compile_us = std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - compile_started).count();
        require(ir.states.size() == state_count && ir.algebraics.size() == 1 &&
                    ir.constraints.size() == 1,
                "large DAE frontend lost state/algebraic structure");
        const auto solve_started = std::chrono::steady_clock::now();
        const auto result = smave::simulate_index_one_dae(ir, 0.03, 0.01);
        const auto solve_us = std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - solve_started).count();
        require(result.success && result.steps.size() == 3 &&
                    result.sparse_newton_steps == 3 &&
                    result.sparse_newton_iterations > 0 &&
                    result.sparse_krylov_iterations > 0 &&
                    result.sparse_jacobian_nonzeros > joint_count &&
                    result.sparse_jacobian_colors > 0 &&
                    result.sparse_jacobian_colors < result.sparse_jacobian_nonzeros / 100 &&
                    result.sparse_jacobian_evaluation_batches ==
                        result.sparse_jacobian_colors *
                            result.sparse_newton_iterations &&
                    result.sparse_jacobian_ad_batches ==
                        result.sparse_jacobian_evaluation_batches &&
                    result.sparse_jacobian_fd_fallback_batches == 0 &&
                    result.sparse_jacobian_storage_bytes <
                        joint_count * joint_count * sizeof(double) / 10 &&
                    result.sparse_inner_backend == "pcg-ic0-cpu-v1" &&
                    result.dense_step_fallbacks == 0 &&
                    result.algebraic_rank_checks == 4 &&
                    result.maximum_residual_inf < 1.0e-6,
                "large DAE ordinary steps did not use gated sparse Newton-Krylov");

        smave::write_dae_report(ir, result, directory / "report.txt");
        std::ofstream evidence(directory / "evidence.txt");
        evidence << "SMAVE_LARGE_DAE_EVIDENCE 3\n"
                 << "FAMILY semi-explicit-index1-reaction-diffusion\n"
                 << "STATES " << state_count << '\n'
                 << "ALGEBRAICS 1\n"
                 << "JOINT_UNKNOWNS " << joint_count << '\n'
                 << "JACOBIAN_NONZEROS " << result.sparse_jacobian_nonzeros << '\n'
                 << "DENSE_JACOBIAN_BYTES "
                 << joint_count * joint_count * sizeof(double) << '\n'
                 << "CSR_JACOBIAN_BYTES "
                 << result.sparse_jacobian_storage_bytes << '\n'
                 << "JACOBIAN_COLORS " << result.sparse_jacobian_colors << '\n'
                 << "JACOBIAN_EVALUATION_BATCHES "
                 << result.sparse_jacobian_evaluation_batches << '\n'
                 << "JACOBIAN_AD_BATCHES "
                 << result.sparse_jacobian_ad_batches << '\n'
                 << "JACOBIAN_FD_FALLBACK_BATCHES "
                 << result.sparse_jacobian_fd_fallback_batches << '\n'
                 << "COMPILE_US " << compile_us << '\n'
                 << "SIMULATE_US " << solve_us << '\n'
                 << "STEPS " << result.steps.size() << '\n'
                 << "SPARSE_NEWTON_STEPS " << result.sparse_newton_steps << '\n'
                 << "SPARSE_NEWTON_ITERATIONS "
                 << result.sparse_newton_iterations << '\n'
                 << "SPARSE_KRYLOV_ITERATIONS "
                 << result.sparse_krylov_iterations << '\n'
                 << "INNER_BACKEND " << result.sparse_inner_backend << '\n'
                 << "ALGEBRAIC_RANK_CHECKS " << result.algebraic_rank_checks << '\n'
                 << "MAXIMUM_RESIDUAL " << result.maximum_residual_inf << '\n'
                 << "DENSE_STEP_FALLBACKS " << result.dense_step_fallbacks << '\n'
                 << "INITIALIZATION_EVENT_PATH separately-covered\n";
        std::cout << "large DAE evidence passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "large DAE evidence failed: " << error.what() << '\n';
        return 1;
    }
}
