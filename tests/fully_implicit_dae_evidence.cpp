#include "smave/dae.hpp"
#include "smave/routing.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::filesystem::path write_model(const std::filesystem::path& directory) {
    constexpr std::size_t width = 33;
    constexpr std::size_t state_count = width * width;
    const auto source = directory / "LargeFullyImplicitDae33.mo";
    std::ofstream model(source);
    model << "model LargeFullyImplicitDae33\n";
    for (std::size_t index = 0; index < state_count; ++index) {
        model << "  Real x" << index + 1 << "(start=0, nominal=1);\n";
    }
    model << "  Real z(start=0, nominal=1);\n"
          << "equation\n";
    for (std::size_t row = 0; row < width; ++row) {
        for (std::size_t column = 0; column < width; ++column) {
            const auto index = row * width + column;
            const auto next = (index + 1) % state_count;
            model << "  2*der(x" << index + 1 << ") + 0.1*der(x" << next + 1
                  << ") + 4*x" << index + 1 << " + 0.01*x" << index + 1
                  << "^3 + 0.001*z";
            if (row > 0) model << " - x" << index + 1 - width;
            if (row + 1 < width) model << " - x" << index + 1 + width;
            if (column > 0) model << " - x" << index;
            if (column + 1 < width) model << " - x" << index + 2;
            model << " = 1;\n";
        }
    }
    model << "  z - x1 = 0;\n"
          << "end LargeFullyImplicitDae33;\n";
    return source;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2) {
            std::cerr << "usage: smave_fully_implicit_dae_evidence OUTPUT_DIRECTORY\n";
            return 2;
        }
        const std::filesystem::path directory(argv[1]);
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory);
        constexpr std::size_t state_count = 33 * 33;
        constexpr std::size_t joint_count = state_count + 1;
        const auto source = write_model(directory);

        const auto compile_started = std::chrono::steady_clock::now();
        const auto model = smave::compile_fully_implicit_dae(
            source, "LargeFullyImplicitDae33");
        const auto compile_us = std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - compile_started).count();
        require(
            model.schema_version == smave::kFullyImplicitDaeSchemaVersion &&
                model.structural_class == "fully-implicit-first-order-candidate" &&
                model.states.size() == state_count && model.algebraics.size() == 1 &&
                model.equations.size() == joint_count &&
                model.equations.front().residual.find("__smave_der_x1") !=
                    std::string::npos &&
                model.equations.front().residual.find("__smave_der_x2") !=
                    std::string::npos,
            "fully implicit frontend lost derivative coupling or variable structure");

        const auto ir_path = directory / "model.implicit-dae";
        model.write(ir_path);
        const auto restored = smave::FullyImplicitDaeIR::read(ir_path);
        require(
            restored.source_hash == model.source_hash &&
                restored.states.size() == model.states.size() &&
                restored.equations.front().residual == model.equations.front().residual,
            "fully implicit DAE IR round-trip changed the model contract");
        const auto assessment = smave::assess_equation(restored);
        const auto plan = smave::route_fully_implicit_dae(restored);
        require(
            assessment.equation_family ==
                    "dae-fully-implicit-first-order-smooth" &&
                assessment.unknown_count == joint_count &&
                assessment.scale_class == "large" &&
                !plan.steps.empty() &&
                plan.steps.front().expert_version ==
                    "fully-implicit-csr-newton-krylov-cpu-v1" &&
                plan.terminal_fallback ==
                    "fully-implicit-dense-newton-cpu-v1",
            "Equation Expert did not classify and route fully implicit DAE");
        bool rejected_missing_fallback = false;
        try {
            (void)smave::route_fully_implicit_dae(
                restored, smave::RoutingConfig{.require_original_fallback = false});
        } catch (const std::invalid_argument&) {
            rejected_missing_fallback = true;
        }
        require(
            rejected_missing_fallback,
            "fully implicit Router allowed terminal fallback removal");

        const auto event_source = directory / "UnsupportedImplicitDerivativeGuard.mo";
        std::ofstream(event_source)
            << "model UnsupportedImplicitDerivativeGuard\n"
            << "Real x;\n"
            << "equation\n"
            << "der(x)=1;\n"
            << "when der(x)>=1 then reinit(x,0); end when;\n"
            << "end UnsupportedImplicitDerivativeGuard;\n";
        bool rejected_event = false;
        try {
            (void)smave::compile_fully_implicit_dae(event_source);
        } catch (const std::invalid_argument&) {
            rejected_event = true;
        }
        require(
            rejected_event,
            "fully implicit frontend accepted derivative-dependent event guard");

        smave::FullyImplicitDaeIR missing_derivative;
        missing_derivative.model_id = "MissingDerivative";
        missing_derivative.source_hash = "fixture";
        missing_derivative.states.push_back({"x", 0.0, 1.0});
        missing_derivative.equations.push_back({"equation-1", "x", {"x"}});
        bool rejected_missing_derivative = false;
        try {
            missing_derivative.validate();
        } catch (const std::invalid_argument&) {
            rejected_missing_derivative = true;
        }
        require(
            rejected_missing_derivative,
            "fully implicit IR accepted a state without derivative incidence");

        smave::FullyImplicitDaeIR singular_incidence;
        singular_incidence.model_id = "SingularIncidence";
        singular_incidence.source_hash = "fixture";
        singular_incidence.states = {
            {"x1", 0.0, 1.0}, {"x2", 0.0, 1.0}, {"x3", 0.0, 1.0}};
        singular_incidence.equations = {
            {"equation-1", "__smave_der_x1", {"__smave_der_x1"}},
            {"equation-2", "2*__smave_der_x1", {"__smave_der_x1"}},
            {"equation-3", "__smave_der_x2+__smave_der_x3",
             {"__smave_der_x2", "__smave_der_x3"}}};
        bool rejected_singular_incidence = false;
        try {
            singular_incidence.validate();
        } catch (const std::invalid_argument&) {
            rejected_singular_incidence = true;
        }
        require(
            rejected_singular_incidence,
            "fully implicit IR accepted structurally singular incidence");

        const auto zero_residual = smave::evaluate_fully_implicit_dae_step_residual(
            restored, std::vector<double>(state_count),
            std::vector<double>(state_count), std::vector<double>(1), 0.01, 0.01);
        require(
            zero_residual.size() == joint_count &&
                std::abs(zero_residual.front() + 1.0) < 1.0e-12 &&
                std::abs(zero_residual.back()) < 1.0e-12,
            "fully implicit backward-Euler residual does not bind derivative history");
        const auto zero_initial_residual =
            smave::evaluate_fully_implicit_dae_initial_residual(
                restored, std::vector<double>(state_count),
                std::vector<double>(state_count), std::vector<double>(1));
        require(
            zero_initial_residual.size() == joint_count &&
                std::abs(zero_initial_residual.front() + 1.0) < 1.0e-12 &&
                std::abs(zero_initial_residual.back()) < 1.0e-12,
            "fully implicit initialization residual lost derivative/algebraic context");

        const auto solve_started = std::chrono::steady_clock::now();
        const auto result = smave::simulate_fully_implicit_dae(restored, 0.02, 0.01);
        const auto solve_us = std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - solve_started).count();
        require(
            result.success && result.steps.size() == 2 &&
                !result.plan_id.empty() &&
                result.plan_id == plan.plan_id &&
                result.solver_backend ==
                    "fully-implicit-csr-newton-krylov-cpu-v1" &&
                result.backend_chain == std::vector<std::string>{
                    "fixed-state-derivative-algebraic-initializer",
                    "colored-directional-ad-csr-jacobian",
                    "classic-ic0-or-ilu0-preconditioner",
                    "pcg-ic0-or-gmres-ilu0-linear-solver",
                    "damped-newton-corrector",
                    "directional-root-localizer",
                    "atomic-reinit-consistency-projector",
                    "original-dae-residual-gate"} &&
                result.terminal_fallback ==
                    "fully-implicit-dense-newton-cpu-v1" &&
                result.sparse_initialization &&
                result.initialization_iterations > 0 &&
                result.initialization_krylov_iterations > 0 &&
                result.initialization_residual_inf < 1.0e-6 &&
                result.initialization_jacobian_nonzeros > joint_count &&
                result.initialization_jacobian_storage_bytes <
                    joint_count * joint_count * sizeof(double) / 10 &&
                result.initialization_jacobian_colors > 0 &&
                result.initialization_jacobian_colors < 10 &&
                result.initialization_jacobian_evaluation_batches ==
                    result.initialization_jacobian_colors *
                        static_cast<std::size_t>(result.initialization_iterations) &&
                result.initialization_jacobian_ad_batches ==
                    result.initialization_jacobian_evaluation_batches &&
                result.initialization_jacobian_fd_fallback_batches == 0 &&
                result.initialization_inner_backend == "gmres-ilu0-cpu-v1" &&
                result.dense_initialization_fallbacks == 0 &&
                result.initial_derivatives.size() == state_count &&
                result.sparse_newton_steps == 2 &&
                result.sparse_newton_iterations > 0 &&
                result.sparse_krylov_iterations > 0 &&
                result.sparse_jacobian_nonzeros > joint_count &&
                result.sparse_jacobian_storage_bytes <
                    joint_count * joint_count * sizeof(double) / 10 &&
                result.sparse_jacobian_colors > 0 &&
                result.sparse_jacobian_colors < 20 &&
                result.sparse_jacobian_evaluation_batches ==
                    result.sparse_jacobian_colors * result.sparse_newton_iterations &&
                result.sparse_jacobian_ad_batches ==
                    result.sparse_jacobian_evaluation_batches &&
                result.sparse_jacobian_fd_fallback_batches == 0 &&
                result.sparse_inner_backend == "gmres-ilu0-cpu-v1" &&
                result.dense_step_fallbacks == 0 &&
                result.maximum_residual_inf < 1.0e-6,
            "fully implicit DAE did not use gated sparse Newton-Krylov");

        smave::FullyImplicitDaeIR inconsistent_initialization;
        inconsistent_initialization.model_id = "InconsistentInitialization";
        inconsistent_initialization.source_hash = "fixture";
        inconsistent_initialization.states = {
            {"x1", 0.0, 1.0}, {"x2", 0.0, 1.0}};
        inconsistent_initialization.equations = {
            {"equation-1", "__smave_der_x1", {"__smave_der_x1"}},
            {"equation-2", "2*__smave_der_x1+0*__smave_der_x2-1",
             {"__smave_der_x1", "__smave_der_x2"}}};
        const auto rejected_initialization = smave::simulate_fully_implicit_dae(
            inconsistent_initialization, 0.01, 0.01);
        require(
            !rejected_initialization.success &&
                rejected_initialization.final_time == 0.0 &&
                rejected_initialization.steps.empty(),
            "failed fully implicit initialization advanced committed time");

        smave::write_fully_implicit_dae_report(restored, result, directory / "report.txt");
        std::ofstream evidence(directory / "evidence.txt");
        evidence << "SMAVE_FULLY_IMPLICIT_DAE_EVIDENCE 2\n"
                 << "FAMILY fully-implicit-first-order-coupled-mass-reaction-diffusion\n"
                 << "STATES " << state_count << '\n'
                 << "ALGEBRAICS 1\n"
                 << "JOINT_UNKNOWNS " << joint_count << '\n'
                 << "NONDIAGONAL_MASS_COUPLING 1\n"
                 << "PLAN_ID " << result.plan_id << '\n'
                 << "EQUATION_FAMILY " << assessment.equation_family << '\n'
                 << "SOLVER_BACKEND " << result.solver_backend << '\n'
                 << "TERMINAL_FALLBACK " << result.terminal_fallback << '\n'
                 << "INITIALIZATION_JACOBIAN_NONZEROS "
                 << result.initialization_jacobian_nonzeros << '\n'
                 << "INITIALIZATION_CSR_JACOBIAN_BYTES "
                 << result.initialization_jacobian_storage_bytes << '\n'
                 << "INITIALIZATION_JACOBIAN_COLORS "
                 << result.initialization_jacobian_colors << '\n'
                 << "INITIALIZATION_JACOBIAN_EVALUATION_BATCHES "
                 << result.initialization_jacobian_evaluation_batches << '\n'
                 << "INITIALIZATION_JACOBIAN_AD_BATCHES "
                 << result.initialization_jacobian_ad_batches << '\n'
                 << "INITIALIZATION_JACOBIAN_FD_FALLBACK_BATCHES "
                 << result.initialization_jacobian_fd_fallback_batches << '\n'
                 << "INITIALIZATION_KRYLOV_ITERATIONS "
                 << result.initialization_krylov_iterations << '\n'
                 << "INITIALIZATION_INNER_BACKEND "
                 << result.initialization_inner_backend << '\n'
                 << "INITIALIZATION_RESIDUAL "
                 << result.initialization_residual_inf << '\n'
                 << "DENSE_INITIALIZATION_FALLBACKS "
                 << result.dense_initialization_fallbacks << '\n'
                 << "JACOBIAN_NONZEROS " << result.sparse_jacobian_nonzeros << '\n'
                 << "DENSE_JACOBIAN_BYTES "
                 << joint_count * joint_count * sizeof(double) << '\n'
                 << "CSR_JACOBIAN_BYTES "
                 << result.sparse_jacobian_storage_bytes << '\n'
                 << "JACOBIAN_COLORS " << result.sparse_jacobian_colors << '\n'
                 << "JACOBIAN_EVALUATION_BATCHES "
                 << result.sparse_jacobian_evaluation_batches << '\n'
                 << "JACOBIAN_AD_BATCHES " << result.sparse_jacobian_ad_batches << '\n'
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
                 << "MAXIMUM_RESIDUAL " << result.maximum_residual_inf << '\n'
                 << "DENSE_STEP_FALLBACKS " << result.dense_step_fallbacks << '\n'
                 << "ORIGINAL_RESIDUAL_GATE 1\n"
                 << "FAILED_INITIALIZATION_TIME_ADVANCE 0\n"
                 << "TERMINAL_DENSE_NEWTON_FALLBACK_RETAINED 1\n";
        std::cout << "fully implicit DAE evidence passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "fully implicit DAE evidence failed: " << error.what() << '\n';
        return 1;
    }
}
