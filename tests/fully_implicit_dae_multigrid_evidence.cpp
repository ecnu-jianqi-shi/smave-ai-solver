#include "smave/dae.hpp"
#include "smave/dae_learning.hpp"
#include "smave/routing.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

double solve_candidate(double previous, double coefficient, double step) {
    double candidate = previous / (1.0 + step * coefficient);
    for (int iteration = 0; iteration < 12; ++iteration) {
        const double residual =
            (candidate - previous) / step + coefficient * candidate +
            0.01 * candidate * candidate * candidate;
        const double derivative =
            1.0 / step + coefficient + 0.03 * candidate * candidate;
        candidate -= residual / derivative;
    }
    return candidate;
}

std::vector<double> write_scenario(
    const std::filesystem::path& path,
    double step,
    double time,
    const std::vector<double>& previous) {
    std::ofstream output(path);
    output << std::setprecision(17);
    output << "step=" << step << '\n' << "time=" << time << '\n';
    std::vector<double> candidates;
    for (std::size_t index = 0; index < previous.size(); ++index) {
        const double coefficient = static_cast<double>(index + 1);
        const double candidate = solve_candidate(previous[index], coefficient, step);
        candidates.push_back(candidate);
        output << "previous.x" << index + 1 << '=' << previous[index] << '\n'
               << "state.x" << index + 1 << '=' << candidate << '\n';
    }
    return candidates;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::invalid_argument("expected evidence output directory");
        const std::filesystem::path directory = argv[1];
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory / "scenarios");
        const auto source = directory / "FullyImplicitLearnedMultigrid.mo";
        std::ofstream model(source);
        model << "model FullyImplicitLearnedMultigrid\n";
        for (std::size_t index = 0; index < 8; ++index) {
            model << "  Real x" << index + 1 << "(start=" << index + 1
                  << ", nominal=1);\n";
        }
        model << "equation\n";
        for (std::size_t index = 0; index < 8; ++index) {
            model << "  der(x" << index + 1 << ") + " << index + 1
                  << "*x" << index + 1 << " + 0.01*x" << index + 1
                  << "^3 = 0;\n";
        }
        model << "end FullyImplicitLearnedMultigrid;\n";
        model.close();

        const auto compiled = smave::compile_fully_implicit_dae(source);
        const auto first_candidates = write_scenario(
            directory / "scenarios" / "step-01.conf", 0.1, 0.1,
            {1, 2, 3, 4, 5, 6, 7, 8});
        write_scenario(
            directory / "scenarios" / "step-02.conf", 0.1, 0.2,
            first_candidates);
        const auto artifact = smave::train_dae_multigrid(
            compiled, directory / "scenarios");
        require(artifact.schema_version == "smave.dae-multigrid.v3" &&
                    artifact.residual_family == "fully-implicit-first-order-step" &&
                    artifact.unknown_count == 8 && artifact.training_samples == 4 &&
                    artifact.hierarchy.maximum_probe_contraction < 1.0,
                "fully implicit learned multigrid artifact contract is invalid");
        const auto artifact_path = directory / "implicit-dae-mg.artifact";
        artifact.write(artifact_path);
        const auto restored = smave::DaeMultigridArtifact::read(artifact_path);
        require(restored.artifact_hash == artifact.artifact_hash &&
                    restored.residual_family == artifact.residual_family,
                "fully implicit learned multigrid v3 round-trip lost binding");

        const auto plan = smave::route_fully_implicit_dae(compiled, {}, &restored);
        require(!plan.steps.empty() &&
                    plan.steps.front().expert_version ==
                        "fully-implicit-learned-multigrid-pcg-cpu-v1" &&
                    std::find(
                        plan.steps.front().backend_chain.begin(),
                        plan.steps.front().backend_chain.end(),
                        "verified-learned-multigrid-preconditioner") !=
                        plan.steps.front().backend_chain.end(),
                "Equation Expert did not route the verified learned artifact");

        const auto accelerated = smave::simulate_fully_implicit_dae(
            compiled, 0.2, 0.1, {}, &restored);
        require(accelerated.success && accelerated.plan_id == plan.plan_id &&
                    accelerated.solver_backend ==
                        "fully-implicit-learned-multigrid-pcg-cpu-v1" &&
                    accelerated.learned_preconditioned_steps == 2 &&
                    accelerated.learned_preconditioned_newton_iterations > 0 &&
                    accelerated.learned_krylov_iterations > 0 &&
                    accelerated.learned_rejections == 0 &&
                    accelerated.dense_step_fallbacks == 0 &&
                    accelerated.maximum_residual_inf < 1.0e-6,
                "fully implicit steps did not execute learned multigrid PCG");

        const auto ood = smave::simulate_fully_implicit_dae(
            compiled, 0.05, 0.05, {}, &restored);
        require(ood.success && ood.learned_preconditioned_steps == 0 &&
                    ood.learned_rejections == 1 && ood.dense_step_fallbacks == 0 &&
                    ood.maximum_residual_inf < 1.0e-6,
                "step OOD did not reject learned acceleration and retry classic CSR Newton");

        std::filesystem::create_directories(directory / "matrix-ood-scenarios");
        write_scenario(
            directory / "matrix-ood-scenarios" / "small-01.conf",
            0.1, 0.1, std::vector<double>(8, 0.1));
        write_scenario(
            directory / "matrix-ood-scenarios" / "small-02.conf",
            0.1, 0.2, std::vector<double>(8, 0.1));
        const auto narrow_artifact = smave::train_dae_multigrid(
            compiled, directory / "matrix-ood-scenarios");
        const auto matrix_ood = smave::simulate_fully_implicit_dae(
            compiled, 0.1, 0.1, {}, &narrow_artifact);
        require(matrix_ood.success &&
                    matrix_ood.learned_preconditioned_steps == 0 &&
                    matrix_ood.learned_rejections == 1 &&
                    matrix_ood.dense_step_fallbacks == 0,
                "Jacobian drift OOD did not reject learned acceleration");

        auto mismatched = restored;
        mismatched.model_source_hash = "different-source";
        mismatched.hierarchy.model_source_hash = "different-source";
        mismatched.hierarchy.expert_version.clear();
        mismatched.hierarchy.seal();
        mismatched.seal();
        const auto source_fallback = smave::simulate_fully_implicit_dae(
            compiled, 0.1, 0.1, {}, &mismatched);
        require(source_fallback.success &&
                    source_fallback.learned_preconditioned_steps == 0 &&
                    source_fallback.learned_rejections == 1 &&
                    source_fallback.dense_step_fallbacks == 0,
                "source mismatch did not fail closed to classic CSR Newton");

        auto corrupted = restored;
        corrupted.maximum_step = 0.3;
        const auto corrupted_fallback = smave::simulate_fully_implicit_dae(
            compiled, 0.1, 0.1, {}, &corrupted);
        require(corrupted_fallback.success &&
                    corrupted_fallback.learned_preconditioned_steps == 0 &&
                    corrupted_fallback.learned_rejections == 1 &&
                    corrupted_fallback.dense_step_fallbacks == 0,
                "corrupt artifact did not fail closed to classic CSR Newton");

        smave::write_fully_implicit_dae_report(
            compiled, accelerated, directory / "accelerated-report.txt");
        smave::write_fully_implicit_dae_report(
            compiled, ood, directory / "ood-report.txt");
        std::cout << "fully implicit DAE learned multigrid evidence passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
