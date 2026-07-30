#include "smave/benchmark.hpp"
#include "smave/config.hpp"
#include "smave/learning.hpp"
#include "smave/routing.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace {

class RejectingInitializer final : public smave::Expert {
public:
    [[nodiscard]] std::string version() const override {
        return "cascade-reject-probe-v1";
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
            .pass_probability = 0.99,
            .expected_solve_time_us = 0.01,
            .failure_cost_us = 1.0,
            .risk_score = 0.0,
        };
    }

    [[nodiscard]] smave::ExpertResult solve(
        const smave::BlockIR&,
        const smave::BlockContext&,
        const smave::SolveBudget&) const override {
        return smave::ExpertResult{.status = "injected-rejection"};
    }
};

std::unordered_map<std::string, double> read_scenario(
    const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> paths;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".conf") {
            paths.push_back(entry.path());
        }
    }
    std::sort(paths.begin(), paths.end());
    if (paths.empty()) throw std::invalid_argument("cascade evidence has no scenarios");
    std::ifstream input(paths.front());
    if (!input) throw std::runtime_error("cannot read cascade evidence scenario");
    std::unordered_map<std::string, double> scenario;
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty() || line.front() == '#') continue;
        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            throw std::invalid_argument("invalid cascade evidence scenario field");
        }
        scenario.emplace(line.substr(0, separator), std::stod(line.substr(separator + 1)));
    }
    return scenario;
}

smave::Registry learned_registry(
    const smave::ModelIR& model,
    const smave::AffineWarmStartArtifact& artifact,
    const smave::VerificationCertificate& certificate) {
    auto registry = smave::make_default_registry(model);
    smave::register_affine_expert(
        registry, artifact, "domain-v1", "default", "cpu", certificate);
    return registry;
}

smave::BlockContext block_context(
    const smave::ModelIR& model,
    const std::unordered_map<std::string, double>& scenario) {
    smave::BlockContext context;
    context.values = scenario;
    for (const auto& unknown : model.blocks.front().unknowns) {
        const auto previous = scenario.find(unknown + "_previous");
        if (previous != scenario.end()) context.previous_solution[unknown] = previous->second;
    }
    return context;
}

void register_rejecting_initializer(smave::Registry& registry) {
    auto expert = std::make_shared<RejectingInitializer>();
    registry.register_expert(
        expert,
        smave::ExpertGrant{
            .expert_version = expert->version(),
            .block_family = "*",
            .permission = smave::Permission::warm_start,
            .evidence_level = smave::EvidenceLevel::e2,
            .evidence_bundle = "cascade-failure-injection-v1",
            .artifact_hash = "cascade-reject-probe-v1",
        });
}

void write_report(
    const smave::Runtime& baseline,
    const smave::Runtime& accelerated,
    const std::filesystem::path& scenarios,
    const std::filesystem::path& output_directory,
    const std::string& name) {
    auto report = smave::benchmark_runtimes(
        baseline,
        accelerated,
        scenarios,
        output_directory / (name + "-traces"),
        100,
        3,
        10000);
    smave::write_performance_report(report, output_directory / (name + ".txt"));
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 8) {
        throw std::invalid_argument(
            "usage: nonlinear-cascade-evidence MODEL EXPERT CERTIFICATE BUNDLE "
            "SCENARIOS CONFIG OUTPUT_DIRECTORY");
    }
    const auto model = smave::ModelIR::read(argv[1]);
    const auto artifact = smave::AffineWarmStartArtifact::read(argv[2]);
    const auto certificate = smave::VerificationCertificate::read(argv[3]);
    const auto bundle = smave::RuntimeBundle::read(argv[4]);
    const std::filesystem::path scenarios = argv[5];
    const auto config = smave::RuntimeConfig::read(argv[6]);
    const std::filesystem::path output_directory = argv[7];
    std::filesystem::create_directories(output_directory);

    auto fixed_routing = config.routing;
    fixed_routing.expert_allowlist = {artifact.expert_version};
    auto online_routing = config.routing;
    online_routing.expert_allowlist.clear();

    const smave::Runtime classic(
        model,
        smave::make_default_registry(model),
        smave::make_default_bundle(model),
        config.tolerance,
        config.routing);
    const smave::Runtime fixed(
        model,
        learned_registry(model, artifact, certificate),
        bundle,
        config.tolerance,
        fixed_routing);
    const smave::Runtime online(
        model,
        learned_registry(model, artifact, certificate),
        bundle,
        config.tolerance,
        online_routing);

    write_report(classic, fixed, scenarios, output_directory, "classic-vs-fixed");
    write_report(classic, online, scenarios, output_directory, "classic-vs-online");
    write_report(fixed, online, scenarios, output_directory, "fixed-vs-online");

    const auto scenario = read_scenario(scenarios);
    const auto context = block_context(model, scenario);
    const auto classic_registry = smave::make_default_registry(model);
    const auto classic_bundle = smave::make_default_bundle(model);
    smave::write_equation_assessment_report(
        model,
        model.blocks.front(),
        context,
        classic_registry,
        classic_bundle,
        config.routing,
        output_directory / "classic-plan.txt");
    const auto fixed_registry = learned_registry(model, artifact, certificate);
    smave::write_equation_assessment_report(
        model,
        model.blocks.front(),
        context,
        fixed_registry,
        bundle,
        fixed_routing,
        output_directory / "fixed-plan.txt");
    const auto online_registry = learned_registry(model, artifact, certificate);
    smave::write_equation_assessment_report(
        model,
        model.blocks.front(),
        context,
        online_registry,
        bundle,
        online_routing,
        output_directory / "online-plan.txt");

    const auto fixed_outcome = fixed.solve(scenario, output_directory / "fixed-probe");
    const auto online_outcome = online.solve(scenario, output_directory / "online-probe");
    if (!fixed_outcome.success || !online_outcome.success ||
        fixed_outcome.blocks.empty() || online_outcome.blocks.empty()) {
        throw std::runtime_error("cascade probe solve failed");
    }

    auto rejection_bundle = smave::make_default_bundle(model);
    rejection_bundle.add_expert(
        "cascade-reject-probe-v1",
        "cascade-reject-probe-v1",
        "cascade-failure-injection-v1");
    auto fixed_rejection_registry = smave::make_default_registry(model);
    register_rejecting_initializer(fixed_rejection_registry);
    auto fixed_rejection_routing = config.routing;
    fixed_rejection_routing.expert_allowlist = {"cascade-reject-probe-v1"};
    const smave::Runtime fixed_rejection(
        model,
        std::move(fixed_rejection_registry),
        rejection_bundle,
        config.tolerance,
        fixed_rejection_routing);
    auto online_rejection_registry = smave::make_default_registry(model);
    register_rejecting_initializer(online_rejection_registry);
    const smave::Runtime online_rejection(
        model,
        std::move(online_rejection_registry),
        rejection_bundle,
        config.tolerance,
        config.routing);
    const auto fixed_rejection_outcome = fixed_rejection.solve(
        scenario, output_directory / "fixed-rejection-probe");
    const auto online_rejection_outcome = online_rejection.solve(
        scenario, output_directory / "online-rejection-probe");
    if (!fixed_rejection_outcome.success || !online_rejection_outcome.success ||
        fixed_rejection_outcome.blocks.empty() || online_rejection_outcome.blocks.empty()) {
        throw std::runtime_error("cascade rejection probe solve failed");
    }
    const auto& fixed_rejection_block = fixed_rejection_outcome.blocks.front();
    const auto& online_rejection_block = online_rejection_outcome.blocks.front();
    std::ofstream evidence(output_directory / "evidence.txt");
    if (!evidence) throw std::runtime_error("cannot write cascade evidence");
    evidence << std::setprecision(17)
             << "SMAVE_NONLINEAR_CASCADE_EVIDENCE 1\n"
             << "EXPERT_VERSION " << std::quoted(artifact.expert_version) << '\n'
             << "FIXED_PLAN_ID " << std::quoted(fixed_outcome.blocks.front().plan_id) << '\n'
             << "ONLINE_PLAN_ID " << std::quoted(online_outcome.blocks.front().plan_id) << '\n'
             << "FIXED_PATH " << static_cast<int>(fixed_outcome.blocks.front().path) << '\n'
             << "ONLINE_PATH " << static_cast<int>(online_outcome.blocks.front().path) << '\n'
             << "FIXED_ATTEMPTS " << fixed_outcome.blocks.front().attempted_experts.size() << '\n';
    for (const auto& expert : fixed_outcome.blocks.front().attempted_experts) {
        evidence << "FIXED_ATTEMPT " << std::quoted(expert) << '\n';
    }
    evidence << "ONLINE_ATTEMPTS " << online_outcome.blocks.front().attempted_experts.size() << '\n';
    for (const auto& expert : online_outcome.blocks.front().attempted_experts) {
        evidence << "ONLINE_ATTEMPT " << std::quoted(expert) << '\n';
    }
    evidence << "REJECTION_FIXED_PATH "
             << static_cast<int>(fixed_rejection_block.path) << '\n'
             << "REJECTION_FIXED_ATTEMPTS "
             << fixed_rejection_block.attempted_experts.size() << '\n';
    for (const auto& expert : fixed_rejection_block.attempted_experts) {
        evidence << "REJECTION_FIXED_ATTEMPT " << std::quoted(expert) << '\n';
    }
    evidence << "REJECTION_ONLINE_PATH "
             << static_cast<int>(online_rejection_block.path) << '\n'
             << "REJECTION_ONLINE_ATTEMPTS "
             << online_rejection_block.attempted_experts.size() << '\n';
    for (const auto& expert : online_rejection_block.attempted_experts) {
        evidence << "REJECTION_ONLINE_ATTEMPT " << std::quoted(expert) << '\n';
    }
    evidence << "END\n";
}
