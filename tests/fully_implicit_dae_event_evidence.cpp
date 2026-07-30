#include "smave/dae.hpp"
#include "smave/routing.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

double maximum_absolute(const std::vector<double>& values) {
    double result{};
    for (const auto value : values) result = std::max(result, std::abs(value));
    return result;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::invalid_argument("expected evidence output directory");
        const std::filesystem::path directory = argv[1];
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory);
        const auto source = directory / "FullyImplicitEvent.mo";
        std::ofstream model(source);
        model << "model FullyImplicitEvent\n"
              << "  Real x(start=1, nominal=1);\n"
              << "  Real y(start=0, nominal=1);\n"
              << "  Real z(start=1, nominal=1);\n"
              << "equation\n"
              << "  2*der(x) + 0.1*der(y) + x = 0;\n"
              << "  der(y) + y = 0;\n"
              << "  z - x - y = 0;\n"
              << "  when x <= 0.8 then\n"
              << "    reinit(x, 1.2);\n"
              << "  end when;\n"
              << "end FullyImplicitEvent;\n";
        model.close();

        const auto compiled = smave::compile_fully_implicit_dae(source);
        require(compiled.events.size() == 1, "fully implicit event was not compiled");
        const auto ir = directory / "event.ir";
        compiled.write(ir);
        const auto restored = smave::FullyImplicitDaeIR::read(ir);
        require(restored.events.size() == 1, "fully implicit event IR did not round-trip");
        const auto assessment = smave::assess_equation(restored);
        require(assessment.event_related, "EquationAssessment missed event partition");
        const auto plan = smave::route_fully_implicit_dae(restored);
        require(!plan.steps.empty(), "fully implicit event plan has no executable candidate");
        require(std::find(
                    plan.steps.front().backend_chain.begin(),
                    plan.steps.front().backend_chain.end(),
                    "directional-root-localizer") != plan.steps.front().backend_chain.end(),
                "fully implicit event plan lacks root localizer");
        require(std::find(
                    plan.steps.front().backend_chain.begin(),
                    plan.steps.front().backend_chain.end(),
                    "atomic-reinit-consistency-projector") !=
                    plan.steps.front().backend_chain.end(),
                "fully implicit event plan lacks reset projector");

        smave::DaeTolerance tolerance;
        tolerance.root_time = 1.0e-10;
        tolerance.guard = 1.0e-8;
        const auto result = smave::simulate_fully_implicit_dae(
            restored, 1.0, 0.25, tolerance);
        require(result.success, "fully implicit event simulation failed: " + result.message);
        require(result.events.size() == 1, "expected exactly one fully implicit event");
        require(result.event_root_solves == 1, "expected one event root solve");
        require(result.event_projection_solves == 1, "expected one event projection");
        require(result.maximum_guard_residual <= tolerance.guard,
                "event root guard exceeded tolerance");
        require(result.maximum_event_projection_residual_inf <= 1.0e-7,
                "event consistency projection residual exceeded tolerance");
        const auto& event = result.events.front();
        require(std::abs(event.pre_state.at("x") - 0.8) <= 2.0e-7,
                "event pre-state is not on the root");
        require(std::abs(event.post_state.at("x") - 1.2) <= 1.0e-12,
                "event reset was not atomic");
        require(std::abs(
                    event.post_algebraics.at("z") -
                    event.post_state.at("x") - event.post_state.at("y")) <= 1.0e-8,
                "event algebraic projection is inconsistent");
        const std::vector<double> post_state{
            event.post_state.at("x"), event.post_state.at("y")};
        const std::vector<double> post_derivative{-0.6, 0.0};
        const std::vector<double> post_algebraic{event.post_algebraics.at("z")};
        require(maximum_absolute(smave::evaluate_fully_implicit_dae_initial_residual(
                    restored, post_state, post_derivative, post_algebraic, event.time)) <= 1.0e-8,
                "post-event original DAE residual gate failed");

        const auto report = directory / "event-report.txt";
        smave::write_fully_implicit_dae_report(restored, result, report);
        std::ifstream report_stream(report);
        const std::string report_text{
            std::istreambuf_iterator<char>(report_stream),
            std::istreambuf_iterator<char>()};
        require(report_text.find("SMAVE_FULLY_IMPLICIT_DAE_REPORT 2") != std::string::npos,
                "event report schema is stale");
        require(report_text.find("EVENT_ROOT_SOLVES 1") != std::string::npos,
                "event report lacks root telemetry");
        require(report_text.find("EVENT \"event-1\"") != std::string::npos,
                "event report lacks committed event record");

        const auto rollback_source = directory / "FullyImplicitEventRollback.mo";
        std::ofstream rollback_model(rollback_source);
        rollback_model << "model FullyImplicitEventRollback\n"
                       << "  Real x(start=1, nominal=1);\n"
                       << "  Real z(start=1, nominal=1);\n"
                       << "equation\n"
                       << "  der(x) + x = 0;\n"
                       << "  z^2 - x = 0;\n"
                       << "  when x <= 0.8 then\n"
                       << "    reinit(x, -1);\n"
                       << "  end when;\n"
                       << "end FullyImplicitEventRollback;\n";
        rollback_model.close();
        const auto rollback = smave::simulate_fully_implicit_dae(
            smave::compile_fully_implicit_dae(rollback_source),
            1.0, 0.25, tolerance);
        require(!rollback.success, "inconsistent event projection was accepted");
        require(rollback.events.empty(), "failed event transaction leaked a record");
        require(rollback.final_time < 0.25,
                "failed event transaction advanced beyond the root boundary");
        require(rollback.final_state.at("x") > 0.8,
                "failed event transaction leaked reset state");

        std::cout << "fully implicit DAE event evidence passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
