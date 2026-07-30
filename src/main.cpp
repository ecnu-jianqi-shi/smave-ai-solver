#include "smave/benchmark.hpp"
#include "smave/block_graph.hpp"
#include "smave/compiler.hpp"
#include "smave/competition.hpp"
#include "smave/complementarity.hpp"
#include "smave/continuous.hpp"
#include "smave/cosimulation.hpp"
#include "smave/dae.hpp"
#include "smave/dae_learning.hpp"
#include "smave/data_registry.hpp"
#include "smave/config.hpp"
#include "smave/embedding.hpp"
#include "smave/family_routing.hpp"
#include "smave/fmi.hpp"
#include "smave/ir.hpp"
#include "smave/learning.hpp"
#include "smave/model_group.hpp"
#include "smave/hybrid.hpp"
#include "smave/high_index_dae.hpp"
#include "smave/operator.hpp"
#include "smave/release.hpp"
#include "smave/runtime.hpp"
#include "smave/solve_service.hpp"
#include "smave/ssp.hpp"
#include "smave/tensor.hpp"
#include "smave/validation.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>

namespace {

std::optional<std::string> option(int argc, char** argv, const std::string& name) {
    for (int index = 0; index + 1 < argc; ++index) {
        if (argv[index] == name) return argv[index + 1];
    }
    return std::nullopt;
}

bool flag(int argc, char** argv, const std::string& name) {
    for (int index = 0; index < argc; ++index) {
        if (argv[index] == name) return true;
    }
    return false;
}

std::vector<std::string> options(int argc, char** argv, const std::string& name) {
    std::vector<std::string> result;
    for (int index = 0; index + 1 < argc; ++index) {
        if (argv[index] == name) result.emplace_back(argv[index + 1]);
    }
    return result;
}

std::vector<std::uint8_t> hexadecimal_bytes(const std::string& value) {
    if (value.size() % 2 != 0) {
        throw std::invalid_argument("--binary-input requires an even-length hexadecimal value");
    }
    const auto digit = [](char character) -> std::uint8_t {
        if (character >= '0' && character <= '9') return character - '0';
        if (character >= 'a' && character <= 'f') return character - 'a' + 10;
        if (character >= 'A' && character <= 'F') return character - 'A' + 10;
        throw std::invalid_argument("--binary-input contains a non-hexadecimal character");
    };
    std::vector<std::uint8_t> result(value.size() / 2);
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = static_cast<std::uint8_t>(
            digit(value[index * 2]) * 16U + digit(value[index * 2 + 1]));
    }
    return result;
}

std::vector<double> array_values(const std::string& value) {
    if (value.empty()) throw std::invalid_argument("--array-input requires values");
    std::vector<double> result;
    std::size_t begin{};
    while (true) {
        const auto end = value.find(',', begin);
        const auto item = value.substr(begin, end == std::string::npos
            ? std::string::npos : end - begin);
        if (item.empty()) throw std::invalid_argument("--array-input contains an empty element");
        std::size_t parsed{};
        const double number = std::stod(item, &parsed);
        if (parsed != item.size()) {
            throw std::invalid_argument("--array-input contains an invalid number");
        }
        result.push_back(number);
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return result;
}

std::vector<std::size_t> index_values(const std::string& value) {
    const auto numbers = array_values(value);
    std::vector<std::size_t> result;
    result.reserve(numbers.size());
    for (double number : numbers) {
        if (number < 0.0 || number != std::floor(number) ||
            number > static_cast<double>(std::numeric_limits<std::size_t>::max())) {
            throw std::invalid_argument("index list requires non-negative integers");
        }
        result.push_back(static_cast<std::size_t>(number));
    }
    return result;
}

struct ScenarioSource {
    std::filesystem::path directory;
    std::optional<smave::DatasetManifest> dataset;
};

ScenarioSource scenario_source(
    int argc,
    char** argv,
    const std::optional<std::string>& scenarios) {
    const auto store = option(argc, argv, "--dataset-store");
    const auto manifest_path = option(argc, argv, "--dataset-manifest");
    if (store.has_value() != manifest_path.has_value()) {
        throw std::invalid_argument(
            "scenario snapshot requires both --dataset-store and --dataset-manifest");
    }
    if (store) {
        if (scenarios) {
            throw std::invalid_argument(
                "choose either --scenarios or a verified dataset snapshot");
        }
        const auto manifest = smave::DatasetManifest::read(*manifest_path);
        const smave::DatasetStore dataset_store(*store);
        const auto verified = dataset_store.verify(
            manifest.dataset_id, manifest.version);
        if (verified.manifest_hash != manifest.manifest_hash ||
            smave::sha256_file(*manifest_path) !=
                smave::sha256_file(
                    dataset_store.version_directory(
                        verified.dataset_id, verified.version) / "dataset.manifest")) {
            throw std::invalid_argument(
                "scenario dataset manifest differs from the verified store snapshot");
        }
        return ScenarioSource{
            .directory = dataset_store.version_directory(
                verified.dataset_id, verified.version),
            .dataset = verified,
        };
    }
    if (!scenarios) throw std::invalid_argument("scenario directory is required");
    return ScenarioSource{.directory = *scenarios};
}

enum class ArtifactKind {
    affine_warm_start,
    diagonal_preconditioner,
    learned_multigrid,
    latent_operator,
};

ArtifactKind artifact_kind(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read expert artifact: " + path.string());
    std::string magic;
    input >> magic;
    if (magic == "SMAVE_AFFINE") return ArtifactKind::affine_warm_start;
    if (magic == "SMAVE_LINEAR_PC") return ArtifactKind::diagonal_preconditioner;
    if (magic == "SMAVE_LEARNED_MULTIGRID") return ArtifactKind::learned_multigrid;
    if (magic == "SMAVE_LATENT_OPERATOR") return ArtifactKind::latent_operator;
    throw std::invalid_argument("unknown expert artifact type: " + magic);
}

std::pair<std::string, std::string> register_artifact(
    smave::Registry& registry,
    const smave::ModelIR& model,
    const std::filesystem::path& path,
    std::optional<smave::VerificationCertificate> certificate = std::nullopt) {
    if (artifact_kind(path) == ArtifactKind::affine_warm_start) {
        const auto artifact = smave::AffineWarmStartArtifact::read(path);
        if (artifact.model_source_hash != model.source_hash) {
            throw std::invalid_argument("expert artifact targets a different source model");
        }
        smave::register_affine_expert(
            registry, artifact, "domain-v1", "default", "cpu", std::move(certificate));
        return {artifact.expert_version, artifact.artifact_hash};
    }
    if (artifact_kind(path) == ArtifactKind::diagonal_preconditioner) {
        const auto artifact = smave::LinearPreconditionerArtifact::read(path);
        if (artifact.model_source_hash != model.source_hash) {
            throw std::invalid_argument("expert artifact targets a different source model");
        }
        smave::register_linear_preconditioner(
            registry, artifact, "domain-v1", "default", "cpu", std::move(certificate));
        return {artifact.expert_version, artifact.artifact_hash};
    }
    if (artifact_kind(path) == ArtifactKind::learned_multigrid) {
        const auto artifact = smave::LearnedMultigridArtifact::read(path);
        if (artifact.model_source_hash != model.source_hash) {
            throw std::invalid_argument("expert artifact targets a different source model");
        }
        smave::register_learned_multigrid(
            registry, artifact, "domain-v1", "default", "cpu", std::move(certificate));
        return {artifact.expert_version, artifact.artifact_hash};
    }
    const auto artifact = smave::LatentOperatorArtifact::read(path);
    if (artifact.model_source_hash != model.source_hash) {
        throw std::invalid_argument("expert artifact targets a different source model");
    }
    smave::register_latent_operator(
        registry, artifact, "domain-v1", "default", "cpu", std::move(certificate));
    return {artifact.expert_version, artifact.artifact_hash};
}

std::vector<std::string> comma_list(const std::optional<std::string>& value) {
    std::vector<std::string> result;
    if (!value || value->empty()) return result;
    std::size_t begin = 0;
    while (begin <= value->size()) {
        const auto end = value->find(',', begin);
        const auto item = value->substr(begin, end == std::string::npos
            ? std::string::npos
            : end - begin);
        if (item.empty()) throw std::invalid_argument("empty comma-list item");
        result.push_back(item);
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return result;
}

void usage() {
    std::cout
        << "SMAVE AI Solver 0.1.0 (C++20)\n"
        << "Usage:\n"
        << "  smave solve-linear --storage dense --matrix a00,a01,... --rhs b0,b1,... --output report.txt [--absolute-tolerance A] [--relative-tolerance R]\n"
        << "  smave solve-linear --storage csr --row-offsets 0,... --columns j0,... --values a0,... --rhs b0,... --output report.txt [--absolute-tolerance A] [--relative-tolerance R]\n"
        << "  smave solve-nonlinear --unknowns x,y --initial x0,y0 --residual 'f0' --residual 'f1' --output report.txt [--jacobian directional]\n"
        << "  smave solve-ode --states x,y --initial x0,y0 --rhs 'f0' --rhs 'f1' --start T0 --end T1 --max-step H --output report.txt\n"
        << "  smave compile model.mo [--top Model] --output model.ir\n"
        << "  smave compile-continuous model.mo [--top Model] --output continuous.ir\n"
        << "  smave simulate-continuous continuous.ir --end T --max-step H --output report.txt [--start T0] [--reference events.ref]\n"
        << "  smave simulate-coupled continuous.ir --hybrid sampled.hybrid --end T --max-step H --output report.txt [--candidates candidates.txt]\n"
        << "  smave compile-dae model.mo [--top Model] --output dae.ir\n"
        << "  smave compile-index2-dae model.mo [--top Model] --output index2.ir\n"
        << "  smave assess-index2-dae index2.ir --output assessment.txt\n"
        << "  smave simulate-index2-dae index2.ir --end T --max-step H --output report.txt\n"
        << "  smave compile-complementarity model.mo [--top Model] --output lcp.ir\n"
        << "  smave assess-complementarity lcp.ir --output assessment.txt\n"
        << "  smave solve-complementarity lcp.ir --output report.txt\n"
        << "  smave train-dae-multigrid dae.ir (--scenarios directory | --dataset-store root --dataset-manifest manifest) --output dae-mg.artifact\n"
        << "  smave simulate-dae dae.ir --end T --max-step H --output report.txt [--multigrid dae-mg.artifact]\n"
        << "  smave compile-implicit-dae model.mo [--top Model] --output implicit-dae.ir\n"
        << "  smave train-implicit-dae-multigrid implicit-dae.ir (--scenarios directory | --dataset-store root --dataset-manifest manifest) --output dae-mg.artifact\n"
        << "  smave assess-implicit-dae implicit-dae.ir --output assessment.txt [--multigrid dae-mg.artifact]\n"
        << "  smave simulate-implicit-dae implicit-dae.ir --end T --max-step H --output report.txt [--multigrid dae-mg.artifact]\n"
        << "  smave inspect model.ir\n"
        << "  smave assess-equation model.ir --output assessment.txt [--block block-1] [--scenario case.conf] [--expert expert.artifact] [--certificate certificate.verify] [--config smave.yaml] [--profile competition.txt | --family-profile evaluation.txt] [--bundle runtime.bundle]\n"
        << "  smave import-fmu model.fmu --output model.fmi.ir [--report import-report.txt]\n"
        << "  smave inspect-fmi model.fmi.ir\n"
        << "  smave smoke-fmu model.fmu --end T --step H --output smoke.txt [--input name=value,...] [--async-timeout-ms N] --allow-native-execution\n"
        << "  smave smoke-fmu-se model.fmu --end T --interval H --output smoke.txt [--input name=value,...] --allow-native-execution\n"
        << "  smave smoke-fmu-me model.fmu --end T --step H --output smoke.txt [--input name=value,...] [--array-input name=v1,v2,... ...] [--string-input name=value ...] [--string-array-input name=value ...] [--binary-input name=hex ...] [--binary-array-input name=hex ...] --allow-native-execution\n"
        << "  smave simulate-ssp system.ssp --end T --step H --output report.txt --allow-native-execution\n"
        << "  smave train-expert model.ir --block block-1 (--scenarios directory | --dataset-store root --dataset-manifest manifest) --output expert.artifact\n"
        << "  smave train-preconditioner model.ir --block block-1 (--scenarios directory | --dataset-store root --dataset-manifest manifest) --output pc.artifact\n"
        << "  smave train-multigrid model.ir --block block-1 (--scenarios directory | --dataset-store root --dataset-manifest manifest) --output mg.artifact\n"
        << "  smave train-operator model.ir --block block-1 (--scenarios directory | --dataset-store root --dataset-manifest manifest) --output operator.artifact [--rank 8] [--qoi x1,x2]\n"
        << "  smave verify-expert model.ir --expert expert.artifact --output certificate.verify [--max-depth 4] [--trace-dir traces]\n"
        << "  smave export-counterexamples certificate.verify --output directory\n"
        << "  smave index-family model.ir --block block-1 --expert expert.artifact --family family-id --output families.index\n"
        << "  smave retrieve-family model.ir --block block-1 --index families.index [--top-k 4]\n"
        << "  smave evaluate-family-router source.ir --profile competition.txt --heldout heldout.ir (--scenarios directory | --dataset-store root --dataset-manifest manifest) --output evaluation.txt\n"
        << "  smave bundle model.ir [--expert expert.artifact] [--certificate certificate.verify] --output runtime.bundle\n"
        << "  smave solve model.ir --scenario case.conf [--expert expert.artifact] [--config smave.yaml] [--profile competition.txt | --family-profile evaluation.txt] [--bundle runtime.bundle] [--trace-dir path]\n"
        << "  smave validate model.ir --scenarios directory [--expert expert.artifact] [--config smave.yaml] [--profile competition.txt | --family-profile evaluation.txt] [--bundle runtime.bundle] --output report.txt\n"
        << "  smave compete model.ir (--scenarios directory | --dataset-store root --dataset-manifest manifest) --bundle runtime.bundle [--expert expert.artifact] [--certificate certificate.verify] --output competition.txt [--repetitions 1]\n"
        << "  smave benchmark model.ir (--scenarios directory | --dataset-store root --dataset-manifest manifest) --expert expert.artifact --bundle runtime.bundle --output performance.txt [--repetitions 20] [--bootstrap-samples 2000]\n"
        << "  smave benchmark-operator model.ir --scenarios directory --expert operator.artifact --bundle runtime.bundle --output report.txt --projected-queries 10000\n"
        << "  smave batch-solve model.ir --scenarios directory --expert pc.artifact [--max-batch 32] [--device auto|cpu|metal-gpu|coreml-neural-engine] --output batch.txt\n"
        << "  smave snapshot-data directory --dataset ID --store data-store\n"
        << "  smave verify-data --dataset ID --version SHA256 --store data-store\n"
        << "  smave replay trace-file\n"
        << "  smave import-block-graph model.sbg|model.slx --output model-group.ir\n"
        << "  smave run-model-group model-group.ir --output group-report.txt [--scenario inputs.conf]\n"
        << "  smave run-model-group-multirate model-group.ir --end T --base-step H --output report.txt [--scenario inputs.conf] [--trace-dir path]\n"
        << "  smave run-hybrid hybrid.ir --ticks N --output event-report.txt [--candidates candidates.txt]\n"
        << "  smave audit-release model.ir --bundle runtime.bundle --validation validation.txt --performance performance.txt --stage shadow|canary --traffic F --observed-hours H --minimum-hours H --minimum-requests N [--parent shadow.audit] [--dataset-manifest dataset.manifest] --output release.audit\n"
        << "  smave sign-release runtime.bundle --model model.ir --expert expert.artifact --certificate certificate.verify --audit release.audit --key signing.key --release ID [--dataset-manifest dataset.manifest] --output release.manifest\n"
        << "  smave activate-release release.manifest --bundle runtime.bundle --model model.ir --expert expert.artifact --certificate certificate.verify --audit canary.audit --parent shadow.audit --key signing.key --store directory\n"
        << "  smave rollback-release --key signing.key --store directory\n"
        << "  smave release-status --store directory --key signing.key\n"
        << "  smave solve-release --store directory --key signing.key --scenario case.conf [--trace-dir path]\n";
}

void inspect(const smave::ModelIR& model) {
    std::cout << "model: " << model.model_id << '\n'
              << "schema: " << model.schema_version << '\n'
              << "source_hash: " << model.source_hash << '\n'
              << "variables: " << model.variables.size() << '\n'
              << "equations: " << model.equations.size() << '\n'
              << "blocks: " << model.blocks.size() << '\n';
    for (const auto& block : model.blocks) {
        std::cout << "  " << block.id << " type="
                  << (block.linear ? "linear" : "nonlinear") << " unknowns=";
        for (const auto& unknown : block.unknowns) std::cout << unknown << ',';
        std::cout << " fallback=" << block.original_solver
                  << " fingerprint=" << block.fingerprint << '\n';
    }
    std::cout << "capabilities:";
    for (const auto& capability : model.capabilities) std::cout << ' ' << capability;
    std::cout << '\n';
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2 || std::string(argv[1]) == "--help") {
            usage();
            return argc < 2 ? 2 : 0;
        }
        const std::string command = argv[1];
        if (command == "solve-linear") {
            const auto storage = option(argc, argv, "--storage");
            const auto right = option(argc, argv, "--rhs");
            const auto output = option(argc, argv, "--output");
            if (!storage || !right || !output) {
                throw std::invalid_argument(
                    "solve-linear requires --storage, --rhs and --output");
            }
            smave::LinearSystem system;
            system.right_hand_side = array_values(*right);
            const std::size_t dimension = system.right_hand_side.size();
            if (*storage == "dense") {
                const auto matrix = option(argc, argv, "--matrix");
                if (!matrix) throw std::invalid_argument("dense solve-linear requires --matrix");
                const auto values = array_values(*matrix);
                if (dimension > std::numeric_limits<std::size_t>::max() / dimension) {
                    throw std::invalid_argument("dense matrix dimension is too large");
                }
                if (values.size() != dimension * dimension) {
                    throw std::invalid_argument("dense matrix must contain dimension^2 values");
                }
                system.matrix.assign(dimension, std::vector<double>(dimension));
                for (std::size_t row = 0; row < dimension; ++row) {
                    std::copy_n(
                        values.begin() + static_cast<std::ptrdiff_t>(row * dimension),
                        dimension, system.matrix[row].begin());
                }
            } else if (*storage == "csr") {
                const auto rows = option(argc, argv, "--row-offsets");
                const auto columns = option(argc, argv, "--columns");
                const auto values = option(argc, argv, "--values");
                if (!rows || !columns || !values) {
                    throw std::invalid_argument(
                        "CSR solve-linear requires --row-offsets, --columns and --values");
                }
                system.sparsity.row_offsets = index_values(*rows);
                system.sparsity.column_indices = index_values(*columns);
                system.sparse_values = array_values(*values);
                system.sparsity.row_count = dimension;
                system.sparsity.column_count = dimension;
                if (system.sparsity.row_offsets.size() != dimension + 1 ||
                    system.sparsity.column_indices.size() != system.sparse_values.size() ||
                    system.sparsity.row_offsets.back() != system.sparse_values.size()) {
                    throw std::invalid_argument("invalid CSR dimensions");
                }
            } else {
                throw std::invalid_argument("solve-linear storage must be dense or csr");
            }
            system.unknowns.reserve(dimension);
            for (std::size_t index = 0; index < dimension; ++index) {
                system.unknowns.push_back("x" + std::to_string(index));
            }
            smave::classify_linear_system(system);
            const smave::VerifiedLinearSolveOptions solve_options{
                .absolute_tolerance = option(argc, argv, "--absolute-tolerance")
                    ? std::stod(*option(argc, argv, "--absolute-tolerance"))
                    : 1.0e-12,
                .relative_tolerance = option(argc, argv, "--relative-tolerance")
                    ? std::stod(*option(argc, argv, "--relative-tolerance"))
                    : 1.0e-10,
            };
            const auto result = smave::verified_linear_solve(system, solve_options);
            std::ofstream report(*output);
            if (!report) throw std::runtime_error("cannot write solve-linear report");
            report << std::setprecision(17)
                   << "SMAVE_VERIFIED_LINEAR_SOLVE 1\n"
                   << "service_id=" << std::quoted(smave::verified_linear_solve_service_v1) << '\n'
                   << "storage=" << std::quoted(*storage) << '\n'
                   << "success=" << result.success << '\n'
                   << "used_fallback=" << result.used_fallback << '\n'
                   << "backend=" << std::quoted(result.backend) << '\n'
                   << "plan_id=" << std::quoted(result.plan_id) << '\n'
                   << "equation_family=" << std::quoted(result.equation_family) << '\n'
                   << "residual_inf=" << result.residual_inf << '\n'
                   << "backward_error=" << result.backward_error << '\n'
                   << "solution=";
            for (std::size_t index = 0; index < result.solution.size(); ++index) {
                if (index != 0) report << ',';
                report << result.solution[index];
            }
            report << '\n'
                   << "diagnostic=" << std::quoted(result.diagnostic) << '\n'
                   << "END\n";
            std::cout << "service_id: " << smave::verified_linear_solve_service_v1 << '\n'
                      << "success: " << result.success << '\n'
                      << "plan_id: " << result.plan_id << '\n'
                      << "equation_family: " << result.equation_family << '\n'
                      << "backend: " << result.backend << '\n'
                      << "residual_inf: " << result.residual_inf << '\n'
                      << "report: " << *output << '\n';
            return result.success ? 0 : 5;
        }
        if (command == "solve-nonlinear") {
            const auto unknown_option = option(argc, argv, "--unknowns");
            const auto initial_option = option(argc, argv, "--initial");
            const auto residual_sources = options(argc, argv, "--residual");
            const auto output = option(argc, argv, "--output");
            if (!unknown_option || !initial_option || residual_sources.empty() || !output) {
                throw std::invalid_argument(
                    "solve-nonlinear requires --unknowns, --initial, repeated --residual and --output");
            }
            const auto unknowns = comma_list(unknown_option);
            const auto initial_state = array_values(*initial_option);
            if (unknowns.empty() || unknowns.size() != initial_state.size() ||
                residual_sources.size() != unknowns.size()) {
                throw std::invalid_argument(
                    "nonlinear unknown, initial-state and residual counts must match");
            }
            std::vector<smave::Expression> residuals;
            residuals.reserve(residual_sources.size());
            const std::set<std::string> allowed_names(unknowns.begin(), unknowns.end());
            for (const auto& source : residual_sources) {
                residuals.emplace_back(source);
                for (const auto& name : residuals.back().names()) {
                    if (!allowed_names.contains(name)) {
                        throw std::invalid_argument(
                            "nonlinear residual references unknown name: " + name);
                    }
                }
            }
            smave::VerifiedNonlinearSolveProblem problem;
            problem.initial_state = initial_state;
            problem.residual = [&](const std::vector<double>& state,
                                   std::vector<double>& values) {
                if (state.size() != unknowns.size()) return false;
                std::unordered_map<std::string, double> context;
                for (std::size_t index = 0; index < unknowns.size(); ++index) {
                    context.emplace(unknowns[index], state[index]);
                }
                values.resize(residuals.size());
                try {
                    for (std::size_t index = 0; index < residuals.size(); ++index) {
                        values[index] = residuals[index].evaluate(context);
                    }
                } catch (...) {
                    return false;
                }
                return true;
            };
            const auto jacobian_mode = option(argc, argv, "--jacobian").value_or("finite-difference");
            if (jacobian_mode == "directional") {
                problem.jacobian = [&](const std::vector<double>& state,
                                       std::vector<std::vector<double>>& jacobian) {
                    if (state.size() != unknowns.size()) return false;
                    std::unordered_map<std::string, double> context;
                    for (std::size_t index = 0; index < unknowns.size(); ++index) {
                        context.emplace(unknowns[index], state[index]);
                    }
                    jacobian.assign(unknowns.size(), std::vector<double>(unknowns.size()));
                    for (std::size_t column = 0; column < unknowns.size(); ++column) {
                        std::unordered_map<std::string, double> direction{
                            {unknowns[column], 1.0}};
                        for (std::size_t row = 0; row < residuals.size(); ++row) {
                            const auto derivative = residuals[row].directional_derivative(
                                context, direction);
                            if (!derivative) return false;
                            jacobian[row][column] = *derivative;
                        }
                    }
                    return true;
                };
            } else if (jacobian_mode != "finite-difference") {
                throw std::invalid_argument(
                    "solve-nonlinear --jacobian must be directional or finite-difference");
            }
            const auto result = smave::verified_nonlinear_solve(
                problem,
                {.absolute_tolerance = option(argc, argv, "--absolute-tolerance")
                     ? std::stod(*option(argc, argv, "--absolute-tolerance")) : 1.0e-12,
                 .relative_tolerance = option(argc, argv, "--relative-tolerance")
                     ? std::stod(*option(argc, argv, "--relative-tolerance")) : 1.0e-10,
                 .maximum_iterations = option(argc, argv, "--maximum-iterations")
                     ? std::stoi(*option(argc, argv, "--maximum-iterations")) : 1000});
            std::ofstream report(*output);
            if (!report) throw std::runtime_error("cannot write solve-nonlinear report");
            report << std::setprecision(17)
                   << "SMAVE_VERIFIED_NONLINEAR_SOLVE 1\n"
                   << "service_id=" << std::quoted(smave::verified_nonlinear_solve_service_v1) << '\n'
                   << "success=" << result.success << '\n'
                   << "used_fallback=" << result.used_fallback << '\n'
                   << "backend=" << std::quoted(result.backend) << '\n'
                   << "plan_id=" << std::quoted(result.plan_id) << '\n'
                   << "equation_family=" << std::quoted(result.equation_family) << '\n'
                   << "residual_inf=" << result.residual_inf << '\n'
                   << "backward_error=" << result.backward_error << '\n'
                   << "solution=";
            for (std::size_t index = 0; index < result.solution.size(); ++index) {
                if (index != 0) report << ',';
                report << result.solution[index];
            }
            report << '\n' << "diagnostic=" << std::quoted(result.diagnostic) << '\n'
                   << "END\n";
            std::cout << "service_id: " << smave::verified_nonlinear_solve_service_v1 << '\n'
                      << "success: " << result.success << '\n'
                      << "plan_id: " << result.plan_id << '\n'
                      << "equation_family: " << result.equation_family << '\n'
                      << "backend: " << result.backend << '\n'
                      << "residual_inf: " << result.residual_inf << '\n'
                      << "report: " << *output << '\n';
            return result.success ? 0 : 5;
        }
        if (command == "solve-ode") {
            const auto state_option = option(argc, argv, "--states");
            const auto initial_option = option(argc, argv, "--initial");
            const auto rhs_sources = options(argc, argv, "--rhs");
            const auto start = option(argc, argv, "--start");
            const auto end = option(argc, argv, "--end");
            const auto maximum_step = option(argc, argv, "--max-step");
            const auto output = option(argc, argv, "--output");
            if (!state_option || !initial_option || rhs_sources.empty() || !start ||
                !end || !maximum_step || !output) {
                throw std::invalid_argument(
                    "solve-ode requires --states, --initial, repeated --rhs, --start, --end, --max-step and --output");
            }
            const auto states = comma_list(state_option);
            const auto initial_state = array_values(*initial_option);
            if (states.empty() || states.size() != initial_state.size() ||
                rhs_sources.size() != states.size()) {
                throw std::invalid_argument("ODE state, initial-state and RHS counts must match");
            }
            std::set<std::string> allowed_names(states.begin(), states.end());
            allowed_names.insert("time");
            std::vector<smave::Expression> right_hand_sides;
            for (const auto& source : rhs_sources) {
                right_hand_sides.emplace_back(source);
                for (const auto& name : right_hand_sides.back().names()) {
                    if (!allowed_names.contains(name)) {
                        throw std::invalid_argument("ODE RHS references unknown name: " + name);
                    }
                }
            }
            smave::VerifiedOdeSolveProblem problem;
            problem.initial_state = initial_state;
            problem.right_hand_side = [&](double time, const std::vector<double>& state,
                                          std::vector<double>& derivative) {
                if (state.size() != states.size()) return false;
                std::unordered_map<std::string, double> context{{"time", time}};
                for (std::size_t index = 0; index < states.size(); ++index) {
                    context.emplace(states[index], state[index]);
                }
                derivative.resize(right_hand_sides.size());
                try {
                    for (std::size_t index = 0; index < right_hand_sides.size(); ++index) {
                        derivative[index] = right_hand_sides[index].evaluate(context);
                    }
                } catch (...) {
                    return false;
                }
                return true;
            };
            const auto result = smave::verified_ode_solve(
                problem,
                {.start_time = std::stod(*start),
                 .end_time = std::stod(*end),
                 .maximum_step = std::stod(*maximum_step),
                 .absolute_tolerance = option(argc, argv, "--absolute-tolerance")
                     ? std::stod(*option(argc, argv, "--absolute-tolerance")) : 1.0e-9,
                 .relative_tolerance = option(argc, argv, "--relative-tolerance")
                     ? std::stod(*option(argc, argv, "--relative-tolerance")) : 1.0e-7,
                 .maximum_steps = option(argc, argv, "--maximum-steps")
                     ? std::stoi(*option(argc, argv, "--maximum-steps")) : 100000});
            std::ofstream report(*output);
            if (!report) throw std::runtime_error("cannot write solve-ode report");
            report << std::setprecision(17)
                   << "SMAVE_VERIFIED_ODE_SOLVE 1\n"
                   << "service_id=" << std::quoted(smave::verified_ode_solve_service_v1) << '\n'
                   << "success=" << result.success << '\n'
                   << "used_fallback=" << result.used_fallback << '\n'
                   << "backend=" << std::quoted(result.backend) << '\n'
                   << "plan_id=" << std::quoted(result.plan_id) << '\n'
                   << "equation_family=" << std::quoted(result.equation_family) << '\n'
                   << "residual_inf=" << result.maximum_scaled_local_error << '\n'
                   << "backward_error=" << result.maximum_scaled_local_error << '\n'
                   << "final_time=" << result.final_time << '\n'
                   << "maximum_scaled_local_error="
                   << result.maximum_scaled_local_error << '\n'
                   << "accepted_steps=" << result.accepted_steps << '\n'
                   << "rejected_steps=" << result.rejected_steps << '\n'
                   << "solution=";
            for (std::size_t index = 0; index < result.solution.size(); ++index) {
                if (index != 0) report << ',';
                report << result.solution[index];
            }
            report << '\n' << "diagnostic=" << std::quoted(result.diagnostic) << '\n'
                   << "END\n";
            std::cout << "service_id: " << smave::verified_ode_solve_service_v1 << '\n'
                      << "success: " << result.success << '\n'
                      << "plan_id: " << result.plan_id << '\n';
            return result.success ? 0 : 5;
        }
        if (command == "snapshot-data") {
            if (argc < 3) throw std::invalid_argument("snapshot-data requires a source directory");
            const auto dataset = option(argc, argv, "--dataset");
            const auto store = option(argc, argv, "--store");
            if (!dataset || !store) {
                throw std::invalid_argument("snapshot-data requires --dataset and --store");
            }
            const auto manifest = smave::DatasetStore(*store).snapshot(argv[2], *dataset);
            std::cout << "dataset: " << manifest.dataset_id << '\n'
                      << "version: " << manifest.version << '\n'
                      << "files: " << manifest.files.size() << '\n'
                      << "bytes: " << manifest.total_bytes << '\n'
                      << "verified: 1\n";
            return 0;
        }
        if (command == "verify-data") {
            const auto dataset = option(argc, argv, "--dataset");
            const auto version = option(argc, argv, "--version");
            const auto store = option(argc, argv, "--store");
            if (!dataset || !version || !store) {
                throw std::invalid_argument(
                    "verify-data requires --dataset, --version and --store");
            }
            const auto manifest = smave::DatasetStore(*store).verify(*dataset, *version);
            std::cout << "dataset: " << manifest.dataset_id << '\n'
                      << "version: " << manifest.version << '\n'
                      << "files: " << manifest.files.size() << '\n'
                      << "bytes: " << manifest.total_bytes << '\n'
                      << "verified: 1\n";
            return 0;
        }
        if (command == "compile") {
            if (argc < 3) throw std::invalid_argument("compile requires a .mo source");
            const auto output = option(argc, argv, "--output");
            if (!output) throw std::invalid_argument("compile requires --output");
            const auto model = smave::compile_model(argv[2], option(argc, argv, "--top"));
            model.write(*output);
            std::cout << "compiled " << model.model_id << " -> " << *output
                      << " (" << model.blocks.size() << " blocks)\n";
            return 0;
        }
        if (command == "assess-equation") {
            if (argc < 3) throw std::invalid_argument("assess-equation requires an IR file");
            const auto output = option(argc, argv, "--output");
            if (!output) throw std::invalid_argument("assess-equation requires --output");
            const auto model = smave::ModelIR::read(argv[2]);
            const auto block_id = option(argc, argv, "--block");
            if (!block_id && model.blocks.size() != 1) {
                throw std::invalid_argument(
                    "assess-equation requires --block for a multi-block model");
            }
            const auto block = std::find_if(
                model.blocks.begin(), model.blocks.end(), [&](const smave::BlockIR& candidate) {
                    return !block_id || candidate.id == *block_id;
                });
            if (block == model.blocks.end()) {
                throw std::invalid_argument("assessment block does not exist");
            }

            const auto config_path = option(argc, argv, "--config");
            auto config = config_path
                ? smave::RuntimeConfig::read(*config_path)
                : smave::RuntimeConfig{};
            config.validate();
            if (option(argc, argv, "--profile") && option(argc, argv, "--family-profile")) {
                throw std::invalid_argument("choose only one Router profile");
            }
            if (const auto profile_path = option(argc, argv, "--profile")) {
                const auto profile = smave::read_competition_report(*profile_path);
                if (profile.block_fingerprint != block->fingerprint) {
                    throw std::invalid_argument("competition profile targets another block");
                }
                smave::apply_competition_profile(profile, config.routing);
            }
            if (const auto profile_path = option(argc, argv, "--family-profile")) {
                const auto profile = smave::read_family_router_evaluation(*profile_path);
                if (profile.heldout_block_fingerprint != block->fingerprint) {
                    throw std::invalid_argument("family Router profile targets another block");
                }
                smave::apply_family_router_evaluation(profile, config.routing);
            }

            auto registry = smave::make_default_registry(model);
            const auto bundle_path = option(argc, argv, "--bundle");
            auto bundle = bundle_path
                ? smave::RuntimeBundle::read(*bundle_path)
                : smave::make_default_bundle(model);
            if (const auto expert_path = option(argc, argv, "--expert")) {
                const auto certificate_path = option(argc, argv, "--certificate");
                const auto certificate = certificate_path
                    ? std::optional<smave::VerificationCertificate>(
                          smave::VerificationCertificate::read(*certificate_path))
                    : std::nullopt;
                const auto [version, hash] = register_artifact(
                    registry, model, *expert_path, certificate);
                if (!bundle_path) {
                    bundle.add_expert(
                        version, hash, registry.grant(version).evidence_bundle);
                }
            }
            smave::BlockContext context;
            if (const auto scenario = option(argc, argv, "--scenario")) {
                context.values = smave::read_scenario(*scenario);
            }
            smave::write_equation_assessment_report(
                model, *block, context, registry, bundle, config.routing, *output);
            const auto assessment = smave::assess_equation(*block);
            std::cout << "block: " << block->id << '\n'
                      << "family: " << assessment.equation_family << '\n'
                      << "unknowns: " << assessment.unknown_count << '\n'
                      << "structural_density: " << assessment.structural_density << '\n'
                      << "report: " << *output << '\n';
            return 0;
        }
        if (command == "import-fmu") {
            if (argc < 3) throw std::invalid_argument("import-fmu requires a .fmu or directory");
            const auto output = option(argc, argv, "--output");
            if (!output) throw std::invalid_argument("import-fmu requires --output");
            const auto model = smave::import_fmu(argv[2]);
            model.write(*output);
            if (const auto report = option(argc, argv, "--report")) {
                smave::write_fmi_import_report(model, *report);
            }
            std::cout << "imported FMI " << model.fmi_version << ' ' << model.model_name
                      << " interfaces=" << model.interfaces.size()
                      << " variables=" << model.variables.size()
                      << " mode=blackbox-degraded -> " << *output << '\n';
            return 0;
        }
        if (command == "inspect-fmi") {
            if (argc < 3) throw std::invalid_argument("inspect-fmi requires an FMI IR");
            const auto model = smave::FmiBlackboxIR::read(argv[2]);
            std::cout << "model: " << model.model_name << '\n'
                      << "fmi_version: " << model.fmi_version << '\n'
                      << "source_hash: " << model.source_hash << '\n'
                      << "host_platform: " << model.host_platform << '\n'
                      << "host_binary_candidate: "
                      << model.host_binary_candidate_available << '\n'
                      << "interfaces: " << model.interfaces.size() << '\n'
                      << "variables: " << model.variables.size() << '\n'
                      << "equation_level_validation_allowed: "
                      << model.equation_level_validation_allowed << '\n'
                      << "direct_expert_allowed: " << model.direct_expert_allowed << '\n';
            return 0;
        }
        if (command == "simulate-ssp") {
            if (argc < 3) throw std::invalid_argument("simulate-ssp requires a .ssp archive");
            const auto end = option(argc, argv, "--end");
            const auto step = option(argc, argv, "--step");
            const auto output = option(argc, argv, "--output");
            if (!end || !step || !output) {
                throw std::invalid_argument(
                    "simulate-ssp requires --end, --step and --output");
            }
            const auto result = smave::simulate_ssp(
                argv[2], std::stod(*end), std::stod(*step),
                flag(argc, argv, "--allow-native-execution"));
            smave::write_ssp_report(result, *output);
            std::cout << "success: " << result.success << '\n'
                      << "components: " << result.components.size() << '\n'
                      << "communication_steps: " << result.communication_steps << '\n'
                      << "report: " << *output << '\n';
            return result.success ? 0 : 10;
        }
        if (command == "smoke-fmu") {
            if (argc < 3) throw std::invalid_argument("smoke-fmu requires a .fmu or directory");
            const auto end = option(argc, argv, "--end");
            const auto step = option(argc, argv, "--step");
            const auto output = option(argc, argv, "--output");
            if (!end || !step || !output) {
                throw std::invalid_argument("smoke-fmu requires --end, --step and --output");
            }
            const auto input_values = option(argc, argv, "--input");
            std::map<std::string, double> inputs;
            if (input_values) {
                for (const auto& item : comma_list(input_values)) {
                    const auto separator = item.find('=');
                    if (separator == std::string::npos || separator == 0 ||
                        separator + 1 == item.size()) {
                        throw std::invalid_argument("--input requires name=value pairs");
                    }
                    inputs[item.substr(0, separator)] =
                        std::stod(item.substr(separator + 1));
                }
            }
            const auto model = smave::import_fmu(argv[2]);
            const bool allow_native_execution =
                flag(argc, argv, "--allow-native-execution");
            const auto asynchronous_timeout = option(
                argc, argv, "--async-timeout-ms");
            if (asynchronous_timeout && model.fmi_version != "2.0") {
                throw std::invalid_argument(
                    "--async-timeout-ms is supported only for FMI 2.0 Co-Simulation");
            }
            const auto result = model.fmi_version == "2.0"
                ? smave::smoke_fmi2_co_simulation(
                    argv[2], std::stod(*end), std::stod(*step), inputs,
                    allow_native_execution,
                    asynchronous_timeout
                        ? std::stoull(*asynchronous_timeout)
                        : 100U)
                : smave::smoke_fmi3_co_simulation(
                    argv[2], std::stod(*end), std::stod(*step), inputs,
                    allow_native_execution);
            smave::write_fmi_smoke_report(model, result, *output);
            std::cout << "success: " << result.success << '\n'
                      << "samples: " << result.samples.size() << '\n'
                      << "state_roundtrip_passed: " << result.state_roundtrip_passed << '\n'
                      << "report: " << *output << '\n';
            return result.success ? 0 : 10;
        }
        if (command == "smoke-fmu-me") {
            if (argc < 3) throw std::invalid_argument("smoke-fmu-me requires a .fmu archive");
            const auto end = option(argc, argv, "--end");
            const auto step = option(argc, argv, "--step");
            const auto output = option(argc, argv, "--output");
            if (!end || !step || !output) {
                throw std::invalid_argument("smoke-fmu-me requires --end, --step and --output");
            }
            std::map<std::string, double> inputs;
            if (const auto input_values = option(argc, argv, "--input")) {
                for (const auto& item : comma_list(input_values)) {
                    const auto separator = item.find('=');
                    if (separator == std::string::npos || separator == 0 ||
                        separator + 1 == item.size()) {
                        throw std::invalid_argument("--input requires name=value pairs");
                    }
                    inputs[item.substr(0, separator)] =
                        std::stod(item.substr(separator + 1));
                }
            }
            std::map<std::string, std::string> string_inputs;
            for (const auto& item : options(argc, argv, "--string-input")) {
                const auto separator = item.find('=');
                if (separator == std::string::npos || separator == 0) {
                    throw std::invalid_argument(
                        "--string-input requires name=value pairs");
                }
                const auto [iterator, inserted] = string_inputs.emplace(
                    item.substr(0, separator), item.substr(separator + 1));
                if (!inserted) {
                    throw std::invalid_argument(
                        "duplicate --string-input variable: " + iterator->first);
                }
            }
            std::map<std::string, std::vector<std::uint8_t>> binary_inputs;
            for (const auto& item : options(argc, argv, "--binary-input")) {
                const auto separator = item.find('=');
                if (separator == std::string::npos || separator == 0) {
                    throw std::invalid_argument(
                        "--binary-input requires name=hex pairs");
                }
                const auto [iterator, inserted] = binary_inputs.emplace(
                    item.substr(0, separator), hexadecimal_bytes(item.substr(separator + 1)));
                if (!inserted) {
                    throw std::invalid_argument(
                        "duplicate --binary-input variable: " + iterator->first);
                }
            }
            std::map<std::string, std::vector<double>> array_inputs;
            for (const auto& item : options(argc, argv, "--array-input")) {
                const auto separator = item.find('=');
                if (separator == std::string::npos || separator == 0) {
                    throw std::invalid_argument(
                        "--array-input requires name=value-list pairs");
                }
                const auto [iterator, inserted] = array_inputs.emplace(
                    item.substr(0, separator), array_values(item.substr(separator + 1)));
                if (!inserted) {
                    throw std::invalid_argument(
                        "duplicate --array-input variable: " + iterator->first);
                }
            }
            std::map<std::string, std::vector<std::string>> string_array_inputs;
            for (const auto& item : options(argc, argv, "--string-array-input")) {
                const auto separator = item.find('=');
                if (separator == std::string::npos || separator == 0) {
                    throw std::invalid_argument(
                        "--string-array-input requires name=value elements");
                }
                string_array_inputs[item.substr(0, separator)].push_back(
                    item.substr(separator + 1));
            }
            std::map<std::string, std::vector<std::vector<std::uint8_t>>>
                binary_array_inputs;
            for (const auto& item : options(argc, argv, "--binary-array-input")) {
                const auto separator = item.find('=');
                if (separator == std::string::npos || separator == 0) {
                    throw std::invalid_argument(
                        "--binary-array-input requires name=hex elements");
                }
                binary_array_inputs[item.substr(0, separator)].push_back(
                    hexadecimal_bytes(item.substr(separator + 1)));
            }
            const auto model = smave::import_fmu(argv[2]);
            if (model.fmi_version == "2.0" && !binary_inputs.empty()) {
                throw std::invalid_argument("FMI 2.0 does not define Binary variables");
            }
            if (model.fmi_version == "2.0" &&
                (!array_inputs.empty() || !string_array_inputs.empty() ||
                 !binary_array_inputs.empty())) {
                throw std::invalid_argument("FMI 2.0 smoke does not support array variables");
            }
            const bool allow_native_execution =
                flag(argc, argv, "--allow-native-execution");
            const auto result = model.fmi_version == "2.0"
                ? smave::smoke_fmi2_model_exchange(
                    argv[2], std::stod(*end), std::stod(*step), inputs,
                    allow_native_execution, string_inputs)
                : smave::smoke_fmi3_model_exchange(
                    argv[2], std::stod(*end), std::stod(*step), inputs,
                    allow_native_execution, string_inputs, binary_inputs, array_inputs,
                    string_array_inputs, binary_array_inputs);
            smave::write_fmi_smoke_report(model, result, *output);
            std::cout << "success: " << result.success << '\n'
                      << "samples: " << result.samples.size() << '\n'
                      << "state_roundtrip_passed: " << result.state_roundtrip_passed << '\n'
                      << "report: " << *output << '\n';
            return result.success ? 0 : 10;
        }
        if (command == "smoke-fmu-se") {
            if (argc < 3) throw std::invalid_argument("smoke-fmu-se requires a .fmu archive");
            const auto end = option(argc, argv, "--end");
            const auto interval = option(argc, argv, "--interval");
            const auto output = option(argc, argv, "--output");
            if (!end || !interval || !output) {
                throw std::invalid_argument(
                    "smoke-fmu-se requires --end, --interval and --output");
            }
            std::map<std::string, double> inputs;
            if (const auto input_values = option(argc, argv, "--input")) {
                for (const auto& item : comma_list(input_values)) {
                    const auto separator = item.find('=');
                    if (separator == std::string::npos || separator == 0 ||
                        separator + 1 == item.size()) {
                        throw std::invalid_argument("--input requires name=value pairs");
                    }
                    inputs[item.substr(0, separator)] =
                        std::stod(item.substr(separator + 1));
                }
            }
            const auto result = smave::smoke_fmi3_scheduled_execution(
                argv[2], std::stod(*end), std::stod(*interval),
                inputs,
                flag(argc, argv, "--allow-native-execution"));
            const auto model = smave::import_fmu(argv[2]);
            smave::write_fmi_smoke_report(model, result, *output);
            std::cout << "success: " << result.success << '\n'
                      << "activations: " << result.model_partition_activations << '\n'
                      << "report: " << *output << '\n';
            return result.success ? 0 : 10;
        }
        if (command == "compile-continuous") {
            if (argc < 3) throw std::invalid_argument("compile-continuous requires a .mo source");
            const auto output = option(argc, argv, "--output");
            if (!output) throw std::invalid_argument("compile-continuous requires --output");
            const auto model = smave::compile_continuous_model(
                argv[2], option(argc, argv, "--top").value_or(""));
            model.write(*output);
            std::cout << "compiled continuous " << model.model_id
                      << " states=" << model.states.size()
                      << " events=" << model.events.size()
                      << " -> " << *output << '\n';
            return 0;
        }
        if (command == "compile-dae") {
            if (argc < 3) throw std::invalid_argument("compile-dae requires a .mo source");
            const auto output = option(argc, argv, "--output");
            if (!output) throw std::invalid_argument("compile-dae requires --output");
            const auto model = smave::compile_index_one_dae(
                argv[2], option(argc, argv, "--top").value_or(""));
            model.write(*output);
            std::cout << "compiled index-1 DAE " << model.model_id
                      << " states=" << model.states.size()
                      << " algebraics=" << model.algebraics.size()
                      << " -> " << *output << '\n';
            return 0;
        }
        if (command == "compile-index2-dae") {
            if (argc < 3) {
                throw std::invalid_argument(
                    "compile-index2-dae requires a .mo source");
            }
            const auto output = option(argc, argv, "--output");
            if (!output) {
                throw std::invalid_argument("compile-index2-dae requires --output");
            }
            const auto model = smave::compile_index_two_dae(
                argv[2], option(argc, argv, "--top").value_or(""));
            model.write(*output);
            std::cout << "compiled index-2 DAE " << model.model_id
                      << " states=" << model.states.size()
                      << " multipliers=" << model.multipliers.size()
                      << " -> " << *output << '\n';
            return 0;
        }
        if (command == "assess-index2-dae") {
            if (argc < 3) {
                throw std::invalid_argument("assess-index2-dae requires an IR file");
            }
            const auto output = option(argc, argv, "--output");
            if (!output) {
                throw std::invalid_argument("assess-index2-dae requires --output");
            }
            const auto model = smave::IndexTwoDaeIR::read(argv[2]);
            const auto plan = smave::route_index_two_dae(model);
            std::ofstream report(*output);
            if (!report) throw std::runtime_error("cannot write index-2 assessment");
            report << std::setprecision(17)
                   << "SMAVE_INDEX2_DAE_ASSESSMENT 1\n"
                   << "MODEL " << std::quoted(model.model_id) << '\n'
                   << "FAMILY " << std::quoted(plan.assessment.equation_family) << '\n'
                   << "UNKNOWNS " << plan.assessment.unknown_count << '\n'
                   << "PLAN " << std::quoted(plan.plan_id) << '\n';
            for (const auto& step : plan.steps) {
                report << "EXPERT " << std::quoted(step.expert_version)
                       << " ROLE " << std::quoted(smave::to_string(step.backend_role))
                       << " REASON " << std::quoted(step.selection_reason) << '\n';
                for (const auto& stage : step.backend_chain) {
                    report << "STAGE " << std::quoted(stage) << '\n';
                }
            }
            report << "TERMINAL_FALLBACK "
                   << std::quoted(plan.terminal_fallback) << '\n';
            std::cout << "family: " << plan.assessment.equation_family << '\n'
                      << "unknowns: " << plan.assessment.unknown_count << '\n'
                      << "plan_id: " << plan.plan_id << '\n'
                      << "report: " << *output << '\n';
            return 0;
        }
        if (command == "simulate-index2-dae") {
            if (argc < 3) {
                throw std::invalid_argument("simulate-index2-dae requires an IR file");
            }
            const auto end = option(argc, argv, "--end");
            const auto maximum_step = option(argc, argv, "--max-step");
            const auto output = option(argc, argv, "--output");
            if (!end || !maximum_step || !output) {
                throw std::invalid_argument(
                    "simulate-index2-dae requires --end, --max-step and --output");
            }
            const auto model = smave::IndexTwoDaeIR::read(argv[2]);
            const auto result = smave::simulate_index_two_dae(
                model, std::stod(*end), std::stod(*maximum_step));
            smave::write_index_two_dae_report(model, result, *output);
            std::cout << "success: " << result.success << '\n'
                      << "plan_id: " << result.plan_id << '\n'
                      << "solver_backend: " << result.solver_backend << '\n'
                      << "steps: " << result.steps.size() << '\n'
                      << "hidden_rank_checks: " << result.hidden_rank_checks << '\n'
                      << "report: " << *output << '\n';
            return result.success ? 0 : 21;
        }
        if (command == "compile-complementarity") {
            if (argc < 3) {
                throw std::invalid_argument(
                    "compile-complementarity requires a .mo source");
            }
            const auto output = option(argc, argv, "--output");
            if (!output) {
                throw std::invalid_argument(
                    "compile-complementarity requires --output");
            }
            const auto model = smave::compile_complementarity(
                argv[2], option(argc, argv, "--top").value_or(""));
            model.write(*output);
            std::cout << "compiled complementarity " << model.model_id
                      << " pairs=" << model.variables.size()
                      << " -> " << *output << '\n';
            return 0;
        }
        if (command == "assess-complementarity") {
            if (argc < 3) {
                throw std::invalid_argument(
                    "assess-complementarity requires an IR file");
            }
            const auto output = option(argc, argv, "--output");
            if (!output) {
                throw std::invalid_argument(
                    "assess-complementarity requires --output");
            }
            const auto model = smave::ComplementarityIR::read(argv[2]);
            const auto plan = smave::route_complementarity(model);
            std::ofstream report(*output);
            if (!report) {
                throw std::runtime_error(
                    "cannot write complementarity assessment report");
            }
            report << std::setprecision(17)
                   << "SMAVE_COMPLEMENTARITY_ASSESSMENT 1\n"
                   << "MODEL " << std::quoted(model.model_id) << '\n'
                   << "FAMILY " << std::quoted(plan.assessment.equation_family) << '\n'
                   << "UNKNOWNS " << plan.assessment.unknown_count << '\n'
                   << "STRUCTURAL_NONZEROS "
                   << plan.assessment.structural_nonzeros << '\n'
                   << "PLAN " << std::quoted(plan.plan_id) << '\n';
            for (const auto& step : plan.steps) {
                report << "EXPERT " << std::quoted(step.expert_version)
                       << " ROLE " << std::quoted(smave::to_string(step.backend_role))
                       << " REASON " << std::quoted(step.selection_reason) << '\n';
            }
            report << "TERMINAL_FALLBACK "
                   << std::quoted(plan.terminal_fallback) << '\n';
            std::cout << "family: " << plan.assessment.equation_family << '\n'
                      << "unknowns: " << plan.assessment.unknown_count << '\n'
                      << "plan_id: " << plan.plan_id << '\n'
                      << "report: " << *output << '\n';
            return 0;
        }
        if (command == "solve-complementarity") {
            if (argc < 3) {
                throw std::invalid_argument(
                    "solve-complementarity requires an IR file");
            }
            const auto output = option(argc, argv, "--output");
            if (!output) {
                throw std::invalid_argument(
                    "solve-complementarity requires --output");
            }
            const auto model = smave::ComplementarityIR::read(argv[2]);
            const auto result = smave::solve_complementarity(model);
            smave::write_complementarity_report(model, result, *output);
            std::cout << "success: " << result.success << '\n'
                      << "plan_id: " << result.plan_id << '\n'
                      << "accepted_backend: " << result.accepted_backend << '\n'
                      << "attempts: " << result.attempts.size() << '\n'
                      << "report: " << *output << '\n';
            return result.success ? 0 : 20;
        }
        if (command == "compile-implicit-dae") {
            if (argc < 3) {
                throw std::invalid_argument("compile-implicit-dae requires a .mo source");
            }
            const auto output = option(argc, argv, "--output");
            if (!output) {
                throw std::invalid_argument("compile-implicit-dae requires --output");
            }
            const auto model = smave::compile_fully_implicit_dae(
                argv[2], option(argc, argv, "--top").value_or(""));
            model.write(*output);
            std::cout << "compiled fully implicit DAE " << model.model_id
                      << " states=" << model.states.size()
                      << " algebraics=" << model.algebraics.size()
                      << " -> " << *output << '\n';
            return 0;
        }
        if (command == "assess-implicit-dae") {
            if (argc < 3) {
                throw std::invalid_argument("assess-implicit-dae requires an IR file");
            }
            const auto output = option(argc, argv, "--output");
            if (!output) {
                throw std::invalid_argument("assess-implicit-dae requires --output");
            }
            const auto model = smave::FullyImplicitDaeIR::read(argv[2]);
            std::optional<smave::DaeMultigridArtifact> artifact;
            if (const auto multigrid = option(argc, argv, "--multigrid")) {
                artifact = smave::DaeMultigridArtifact::read(*multigrid);
            }
            const auto assessment = smave::assess_equation(model);
            smave::write_equation_assessment_report(
                model, {}, *output, artifact ? &*artifact : nullptr);
            std::cout << "family: " << assessment.equation_family << '\n'
                      << "unknowns: " << assessment.unknown_count << '\n'
                      << "structural_density: " << assessment.structural_density << '\n'
                      << "report: " << *output << '\n';
            return 0;
        }
        if (command == "simulate-implicit-dae") {
            if (argc < 3) {
                throw std::invalid_argument("simulate-implicit-dae requires an IR file");
            }
            const auto end = option(argc, argv, "--end");
            const auto maximum_step = option(argc, argv, "--max-step");
            const auto output = option(argc, argv, "--output");
            if (!end || !maximum_step || !output) {
                throw std::invalid_argument(
                    "simulate-implicit-dae requires --end, --max-step and --output");
            }
            const auto model = smave::FullyImplicitDaeIR::read(argv[2]);
            std::optional<smave::DaeMultigridArtifact> artifact;
            if (const auto multigrid = option(argc, argv, "--multigrid")) {
                artifact = smave::DaeMultigridArtifact::read(*multigrid);
            }
            const auto result = smave::simulate_fully_implicit_dae(
                model, std::stod(*end), std::stod(*maximum_step), {},
                artifact ? &*artifact : nullptr);
            smave::write_fully_implicit_dae_report(model, result, *output);
            std::cout << "success: " << result.success << '\n'
                      << "plan_id: " << result.plan_id << '\n'
                      << "solver_backend: " << result.solver_backend << '\n'
                      << "initialization_iterations: "
                      << result.initialization_iterations << '\n'
                      << "initialization_residual: "
                      << result.initialization_residual_inf << '\n'
                      << "steps: " << result.steps.size() << '\n'
                      << "maximum_residual: " << result.maximum_residual_inf << '\n'
                      << "dense_initialization_fallbacks: "
                      << result.dense_initialization_fallbacks << '\n'
                      << "dense_step_fallbacks: " << result.dense_step_fallbacks << '\n'
                      << "learned_preconditioned_steps: "
                      << result.learned_preconditioned_steps << '\n'
                      << "learned_rejections: " << result.learned_rejections << '\n'
                      << "report: " << *output << '\n';
            return result.success ? 0 : 19;
        }
        if (command == "train-implicit-dae-multigrid") {
            if (argc < 3) {
                throw std::invalid_argument(
                    "train-implicit-dae-multigrid requires an implicit DAE IR");
            }
            const auto scenarios = option(argc, argv, "--scenarios");
            const auto output = option(argc, argv, "--output");
            if (!output) {
                throw std::invalid_argument(
                    "train-implicit-dae-multigrid requires --output");
            }
            const auto scenario_input = scenario_source(argc, argv, scenarios);
            const auto model = smave::FullyImplicitDaeIR::read(argv[2]);
            auto artifact = smave::train_dae_multigrid(
                model, scenario_input.directory);
            if (scenario_input.dataset) {
                artifact.hierarchy.schema_version = "smave.learned-multigrid.v3";
                artifact.hierarchy.training_dataset_id = scenario_input.dataset->dataset_id;
                artifact.hierarchy.training_dataset_version = scenario_input.dataset->version;
                artifact.hierarchy.training_dataset_manifest_hash =
                    scenario_input.dataset->manifest_hash;
                artifact.hierarchy.expert_version.clear();
                artifact.hierarchy.seal();
                artifact.training_dataset_id = scenario_input.dataset->dataset_id;
                artifact.training_dataset_version = scenario_input.dataset->version;
                artifact.training_dataset_manifest_hash =
                    scenario_input.dataset->manifest_hash;
                artifact.seal();
                artifact.validate();
            }
            artifact.write(*output);
            std::cout << "trained " << artifact.hierarchy.expert_version
                      << " family=" << artifact.residual_family
                      << " samples=" << artifact.training_samples
                      << " step_domain=[" << artifact.minimum_step << ','
                      << artifact.maximum_step << "] contraction="
                      << artifact.hierarchy.maximum_probe_contraction
                      << " -> " << *output << '\n';
            return 0;
        }
        if (command == "simulate-dae") {
            if (argc < 3) throw std::invalid_argument("simulate-dae requires a DAE IR");
            const auto end = option(argc, argv, "--end");
            const auto maximum_step = option(argc, argv, "--max-step");
            const auto output = option(argc, argv, "--output");
            if (!end || !maximum_step || !output) {
                throw std::invalid_argument("simulate-dae requires --end, --max-step and --output");
            }
            const auto model = smave::IndexOneDaeIR::read(argv[2]);
            std::optional<smave::DaeMultigridArtifact> artifact;
            if (const auto multigrid = option(argc, argv, "--multigrid")) {
                artifact = smave::DaeMultigridArtifact::read(*multigrid);
            }
            const auto result = smave::simulate_index_one_dae(
                model, std::stod(*end), std::stod(*maximum_step), {},
                artifact ? &*artifact : nullptr);
            smave::write_dae_report(model, result, *output);
            std::cout << "success: " << result.success << '\n'
                      << "initialization_iterations: "
                      << result.initialization_iterations << '\n'
                      << "initialization_residual: "
                      << result.initialization_residual_inf << '\n'
                      << "steps: " << result.steps.size() << '\n'
                      << "maximum_residual: " << result.maximum_residual_inf << '\n'
                      << "minimum_algebraic_rank_margin: "
                      << result.minimum_algebraic_rank_margin << '\n'
                      << "learned_preconditioned_steps: "
                      << result.learned_preconditioned_steps << '\n'
                      << "learned_rejections: " << result.learned_rejections << '\n'
                      << "dense_step_fallbacks: " << result.dense_step_fallbacks << '\n'
                      << "report: " << *output << '\n';
            return result.success ? 0 : 18;
        }
        if (command == "train-dae-multigrid") {
            if (argc < 3) {
                throw std::invalid_argument("train-dae-multigrid requires a DAE IR");
            }
            const auto scenarios = option(argc, argv, "--scenarios");
            const auto output = option(argc, argv, "--output");
            if (!output) {
                throw std::invalid_argument(
                    "train-dae-multigrid requires --output");
            }
            const auto scenario_input = scenario_source(argc, argv, scenarios);
            const auto model = smave::IndexOneDaeIR::read(argv[2]);
            auto artifact = smave::train_dae_multigrid(model, scenario_input.directory);
            if (scenario_input.dataset) {
                artifact.hierarchy.schema_version = "smave.learned-multigrid.v3";
                artifact.hierarchy.training_dataset_id = scenario_input.dataset->dataset_id;
                artifact.hierarchy.training_dataset_version = scenario_input.dataset->version;
                artifact.hierarchy.training_dataset_manifest_hash =
                    scenario_input.dataset->manifest_hash;
                artifact.hierarchy.expert_version.clear();
                artifact.hierarchy.seal();
                artifact.schema_version = "smave.dae-multigrid.v2";
                artifact.training_dataset_id = scenario_input.dataset->dataset_id;
                artifact.training_dataset_version = scenario_input.dataset->version;
                artifact.training_dataset_manifest_hash = scenario_input.dataset->manifest_hash;
                artifact.seal();
                artifact.validate();
            }
            artifact.write(*output);
            std::cout << "trained " << artifact.hierarchy.expert_version
                      << " samples=" << artifact.training_samples
                      << " step_domain=[" << artifact.minimum_step << ','
                      << artifact.maximum_step << "] contraction="
                      << artifact.hierarchy.maximum_probe_contraction
                      << " -> " << *output << '\n';
            return 0;
        }
        if (command == "simulate-continuous") {
            if (argc < 3) throw std::invalid_argument("simulate-continuous requires an IR file");
            const auto end = option(argc, argv, "--end");
            const auto maximum_step = option(argc, argv, "--max-step");
            const auto output = option(argc, argv, "--output");
            if (!end || !maximum_step || !output) {
                throw std::invalid_argument("simulate-continuous requires --end, --max-step and --output");
            }
            const auto model = smave::ContinuousHybridIR::read(argv[2]);
            auto result = smave::simulate_continuous(
                model,
                option(argc, argv, "--start")
                    ? std::stod(*option(argc, argv, "--start"))
                    : 0.0,
                std::stod(*end), std::stod(*maximum_step));
            if (const auto reference = option(argc, argv, "--reference")) {
                smave::validate_continuous_reference(result, *reference);
            }
            smave::write_continuous_report(model, result, *output);
            std::cout << "success: " << result.success << '\n'
                      << "events: " << result.events.size() << '\n'
                      << "accepted_steps: " << result.accepted_steps << '\n'
                      << "rejected_steps: " << result.rejected_steps << '\n'
                      << "maximum_guard_residual: " << result.maximum_guard_residual << '\n'
                      << "maximum_event_time_error: " << result.maximum_event_time_error << '\n'
                      << "reference_order_matched: " << result.reference_order_matched << '\n'
                      << "reference_time_matched: " << result.reference_time_matched << '\n'
                      << "report: " << *output << '\n';
            return result.success && result.reference_order_matched &&
                    result.reference_time_matched
                ? 0 : 15;
        }
        if (command == "simulate-coupled") {
            if (argc < 3) throw std::invalid_argument("simulate-coupled requires a continuous IR");
            const auto hybrid = option(argc, argv, "--hybrid");
            const auto end = option(argc, argv, "--end");
            const auto maximum_step = option(argc, argv, "--max-step");
            const auto output = option(argc, argv, "--output");
            if (!hybrid || !end || !maximum_step || !output) {
                throw std::invalid_argument(
                    "simulate-coupled requires --hybrid, --end, --max-step and --output");
            }
            const auto continuous = smave::ContinuousHybridIR::read(argv[2]);
            const auto sampled = smave::HybridProgramIR::read(*hybrid);
            const auto candidate_path = option(argc, argv, "--candidates");
            const auto candidates = candidate_path
                ? smave::read_event_candidates(*candidate_path)
                : std::vector<smave::EventCandidate>{};
            const auto result = smave::simulate_coupled(
                continuous, sampled, std::stod(*end), std::stod(*maximum_step), {}, candidates);
            smave::write_coupled_report(continuous, sampled, result, *output);
            std::cout << "success: " << result.success << '\n'
                      << "samples: " << result.samples.size() << '\n'
                      << "continuous_events: " << result.continuous_events.size() << '\n'
                      << "sampled_events: " << result.sampled_events.size() << '\n'
                      << "rejected_candidates: " << result.rejected_candidates << '\n'
                      << "maximum_guard_residual: " << result.maximum_guard_residual << '\n'
                      << "report: " << *output << '\n';
            return result.success ? 0 : 17;
        }
        if (command == "inspect") {
            if (argc < 3) throw std::invalid_argument("inspect requires an IR file");
            inspect(smave::ModelIR::read(argv[2]));
            return 0;
        }
        if (command == "train-expert") {
            if (argc < 3) throw std::invalid_argument("train-expert requires an IR file");
            const auto block = option(argc, argv, "--block");
            const auto scenarios = option(argc, argv, "--scenarios");
            const auto output = option(argc, argv, "--output");
            if (!block || !output) {
                throw std::invalid_argument(
                    "train-expert requires --block and --output");
            }
            const auto scenario_input = scenario_source(argc, argv, scenarios);
            const auto model = smave::ModelIR::read(argv[2]);
            auto artifact = smave::train_affine_warm_start(
                model, *block, scenario_input.directory,
                option(argc, argv, "--trace-dir").value_or(".smave/training-traces"));
            if (scenario_input.dataset) {
                artifact.schema_version = "smave.affine-warm-start.v2";
                artifact.training_dataset_id = scenario_input.dataset->dataset_id;
                artifact.training_dataset_version = scenario_input.dataset->version;
                artifact.training_dataset_manifest_hash = scenario_input.dataset->manifest_hash;
                artifact.expert_version.clear();
                artifact.seal();
                artifact.validate();
            }
            artifact.write(*output);
            std::cout << "trained " << artifact.expert_version
                      << " samples=" << artifact.training_samples
                      << " rmse=" << artifact.training_rmse
                      << " -> " << *output << '\n';
            return 0;
        }
        if (command == "train-preconditioner") {
            if (argc < 3) {
                throw std::invalid_argument("train-preconditioner requires an IR file");
            }
            const auto block = option(argc, argv, "--block");
            const auto scenarios = option(argc, argv, "--scenarios");
            const auto output = option(argc, argv, "--output");
            if (!block || !output) {
                throw std::invalid_argument(
                    "train-preconditioner requires --block and --output");
            }
            const auto scenario_input = scenario_source(argc, argv, scenarios);
            const auto model = smave::ModelIR::read(argv[2]);
            auto artifact = smave::train_linear_preconditioner(
                model, *block, scenario_input.directory);
            if (scenario_input.dataset) {
                artifact.schema_version = "smave.linear-preconditioner.v2";
                artifact.training_dataset_id = scenario_input.dataset->dataset_id;
                artifact.training_dataset_version = scenario_input.dataset->version;
                artifact.training_dataset_manifest_hash = scenario_input.dataset->manifest_hash;
                artifact.expert_version.clear();
                artifact.seal();
                artifact.validate();
            }
            artifact.write(*output);
            std::cout << "trained " << artifact.expert_version
                      << " samples=" << artifact.training_samples
                      << " max_matrix_drift=" << artifact.maximum_matrix_drift
                      << " -> " << *output << '\n';
            return 0;
        }
        if (command == "train-multigrid") {
            if (argc < 3) throw std::invalid_argument("train-multigrid requires an IR file");
            const auto block = option(argc, argv, "--block");
            const auto scenarios = option(argc, argv, "--scenarios");
            const auto output = option(argc, argv, "--output");
            if (!block || !output) {
                throw std::invalid_argument(
                    "train-multigrid requires --block and --output");
            }
            const auto scenario_input = scenario_source(argc, argv, scenarios);
            const auto model = smave::ModelIR::read(argv[2]);
            auto artifact = smave::train_learned_multigrid(
                model, *block, scenario_input.directory);
            if (scenario_input.dataset) {
                artifact.schema_version = "smave.learned-multigrid.v3";
                artifact.training_dataset_id = scenario_input.dataset->dataset_id;
                artifact.training_dataset_version = scenario_input.dataset->version;
                artifact.training_dataset_manifest_hash = scenario_input.dataset->manifest_hash;
                artifact.expert_version.clear();
                artifact.seal();
                artifact.validate();
            }
            artifact.write(*output);
            std::cout << "trained " << artifact.expert_version
                      << " samples=" << artifact.training_samples
                      << " contraction=" << artifact.maximum_probe_contraction
                      << " weight=" << artifact.smoothing_weight
                      << " -> " << *output << '\n';
            return 0;
        }
        if (command == "train-operator") {
            if (argc < 3) throw std::invalid_argument("train-operator requires an IR file");
            const auto block = option(argc, argv, "--block");
            const auto scenarios = option(argc, argv, "--scenarios");
            const auto output = option(argc, argv, "--output");
            if (!block || !output) {
                throw std::invalid_argument(
                    "train-operator requires --block and --output");
            }
            const auto scenario_input = scenario_source(argc, argv, scenarios);
            const auto model = smave::ModelIR::read(argv[2]);
            auto artifact = smave::train_latent_operator(
                model,
                *block,
                scenario_input.directory,
                option(argc, argv, "--trace-dir").value_or(".smave/operator-labels"),
                option(argc, argv, "--rank")
                    ? std::stoul(*option(argc, argv, "--rank"))
                    : 8,
                comma_list(option(argc, argv, "--qoi")));
            if (scenario_input.dataset) {
                artifact.schema_version = "smave.latent-operator.v2";
                artifact.training_dataset_id = scenario_input.dataset->dataset_id;
                artifact.training_dataset_version = scenario_input.dataset->version;
                artifact.training_dataset_manifest_hash =
                    scenario_input.dataset->manifest_hash;
                artifact.expert_version.clear();
                artifact.seal();
                artifact.validate();
            }
            artifact.write(*output);
            std::cout << "trained " << artifact.expert_version
                      << " samples=" << artifact.training_samples
                      << " rank=" << artifact.state_basis.size()
                      << " retained_energy=" << artifact.retained_energy
                      << " rmse=" << artifact.training_rmse
                      << " training_us=" << artifact.training_wall_us
                      << " -> " << *output << '\n';
            return 0;
        }
        if (command == "verify-expert") {
            if (argc < 3) throw std::invalid_argument("verify-expert requires an IR file");
            const auto expert_path = option(argc, argv, "--expert");
            const auto output = option(argc, argv, "--output");
            if (!expert_path || !output) {
                throw std::invalid_argument("verify-expert requires --expert and --output");
            }
            const auto model = smave::ModelIR::read(argv[2]);
            const std::size_t maximum_depth = option(argc, argv, "--max-depth")
                ? std::stoul(*option(argc, argv, "--max-depth"))
                : 4;
            const std::filesystem::path trace_directory = option(
                argc, argv, "--trace-dir").value_or(".smave/verification-probes");
            smave::VerificationCertificate certificate;
            if (artifact_kind(*expert_path) == ArtifactKind::affine_warm_start) {
                certificate = smave::verify_affine_warm_start(
                    model,
                    smave::AffineWarmStartArtifact::read(*expert_path),
                    maximum_depth,
                    trace_directory);
            } else if (artifact_kind(*expert_path) == ArtifactKind::diagonal_preconditioner) {
                certificate = smave::verify_linear_preconditioner(
                    model,
                    smave::LinearPreconditionerArtifact::read(*expert_path),
                    maximum_depth,
                    trace_directory);
            } else if (artifact_kind(*expert_path) == ArtifactKind::learned_multigrid) {
                certificate = smave::verify_learned_multigrid(
                    model,
                    smave::LearnedMultigridArtifact::read(*expert_path),
                    maximum_depth,
                    trace_directory);
            } else {
                certificate = smave::verify_latent_operator(
                    model,
                    smave::LatentOperatorArtifact::read(*expert_path),
                    maximum_depth,
                    trace_directory);
            }
            certificate.write(*output);
            std::cout << "verified_cells: " << certificate.cells.size() << '\n'
                      << "counterexamples: " << certificate.counterexamples.size() << '\n'
                      << "probes: " << certificate.total_probes << '\n'
                      << "certificate_hash: " << certificate.certificate_hash << '\n'
                      << "output: " << *output << '\n';
            return certificate.cells.empty() ? 7 : 0;
        }
        if (command == "export-counterexamples") {
            if (argc < 3) {
                throw std::invalid_argument(
                    "export-counterexamples requires a verification certificate");
            }
            const auto output = option(argc, argv, "--output");
            if (!output) {
                throw std::invalid_argument("export-counterexamples requires --output");
            }
            const auto certificate = smave::VerificationCertificate::read(argv[2]);
            certificate.export_counterexamples(*output);
            std::cout << "exported: " << certificate.counterexamples.size() << '\n'
                      << "directory: " << *output << '\n';
            return 0;
        }
        if (command == "index-family") {
            if (argc < 3) throw std::invalid_argument("index-family requires an IR file");
            const auto block_id = option(argc, argv, "--block");
            const auto expert_path = option(argc, argv, "--expert");
            const auto family = option(argc, argv, "--family");
            const auto output = option(argc, argv, "--output");
            if (!block_id || !expert_path || !family || !output) {
                throw std::invalid_argument(
                    "index-family requires --block, --expert, --family and --output");
            }
            const auto model = smave::ModelIR::read(argv[2]);
            const auto block = std::find_if(
                model.blocks.begin(), model.blocks.end(),
                [&](const smave::BlockIR& item) { return item.id == *block_id; });
            if (block == model.blocks.end()) throw std::invalid_argument("unknown block");
            std::string version;
            std::string hash;
            if (artifact_kind(*expert_path) == ArtifactKind::affine_warm_start) {
                const auto artifact = smave::AffineWarmStartArtifact::read(*expert_path);
                version = artifact.expert_version;
                hash = artifact.artifact_hash;
            } else if (artifact_kind(*expert_path) == ArtifactKind::diagonal_preconditioner) {
                const auto artifact = smave::LinearPreconditionerArtifact::read(*expert_path);
                version = artifact.expert_version;
                hash = artifact.artifact_hash;
            } else if (artifact_kind(*expert_path) == ArtifactKind::learned_multigrid) {
                const auto artifact = smave::LearnedMultigridArtifact::read(*expert_path);
                version = artifact.expert_version;
                hash = artifact.artifact_hash;
            } else {
                const auto artifact = smave::LatentOperatorArtifact::read(*expert_path);
                version = artifact.expert_version;
                hash = artifact.artifact_hash;
            }
            smave::FamilyIndex index;
            if (std::filesystem::exists(*output)) index = smave::FamilyIndex::read(*output);
            index.add(smave::FamilyEntry{
                .family_id = *family,
                .expert_version = version,
                .artifact_hash = hash,
                .embedding = smave::encode_block(model, *block),
            });
            index.write(*output);
            std::cout << "indexed_family: " << *family << '\n'
                      << "expert_version: " << version << '\n'
                      << "output: " << *output << '\n';
            return 0;
        }
        if (command == "retrieve-family") {
            if (argc < 3) throw std::invalid_argument("retrieve-family requires an IR file");
            const auto block_id = option(argc, argv, "--block");
            const auto index_path = option(argc, argv, "--index");
            if (!block_id || !index_path) {
                throw std::invalid_argument("retrieve-family requires --block and --index");
            }
            const auto model = smave::ModelIR::read(argv[2]);
            const auto block = std::find_if(
                model.blocks.begin(), model.blocks.end(),
                [&](const smave::BlockIR& item) { return item.id == *block_id; });
            if (block == model.blocks.end()) throw std::invalid_argument("unknown block");
            const std::size_t top_k = option(argc, argv, "--top-k")
                ? std::stoul(*option(argc, argv, "--top-k"))
                : 4;
            const auto matches = smave::FamilyIndex::read(*index_path).query(
                smave::encode_block(model, *block), top_k);
            std::cout << "permission: shadow\n"
                      << "matches: " << matches.size() << '\n';
            for (const auto& match : matches) {
                std::cout << match.family_id
                          << " expert=" << match.expert_version
                          << " similarity=" << match.similarity
                          << " transfer_risk=" << match.transfer_risk << '\n';
            }
            return 0;
        }
        if (command == "evaluate-family-router") {
            if (argc < 3) {
                throw std::invalid_argument(
                    "evaluate-family-router requires a source IR file");
            }
            const auto profile = option(argc, argv, "--profile");
            const auto heldout = option(argc, argv, "--heldout");
            const auto scenarios = option(argc, argv, "--scenarios");
            const auto output = option(argc, argv, "--output");
            if (!profile || !heldout || !output) {
                throw std::invalid_argument(
                    "evaluate-family-router requires --profile, --heldout and --output");
            }
            const auto scenario_input = scenario_source(argc, argv, scenarios);
            const auto source_model = smave::ModelIR::read(argv[2]);
            const auto heldout_model = smave::ModelIR::read(*heldout);
            const auto heldout_registry = smave::make_default_registry(heldout_model);
            const auto heldout_bundle = smave::make_default_bundle(heldout_model);
            const auto evaluation = smave::evaluate_family_router(
                source_model,
                smave::read_competition_report(*profile),
                heldout_model,
                heldout_registry,
                heldout_bundle,
                scenario_input.directory,
                option(argc, argv, "--trace-dir")
                    .value_or(".smave/family-router-traces"),
                option(argc, argv, "--minimum-similarity")
                    ? std::stod(*option(argc, argv, "--minimum-similarity"))
                    : 0.95,
                option(argc, argv, "--minimum-speedup")
                    ? std::stod(*option(argc, argv, "--minimum-speedup"))
                    : 1.01,
                option(argc, argv, "--repetitions")
                    ? std::stoul(*option(argc, argv, "--repetitions"))
                    : 20,
                {},
                scenario_input.dataset);
            smave::write_family_router_evaluation(evaluation, *output);
            std::cout << "embedding_similarity: " << evaluation.embedding_similarity << '\n'
                      << "fixed_expert: " << evaluation.fixed_expert << '\n'
                      << "calibrated_expert: " << evaluation.calibrated_expert << '\n'
                      << "oracle_expert: " << evaluation.oracle_expert << '\n'
                      << "calibrated_speedup: " << evaluation.calibrated_speedup << '\n'
                      << "paired_speedup_ci95: ["
                      << evaluation.paired_speedup_ci95_lower << ", "
                      << evaluation.paired_speedup_ci95_upper << "]\n"
                      << "p99_not_regressed: " << evaluation.p99_not_regressed << '\n'
                      << "dangerous_misroutes: "
                      << evaluation.calibrated_dangerous_misroutes << '\n'
                      << "improved: " << evaluation.improved << '\n'
                      << "safe: " << evaluation.safe << '\n'
                      << "report: " << *output << '\n';
            return evaluation.improved && evaluation.safe ? 0 : 9;
        }
        if (command == "bundle") {
            if (argc < 3) throw std::invalid_argument("bundle requires an IR file");
            const auto output = option(argc, argv, "--output");
            if (!output) throw std::invalid_argument("bundle requires --output");
            const auto model = smave::ModelIR::read(argv[2]);
            auto bundle = smave::make_default_bundle(model);
            if (const auto expert_path = option(argc, argv, "--expert")) {
                auto registry = smave::make_default_registry(model);
                const auto certificate_path = option(argc, argv, "--certificate");
                const auto certificate = certificate_path
                    ? std::optional<smave::VerificationCertificate>(
                          smave::VerificationCertificate::read(*certificate_path))
                    : std::nullopt;
                const auto [version, hash] = register_artifact(
                    registry, model, *expert_path, certificate);
                bundle.add_expert(
                    version, hash, registry.grant(version).evidence_bundle);
            }
            bundle.write(*output);
            std::cout << "bundled " << bundle.bundle_id << " -> " << *output
                      << " hash=" << bundle.bundle_hash << '\n';
            return 0;
        }
        if (command == "solve") {
            if (argc < 3) throw std::invalid_argument("solve requires an IR file");
            const auto scenario = option(argc, argv, "--scenario");
            if (!scenario) throw std::invalid_argument("solve requires --scenario");
            const auto trace_directory = option(argc, argv, "--trace-dir")
                .value_or(".smave/traces");
            const auto model = smave::ModelIR::read(argv[2]);
            const auto config_path = option(argc, argv, "--config");
            auto config = config_path
                ? smave::RuntimeConfig::read(*config_path)
                : smave::RuntimeConfig{};
            config.validate();
            if (option(argc, argv, "--profile") && option(argc, argv, "--family-profile")) {
                throw std::invalid_argument("choose only one Router profile");
            }
            if (const auto profile_path = option(argc, argv, "--profile")) {
                const auto profile = smave::read_competition_report(*profile_path);
                if (model.blocks.size() != 1 ||
                    profile.block_fingerprint != model.blocks.front().fingerprint) {
                    throw std::invalid_argument("competition profile targets another block");
                }
                smave::apply_competition_profile(profile, config.routing);
            }
            if (const auto profile_path = option(argc, argv, "--family-profile")) {
                const auto profile = smave::read_family_router_evaluation(*profile_path);
                if (model.blocks.size() != 1 ||
                    profile.heldout_block_fingerprint != model.blocks.front().fingerprint) {
                    throw std::invalid_argument("family Router profile targets another block");
                }
                smave::apply_family_router_evaluation(profile, config.routing);
            }
            const auto bundle_path = option(argc, argv, "--bundle");
            auto registry = smave::make_default_registry(model);
            if (const auto expert_path = option(argc, argv, "--expert")) {
                const auto certificate_path = option(argc, argv, "--certificate");
                const auto certificate = certificate_path
                    ? std::optional<smave::VerificationCertificate>(
                          smave::VerificationCertificate::read(*certificate_path))
                    : std::nullopt;
                (void)register_artifact(registry, model, *expert_path, certificate);
            }
            const smave::Runtime runtime = bundle_path
                ? smave::Runtime(
                      model,
                      std::move(registry),
                      smave::RuntimeBundle::read(*bundle_path),
                      config.tolerance,
                      config.routing)
                : smave::Runtime(model, config.tolerance, config.routing);
            const auto outcome = runtime.solve(
                smave::read_scenario(*scenario), trace_directory);
            std::cout << std::setprecision(17);
            std::cout << "status: " << (outcome.success ? "success" : "failure") << '\n';
            for (const auto& block : outcome.blocks) {
                std::cout << block.block_id << ": path=" << smave::to_string(block.path)
                          << " plan=" << block.plan_id
                          << " residual_inf=" << block.gate.residual_inf
                          << " expert_iterations=" << block.expert_iterations
                          << " fallback_iterations=" << block.fallback_iterations
                          << " krylov_iterations=" << block.krylov_iterations
                          << " total_us=" << block.timing.total_us << '\n';
            }
            for (const auto& variable : model.variables) {
                if (outcome.values.contains(variable.name)) {
                    std::cout << variable.name << '=' << outcome.values.at(variable.name) << '\n';
                }
            }
            std::cout << "trace: " << trace_directory << '/' << outcome.trace_id << ".trace\n";
            std::cout << "summary: direct=" << outcome.direct_count
                      << " corrected=" << outcome.corrected_count
                      << " warm_start=" << outcome.warm_start_count
                      << " fallback=" << outcome.fallback_count
                      << " total_us=" << outcome.timing.total_us << '\n';
            std::cout << "message: " << outcome.message << '\n';
            return outcome.success ? 0 : 3;
        }
        if (command == "validate") {
            if (argc < 3) throw std::invalid_argument("validate requires an IR file");
            const auto scenarios = option(argc, argv, "--scenarios");
            const auto output = option(argc, argv, "--output");
            if (!output) {
                throw std::invalid_argument("validate requires --output");
            }
            const auto scenario_input = scenario_source(argc, argv, scenarios);
            const auto model = smave::ModelIR::read(argv[2]);
            const auto config_path = option(argc, argv, "--config");
            auto config = config_path
                ? smave::RuntimeConfig::read(*config_path)
                : smave::RuntimeConfig{};
            config.validate();
            if (option(argc, argv, "--profile") && option(argc, argv, "--family-profile")) {
                throw std::invalid_argument("choose only one Router profile");
            }
            if (const auto profile_path = option(argc, argv, "--profile")) {
                const auto profile = smave::read_competition_report(*profile_path);
                if (model.blocks.size() != 1 ||
                    profile.block_fingerprint != model.blocks.front().fingerprint) {
                    throw std::invalid_argument("competition profile targets another block");
                }
                smave::apply_competition_profile(profile, config.routing);
            }
            if (const auto profile_path = option(argc, argv, "--family-profile")) {
                const auto profile = smave::read_family_router_evaluation(*profile_path);
                if (model.blocks.size() != 1 ||
                    profile.heldout_block_fingerprint != model.blocks.front().fingerprint) {
                    throw std::invalid_argument("family Router profile targets another block");
                }
                smave::apply_family_router_evaluation(profile, config.routing);
            }
            const auto bundle_path = option(argc, argv, "--bundle");
            auto registry = smave::make_default_registry(model);
            if (const auto expert_path = option(argc, argv, "--expert")) {
                const auto certificate_path = option(argc, argv, "--certificate");
                const auto certificate = certificate_path
                    ? std::optional<smave::VerificationCertificate>(
                          smave::VerificationCertificate::read(*certificate_path))
                    : std::nullopt;
                (void)register_artifact(registry, model, *expert_path, certificate);
            }
            const smave::Runtime runtime = bundle_path
                ? smave::Runtime(
                      model,
                      std::move(registry),
                      smave::RuntimeBundle::read(*bundle_path),
                      config.tolerance,
                      config.routing)
                : smave::Runtime(model, config.tolerance, config.routing);
            const auto trace_directory = option(argc, argv, "--trace-dir")
                .value_or(".smave/validation-traces");
            auto report = smave::validate_scenarios(
                runtime, scenario_input.directory, trace_directory);
            if (scenario_input.dataset) {
                report.schema_version = 3;
                report.dataset_id = scenario_input.dataset->dataset_id;
                report.dataset_version = scenario_input.dataset->version;
                report.dataset_manifest_hash = scenario_input.dataset->manifest_hash;
            }
            smave::write_validation_report(report, *output);
            std::cout << "scenarios: " << report.scenarios << '\n'
                      << "admitted_invocations: " << report.admitted_invocations << '\n'
                      << "top_k_pass_rate: " << report.top_k_pass_rate << '\n'
                      << "fallback_rate: " << report.fallback_rate << '\n'
                      << "erroneous_accepts: " << report.erroneous_accepts << '\n'
                      << "safety_evaluations: " << report.safety_evaluations << '\n'
                      << "erroneous_accept_rate_upper_bound: "
                      << report.erroneous_accept_rate_upper_bound << '\n'
                      << "top_k_target_met: " << report.top_k_target_met << '\n'
                      << "safety_target_met: " << report.safety_target_met << '\n'
                      << "confidence_target_met: "
                      << report.confidence_target_met << '\n'
                      << "report: " << *output << '\n';
            return report.original_solver_failures == 0 && report.safety_target_met &&
                    report.top_k_target_met
                ? 0
                : 4;
        }
        if (command == "compete") {
            if (argc < 3) throw std::invalid_argument("compete requires an IR file");
            const auto scenarios = option(argc, argv, "--scenarios");
            const auto bundle_path = option(argc, argv, "--bundle");
            const auto output = option(argc, argv, "--output");
            if (!bundle_path || !output) {
                throw std::invalid_argument(
                    "compete requires --bundle and --output");
            }
            const auto scenario_input = scenario_source(argc, argv, scenarios);
            const auto model = smave::ModelIR::read(argv[2]);
            const auto config_path = option(argc, argv, "--config");
            const auto config = config_path
                ? smave::RuntimeConfig::read(*config_path)
                : smave::RuntimeConfig{};
            config.validate();
            auto registry = smave::make_default_registry(model);
            if (const auto expert_path = option(argc, argv, "--expert")) {
                const auto certificate_path = option(argc, argv, "--certificate");
                const auto certificate = certificate_path
                    ? std::optional<smave::VerificationCertificate>(
                          smave::VerificationCertificate::read(*certificate_path))
                    : std::nullopt;
                (void)register_artifact(registry, model, *expert_path, certificate);
            }
            const auto trace_directory = option(argc, argv, "--trace-dir")
                .value_or(".smave/competition-traces");
            auto report = smave::compete_experts(
                model,
                registry,
                smave::RuntimeBundle::read(*bundle_path),
                scenario_input.directory,
                trace_directory,
                config.tolerance,
                option(argc, argv, "--repetitions")
                    ? std::stoul(*option(argc, argv, "--repetitions"))
                    : 1);
            if (scenario_input.dataset) {
                report.schema_version = 4;
                report.dataset_id = scenario_input.dataset->dataset_id;
                report.dataset_version = scenario_input.dataset->version;
                report.dataset_manifest_hash = scenario_input.dataset->manifest_hash;
                report.seal();
                report.validate();
            }
            smave::write_competition_report(report, *output);
            std::cout << "candidates: " << report.entries.size() << '\n'
                      << "winner: " << report.winner << '\n'
                      << "report: " << *output << '\n';
            return report.winner.empty() ? 8 : 0;
        }
        if (command == "benchmark") {
            if (argc < 3) throw std::invalid_argument("benchmark requires an IR file");
            const auto scenarios = option(argc, argv, "--scenarios");
            const auto expert_path = option(argc, argv, "--expert");
            const auto bundle_path = option(argc, argv, "--bundle");
            const auto output = option(argc, argv, "--output");
            if (!expert_path || !bundle_path || !output) {
                throw std::invalid_argument(
                    "benchmark requires --expert, --bundle and --output");
            }
            const auto scenario_input = scenario_source(argc, argv, scenarios);
            const auto model = smave::ModelIR::read(argv[2]);
            const auto config_path = option(argc, argv, "--config");
            const auto config = config_path
                ? smave::RuntimeConfig::read(*config_path)
                : smave::RuntimeConfig{};
            config.validate();
            auto accelerated_registry = smave::make_default_registry(model);
            const auto certificate_path = option(argc, argv, "--certificate");
            const auto certificate = certificate_path
                ? std::optional<smave::VerificationCertificate>(
                      smave::VerificationCertificate::read(*certificate_path))
                : std::nullopt;
            (void)register_artifact(
                accelerated_registry, model, *expert_path, certificate);
            const smave::Runtime baseline(
                model, config.tolerance, config.routing);
            const smave::Runtime accelerated(
                model,
                std::move(accelerated_registry),
                smave::RuntimeBundle::read(*bundle_path),
                config.tolerance,
                config.routing);
            const std::size_t repetitions = option(argc, argv, "--repetitions")
                ? std::stoul(*option(argc, argv, "--repetitions"))
                : 20;
            const std::size_t bootstrap_samples = option(argc, argv, "--bootstrap-samples")
                ? std::stoul(*option(argc, argv, "--bootstrap-samples"))
                : 2000;
            const auto trace_directory = option(argc, argv, "--trace-dir")
                .value_or(".smave/benchmark-traces");
            auto report = smave::benchmark_runtimes(
                baseline,
                accelerated,
                scenario_input.directory,
                trace_directory,
                repetitions,
                3,
                bootstrap_samples);
            if (scenario_input.dataset) {
                report.schema_version = 2;
                report.dataset_id = scenario_input.dataset->dataset_id;
                report.dataset_version = scenario_input.dataset->version;
                report.dataset_manifest_hash = scenario_input.dataset->manifest_hash;
                report.seal();
                report.validate();
            }
            smave::write_performance_report(report, *output);
            std::cout << "samples: " << report.samples << '\n'
                      << "baseline_median_us: " << report.baseline_wall_us.median << '\n'
                      << "accelerated_median_us: " << report.accelerated_wall_us.median << '\n'
                      << "median_speedup: " << report.median_speedup << '\n'
                      << "p99_speedup: " << report.p99_speedup << '\n'
                      << "paired_median_speedup: "
                      << report.paired_median_speedup << '\n'
                      << "paired_p01_speedup: "
                      << report.paired_p01_speedup << '\n'
                      << "paired_win_rate: " << report.paired_win_rate << '\n'
                      << "paired_speedup_ci95: ["
                      << report.paired_speedup_ci95_lower << ", "
                      << report.paired_speedup_ci95_upper << "]\n"
                      << "baseline_median_iterations: "
                      << report.baseline_iterations.median << '\n'
                      << "baseline_mean_iterations: "
                      << report.baseline_iterations.mean << '\n'
                      << "accelerated_median_iterations: "
                      << report.accelerated_iterations.median << '\n'
                      << "accelerated_mean_iterations: "
                      << report.accelerated_iterations.mean << '\n'
                      << "same_accuracy: " << report.same_accuracy << '\n'
                      << "p99_not_regressed: " << report.p99_not_regressed << '\n'
                      << "report: " << *output << '\n';
            return report.same_accuracy ? 0 : 5;
        }
        if (command == "batch-solve") {
            if (argc < 3) throw std::invalid_argument("batch-solve requires an IR file");
            const auto scenarios_path = option(argc, argv, "--scenarios");
            const auto expert_path = option(argc, argv, "--expert");
            const auto output = option(argc, argv, "--output");
            if (!scenarios_path || !expert_path || !output) {
                throw std::invalid_argument(
                    "batch-solve requires --scenarios, --expert and --output");
            }
            const auto model = smave::ModelIR::read(argv[2]);
            const auto artifact = smave::LinearPreconditionerArtifact::read(*expert_path);
            if (artifact.model_source_hash != model.source_hash) {
                throw std::invalid_argument("expert artifact targets a different source model");
            }
            const smave::LearnedLinearPreconditionerExpert expert(artifact);
            std::vector<std::filesystem::path> paths;
            for (const auto& entry : std::filesystem::directory_iterator(*scenarios_path)) {
                if (entry.is_regular_file() && entry.path().extension() == ".conf") {
                    paths.push_back(entry.path());
                }
            }
            std::sort(paths.begin(), paths.end());
            if (paths.empty()) throw std::invalid_argument("batch scenario suite is empty");
            std::vector<std::unordered_map<std::string, double>> scenarios;
            for (const auto& path : paths) scenarios.push_back(smave::read_scenario(path));
            const std::size_t maximum_batch = option(argc, argv, "--max-batch")
                ? std::stoul(*option(argc, argv, "--max-batch"))
                : 32;
            const auto device = option(argc, argv, "--device").value_or("cpu");
            const smave::Runtime fallback(model);
            const auto result = smave::TensorBucketScheduler(
                maximum_batch, device).solve_linear_batch(
                model,
                model.blocks.front(),
                expert,
                scenarios,
                fallback,
                option(argc, argv, "--trace-dir").value_or(".smave/batch-traces"));
            std::ofstream report(*output);
            if (!report) throw std::runtime_error("cannot write batch report");
            report << std::setprecision(17)
                   << "SMAVE_BATCH 1\n"
                   << "device=" << std::quoted(result.metrics.device) << '\n'
                   << "device_backend=" << std::quoted(result.metrics.device_backend) << '\n'
                   << "device_name=" << std::quoted(result.metrics.device_name) << '\n'
                   << "requests=" << result.metrics.requests << '\n'
                   << "batches=" << result.metrics.batches << '\n'
                   << "maximum_batch=" << result.metrics.maximum_batch << '\n'
                   << "average_batch=" << result.metrics.average_batch << '\n'
                   << "utilization=" << result.metrics.utilization << '\n'
                   << "kernel_us=" << result.metrics.kernel_us << '\n'
                   << "gate_us=" << result.metrics.gate_us << '\n'
                   << "fallback_us=" << result.metrics.fallback_us << '\n'
                   << "total_us=" << result.metrics.total_us << '\n'
                   << "sequential_baseline_us="
                   << result.metrics.sequential_baseline_us << '\n'
                   << "throughput_speedup=" << result.metrics.throughput_speedup << '\n'
                   << "accepted=" << result.metrics.accepted << '\n'
                   << "fallback_count=" << result.metrics.fallback_count << '\n'
                   << "baseline_failures=" << result.metrics.baseline_failures << '\n'
                   << "device_batches=" << result.metrics.device_batches << '\n'
                   << "device_rejections=" << result.metrics.device_rejections << '\n'
                   << "device_upload_us=" << result.metrics.device_upload_us << '\n'
                   << "device_download_us=" << result.metrics.device_download_us << '\n'
                   << "device_maximum_absolute_error="
                   << result.metrics.device_maximum_absolute_error << '\n'
                   << "device_maximum_relative_error="
                   << result.metrics.device_maximum_relative_error << '\n'
                   << "cpu_refinement_steps="
                   << result.metrics.cpu_refinement_steps << '\n'
                   << "END\n";
            std::cout << "requests: " << result.metrics.requests << '\n'
                      << "batches: " << result.metrics.batches << '\n'
                      << "average_batch: " << result.metrics.average_batch << '\n'
                      << "utilization: " << result.metrics.utilization << '\n'
                      << "accepted: " << result.metrics.accepted << '\n'
                      << "fallback_count: " << result.metrics.fallback_count << '\n'
                      << "total_us: " << result.metrics.total_us << '\n'
                      << "sequential_baseline_us: "
                      << result.metrics.sequential_baseline_us << '\n'
                      << "throughput_speedup: "
                      << result.metrics.throughput_speedup << '\n'
                      << "report: " << *output << '\n';
            return result.metrics.accepted + result.metrics.fallback_count ==
                    result.metrics.requests
                ? 0
                : 6;
        }
        if (command == "benchmark-operator") {
            if (argc < 3) throw std::invalid_argument("benchmark-operator requires an IR file");
            const auto scenarios = option(argc, argv, "--scenarios");
            const auto expert_path = option(argc, argv, "--expert");
            const auto bundle_path = option(argc, argv, "--bundle");
            const auto output = option(argc, argv, "--output");
            const auto projected = option(argc, argv, "--projected-queries");
            if (!expert_path || !bundle_path || !output || !projected) {
                throw std::invalid_argument(
                    "benchmark-operator requires expert, bundle, output and projected-queries");
            }
            const auto scenario_input = scenario_source(argc, argv, scenarios);
            const auto model = smave::ModelIR::read(argv[2]);
            const auto artifact = smave::LatentOperatorArtifact::read(*expert_path);
            auto registry = smave::make_default_registry(model);
            const auto certificate_path = option(argc, argv, "--certificate");
            const auto certificate = certificate_path
                ? std::optional<smave::VerificationCertificate>(
                      smave::VerificationCertificate::read(*certificate_path))
                : std::nullopt;
            const auto operator_device = option(argc, argv, "--device").value_or("cpu");
            smave::register_latent_operator(
                registry, artifact, "domain-v1", "default", operator_device, certificate);
            auto report = smave::benchmark_latent_operator(
                model,
                registry,
                smave::RuntimeBundle::read(*bundle_path),
                artifact,
                scenario_input.directory,
                option(argc, argv, "--trace-dir").value_or(".smave/operator-benchmark"),
                option(argc, argv, "--repetitions")
                    ? std::stoul(*option(argc, argv, "--repetitions"))
                    : 20,
                std::stoul(*projected), {}, operator_device);
            if (scenario_input.dataset) {
                report.dataset_id = scenario_input.dataset->dataset_id;
                report.dataset_version = scenario_input.dataset->version;
                report.dataset_manifest_hash = scenario_input.dataset->manifest_hash;
                report.seal();
                report.validate();
            }
            smave::write_operator_benchmark_report(report, *output);
            std::cout << "requests: " << report.requests << '\n'
                      << "batches: " << report.batches << '\n'
                      << "average_batch: " << report.average_batch << '\n'
                      << "accepted: " << report.accepted << '\n'
                      << "fallbacks: " << report.fallbacks << '\n'
                      << "acceptance_rate: " << report.acceptance_rate << '\n'
                      << "online_speedup: " << report.online_speedup << '\n'
                      << "paired_speedup_ci95: ["
                      << report.paired_speedup_ci95_lower << ", "
                      << report.paired_speedup_ci95_upper << "]\n"
                      << "paired_median_saving_us: "
                      << report.paired_median_saving_us << '\n'
                      << "break_even_queries: " << report.break_even_queries << '\n'
                      << "amortized_speedup: " << report.amortized_speedup << '\n'
                      << "maximum_qoi_error: " << report.maximum_qoi_error << '\n'
                      << "maximum_candidate_qoi_error: "
                      << report.maximum_candidate_qoi_error << '\n'
                      << "candidate_qoi_within_tolerance: "
                      << report.candidate_qoi_within_tolerance << '\n'
                      << "same_accuracy: " << report.same_accuracy << '\n'
                      << "break_even_met: " << report.break_even_met << '\n'
                      << "report: " << *output << '\n';
            return report.same_accuracy && report.break_even_met &&
                    report.acceptance_rate >= 0.95
                ? 0
                : 10;
        }
        if (command == "import-block-graph") {
            if (argc < 3) throw std::invalid_argument("import-block-graph requires an export file");
            const auto output = option(argc, argv, "--output");
            if (!output) throw std::invalid_argument("import-block-graph requires --output");
            auto graph = smave::import_block_graph(argv[2]);
            graph.write(*output);
            std::cout << "imported " << graph.model_id << " nodes=" << graph.nodes.size()
                      << " -> " << *output << '\n';
            return 0;
        }
        if (command == "run-model-group") {
            if (argc < 3) throw std::invalid_argument("run-model-group requires a group IR");
            const auto output = option(argc, argv, "--output");
            if (!output) throw std::invalid_argument("run-model-group requires --output");
            const auto graph_path = std::filesystem::path(argv[2]);
            const auto graph = smave::BlockGraphIR::read(graph_path);
            const auto scenario = option(argc, argv, "--scenario");
            const auto inputs = scenario
                ? smave::read_scenario(*scenario)
                : std::unordered_map<std::string, double>{};
            smave::ModelGroupRuntime runtime(graph, graph_path.parent_path());
            const auto result = runtime.execute(inputs, *output);
            std::cout << "success: " << result.success << '\n'
                      << "local_fallbacks: " << result.local_fallback_count << '\n'
                      << "maximum_connection_error: " << result.maximum_connection_error << '\n'
                      << "report: " << *output << '\n';
            return result.success && result.maximum_connection_error <= 1.0e-12 ? 0 : 11;
        }
        if (command == "run-model-group-multirate") {
            if (argc < 3) {
                throw std::invalid_argument("run-model-group-multirate requires a group IR");
            }
            const auto end = option(argc, argv, "--end");
            const auto base_step = option(argc, argv, "--base-step");
            const auto output = option(argc, argv, "--output");
            if (!end || !base_step || !output) {
                throw std::invalid_argument(
                    "run-model-group-multirate requires --end, --base-step and --output");
            }
            const auto graph_path = std::filesystem::path(argv[2]);
            const auto graph = smave::BlockGraphIR::read(graph_path);
            const auto scenario = option(argc, argv, "--scenario");
            const auto inputs = scenario
                ? smave::read_scenario(*scenario)
                : std::unordered_map<std::string, double>{};
            smave::ModelGroupRuntime runtime(graph, graph_path.parent_path());
            const auto result = runtime.execute_multirate(
                std::stod(*end), std::stod(*base_step), inputs,
                option(argc, argv, "--trace-dir").value_or(
                    (std::filesystem::path(*output).parent_path() / "multirate-traces").string()));
            smave::write_multirate_model_group_report(graph, result, *output);
            std::cout << "success: " << result.success << '\n'
                      << "ticks: " << result.ticks.size() << '\n'
                      << "local_fallbacks: " << result.local_fallback_count << '\n'
                      << "maximum_connection_error: " << result.maximum_connection_error << '\n'
                      << "report: " << *output << '\n';
            return result.success && result.maximum_connection_error <= 1.0e-12 ? 0 : 16;
        }
        if (command == "run-hybrid") {
            if (argc < 3) throw std::invalid_argument("run-hybrid requires a hybrid IR");
            const auto ticks = option(argc, argv, "--ticks");
            const auto output = option(argc, argv, "--output");
            if (!ticks || !output) {
                throw std::invalid_argument("run-hybrid requires --ticks and --output");
            }
            const auto program = smave::HybridProgramIR::read(argv[2]);
            const auto candidate_path = option(argc, argv, "--candidates");
            const auto candidates = candidate_path
                ? smave::read_event_candidates(*candidate_path)
                : std::vector<smave::EventCandidate>{};
            const auto result = smave::run_hybrid(program, std::stoul(*ticks), candidates);
            smave::write_hybrid_report(program, result, *output);
            std::cout << "success: " << result.success << '\n'
                      << "events: " << result.events.size() << '\n'
                      << "accepted_candidates: " << result.accepted_candidates << '\n'
                      << "rejected_candidates: " << result.rejected_candidates << '\n'
                      << "missed_events: " << result.missed_events << '\n'
                      << "event_recall: " << result.event_recall << '\n'
                      << "report: " << *output << '\n';
            return result.success &&
                    (candidates.empty() ||
                     (result.missed_events == 0 && result.accepted_candidates > 0))
                ? 0
                : 12;
        }
        if (command == "audit-release") {
            if (argc < 3) throw std::invalid_argument("audit-release requires a model IR");
            const auto bundle = option(argc, argv, "--bundle");
            const auto validation = option(argc, argv, "--validation");
            const auto performance = option(argc, argv, "--performance");
            const auto stage = option(argc, argv, "--stage");
            const auto traffic = option(argc, argv, "--traffic");
            const auto observed = option(argc, argv, "--observed-hours");
            const auto minimum_hours = option(argc, argv, "--minimum-hours");
            const auto minimum_requests = option(argc, argv, "--minimum-requests");
            const auto output = option(argc, argv, "--output");
            if (!bundle || !validation || !performance || !stage || !traffic ||
                !observed || !minimum_hours || !minimum_requests || !output) {
                throw std::invalid_argument("audit-release requires all release gate options");
            }
            const auto audit = smave::audit_release(
                argv[2], *bundle, *validation, *performance, *stage,
                std::stod(*traffic), std::stod(*observed),
                std::stoul(*minimum_requests), std::stod(*minimum_hours), 0.95,
                option(argc, argv, "--parent").value_or(""),
                option(argc, argv, "--dataset-manifest").value_or(""));
            audit.write(*output);
            std::cout << "stage: " << audit.stage << '\n'
                      << "requests: " << audit.requests << '\n'
                      << "safety_met: " << audit.safety_met << '\n'
                      << "performance_met: " << audit.performance_met << '\n'
                      << "promotion_ready: " << audit.promotion_ready << '\n'
                      << "dataset_id: " << audit.dataset_id << '\n'
                      << "dataset_version: " << audit.dataset_version << '\n'
                      << "audit_hash: " << audit.audit_hash << '\n';
            return audit.promotion_ready ? 0 : 13;
        }
        if (command == "sign-release") {
            if (argc < 3) throw std::invalid_argument("sign-release requires a bundle");
            const auto audit = option(argc, argv, "--audit");
            const auto model = option(argc, argv, "--model");
            const auto expert = option(argc, argv, "--expert");
            const auto certificate = option(argc, argv, "--certificate");
            const auto key = option(argc, argv, "--key");
            const auto release = option(argc, argv, "--release");
            const auto output = option(argc, argv, "--output");
            if (!audit || !model || !expert || !certificate || !key || !release || !output) {
                throw std::invalid_argument("sign-release requires model, expert, certificate, audit, key, release and output options");
            }
            const auto manifest = smave::create_release_manifest(
                argv[2], *audit, *key, *release, *model, *expert, *certificate,
                option(argc, argv, "--dataset-manifest").value_or(""));
            manifest.write(*output);
            std::cout << "release: " << manifest.release_id << '\n'
                      << "key_id: " << manifest.key_id << '\n'
                      << "dataset_version: " << manifest.dataset_version << '\n'
                      << "manifest_hash: " << manifest.manifest_hash << '\n';
            return 0;
        }
        if (command == "activate-release") {
            if (argc < 3) throw std::invalid_argument("activate-release requires a manifest");
            const auto bundle = option(argc, argv, "--bundle");
            const auto audit = option(argc, argv, "--audit");
            const auto parent = option(argc, argv, "--parent");
            const auto model = option(argc, argv, "--model");
            const auto expert = option(argc, argv, "--expert");
            const auto certificate = option(argc, argv, "--certificate");
            const auto key = option(argc, argv, "--key");
            const auto store = option(argc, argv, "--store");
            if (!bundle || !audit || !parent || !model || !expert || !certificate || !key || !store) {
                throw std::invalid_argument("activate-release requires all signed payload and store options");
            }
            const auto state = smave::ReleaseStore(*store).activate(
                argv[2], *bundle, *audit, *parent, *model, *expert, *certificate, *key,
                option(argc, argv, "--dataset-manifest").value_or(""));
            std::cout << "generation: " << state.generation << '\n'
                      << "current_release: " << state.current_release << '\n'
                      << "previous_release: " << state.previous_release << '\n';
            return 0;
        }
        if (command == "rollback-release") {
            const auto key = option(argc, argv, "--key");
            const auto store = option(argc, argv, "--store");
            if (!key || !store) {
                throw std::invalid_argument("rollback-release requires --key and --store");
            }
            const auto state = smave::ReleaseStore(*store).rollback(*key);
            std::cout << "generation: " << state.generation << '\n'
                      << "current_release: " << state.current_release << '\n'
                      << "previous_release: " << state.previous_release << '\n';
            return 0;
        }
        if (command == "release-status") {
            const auto store = option(argc, argv, "--store");
            const auto key = option(argc, argv, "--key");
            if (!store || !key) {
                throw std::invalid_argument("release-status requires --store and --key");
            }
            const smave::ReleaseStore release_store(*store);
            const auto active = release_store.verified_active(*key);
            const auto& state = active.state;
            const auto& manifest = active.manifest;
            std::cout << "generation: " << state.generation << '\n'
                      << "current_release: " << state.current_release << '\n'
                      << "previous_release: " << state.previous_release << '\n'
                      << "state_schema: " << state.schema_version << '\n'
                      << "state_key_id: " << state.key_id << '\n'
                      << "state_hash: " << state.state_hash << '\n'
                      << "state_signature_verified: 1\n"
                      << "signature_verified: 1\n"
                      << "bundle_hash: " << manifest.bundle_hash << '\n'
                      << "dataset_id: " << manifest.dataset_id << '\n'
                      << "dataset_version: " << manifest.dataset_version << '\n';
            return 0;
        }
        if (command == "solve-release") {
            const auto store = option(argc, argv, "--store");
            const auto key = option(argc, argv, "--key");
            const auto scenario = option(argc, argv, "--scenario");
            if (!store || !key || !scenario) {
                throw std::invalid_argument("solve-release requires --store, --key and --scenario");
            }
            const smave::ReleaseStore release_store(*store);
            const auto active = release_store.verified_active(*key);
            const auto& manifest = active.manifest;
            const auto& directory = active.directory;
            const auto model = smave::ModelIR::read(directory / "model.ir");
            auto registry = smave::make_default_registry(model);
            const auto certificate = smave::VerificationCertificate::read(
                directory / "certificate.verify");
            (void)register_artifact(
                registry, model, directory / "expert.artifact", certificate);
            const auto outcome = smave::Runtime(
                model, std::move(registry),
                smave::RuntimeBundle::read(directory / "runtime.bundle")).solve(
                    smave::read_scenario(*scenario),
                    option(argc, argv, "--trace-dir").value_or(
                        (directory / "runtime-traces").string()));
            std::cout << "release: " << manifest.release_id << '\n'
                      << "signature_verified: 1\n"
                      << "status: " << (outcome.success ? "success" : "failure") << '\n'
                      << "trace: " << outcome.trace_id << '\n';
            return outcome.success ? 0 : 14;
        }
        if (command == "replay") {
            if (argc < 3) throw std::invalid_argument("replay requires a trace file");
            std::ifstream input(argv[2]);
            if (!input) throw std::runtime_error("cannot read trace");
            std::cout << input.rdbuf();
            return 0;
        }
        throw std::invalid_argument("unknown command: " + command);
    } catch (const std::exception& error) {
        std::cerr << "smave: " << error.what() << '\n';
        return 2;
    }
}
