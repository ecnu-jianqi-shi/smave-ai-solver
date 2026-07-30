#include "smave/dae.hpp"

#include <cmath>
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
            std::cerr << "usage: smave_large_dae_event_evidence OUTPUT_DIRECTORY\n";
            return 2;
        }
        const std::filesystem::path directory(argv[1]);
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory);
        constexpr std::size_t state_count = 1089;
        constexpr std::size_t joint_count = state_count + 1;
        const auto source = directory / "LargeDaeEvent.mo";
        std::ofstream model(source);
        model << "model LargeDaeEvent\n";
        for (std::size_t index = 0; index < state_count; ++index) {
            model << "  Real x" << index + 1 << "(start=0, nominal=1);\n";
        }
        model << "  Real z(start=0, nominal=1);\n"
              << "equation\n"
              << "  der(x1) = 1;\n";
        for (std::size_t index = 1; index < state_count; ++index) {
            model << "  der(x" << index + 1 << ") = 0;\n";
        }
        model << "  z = 0;\n"
              << "  when x1 >= 0.005 then\n"
              << "    reinit(x1, -1);\n"
              << "  end when;\n"
              << "end LargeDaeEvent;\n";
        model.close();

        const auto ir = smave::compile_index_one_dae(source, "LargeDaeEvent");
        const auto result = smave::simulate_index_one_dae(ir, 0.01, 0.01);
        require(
            result.success && result.events.size() == 1 &&
                std::abs(result.events.front().time - 0.005) < 2.0e-8 &&
                result.event_root_solves > 1 &&
                result.common_event_root_solves == 1 &&
                result.sparse_event_root_solves == result.event_root_solves &&
                result.sparse_event_root_newton_iterations > 0 &&
                result.sparse_event_root_krylov_iterations > 0 &&
                result.sparse_event_root_jacobian_nonzeros == joint_count &&
                result.sparse_event_root_jacobian_colors == 1 &&
                result.sparse_event_root_jacobian_evaluation_batches ==
                    result.sparse_event_root_solves &&
                result.sparse_event_root_jacobian_ad_batches ==
                    result.sparse_event_root_jacobian_evaluation_batches &&
                result.sparse_event_root_jacobian_fd_fallback_batches == 0 &&
                result.sparse_event_root_jacobian_storage_bytes <
                    joint_count * joint_count * sizeof(double) / 10 &&
                result.sparse_event_root_inner_backend == "pcg-ic0-cpu-v1" &&
                result.maximum_guard_residual < 1.0e-8 &&
                result.maximum_event_projection_residual_inf < 1.0e-8 &&
                result.minimum_algebraic_rank_margin > 0.9,
            "large DAE root localization did not preserve sparse solve and gates");

        smave::write_dae_report(ir, result, directory / "report.txt");
        std::ofstream evidence(directory / "evidence.txt");
        evidence << "SMAVE_LARGE_DAE_EVENT_EVIDENCE 3\n"
                 << "STATES " << state_count << '\n'
                 << "ALGEBRAICS 1\n"
                 << "JOINT_UNKNOWNS " << joint_count << '\n'
                 << "EVENT_TIME " << result.events.front().time << '\n'
                 << "EVENT_ROOT_SOLVES " << result.event_root_solves << '\n'
                 << "COMMON_EVENT_ROOT_SOLVES "
                 << result.common_event_root_solves << '\n'
                 << "SPARSE_EVENT_ROOT_SOLVES "
                 << result.sparse_event_root_solves << '\n'
                 << "SPARSE_EVENT_ROOT_NEWTON_ITERATIONS "
                 << result.sparse_event_root_newton_iterations << '\n'
                 << "SPARSE_EVENT_ROOT_KRYLOV_ITERATIONS "
                 << result.sparse_event_root_krylov_iterations << '\n'
                 << "SPARSE_EVENT_ROOT_JACOBIAN_NONZEROS "
                 << result.sparse_event_root_jacobian_nonzeros << '\n'
                 << "SPARSE_EVENT_ROOT_JACOBIAN_BYTES "
                 << result.sparse_event_root_jacobian_storage_bytes << '\n'
                 << "SPARSE_EVENT_ROOT_JACOBIAN_COLORS "
                 << result.sparse_event_root_jacobian_colors << '\n'
                 << "SPARSE_EVENT_ROOT_JACOBIAN_EVALUATION_BATCHES "
                 << result.sparse_event_root_jacobian_evaluation_batches << '\n'
                 << "SPARSE_EVENT_ROOT_JACOBIAN_AD_BATCHES "
                 << result.sparse_event_root_jacobian_ad_batches << '\n'
                 << "SPARSE_EVENT_ROOT_JACOBIAN_FD_FALLBACK_BATCHES "
                 << result.sparse_event_root_jacobian_fd_fallback_batches << '\n'
                 << "SPARSE_EVENT_ROOT_INNER_BACKEND "
                 << result.sparse_event_root_inner_backend << '\n'
                 << "MAXIMUM_GUARD_RESIDUAL "
                 << result.maximum_guard_residual << '\n'
                 << "MINIMUM_ALGEBRAIC_RANK_MARGIN "
                 << result.minimum_algebraic_rank_margin << '\n';
        std::cout << "large DAE event root evidence passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "large DAE event evidence failed: " << error.what() << '\n';
        return 1;
    }
}
