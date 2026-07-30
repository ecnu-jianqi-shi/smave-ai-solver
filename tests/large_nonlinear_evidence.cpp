#include "smave/compiler.hpp"
#include "smave/runtime.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::filesystem::path write_model(
    const std::filesystem::path& directory,
    std::string_view model_name,
    std::size_t width,
    bool nonsymmetric) {
    const auto size = width * width;
    const auto source = directory / (std::string(model_name) + ".mo");
    std::ofstream model(source);
    model << "model " << model_name << '\n';
    for (std::size_t index = 0; index < size; ++index) {
        model << "  Real x" << index + 1 << "(start=0, nominal=10);\n";
    }
    model << "equation\n";
    for (std::size_t row = 0; row < width; ++row) {
        for (std::size_t column = 0; column < width; ++column) {
            const auto index = row * width + column;
            model << "  4*x" << index + 1 << " + 0.01*x" << index + 1 << "^3";
            if (row > 0) model << " - x" << index + 1 - width;
            if (row + 1 < width) model << " - x" << index + 1 + width;
            if (column > 0) {
                model << (nonsymmetric ? " - 1.25*x" : " - x") << index;
            }
            if (column + 1 < width) {
                model << (nonsymmetric ? " - 0.75*x" : " - x") << index + 2;
            }
            model << " = 1;\n";
        }
    }
    model << "end " << model_name << ";\n";
    return source;
}

std::filesystem::path write_dense_coupled_model(
    const std::filesystem::path& directory) {
    constexpr std::size_t size = 1025;
    constexpr std::size_t coupling_width = 52;
    const auto source = directory / "LargeDenseCoupledNonlinear.mo";
    std::ofstream model(source);
    model << "model LargeDenseCoupledNonlinear\n";
    for (std::size_t index = 0; index < size; ++index) {
        model << "  Real x" << index + 1 << "(start=0, nominal=1);\n";
    }
    model << "equation\n";
    for (std::size_t row = 0; row < size; ++row) {
        if (row == 0) {
            model << "  x2 + 0.01*abs(x1)*abs(x1)";
        } else {
            model << "  x" << row + 1 << " + 0.01*x" << row + 1 << "^3";
        }
        for (std::size_t offset = 1; offset <= coupling_width; ++offset) {
            model << " + 0.001*x" << (row + offset) % size + 1;
        }
        model << " = 1;\n";
    }
    model << "end LargeDenseCoupledNonlinear;\n";
    return source;
}

}

int main(int argc, char** argv) {
    try {
        if (argc != 2) {
            std::cerr << "usage: smave_large_nonlinear_evidence OUTPUT_DIRECTORY\n";
            return 2;
        }
        const std::filesystem::path directory(argv[1]);
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory);
        constexpr std::size_t width = 33;
        constexpr std::size_t size = width * width;
        const auto source = write_model(
            directory, "LargeNonlinearPoisson33", width, false);

        const auto compile_started = std::chrono::steady_clock::now();
        const auto ir = smave::compile_model(source, "LargeNonlinearPoisson33");
        const auto compile_us = std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - compile_started).count();
        require(ir.blocks.size() == 1 && !ir.blocks.front().linear &&
                    ir.blocks.front().unknowns.size() == size,
                "large nonlinear source did not compile to one nonlinear SCC");
        const smave::Runtime runtime(ir);
        const auto solve_started = std::chrono::steady_clock::now();
        const auto outcome = runtime.solve({}, directory / "traces");
        const auto solve_us = std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - solve_started).count();
        require(outcome.success && outcome.blocks.size() == 1,
                "large nonlinear Runtime solve failed");
        const auto& block = outcome.blocks.front();
        require(block.path == smave::SolvePath::corrected_accept &&
                    !block.attempt_records.empty() &&
                    block.attempt_records.front().expert_version ==
                        "newton-krylov-csr-cpu-v1" &&
                    block.attempt_records.front().outcome == "accepted" &&
                    block.inner_linear_backend == "pcg-ic0-cpu-v1" &&
                    block.inner_jacobian_nonzeros ==
                        ir.blocks.front().jacobian_sparsity.nonzeros() &&
                    block.inner_jacobian_colors > 0 &&
                    block.inner_jacobian_colors < block.inner_jacobian_nonzeros / 100 &&
                    block.inner_jacobian_evaluation_batches ==
                        block.inner_jacobian_colors *
                            static_cast<std::size_t>(block.expert_iterations) &&
                    block.inner_jacobian_ad_batches ==
                        block.inner_jacobian_evaluation_batches &&
                    block.inner_jacobian_fd_fallback_batches == 0 &&
                    block.inner_jacobian_storage_bytes <
                        size * size * sizeof(double) / 10 &&
                    block.krylov_iterations > 0 && block.expert_iterations > 0 &&
                    block.gate.decision == smave::GateDecision::direct_accept &&
                    outcome.fallback_count == 0,
                "Equation Expert did not execute and gate CSR Newton-Krylov");

        const auto nonsymmetric_source = write_model(
            directory, "LargeNonlinearConvectionDiffusion33", width, true);
        const auto nonsymmetric_ir = smave::compile_model(
            nonsymmetric_source, "LargeNonlinearConvectionDiffusion33");
        const auto nonsymmetric_outcome = smave::Runtime(nonsymmetric_ir).solve(
            {}, directory / "nonsymmetric-traces");
        require(nonsymmetric_outcome.success &&
                    nonsymmetric_outcome.blocks.size() == 1 &&
                    nonsymmetric_outcome.blocks.front().path ==
                        smave::SolvePath::corrected_accept &&
                    nonsymmetric_outcome.blocks.front().inner_linear_backend ==
                        "gmres-ilu0-cpu-v1" &&
                    nonsymmetric_outcome.blocks.front().inner_jacobian_nonzeros ==
                        nonsymmetric_ir.blocks.front().jacobian_sparsity.nonzeros() &&
                    nonsymmetric_outcome.blocks.front().inner_jacobian_colors > 0 &&
                    nonsymmetric_outcome.blocks.front().gate.decision ==
                        smave::GateDecision::direct_accept &&
                    nonsymmetric_outcome.fallback_count == 0,
                "nonsymmetric nonlinear Jacobian did not route to CSR GMRES+ILU(0)");

        const auto dense_source = write_dense_coupled_model(directory);
        const auto dense_ir = smave::compile_model(
            dense_source, "LargeDenseCoupledNonlinear");
        require(dense_ir.blocks.size() == 1 &&
                    dense_ir.blocks.front().unknowns.size() == 1025 &&
                    dense_ir.blocks.front().jacobian_sparsity.nonzeros() ==
                        1025 * 53,
                "dense-coupled nonlinear source lost expected SCC structure");
        const auto dense_registry = smave::make_default_registry(dense_ir);
        const auto dense_bundle = smave::make_default_bundle(dense_ir);
        const auto dense_candidates = smave::CompileRouter{}.lookup(
            dense_ir.blocks.front(), dense_registry, dense_bundle);
        const auto dense_plan = smave::RuntimeRouter{}.route(
            dense_ir.blocks.front(), {}, dense_candidates, dense_registry, dense_bundle);
        require(
            !dense_plan.steps.empty() &&
                dense_plan.steps.front().expert_version ==
                    "newton-krylov-jfnk-cpu-v1" &&
                dense_plan.steps.front().backend_chain ==
                    std::vector<std::string>{
                        "damped-newton-corrector",
                        "directional-ad-jacobian-vector-product",
                        "matrix-free-diagonal-preconditioner",
                        "restarted-gmres-true-residual",
                        "runtime-residual-constraint-gate"} &&
                !dense_plan.terminal_fallback.empty(),
            "Equation Expert did not expose the composite JFNK backend chain");
        const auto dense_outcome = smave::Runtime(dense_ir).solve(
            {}, directory / "matrix-free-traces");
        require(dense_outcome.success && dense_outcome.blocks.size() == 1,
                "matrix-free dense-coupled nonlinear solve failed");
        const auto& dense_block = dense_outcome.blocks.front();
        require(
            dense_block.path == smave::SolvePath::corrected_accept &&
                !dense_block.attempt_records.empty() &&
                dense_block.attempt_records.front().expert_version ==
                    "newton-krylov-jfnk-cpu-v1" &&
                dense_block.attempt_records.front().outcome == "accepted" &&
                dense_block.inner_matrix_free &&
                dense_block.inner_jacobian_storage_bytes == 0 &&
                dense_block.inner_jacobian_nonzeros == 0 &&
                dense_block.inner_operator_applications > 0 &&
                dense_block.inner_operator_ad_applications > 0 &&
                dense_block.inner_operator_fd_fallback_applications > 0 &&
                dense_block.inner_operator_ad_applications +
                        dense_block.inner_operator_fd_fallback_applications ==
                    dense_block.inner_operator_applications &&
                dense_block.inner_linear_backend ==
                    "jfnk-gmres-diagonal-cpu-v1" &&
                dense_block.inner_preconditioner_storage_bytes ==
                    1025 * sizeof(double) &&
                dense_block.inner_preconditioner_setup_entries > 0 &&
                dense_block.inner_preconditioner_ad_entries > 0 &&
                dense_block.inner_preconditioner_fd_fallback_entries > 0 &&
                dense_block.inner_preconditioner_ad_entries +
                        dense_block.inner_preconditioner_fd_fallback_entries ==
                    dense_block.inner_preconditioner_setup_entries &&
                dense_block.inner_preconditioner_identity_entries > 0 &&
                dense_block.krylov_iterations > 0 &&
                dense_block.gate.decision == smave::GateDecision::direct_accept &&
                dense_outcome.fallback_count == 0,
            "Equation Expert did not execute and gate matrix-free JFNK");

        std::ofstream report(directory / "evidence.txt");
        report << "SMAVE_LARGE_NONLINEAR_EVIDENCE 5\n"
               << "FAMILY nonlinear-poisson-cubic\n"
               << "UNKNOWNS " << size << '\n'
               << "JACOBIAN_NONZEROS " << block.inner_jacobian_nonzeros << '\n'
               << "JACOBIAN_COLORS " << block.inner_jacobian_colors << '\n'
               << "JACOBIAN_EVALUATION_BATCHES "
               << block.inner_jacobian_evaluation_batches << '\n'
               << "JACOBIAN_AD_BATCHES " << block.inner_jacobian_ad_batches << '\n'
               << "JACOBIAN_FD_FALLBACK_BATCHES "
               << block.inner_jacobian_fd_fallback_batches << '\n'
               << "DENSE_JACOBIAN_BYTES " << size * size * sizeof(double) << '\n'
               << "CSR_JACOBIAN_BYTES " << block.inner_jacobian_storage_bytes << '\n'
               << "COMPILE_US " << compile_us << '\n'
               << "SOLVE_US " << solve_us << '\n'
               << "NEWTON_ITERATIONS " << block.expert_iterations << '\n'
               << "INNER_KRYLOV_ITERATIONS " << block.krylov_iterations << '\n'
               << "INNER_BACKEND " << block.inner_linear_backend << '\n'
               << "GATE_DECISION direct_accept\n"
               << "FALLBACK_COUNT " << outcome.fallback_count << '\n'
               << "NONSYMMETRIC_UNKNOWNS " << size << '\n'
               << "NONSYMMETRIC_INNER_BACKEND "
               << nonsymmetric_outcome.blocks.front().inner_linear_backend << '\n'
               << "NONSYMMETRIC_NEWTON_ITERATIONS "
               << nonsymmetric_outcome.blocks.front().expert_iterations << '\n'
               << "NONSYMMETRIC_INNER_KRYLOV_ITERATIONS "
               << nonsymmetric_outcome.blocks.front().krylov_iterations << '\n'
               << "NONSYMMETRIC_GATE_DECISION direct_accept\n"
               << "JFNK_UNKNOWNS 1025\n"
               << "JFNK_STRUCTURAL_NONZEROS "
               << dense_ir.blocks.front().jacobian_sparsity.nonzeros() << '\n'
               << "JFNK_EXPLICIT_JACOBIAN_BYTES "
               << dense_block.inner_jacobian_storage_bytes << '\n'
               << "JFNK_OPERATOR_APPLICATIONS "
               << dense_block.inner_operator_applications << '\n'
               << "JFNK_OPERATOR_AD_APPLICATIONS "
               << dense_block.inner_operator_ad_applications << '\n'
               << "JFNK_OPERATOR_FD_FALLBACK_APPLICATIONS "
               << dense_block.inner_operator_fd_fallback_applications << '\n'
               << "JFNK_PRECONDITIONER_STORAGE_BYTES "
               << dense_block.inner_preconditioner_storage_bytes << '\n'
               << "JFNK_PRECONDITIONER_SETUP_ENTRIES "
               << dense_block.inner_preconditioner_setup_entries << '\n'
               << "JFNK_PRECONDITIONER_AD_ENTRIES "
               << dense_block.inner_preconditioner_ad_entries << '\n'
               << "JFNK_PRECONDITIONER_FD_FALLBACK_ENTRIES "
               << dense_block.inner_preconditioner_fd_fallback_entries << '\n'
               << "JFNK_PRECONDITIONER_IDENTITY_ENTRIES "
               << dense_block.inner_preconditioner_identity_entries << '\n'
               << "JFNK_INNER_BACKEND " << dense_block.inner_linear_backend << '\n'
               << "JFNK_GATE_DECISION direct_accept\n"
               << "TERMINAL_FALLBACK_RETAINED 1\n";
        std::cout << "large nonlinear evidence passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "large nonlinear evidence failed: " << error.what() << '\n';
        return 1;
    }
}
