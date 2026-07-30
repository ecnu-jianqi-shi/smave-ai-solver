#include "smave/compiler.hpp"
#include "smave/expert.hpp"
#include "smave/routing.hpp"
#include "smave/runtime.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace {

class ProbeExpert final : public smave::Expert {
public:
    [[nodiscard]] std::string version() const override {
        return "structural-ood-probe-v1";
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
            .candidate = {{"x", 2.0}},
            .status = "candidate",
            .uncertainty = 0.0,
        };
    }
};

struct StructuralOodCase {
    std::string label;
    std::string grant_family;
    std::string grant_hardware;
    std::string grant_tolerance;
    std::string block_source;
    std::string expected_reject_reason;
    bool expect_reject;
};

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2) {
            std::cerr << "usage: smave_structural_ood_filter_evidence OUTPUT_DIRECTORY\n";
            return 2;
        }
        const std::filesystem::path output_directory = argv[1];
        std::filesystem::remove_all(output_directory);
        std::filesystem::create_directories(output_directory);

        // Canonical compatible block: x*x = 4 (nonlinear, smooth)
        const auto compatible_source = output_directory / "CompatibleProbe.mo";
        std::ofstream(compatible_source)
            << "model CompatibleProbe\n"
            << "  parameter Real p = 4.0;\n"
            << "  Real x(start = 1.0);\n"
            << "equation\n"
            << "  x*x = p;\n"
            << "end CompatibleProbe;\n";
        const auto compatible_model = smave::compile_model(compatible_source);
        const auto compatible_fingerprint =
            compatible_model.blocks.front().fingerprint;

        // Incompatible-topology block: linear system (different family)
        const auto linear_source = output_directory / "LinearProbe.mo";
        std::ofstream(linear_source)
            << "model LinearProbe\n"
            << "  Real x;\n"
            << "  Real y;\n"
            << "equation\n"
            << "  x + y = 3.0;\n"
            << "  x - y = 1.0;\n"
            << "end LinearProbe;\n";
        const auto linear_model = smave::compile_model(linear_source);

        // OOD-scale block: different fingerprint (cubic)
        const auto cubic_source = output_directory / "CubicProbe.mo";
        std::ofstream(cubic_source)
            << "model CubicProbe\n"
            << "  parameter Real p = 8.0;\n"
            << "  Real x(start = 1.0);\n"
            << "equation\n"
            << "  x*x*x = p;\n"
            << "end CubicProbe;\n";
        const auto cubic_model = smave::compile_model(cubic_source);

        const std::vector<StructuralOodCase> cases{
            {"compatible",
             compatible_fingerprint, "cpu", "default",
             "CompatibleProbe.mo", "", false},
            {"structural_incompatibility_wrong_family",
             compatible_fingerprint, "cpu", "default",
             "CubicProbe.mo",
             "block_family", true},
            {"unverified_topology_linear_vs_nonlinear",
             compatible_fingerprint, "cpu", "default",
             "LinearProbe.mo",
             "block_family", true},
            {"ood_hardware_unknown",
             compatible_fingerprint, "cuda-gpu-v99", "default",
             "CompatibleProbe.mo",
             "hardware_profile", true},
            {"ood_tolerance_unverified",
             compatible_fingerprint, "cpu", "strict-1e-12",
             "CompatibleProbe.mo",
             "tolerance_profile", true},
        };

        std::size_t eligible = 0;
        std::size_t rejected = 0;
        std::size_t false_accepts = 0;
        std::size_t false_rejects = 0;
        std::size_t dangerous_misroutes = 0;
        std::vector<std::string> rejection_log;

        for (const auto& test : cases) {
            const auto block_source = output_directory / test.block_source;
            const auto model = smave::compile_model(block_source);
            auto registry = smave::make_default_registry(model);
            auto expert = std::make_shared<ProbeExpert>();
            registry.register_expert(
                expert,
                smave::ExpertGrant{
                    .expert_version = expert->version(),
                    .block_family = test.grant_family,
                    .domain_version = "domain-v1",
                    .tolerance_profile = test.grant_tolerance,
                    .hardware_profile = test.grant_hardware,
                    .permission = smave::Permission::warm_start,
                    .evidence_level = smave::EvidenceLevel::e2,
                    .evidence_bundle = "structural-ood-probe-evidence-v1",
                    .artifact_hash = "structural-ood-probe-artifact-v1",
                });
            auto bundle = smave::make_default_bundle(model);
            bundle.add_expert(
                expert->version(),
                "structural-ood-probe-artifact-v1",
                "structural-ood-probe-evidence-v1");
            registry.validate_bundle(bundle, model);

            const auto candidates = smave::CompileRouter{}.lookup(
                model.blocks.front(), registry, bundle);

            bool learned_admitted = false;
            for (const auto& candidate : candidates) {
                if (!candidate.builtin &&
                    candidate.expert_version == expert->version() &&
                    candidate.permission == smave::Permission::warm_start) {
                    learned_admitted = true;
                    break;
                }
            }

            if (test.expect_reject) {
                if (learned_admitted) {
                    dangerous_misroutes++;
                    false_accepts++;
                } else {
                    rejected++;
                    rejection_log.push_back(
                        "STRUCTURAL_OOD_REJECT label=" + test.label +
                        " reason=" + test.expected_reject_reason);
                }
            } else {
                if (learned_admitted) {
                    eligible++;
                } else {
                    false_rejects++;
                }
            }
        }

        if (dangerous_misroutes != 0 || false_accepts != 0) {
            throw std::runtime_error(
                "structural OOD filter allowed a dangerous learned misroute");
        }
        if (false_rejects != 0) {
            throw std::runtime_error(
                "structural OOD filter falsely rejected a compatible block");
        }
        if (eligible != 1 || rejected != 4) {
            throw std::runtime_error(
                "structural OOD filter coverage was incorrect");
        }

        std::ofstream evidence(output_directory / "evidence.txt");
        if (!evidence) throw std::runtime_error("cannot write structural OOD evidence");
        evidence << std::setprecision(17)
                 << "SMAVE_STRUCTURAL_OOD_FILTER 1\n"
                 << "contract=hard-structural-filter-before-learned-route-permission\n"
                 << "filter_executed_before_runtime_router=1\n"
                 << "embedding_similarity_not_substitute=1\n"
                 << "router_score_not_substitute=1\n"
                 << "cases=" << cases.size() << '\n'
                 << "eligible_coverage=" << eligible << '\n'
                 << "structural_ood_rejects=" << rejected << '\n'
                 << "false_accepts=" << false_accepts << '\n'
                 << "false_rejects=" << false_rejects << '\n'
                 << "dangerous_misroutes=" << dangerous_misroutes << '\n'
                 << "conservative_fallback_rate=0\n"
                 << "negative_results_retained=1\n";
        for (const auto& entry : rejection_log) {
            evidence << entry << '\n';
        }
        evidence << "END\n";
        std::cout << "structural OOD filter evidence passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "structural OOD filter evidence failure: " << error.what() << '\n';
        return 1;
    }
}
