#include "smave/compiler.hpp"
#include "smave/expert.hpp"
#include "smave/routing.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

bool has_candidate(
    const std::vector<smave::CandidateExpert>& candidates,
    const std::string& version) {
    return std::any_of(candidates.begin(), candidates.end(), [&](const auto& candidate) {
        return candidate.expert_version == version;
    });
}

smave::ModelIR compile_symmetric(const std::filesystem::path& root) {
    const auto source = root / "Symmetric.mo";
    std::ofstream(source)
        << "model Symmetric\n"
        << "parameter Real b1=1; parameter Real b2=2; parameter Real b3=3; parameter Real b4=4;\n"
        << "Real x1; Real x2; Real x3; Real x4;\n"
        << "equation\n"
        << "4*x1-x2=b1;\n-x1+4*x2-x3=b2;\n-x2+4*x3-x4=b3;\n-x3+4*x4=b4;\n"
        << "end Symmetric;\n";
    return smave::compile_model(source);
}

smave::ModelIR compile_nonsymmetric(const std::filesystem::path& root) {
    const auto source = root / "Nonsymmetric.mo";
    std::ofstream(source)
        << "model Nonsymmetric\n"
        << "parameter Real b1=1; parameter Real b2=2; parameter Real b3=3; parameter Real b4=4;\n"
        << "Real x1; Real x2; Real x3; Real x4;\n"
        << "equation\n"
        << "2*x1+x2=b1;\n3*x2+x3=b2;\n4*x3+x4=b3;\nx1+5*x4=b4;\n"
        << "end Nonsymmetric;\n";
    return smave::compile_model(source);
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::invalid_argument("usage: equation_routing_evidence output-dir");
        const std::filesystem::path root = argv[1];
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root);

        const auto symmetric_model = compile_symmetric(root);
        const auto& symmetric_block = symmetric_model.blocks.front();
        const auto symmetric_assessment = smave::assess_equation(symmetric_block);
        const auto symmetric_registry = smave::make_default_registry(symmetric_model);
        const auto symmetric_bundle = smave::make_default_bundle(symmetric_model);
        const auto symmetric_candidates = smave::CompileRouter{}.lookup(
            symmetric_block, symmetric_registry, symmetric_bundle);
        const auto symmetric_plan = smave::RuntimeRouter{}.route(
            symmetric_block, {}, symmetric_candidates, symmetric_registry, symmetric_bundle);
        require(
            symmetric_assessment.equation_family == "linear-structurally-symmetric" &&
                symmetric_assessment.structurally_symmetric &&
                symmetric_assessment.runtime_positive_definite_check_required,
            "symmetric block assessment failed");
        require(
            has_candidate(symmetric_candidates, "pcg-ic0-cpu-v1") &&
                has_candidate(symmetric_candidates, "gmres-ilut-cpu-v1") &&
                has_candidate(symmetric_candidates, "dense-direct-cpu-v1"),
            "symmetric backend portfolio is incomplete");
        require(
            std::all_of(
                symmetric_plan.steps.begin(), symmetric_plan.steps.end(),
                [](const auto& step) { return !step.selection_reason.empty(); }),
            "symmetric plan omitted selection reasons");

        const auto nonsymmetric_model = compile_nonsymmetric(root);
        const auto& nonsymmetric_block = nonsymmetric_model.blocks.front();
        const auto nonsymmetric_assessment = smave::assess_equation(nonsymmetric_block);
        const auto nonsymmetric_registry = smave::make_default_registry(nonsymmetric_model);
        const auto nonsymmetric_bundle = smave::make_default_bundle(nonsymmetric_model);
        const auto nonsymmetric_candidates = smave::CompileRouter{}.lookup(
            nonsymmetric_block, nonsymmetric_registry, nonsymmetric_bundle);
        require(
            nonsymmetric_assessment.equation_family.find("nonsymmetric") != std::string::npos &&
                !nonsymmetric_assessment.structurally_symmetric,
            "nonsymmetric block assessment failed");
        require(
            !has_candidate(nonsymmetric_candidates, "pcg-ic0-cpu-v1") &&
                !has_candidate(nonsymmetric_candidates, "pcg-jacobi-cpu-v1") &&
                has_candidate(nonsymmetric_candidates, "gmres-ilut-cpu-v1") &&
                has_candidate(nonsymmetric_candidates, "dense-direct-cpu-v1"),
            "nonsymmetric backend filtering failed");

        smave::BlockIR large_block;
        large_block.id = "large-block";
        large_block.linear = true;
        large_block.smooth = true;
        large_block.fingerprint = "synthetic-large-tridiagonal";
        constexpr std::size_t large_size = 1025;
        large_block.unknowns.reserve(large_size);
        large_block.equation_ids.reserve(large_size);
        std::vector<std::vector<std::size_t>> sparsity_rows(large_size);
        for (std::size_t index = 0; index < large_size; ++index) {
            large_block.unknowns.push_back("x" + std::to_string(index));
            large_block.equation_ids.push_back("eq" + std::to_string(index));
            sparsity_rows[index].push_back(index);
            if (index > 0) sparsity_rows[index].push_back(index - 1);
            if (index + 1 < large_size) {
                sparsity_rows[index].push_back(index + 1);
            }
        }
        large_block.jacobian_sparsity =
            smave::SparsityPattern::from_rows(large_size, sparsity_rows);
        const auto large_assessment = smave::assess_equation(large_block);
        const smave::Registry empty_registry;
        const smave::RuntimeBundle empty_bundle;
        const auto large_candidates = smave::CompileRouter{}.lookup(
            large_block, empty_registry, empty_bundle);
        require(
            large_assessment.scale_class == "large" &&
                !large_assessment.dense_direct_eligible &&
                !has_candidate(large_candidates, "dense-direct-cpu-v1") &&
                has_candidate(
                    large_candidates, "sparse-ordered-threshold-pivot-cpu-v2") &&
                has_candidate(large_candidates, "pcg-ic0-cpu-v1"),
            "large sparse assessment retained an unscalable dense backend");

        std::ofstream report(root / "report.txt");
        report << "SMAVE_EQUATION_ROUTING_EVIDENCE 1\n"
               << "SYMMETRIC_FAMILY " << symmetric_assessment.equation_family << '\n'
               << "SYMMETRIC_DENSITY " << symmetric_assessment.structural_density << '\n'
               << "SYMMETRIC_RUNTIME_SPD_GATE "
               << symmetric_assessment.runtime_positive_definite_check_required << '\n'
               << "SYMMETRIC_PCG "
               << has_candidate(symmetric_candidates, "pcg-ic0-cpu-v1") << '\n'
               << "SYMMETRIC_GMRES "
               << has_candidate(symmetric_candidates, "gmres-ilut-cpu-v1") << '\n'
               << "NONSYMMETRIC_FAMILY " << nonsymmetric_assessment.equation_family << '\n'
               << "NONSYMMETRIC_PCG "
               << has_candidate(nonsymmetric_candidates, "pcg-ic0-cpu-v1") << '\n'
               << "NONSYMMETRIC_GMRES "
               << has_candidate(nonsymmetric_candidates, "gmres-ilut-cpu-v1") << '\n'
               << "LARGE_SCALE_CLASS " << large_assessment.scale_class << '\n'
               << "LARGE_ESTIMATED_DENSE_BYTES "
               << large_assessment.estimated_dense_bytes << '\n'
               << "LARGE_ESTIMATED_SPARSE_BYTES "
               << large_assessment.estimated_sparse_bytes << '\n'
               << "LARGE_DENSE_ELIGIBLE "
               << large_assessment.dense_direct_eligible << '\n'
               << "LARGE_DENSE_CANDIDATE "
               << has_candidate(large_candidates, "dense-direct-cpu-v1") << '\n'
               << "LARGE_SPARSE_CANDIDATE "
               << has_candidate(
                      large_candidates, "sparse-ordered-threshold-pivot-cpu-v2") << '\n'
               << "MANDATORY_FALLBACK " << nonsymmetric_assessment.mandatory_fallback << '\n';
        std::cout << "equation routing evidence passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "equation routing evidence failure: " << error.what() << '\n';
        return 1;
    }
}
