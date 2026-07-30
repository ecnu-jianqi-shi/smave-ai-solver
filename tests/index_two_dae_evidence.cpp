#include "smave/high_index_dae.hpp"
#include "smave/routing.hpp"

#include <cmath>
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
    const std::string& name,
    const std::string& first_dynamic,
    const std::string& constraint = "q=0") {
    const auto path = directory / (name + ".mo");
    std::ofstream(path)
        << "model " << name << "\n"
        << "Real q(start=0.2); Real v(start=1); Real lambda(start=0);\n"
        << "equation\n"
        << "der(q)=" << first_dynamic << ";\n"
        << "der(v)=-q;\n"
        << constraint << ";\n"
        << "end " << name << ";\n";
    return path;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2) {
            std::cerr << "usage: smave_index_two_dae_evidence OUTPUT_DIRECTORY\n";
            return 2;
        }
        const std::filesystem::path directory(argv[1]);
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory);

        const auto source = write_model(
            directory, "IndexTwoConstraint", "v+lambda");
        const auto model = smave::compile_index_two_dae(source);
        const auto ir_path = directory / "model.index2";
        model.write(ir_path);
        const auto restored = smave::IndexTwoDaeIR::read(ir_path);
        const auto assessment = smave::assess_equation(restored);
        const auto plan = smave::route_index_two_dae(restored);
        smave::RoutingConfig restricted;
        restricted.expert_allowlist.insert("not-an-index2-expert");
        const auto restricted_plan = smave::route_index_two_dae(restored, restricted);
        require(
            assessment.equation_family ==
                "hessenberg-index2-affine-constraint-dae" &&
                assessment.unknown_count == 3 && plan.steps.size() == 2 &&
                plan.steps.front().expert_version ==
                    "index2-differentiated-constraint-newton-cpu-v1" &&
                plan.steps.back().expert_version ==
                    "index2-dense-kkt-terminal-cpu-v1" &&
                restricted_plan.steps.size() == 1 &&
                restricted_plan.steps.front().expert_version ==
                    "index2-dense-kkt-terminal-cpu-v1",
            "Equation Expert did not preserve the index-2 terminal fallback");
        const auto result = smave::simulate_index_two_dae(
            restored, 0.3, 0.1);
        require(result.success && !result.terminal_fallback_used &&
                    result.solver_backend ==
                        "index2-differentiated-constraint-newton-cpu-v1" &&
                    result.steps.size() == 3 && result.hidden_rank_checks == 4 &&
                    result.initialization_iterations == 1 &&
                    result.initialization_constraint_residual_inf <= 1.0e-12 &&
                    result.initialization_hidden_residual_inf <= 1.0e-12 &&
                    std::abs(result.initial_state.at("q")) <= 1.0e-12 &&
                    std::abs(result.initial_multipliers.at("lambda") + 1.0) <= 1.0e-12 &&
                    std::abs(result.final_state.at("q")) <= 1.0e-12 &&
                    std::abs(result.final_state.at("v") - 1.0) <= 1.0e-12 &&
                    std::abs(result.final_multipliers.at("lambda") + 1.0) <= 1.0e-12,
                "index-2 differentiated-constraint path failed analytic evidence");
        for (const auto& step : result.steps) {
            require(step.dynamic_residual_inf <= 1.0e-12 &&
                        step.constraint_residual_inf <= 1.0e-12 &&
                        step.hidden_residual_inf <= 1.0e-12 &&
                        step.hidden_rank_margin >= 1.0,
                    "index-2 step failed original or hidden constraint gates");
        }

        const auto nonsmooth_source = write_model(
            directory, "IndexTwoNonsmooth", "v+lambda+abs(q)+time");
        const auto nonsmooth_model = smave::compile_index_two_dae(nonsmooth_source);
        const auto fallback = smave::simulate_index_two_dae(
            nonsmooth_model, 0.2, 0.1);
        require(fallback.success && fallback.terminal_fallback_used &&
                    fallback.rejected_steps >= 1 && fallback.steps.size() == 2 &&
                    fallback.steps.front().solver_backend ==
                        "index2-dense-kkt-terminal-cpu-v1" &&
                    std::abs(fallback.final_state.at("q")) <= 1.0e-10 &&
                    std::abs(fallback.final_multipliers.at("lambda") + 1.2) <= 1.0e-8,
                "index-2 dense KKT fallback did not restart from committed state");

        bool hidden_rank_rejected{};
        try {
            (void)smave::compile_index_two_dae(write_model(
                directory, "IndexTwoHiddenSingular", "v"));
        } catch (const std::invalid_argument&) {
            hidden_rank_rejected = true;
        }
        bool nonlinear_constraint_rejected{};
        try {
            (void)smave::compile_index_two_dae(write_model(
                directory, "IndexTwoNonlinearConstraint", "v+lambda", "q*q=0"));
        } catch (const std::invalid_argument&) {
            nonlinear_constraint_rejected = true;
        }
        require(hidden_rank_rejected && nonlinear_constraint_rejected,
                "unsupported or rank-deficient index-2 systems were accepted");

        smave::write_index_two_dae_report(
            restored, result, directory / "report.txt");
        smave::write_index_two_dae_report(
            nonsmooth_model, fallback, directory / "fallback-report.txt");
        std::ofstream evidence(directory / "evidence.txt");
        evidence << "SMAVE_INDEX2_DAE_EVIDENCE 1\n"
                 << "SUCCESS 1\n"
                 << "FAMILY \"" << assessment.equation_family << "\"\n"
                 << "PLAN \"" << plan.plan_id << "\"\n"
                 << "CONSTRAINT_DIFFERENTIATIONS 1\n"
                 << "INITIAL_PROJECTION 1\n"
                 << "HIDDEN_CONSTRAINT_INITIALIZATION 1\n"
                 << "AD_KKT_STEPS " << result.steps.size() << "\n"
                 << "DENSE_KKT_FALLBACK_STEPS " << fallback.steps.size() << "\n"
                 << "ORIGINAL_DYNAMICS_GATE 1\n"
                 << "ORIGINAL_CONSTRAINT_GATE 1\n"
                 << "HIDDEN_CONSTRAINT_GATE 1\n"
                 << "HIDDEN_RANK_GATE 1\n"
                 << "HIDDEN_SINGULAR_REJECTED 1\n"
                 << "NONLINEAR_CONSTRAINT_REJECTED 1\n";
        std::cout << "index-2 DAE evidence passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "index-2 DAE evidence failure: " << error.what() << '\n';
        return 1;
    }
}
