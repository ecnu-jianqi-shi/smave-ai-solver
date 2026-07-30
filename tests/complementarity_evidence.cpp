#include "smave/complementarity.hpp"
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

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2) {
            std::cerr << "usage: smave_complementarity_evidence OUTPUT_DIRECTORY\n";
            return 2;
        }
        const std::filesystem::path directory(argv[1]);
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory);
        const auto source = directory / "ContactLcp.mo";
        std::ofstream(source)
            << "model ContactLcp\n"
            << "Real lambda1(start=0); Real lambda2(start=0); Real lambda3(start=0);\n"
            << "equation\n"
            << "complementarity(lambda1,2*lambda1-lambda2-1);\n"
            << "complementarity(lambda2,-lambda1+2*lambda2-lambda3+0.25);\n"
            << "complementarity(lambda3,-lambda2+2*lambda3-0.5);\n"
            << "end ContactLcp;\n";
        const auto model = smave::compile_complementarity(source);
        const auto ir_path = directory / "contact.lcp";
        model.write(ir_path);
        const auto restored = smave::ComplementarityIR::read(ir_path);
        const auto assessment = smave::assess_equation(restored);
        const auto plan = smave::route_complementarity(restored);
        smave::RoutingConfig restricted_routing;
        restricted_routing.expert_allowlist.insert(
            "fischer-burmeister-newton-cpu-v1");
        const auto restricted_plan = smave::route_complementarity(
            restored, restricted_routing);
        require(
            assessment.equation_family ==
                "strongly-monotone-linear-complementarity" &&
                assessment.unknown_count == 3 && assessment.structural_nonzeros == 7 &&
                plan.steps.size() == 3 &&
                plan.steps[0].expert_version == "projected-gauss-seidel-cpu-v1" &&
                plan.steps[1].expert_version ==
                    "fischer-burmeister-newton-cpu-v1" &&
                plan.steps[2].expert_version ==
                    "enumerated-active-set-terminal-cpu-v1" &&
                restricted_plan.steps.size() == 2 &&
                restricted_plan.steps.front().expert_version ==
                    "fischer-burmeister-newton-cpu-v1" &&
                restricted_plan.steps.back().expert_version ==
                    "enumerated-active-set-terminal-cpu-v1",
            "Equation Expert did not create the complementarity backend cascade");

        const auto pgs = smave::solve_complementarity(restored);
        require(pgs.success && pgs.accepted_backend ==
                    "projected-gauss-seidel-cpu-v1" &&
                    !pgs.terminal_fallback_used && pgs.attempts.size() == 1,
                "projected complementarity path did not pass the original gate");

        smave::ComplementarityTolerance newton_tolerance;
        newton_tolerance.maximum_pgs_iterations = 0;
        const auto newton = smave::solve_complementarity(restored, newton_tolerance);
        require(newton.success && newton.attempts.size() == 2 &&
                    newton.attempts.front().outcome == "rejected" &&
                    newton.accepted_backend ==
                        "fischer-burmeister-newton-cpu-v1" &&
                    !newton.terminal_fallback_used,
                "semismooth Newton did not recover from an original-state PGS rejection");

        auto fallback_tolerance = newton_tolerance;
        fallback_tolerance.maximum_newton_iterations = 0;
        const auto fallback = smave::solve_complementarity(
            restored, fallback_tolerance);
        require(fallback.success && fallback.attempts.size() == 3 &&
                    fallback.attempts[0].outcome == "rejected" &&
                    fallback.attempts[1].outcome == "rejected" &&
                    fallback.accepted_backend ==
                        "enumerated-active-set-terminal-cpu-v1" &&
                    fallback.terminal_fallback_used,
                "active-set terminal fallback did not restart from the original LCP");
        for (const auto& result : {pgs, newton, fallback}) {
            require(result.solution.size() == 3 &&
                        std::abs(result.solution[0] - 0.75) < 1.0e-7 &&
                        std::abs(result.solution[1] - 0.5) < 1.0e-7 &&
                        std::abs(result.solution[2] - 0.5) < 1.0e-7 &&
                        result.attempts.back().primal_violation <= 1.0e-8 &&
                        result.attempts.back().dual_violation <= 1.0e-8 &&
                        result.attempts.back().complementarity_violation <= 1.0e-8 &&
                        result.attempts.back().equation_residual_inf <= 1.0e-14,
                    "accepted complementarity candidate failed analytic or original gates");
        }

        const auto nonmonotone_source = directory / "Nonmonotone.mo";
        std::ofstream(nonmonotone_source)
            << "model Nonmonotone\nReal z1; Real z2;\nequation\n"
            << "complementarity(z1,z1+2*z2-1);\n"
            << "complementarity(z2,2*z1+z2-1);\nend Nonmonotone;\n";
        bool nonmonotone_rejected{};
        try {
            (void)smave::compile_complementarity(nonmonotone_source);
        } catch (const std::invalid_argument&) {
            nonmonotone_rejected = true;
        }
        const auto nonlinear_source = directory / "Nonlinear.mo";
        std::ofstream(nonlinear_source)
            << "model Nonlinear\nReal z;\nequation\n"
            << "complementarity(z,z*z-1);\nend Nonlinear;\n";
        bool nonlinear_rejected{};
        try {
            (void)smave::compile_complementarity(nonlinear_source);
        } catch (const std::invalid_argument&) {
            nonlinear_rejected = true;
        }
        require(nonmonotone_rejected && nonlinear_rejected,
                "unsupported complementarity families were not rejected at compile time");

        smave::write_complementarity_report(
            restored, pgs, directory / "pgs-report.txt");
        smave::write_complementarity_report(
            restored, newton, directory / "newton-report.txt");
        smave::write_complementarity_report(
            restored, fallback, directory / "fallback-report.txt");
        std::ofstream evidence(directory / "evidence.txt");
        evidence << "SMAVE_COMPLEMENTARITY_EVIDENCE 1\n"
                 << "SUCCESS 1\n"
                 << "FAMILY \"" << assessment.equation_family << "\"\n"
                 << "PLAN \"" << plan.plan_id << "\"\n"
                 << "PGS_ACCEPTED 1\n"
                 << "NEWTON_AFTER_PGS_REJECTION 1\n"
                 << "ACTIVE_SET_AFTER_TWO_REJECTIONS 1\n"
                 << "ORIGINAL_EQUATION_GATE 1\n"
                 << "INEQUALITY_GATE 1\n"
                 << "COMPLEMENTARITY_GATE 1\n"
                 << "NONMONOTONE_REJECTED 1\n"
                 << "NONLINEAR_REJECTED 1\n";
        std::cout << "complementarity evidence passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "complementarity evidence failure: " << error.what() << '\n';
        return 1;
    }
}
