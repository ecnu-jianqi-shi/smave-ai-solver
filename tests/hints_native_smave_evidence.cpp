#include "smave/ir.hpp"
#include "smave/runtime.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

double quantile(std::vector<double> values, double probability) {
    if (values.empty()) throw std::invalid_argument("quantile requires samples");
    std::sort(values.begin(), values.end());
    const double position = probability * static_cast<double>(values.size() - 1);
    const auto lower = static_cast<std::size_t>(std::floor(position));
    const auto upper = static_cast<std::size_t>(std::ceil(position));
    const double fraction = position - static_cast<double>(lower);
    return values[lower] * (1.0 - fraction) + values[upper] * fraction;
}

double infinity_norm(const std::vector<double>& values) {
    double result = 0.0;
    for (const double value : values) result = std::max(result, std::abs(value));
    return result;
}

std::vector<std::filesystem::path> scenarios(const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> result;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".conf") {
            result.push_back(entry.path());
        }
    }
    std::sort(result.begin(), result.end());
    if (result.empty()) throw std::invalid_argument("HINTS native scenario suite is empty");
    return result;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 4) {
        throw std::invalid_argument(
            "usage: hints-native-smave-evidence MODEL_IR SCENARIOS OUTPUT_DIRECTORY");
    }
    const auto model = smave::ModelIR::read(argv[1]);
    std::unordered_map<std::string, smave::Expression> residuals;
    for (const auto& equation : model.equations) {
        residuals.emplace(equation.id, smave::Expression(equation.residual));
    }
    const auto scenario_paths = scenarios(argv[2]);
    if (scenario_paths.size() != 750) {
        throw std::invalid_argument("expected all 750 official HINTS test scenarios");
    }
    const std::filesystem::path output_directory(argv[3]);
    std::filesystem::create_directories(output_directory);
    const auto setup_started = std::chrono::steady_clock::now();
    const smave::Runtime runtime(model);
    const double setup_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - setup_started).count();
    constexpr int warmups = 3;
    constexpr int repetitions = 5;
    for (int warmup = 0; warmup < warmups; ++warmup) {
        const auto inputs = smave::read_scenario(
            scenario_paths[static_cast<std::size_t>(warmup) % scenario_paths.size()]);
        const auto outcome = runtime.solve(inputs, output_directory / "warmup-traces");
        if (!outcome.success) throw std::runtime_error("SMAVE HINTS warmup failed");
    }

    std::vector<std::vector<double>> samples(scenario_paths.size());
    std::vector<double> all_samples;
    std::size_t failures = 0;
    std::size_t gate_mismatches = 0;
    std::size_t non_direct_paths = 0;
    double maximum_gate_residual = 0.0;
    double maximum_runtime_scaled_residual = 0.0;
    std::map<std::string, std::size_t> accepted_experts;
    std::ofstream raw(output_directory / "raw-samples.txt");
    if (!raw) throw std::runtime_error("cannot write SMAVE HINTS raw samples");
    raw << "case repetition elapsed_us gate_relative_inf path expert\n";
    for (int repetition = 0; repetition < repetitions; ++repetition) {
        for (std::size_t ordinal = 0; ordinal < scenario_paths.size(); ++ordinal) {
            const std::size_t case_index = repetition % 2 == 0
                ? ordinal
                : scenario_paths.size() - ordinal - 1;
            const auto inputs = smave::read_scenario(scenario_paths[case_index]);
            const auto outcome = runtime.solve(
                inputs,
                output_directory / "traces" / std::to_string(repetition) /
                    std::to_string(case_index));
            if (!outcome.success || outcome.blocks.size() != 1) {
                ++failures;
                continue;
            }
            const auto& block = outcome.blocks.front();
            if (block.gate.decision != smave::GateDecision::direct_accept) {
                ++gate_mismatches;
            }
            if (block.path != smave::SolvePath::direct_accept) {
                ++non_direct_paths;
            }
            const std::string expert = block.attempted_experts.empty()
                ? "none"
                : block.attempted_experts.back();
            ++accepted_experts[expert];
            const auto system = smave::assemble_linear_system(
                model, model.blocks.front(), residuals, inputs);
            std::vector<double> solution;
            solution.reserve(system.unknowns.size());
            for (const auto& unknown : system.unknowns) {
                solution.push_back(block.solution.at(unknown));
            }
            const auto product = system.multiply(solution);
            std::vector<double> residual(product.size());
            for (std::size_t index = 0; index < residual.size(); ++index) {
                residual[index] = product[index] - system.right_hand_side[index];
            }
            const double gate_residual = infinity_norm(residual) /
                std::max(1.0, infinity_norm(system.right_hand_side));
            maximum_gate_residual = std::max(maximum_gate_residual, gate_residual);
            maximum_runtime_scaled_residual = std::max(
                maximum_runtime_scaled_residual, block.gate.residual_inf);
            samples[case_index].push_back(outcome.timing.total_us);
            all_samples.push_back(outcome.timing.total_us);
            raw << case_index << ' ' << repetition << ' '
                << std::setprecision(17) << outcome.timing.total_us << ' '
                << gate_residual << ' ' << smave::to_string(block.path) << ' '
                << expert << '\n';
        }
    }

    std::vector<double> case_medians;
    for (const auto& case_samples : samples) {
        if (case_samples.size() != repetitions) {
            throw std::runtime_error("incomplete SMAVE HINTS case timing samples");
        }
        case_medians.push_back(quantile(case_samples, 0.5));
    }
    std::ofstream evidence(output_directory / "evidence.txt");
    if (!evidence) throw std::runtime_error("cannot write SMAVE HINTS evidence");
    evidence << std::setprecision(17)
             << "SMAVE_HINTS_NATIVE_SMAVE 1\n"
             << "contract=default-production-router-original-equation-gate\n"
             << "test_cases=" << scenario_paths.size() << '\n'
             << "repetitions=" << repetitions << '\n'
             << "warmups=" << warmups << '\n'
             << "counterbalanced_case_order=1\n"
             << "process_launch_timed=0\n"
             << "model_parse_and_runtime_setup_timed_separately=1\n"
             << "setup_us=" << setup_us << '\n'
             << "online_case_median_us=" << quantile(case_medians, 0.5) << '\n'
             << "online_case_p90_us=" << quantile(case_medians, 0.9) << '\n'
             << "online_sample_median_us=" << quantile(all_samples, 0.5) << '\n'
             << "maximum_gate_relative_inf=" << maximum_gate_residual << '\n'
             << "maximum_runtime_scaled_residual="
             << maximum_runtime_scaled_residual << '\n'
             << "failures=" << failures << '\n'
             << "gate_mismatches=" << gate_mismatches << '\n'
             << "non_direct_paths=" << non_direct_paths << '\n';
    for (const auto& [expert, count] : accepted_experts) {
        evidence << "accepted_expert." << expert << '=' << count << '\n';
    }
    evidence << "all_requests_pass=" << (failures == 0) << '\n'
             << "all_original_equation_gates_pass=" << (gate_mismatches == 0) << '\n'
             << "END\n";
    std::cout << "SMAVE_HINTS_NATIVE_SMAVE 1 cases=" << scenario_paths.size()
              << " median_us=" << quantile(case_medians, 0.5)
              << " failures=" << failures << '\n';
    return failures == 0 && gate_mismatches == 0 ? 0 : 1;
}
