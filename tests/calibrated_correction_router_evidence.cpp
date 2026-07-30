#include "smave/compiler.hpp"
#include "smave/competition.hpp"
#include "smave/routing.hpp"
#include "smave/runtime.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace {

class BudgetProbeExpert final : public smave::Expert {
public:
    [[nodiscard]] std::string version() const override {
        return "calibrated-budget-probe-v1";
    }

    [[nodiscard]] smave::Capability match(const smave::BlockIR& block) const override {
        return smave::Capability{
            .nonlinear = !block.linear,
            .backend_roles = {smave::BackendRole::initializer},
            .evidence_level = smave::EvidenceLevel::e2,
            .maximum_permission = smave::Permission::warm_start,
        };
    }

    [[nodiscard]] smave::Estimate estimate(
        const smave::BlockIR&, const smave::BlockContext&) const override {
        return smave::Estimate{
            .pass_probability = 1.0,
            .expected_solve_time_us = 1.0,
            .expected_correction_time_us = 1.0,
            .failure_cost_us = 10.0,
            .risk_score = 0.0,
        };
    }

    [[nodiscard]] smave::ExpertResult solve(
        const smave::BlockIR&,
        const smave::BlockContext&,
        const smave::SolveBudget&) const override {
        return smave::ExpertResult{
            .candidate = {{"x", 1.99}},
            .status = "candidate",
            .uncertainty = 0.0,
        };
    }
};

smave::RoutingConfig routing_for(
    const smave::ModelIR& model,
    const std::string& expert_version,
    int work_iterations) {
    smave::CompetitionReport profile;
    profile.block_fingerprint = model.blocks.front().fingerprint;
    profile.winner = expert_version;
    profile.entries.push_back(smave::CompetitionEntry{
        .expert_version = expert_version,
        .attempts = 64,
        .passes = 64,
        .empirical_pass_rate = 1.0,
        .predicted_pass_rate = 1.0,
        .median_wall_us = 2.0,
        .p90_wall_us = 2.0,
        .p99_wall_us = 2.0,
        .median_iterations = static_cast<double>(work_iterations),
    });
    profile.seal();
    smave::RoutingConfig routing;
    routing.top_k = 1;
    smave::apply_competition_profile(profile, routing);
    return routing;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        throw std::invalid_argument(
            "usage: calibrated-correction-router-evidence OUTPUT_DIRECTORY");
    }
    const std::filesystem::path output_directory = argv[1];
    std::filesystem::create_directories(output_directory);
    const auto source = output_directory / "CalibratedBudgetProbe.mo";
    std::ofstream(source)
        << "model CalibratedBudgetProbe\n"
        << "  parameter Real p = 4.0;\n"
        << "  Real x(start = 1.0);\n"
        << "equation\n"
        << "  x*x = p;\n"
        << "end CalibratedBudgetProbe;\n";
    const auto model = smave::compile_model(source);
    auto registry = smave::make_default_registry(model);
    auto expert = std::make_shared<BudgetProbeExpert>();
    registry.register_expert(
        expert,
        smave::ExpertGrant{
            .expert_version = expert->version(),
            .block_family = "*",
            .permission = smave::Permission::warm_start,
            .evidence_level = smave::EvidenceLevel::e2,
            .evidence_bundle = "calibrated-budget-probe-evidence-v1",
            .artifact_hash = "calibrated-budget-probe-artifact-v1",
        });
    auto bundle = smave::make_default_bundle(model);
    bundle.add_expert(
        expert->version(),
        "calibrated-budget-probe-artifact-v1",
        "calibrated-budget-probe-evidence-v1");
    registry.validate_bundle(bundle, model);

    const std::unordered_map<std::string, double> scenario{{"p", 4.0}};
    smave::BlockContext context;
    context.values = scenario;
    const auto candidates = smave::CompileRouter{}.lookup(
        model.blocks.front(), registry, bundle);
    const auto zero_routing = routing_for(model, expert->version(), 0);
    const auto two_routing = routing_for(model, expert->version(), 2);
    const auto zero_plan = smave::RuntimeRouter(zero_routing).route(
        model.blocks.front(), context, candidates, registry, bundle);
    const auto two_plan = smave::RuntimeRouter(two_routing).route(
        model.blocks.front(), context, candidates, registry, bundle);
    if (zero_plan.steps.size() != 1 || two_plan.steps.size() != 1 ||
        zero_plan.steps.front().budget.work_iterations != 0 ||
        two_plan.steps.front().budget.work_iterations != 2) {
        throw std::runtime_error("calibrated correction budget was not propagated");
    }

    const auto zero_outcome = smave::Runtime(
        model, registry, bundle, {}, zero_routing).solve(
            scenario, output_directory / "budget-0-traces");
    const auto two_outcome = smave::Runtime(
        model, registry, bundle, {}, two_routing).solve(
            scenario, output_directory / "budget-2-traces");
    if (!zero_outcome.success || zero_outcome.blocks.empty() ||
        zero_outcome.blocks.front().path != smave::SolvePath::full_fallback ||
        !two_outcome.success || two_outcome.blocks.empty() ||
        two_outcome.blocks.front().path != smave::SolvePath::warm_start_accept ||
        two_outcome.blocks.front().gate.decision != smave::GateDecision::direct_accept ||
        two_outcome.blocks.front().expert_iterations != 2) {
        throw std::runtime_error(
            "production Runtime did not distinguish calibrated correction budgets");
    }

    std::ofstream evidence(output_directory / "evidence.txt");
    if (!evidence) throw std::runtime_error("cannot write correction-router evidence");
    evidence << std::setprecision(17)
             << "SMAVE_CALIBRATED_CORRECTION_ROUTER 1\n"
             << "contract=production-router-propagates-profiled-correction-budget\n"
             << "expert=" << expert->version() << '\n'
             << "budget0.plan_budget="
             << zero_plan.steps.front().budget.work_iterations << '\n'
             << "budget0.full_fallback=1\n"
             << "budget0.success=1\n"
             << "budget2.plan_budget="
             << two_plan.steps.front().budget.work_iterations << '\n'
             << "budget2.warm_start_accept=1\n"
             << "budget2.expert_iterations="
             << two_outcome.blocks.front().expert_iterations << '\n'
             << "budget2.original_equation_gate_accept=1\n"
             << "zero_budget_raw_residual_check_preserved=1\n"
             << "numerical_fallback_preserved=1\n"
             << "END\n";
    return 0;
}
