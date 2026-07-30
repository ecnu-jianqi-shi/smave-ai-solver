#include "smave/compiler.hpp"
#include "smave/expression.hpp"
#include "smave/linear.hpp"
#include "smave/runtime.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

double elapsed_us(Clock::time_point started) {
    return std::chrono::duration<double, std::micro>(Clock::now() - started).count();
}

struct ScaleEvidence {
    std::size_t width{};
    std::size_t unknowns{};
    std::size_t nonzeros{};
    std::size_t dense_bytes{};
    std::size_t csr_bytes{};
    double compile_us{};
    double assemble_us{};
    double setup_us{};
    double solve_us{};
    int iterations{};
    double final_residual{};
    smave::ModelIR ir;
};

std::filesystem::path write_poisson_source(
    const std::filesystem::path& directory, std::size_t width,
    bool nonsymmetric = false) {
    const auto model_name = std::string(nonsymmetric ? "LargeConvectionDiffusion" :
                                                       "LargePoisson") +
        std::to_string(width);
    const auto source = directory / (model_name + ".mo");
    const auto size = width * width;
    std::ofstream model(source);
    model << "model " << model_name << '\n';
    for (std::size_t index = 0; index < size; ++index) {
        model << "  Real x" << index + 1 << "(start=0);\n";
    }
    model << "equation\n";
    for (std::size_t row = 0; row < width; ++row) {
        for (std::size_t column = 0; column < width; ++column) {
            const auto index = row * width + column;
            model << "  4*x" << index + 1;
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

ScaleEvidence measure_scale(
    const std::filesystem::path& directory, std::size_t width) {
    ScaleEvidence evidence;
    evidence.width = width;
    evidence.unknowns = width * width;
    const auto source = write_poisson_source(directory, width);
    const auto model_name = "LargePoisson" + std::to_string(width);
    auto started = Clock::now();
    evidence.ir = smave::compile_model(source, model_name);
    evidence.compile_us = elapsed_us(started);
    require(evidence.ir.blocks.size() == 1 &&
                evidence.ir.blocks.front().unknowns.size() == evidence.unknowns,
            "large Poisson source did not compile to one SCC");

    std::unordered_map<std::string, smave::Expression> residuals;
    for (const auto& equation : evidence.ir.equations) {
        residuals.emplace(equation.id, smave::Expression(equation.residual));
    }
    started = Clock::now();
    const auto system = smave::assemble_linear_system(
        evidence.ir, evidence.ir.blocks.front(), residuals, {});
    evidence.assemble_us = elapsed_us(started);
    require(system.has_sparse_matrix() && !system.has_dense_matrix(),
            "large system retained a dense numeric matrix");
    require(system.symmetric && system.positive_definite,
            "large Poisson SPD sufficient-condition probe did not pass");

    started = Clock::now();
    const auto preconditioner = smave::incomplete_cholesky_zero_preconditioner(
        system, evidence.ir.blocks.front().jacobian_sparsity);
    evidence.setup_us = elapsed_us(started);
    require(static_cast<bool>(preconditioner), "CSR IC(0) construction failed");
    started = Clock::now();
    const auto krylov = smave::preconditioned_conjugate_gradient(
        system, std::vector<double>(evidence.unknowns), preconditioner,
        1.0e-10, 1.0e-8, 10000);
    evidence.solve_us = elapsed_us(started);
    require(krylov.converged && !krylov.residual_history.empty() &&
                krylov.residual_history.back() < 2.0e-8,
            "CSR IC(0)+PCG did not solve the large Poisson system");
    evidence.nonzeros = system.nonzeros();
    evidence.dense_bytes = evidence.unknowns * evidence.unknowns * sizeof(double);
    evidence.csr_bytes = system.sparse_storage_bytes();
    evidence.iterations = krylov.iterations;
    evidence.final_residual = krylov.residual_history.back();
    return evidence;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2) {
            std::cerr << "usage: smave_large_sparse_evidence OUTPUT_DIRECTORY\n";
            return 2;
        }
        const std::filesystem::path output_directory(argv[1]);
        std::filesystem::remove_all(output_directory);
        std::filesystem::create_directories(output_directory);

        std::vector<ScaleEvidence> scales;
        for (const auto width : {33U, 41U, 49U}) {
            scales.push_back(measure_scale(output_directory, width));
        }
        for (std::size_t index = 1; index < scales.size(); ++index) {
            require(scales[index].unknowns > scales[index - 1].unknowns &&
                        scales[index].nonzeros > scales[index - 1].nonzeros &&
                        scales[index].csr_bytes > scales[index - 1].csr_bytes,
                    "large sparse scale curve is not monotonic");
        }
        for (const auto& scale : scales) {
            require(scale.csr_bytes * 10 < scale.dense_bytes,
                    "CSR storage did not materially reduce numeric matrix memory");
        }

        const smave::Runtime runtime(scales.back().ir);
        std::vector<double> runtime_samples;
        constexpr std::size_t repetitions = 32;
        runtime_samples.reserve(repetitions);
        smave::SolveOutcome last_outcome;
        std::string runtime_expert;
        for (std::size_t repetition = 0; repetition < repetitions; ++repetition) {
            const auto started = Clock::now();
            last_outcome = runtime.solve({}, output_directory / "traces");
            runtime_samples.push_back(elapsed_us(started));
            const auto accepted_pcg = std::find_if(
                last_outcome.blocks.front().attempt_records.begin(),
                last_outcome.blocks.front().attempt_records.end(),
                [](const auto& attempt) {
                    return attempt.expert_version == "pcg-ic0-cpu-v1" &&
                        attempt.outcome == "accepted";
                });
            require(last_outcome.success && last_outcome.blocks.size() == 1 &&
                        last_outcome.blocks.front().path == smave::SolvePath::direct_accept &&
                        accepted_pcg !=
                            last_outcome.blocks.front().attempt_records.end() &&
                        last_outcome.blocks.front().gate.decision ==
                            smave::GateDecision::direct_accept,
                    "Runtime did not accept the large CSR PCG chain through the original gate");
            runtime_expert = accepted_pcg->expert_version;
        }
        std::sort(runtime_samples.begin(), runtime_samples.end());
        const auto median_us = runtime_samples[runtime_samples.size() / 2];
        const auto p99_us = runtime_samples.back();

        constexpr std::size_t nonsymmetric_width = 33;
        const auto nonsymmetric_source = write_poisson_source(
            output_directory, nonsymmetric_width, true);
        const auto nonsymmetric_ir = smave::compile_model(
            nonsymmetric_source, "LargeConvectionDiffusion33");
        const smave::Runtime nonsymmetric_runtime(nonsymmetric_ir);
        const auto nonsymmetric_outcome = nonsymmetric_runtime.solve(
            {}, output_directory / "nonsymmetric-traces");
        const auto accepted_gmres = std::find_if(
            nonsymmetric_outcome.blocks.front().attempt_records.begin(),
            nonsymmetric_outcome.blocks.front().attempt_records.end(),
            [](const auto& attempt) {
                return attempt.expert_version == "gmres-ilu0-cpu-v1" &&
                    attempt.outcome == "accepted";
            });
        require(nonsymmetric_outcome.success && nonsymmetric_outcome.blocks.size() == 1 &&
                    accepted_gmres !=
                        nonsymmetric_outcome.blocks.front().attempt_records.end() &&
                    nonsymmetric_outcome.blocks.front().gate.decision ==
                        smave::GateDecision::direct_accept &&
                    std::none_of(
                        nonsymmetric_outcome.blocks.front().attempted_experts.begin(),
                        nonsymmetric_outcome.blocks.front().attempted_experts.end(),
                        [](const std::string& expert) {
                            return expert == "gmres-ilut-cpu-v1";
                        }),
                "large nonsymmetric system did not accept CSR GMRES+ILU(0)");

        std::ofstream report(output_directory / "evidence.txt");
        report << "SMAVE_LARGE_SPARSE_EVIDENCE 2\n"
               << "FAMILY poisson-2d-five-point\n"
               << "SCALES " << scales.size() << '\n';
        for (const auto& scale : scales) {
            report << "SCALE width=" << scale.width
                   << " unknowns=" << scale.unknowns
                   << " nnz=" << scale.nonzeros
                   << " dense_bytes=" << scale.dense_bytes
                   << " csr_bytes=" << scale.csr_bytes
                   << " compile_us=" << scale.compile_us
                   << " assemble_us=" << scale.assemble_us
                   << " setup_us=" << scale.setup_us
                   << " solve_us=" << scale.solve_us
                   << " iterations=" << scale.iterations
                   << " final_residual=" << scale.final_residual << '\n';
        }
        report << "RUNTIME_REPETITIONS " << repetitions << '\n'
               << "RUNTIME_MEDIAN_US " << median_us << '\n'
               << "RUNTIME_P99_US " << p99_us << '\n'
               << "RUNTIME_EXPERT "
               << runtime_expert << '\n'
               << "RUNTIME_GATE_DECISION direct_accept\n"
               << "RUNTIME_GATE_SCALED_RESIDUAL "
               << last_outcome.blocks.front().gate.residual_inf << '\n'
               << "NONSYMMETRIC_UNKNOWNS "
               << nonsymmetric_width * nonsymmetric_width << '\n'
               << "NONSYMMETRIC_EXPERT "
               << accepted_gmres->expert_version
               << '\n'
               << "NONSYMMETRIC_ITERATIONS "
               << nonsymmetric_outcome.blocks.front().krylov_iterations << '\n'
               << "NONSYMMETRIC_GATE_DECISION direct_accept\n"
               << "TERMINAL_FALLBACK_RETAINED 1\n";
        std::cout << "large sparse scale evidence passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "large sparse evidence failed: " << error.what() << '\n';
        return 1;
    }
}
