#include "smave/dae.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

smave::IndexOneDaeIR write_joint_initialization_model(
    const std::filesystem::path& directory) {
    constexpr std::size_t state_count = 1089;
    const auto source = directory / "LargeDaeInitialization.mo";
    std::ofstream model(source);
    model << "model LargeDaeInitialization\n";
    for (std::size_t index = 0; index < state_count; ++index) {
        model << "  Real x" << index + 1 << "(start=0, nominal=1);\n";
    }
    model << "  Real z(start=0, nominal=1);\n"
          << "initial equation\n";
    for (std::size_t index = 0; index < state_count; ++index) {
        model << "  x" << index + 1 << " = 1;\n";
    }
    model << "equation\n";
    for (std::size_t index = 0; index < state_count; ++index) {
        model << "  der(x" << index + 1 << ") = 0;\n";
    }
    model << "  z = 0;\n"
          << "end LargeDaeInitialization;\n";
    model.close();
    return smave::compile_index_one_dae(source, "LargeDaeInitialization");
}

smave::IndexOneDaeIR write_large_projection_model(
    const std::filesystem::path& directory) {
    constexpr std::size_t algebraic_count = 1089;
    const auto source = directory / "LargeDaeProjection.mo";
    std::ofstream model(source);
    model << "model LargeDaeProjection\n"
          << "  Real x(start=0, nominal=1);\n";
    for (std::size_t index = 0; index < algebraic_count; ++index) {
        model << "  Real z" << index + 1 << "(start=0, nominal=1);\n";
    }
    model << "equation\n"
          << "  der(x) = 0;\n";
    for (std::size_t index = 0; index < algebraic_count; ++index) {
        model << "  z" << index + 1 << " = x;\n";
    }
    model << "  when x >= 0 then\n"
          << "    reinit(x, 1);\n"
          << "  end when;\n"
          << "end LargeDaeProjection;\n";
    model.close();
    return smave::compile_index_one_dae(source, "LargeDaeProjection");
}

smave::IndexOneDaeIR write_singular_rank_model(
    const std::filesystem::path& directory) {
    constexpr std::size_t algebraic_count = 1089;
    const auto source = directory / "LargeDaeSingularRank.mo";
    std::ofstream model(source);
    model << "model LargeDaeSingularRank\n"
          << "  Real x(start=0, nominal=1);\n";
    for (std::size_t index = 0; index < algebraic_count; ++index) {
        model << "  Real z" << index + 1 << "(start=0, nominal=1);\n";
    }
    model << "equation\n"
          << "  der(x) = 0;\n";
    for (std::size_t index = 0; index + 1 < algebraic_count; ++index) {
        model << "  z" << index + 1 << " = 0;\n";
    }
    model << "  x*z" << algebraic_count << " = 0;\n"
          << "end LargeDaeSingularRank;\n";
    model.close();
    return smave::compile_index_one_dae(source, "LargeDaeSingularRank");
}

smave::IndexOneDaeIR write_ad_fallback_model(
    const std::filesystem::path& directory) {
    constexpr std::size_t state_count = 1089;
    const auto source = directory / "LargeDaeAdFallback.mo";
    std::ofstream model(source);
    model << "model LargeDaeAdFallback\n";
    for (std::size_t index = 0; index < state_count; ++index) {
        model << "  Real x" << index + 1 << "(start=0, nominal=1);\n";
    }
    model << "  Real z(start=0, nominal=1);\n"
          << "initial equation\n"
          << "  x1 + abs(x1)*abs(x1) = 1;\n";
    for (std::size_t index = 1; index < state_count; ++index) {
        model << "  x" << index + 1 << " = 1;\n";
    }
    model << "equation\n";
    for (std::size_t index = 0; index < state_count; ++index) {
        model << "  der(x" << index + 1 << ") = 0;\n";
    }
    model << "  z = 0;\n"
          << "end LargeDaeAdFallback;\n";
    model.close();
    return smave::compile_index_one_dae(source, "LargeDaeAdFallback");
}

}

int main(int argc, char** argv) {
    try {
        if (argc != 2) {
            std::cerr << "usage: smave_large_dae_initialization_evidence OUTPUT_DIRECTORY\n";
            return 2;
        }
        const std::filesystem::path directory(argv[1]);
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory);

        const auto initialization_ir = write_joint_initialization_model(directory);
        const auto initialization = smave::simulate_index_one_dae(
            initialization_ir, 0.01, 0.01);
        constexpr std::size_t initialization_unknowns = 1090;
        require(
            initialization.success && initialization.sparse_initialization &&
                initialization.sparse_initialization_iterations > 0 &&
                initialization.sparse_initialization_krylov_iterations > 0 &&
                initialization.sparse_initialization_jacobian_nonzeros ==
                    initialization_unknowns &&
                initialization.sparse_initialization_jacobian_colors == 1 &&
                initialization.sparse_initialization_jacobian_evaluation_batches == 1 &&
                initialization.sparse_initialization_jacobian_ad_batches == 1 &&
                initialization.sparse_initialization_jacobian_fd_fallback_batches == 0 &&
                initialization.sparse_initialization_jacobian_storage_bytes <
                    initialization_unknowns * initialization_unknowns * sizeof(double) / 10 &&
                initialization.sparse_initialization_inner_backend ==
                    "pcg-ic0-cpu-v1" &&
                initialization.initialization_residual_inf < 1.0e-6,
            "large joint DAE initialization did not use gated sparse Newton-Krylov");

        const auto projection_ir = write_large_projection_model(directory);
        const auto projection = smave::simulate_index_one_dae(
            projection_ir, 0.01, 0.01);
        constexpr std::size_t algebraic_count = 1089;
        require(
            projection.success && projection.initial_events.size() == 1 &&
                projection.sparse_event_projections == 1 &&
                projection.sparse_event_projection_iterations > 0 &&
                projection.sparse_event_projection_krylov_iterations > 0 &&
                projection.sparse_event_projection_jacobian_nonzeros == algebraic_count &&
                projection.sparse_event_projection_jacobian_colors == 1 &&
                projection.sparse_event_projection_jacobian_evaluation_batches == 1 &&
                projection.sparse_event_projection_jacobian_ad_batches == 1 &&
                projection.sparse_event_projection_jacobian_fd_fallback_batches == 0 &&
                projection.sparse_event_projection_jacobian_storage_bytes <
                    algebraic_count * algebraic_count * sizeof(double) / 10 &&
                projection.sparse_event_projection_inner_backend ==
                    "pcg-ic0-cpu-v1" &&
                projection.initial_event_projection_residual_inf < 1.0e-6 &&
                projection.minimum_algebraic_rank_margin > 0.9,
            "large DAE event projection or sparse algebraic rank gate failed");

        const auto singular_ir = write_singular_rank_model(directory);
        const auto singular = smave::simulate_index_one_dae(
            singular_ir, 0.01, 0.01);
        require(
            !singular.success && singular.algebraic_rank_checks == 1 &&
                singular.minimum_algebraic_rank_margin == 0.0 &&
                singular.message.find("rank gate failed") != std::string::npos,
            "large singular algebraic Jacobian was not rejected before commit");

        const auto fallback_ir = write_ad_fallback_model(directory);
        const auto fallback = smave::simulate_index_one_dae(
            fallback_ir, 0.01, 0.01);
        require(
            fallback.success && fallback.sparse_initialization &&
                fallback.sparse_initialization_jacobian_colors == 1 &&
                fallback.sparse_initialization_jacobian_fd_fallback_batches == 1 &&
                fallback.sparse_initialization_jacobian_ad_batches > 0 &&
                fallback.initialization_residual_inf < 1.0e-6,
            "nondifferentiable large DAE point did not use local FD fallback");

        smave::write_dae_report(
            initialization_ir, initialization,
            directory / "initialization-report.txt");
        smave::write_dae_report(
            projection_ir, projection,
            directory / "projection-report.txt");
        std::ofstream evidence(directory / "evidence.txt");
        evidence << "SMAVE_LARGE_DAE_INITIALIZATION_EVIDENCE 3\n"
                 << "INITIALIZATION_UNKNOWNS " << initialization_unknowns << '\n'
                 << "INITIALIZATION_JACOBIAN_NONZEROS "
                 << initialization.sparse_initialization_jacobian_nonzeros << '\n'
                 << "INITIALIZATION_JACOBIAN_BYTES "
                 << initialization.sparse_initialization_jacobian_storage_bytes << '\n'
                 << "INITIALIZATION_JACOBIAN_COLORS "
                 << initialization.sparse_initialization_jacobian_colors << '\n'
                 << "INITIALIZATION_JACOBIAN_EVALUATION_BATCHES "
                 << initialization.sparse_initialization_jacobian_evaluation_batches << '\n'
                 << "INITIALIZATION_JACOBIAN_AD_BATCHES "
                 << initialization.sparse_initialization_jacobian_ad_batches << '\n'
                 << "INITIALIZATION_JACOBIAN_FD_FALLBACK_BATCHES "
                 << initialization.sparse_initialization_jacobian_fd_fallback_batches << '\n'
                 << "INITIALIZATION_KRYLOV_ITERATIONS "
                 << initialization.sparse_initialization_krylov_iterations << '\n'
                 << "INITIALIZATION_INNER_BACKEND "
                 << initialization.sparse_initialization_inner_backend << '\n'
                 << "EVENT_ALGEBRAICS " << algebraic_count << '\n'
                 << "SPARSE_EVENT_PROJECTIONS "
                 << projection.sparse_event_projections << '\n'
                 << "EVENT_PROJECTION_JACOBIAN_NONZEROS "
                 << projection.sparse_event_projection_jacobian_nonzeros << '\n'
                 << "EVENT_PROJECTION_JACOBIAN_BYTES "
                 << projection.sparse_event_projection_jacobian_storage_bytes << '\n'
                 << "EVENT_PROJECTION_JACOBIAN_COLORS "
                 << projection.sparse_event_projection_jacobian_colors << '\n'
                 << "EVENT_PROJECTION_JACOBIAN_EVALUATION_BATCHES "
                 << projection.sparse_event_projection_jacobian_evaluation_batches << '\n'
                 << "EVENT_PROJECTION_JACOBIAN_AD_BATCHES "
                 << projection.sparse_event_projection_jacobian_ad_batches << '\n'
                 << "EVENT_PROJECTION_JACOBIAN_FD_FALLBACK_BATCHES "
                 << projection.sparse_event_projection_jacobian_fd_fallback_batches << '\n'
                 << "EVENT_PROJECTION_KRYLOV_ITERATIONS "
                 << projection.sparse_event_projection_krylov_iterations << '\n'
                 << "EVENT_PROJECTION_INNER_BACKEND "
                 << projection.sparse_event_projection_inner_backend << '\n'
                 << "MINIMUM_ALGEBRAIC_RANK_MARGIN "
                 << projection.minimum_algebraic_rank_margin << '\n'
                 << "SINGULAR_RANK_REJECTED " << !singular.success << '\n';
        evidence << "AD_FALLBACK_BATCHES "
                 << fallback.sparse_initialization_jacobian_fd_fallback_batches << '\n'
                 << "AD_RECOVERED_BATCHES "
                 << fallback.sparse_initialization_jacobian_ad_batches << '\n';
        std::cout << "large DAE initialization and projection evidence passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "large DAE initialization evidence failed: "
                  << error.what() << '\n';
        return 1;
    }
}
