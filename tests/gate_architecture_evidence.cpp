#include "smave/compiler.hpp"
#include "smave/runtime.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

double median(std::vector<double> values) {
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    const std::size_t middle = values.size() / 2;
    return values.size() % 2 == 0
        ? 0.5 * (values[middle - 1] + values[middle]) : values[middle];
}

struct Measurement {
    std::string workload;
    std::size_t requests{};
    std::size_t repetitions{};
    double scalar_median_us{};
    double fused_median_us{};
    double paired_speedup_ci95_lower{};
    std::size_t decision_mismatches{};
    std::size_t residual_mismatches{};
    std::size_t false_accepts{};
    std::size_t false_rejects{};
};

Measurement measure(
    const std::string& workload,
    const smave::ModelIR& model,
    const std::vector<std::unordered_map<std::string, double>>& values,
    std::size_t repetitions) {
    const smave::Runtime runtime(model);
    const auto& block = model.blocks.front();
    std::vector<double> scalar_times;
    std::vector<double> fused_times;
    Measurement measurement{.workload = workload, .requests = values.size(),
                            .repetitions = repetitions};
    std::vector<smave::GateResult> scalar(values.size());
    std::vector<smave::GateResult> fused(values.size());
    for (std::size_t repetition = 0; repetition < repetitions; ++repetition) {
        const auto run_scalar = [&] {
            const auto started = std::chrono::steady_clock::now();
            for (std::size_t index = 0; index < values.size(); ++index) {
                scalar[index] = runtime.evaluate_gate_reference(block, values[index], true);
            }
            scalar_times.push_back(std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - started).count() /
                static_cast<double>(values.size()));
        };
        const auto run_fused = [&] {
            const auto started = std::chrono::steady_clock::now();
            for (std::size_t index = 0; index < values.size(); ++index) {
                fused[index] = runtime.evaluate_gate_fused(block, values[index], true);
            }
            fused_times.push_back(std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - started).count() /
                static_cast<double>(values.size()));
        };
        if (repetition % 2 == 0) {
            run_scalar();
            run_fused();
        } else {
            run_fused();
            run_scalar();
        }
        for (std::size_t index = 0; index < values.size(); ++index) {
            if (scalar[index].decision != fused[index].decision) {
                ++measurement.decision_mismatches;
                if (scalar[index].decision == smave::GateDecision::reject &&
                    fused[index].decision != smave::GateDecision::reject) {
                    ++measurement.false_accepts;
                } else if (scalar[index].decision != smave::GateDecision::reject &&
                           fused[index].decision == smave::GateDecision::reject) {
                    ++measurement.false_rejects;
                }
            }
            const double scale = std::max(
                {1.0, std::abs(scalar[index].residual_inf),
                 std::abs(fused[index].residual_inf)});
            if (std::abs(scalar[index].residual_inf - fused[index].residual_inf) >
                1.0e-15 * scale) ++measurement.residual_mismatches;
        }
    }
    measurement.scalar_median_us = median(scalar_times);
    measurement.fused_median_us = median(fused_times);
    std::vector<double> paired_speedups(repetitions);
    for (std::size_t index = 0; index < repetitions; ++index) {
        paired_speedups[index] = scalar_times[index] / fused_times[index];
    }
    std::mt19937_64 generator(UINT64_C(20260720));
    std::uniform_int_distribution<std::size_t> sample(0, repetitions - 1);
    std::vector<double> bootstrap_medians;
    bootstrap_medians.reserve(10000);
    std::vector<double> draw(repetitions);
    for (std::size_t iteration = 0; iteration < 10000; ++iteration) {
        for (double& value : draw) value = paired_speedups[sample(generator)];
        bootstrap_medians.push_back(median(draw));
    }
    std::sort(bootstrap_medians.begin(), bootstrap_medians.end());
    measurement.paired_speedup_ci95_lower = bootstrap_medians[249];
    return measurement;
}

std::vector<std::unordered_map<std::string, double>> linear_values(
    const smave::ModelIR& model,
    const std::filesystem::path& scenario_directory,
    const std::filesystem::path& trace_directory) {
    std::vector<std::unordered_map<std::string, double>> values;
    const smave::Runtime runtime(model);
    for (const auto& entry : std::filesystem::directory_iterator(scenario_directory)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".conf") continue;
        auto item = smave::read_scenario(entry.path());
        const auto outcome = runtime.solve(
            item, trace_directory / std::to_string(values.size()));
        if (!outcome.success) throw std::runtime_error("linear gate fixture solve failed");
        values.push_back(outcome.values);
    }
    std::sort(values.begin(), values.end(), [&](const auto& left, const auto& right) {
        return left.size() < right.size();
    });
    auto rejected = values.front();
    rejected.at(model.blocks.front().unknowns.front()) += 1.0e-2;
    values.push_back(std::move(rejected));
    return values;
}

std::vector<std::unordered_map<std::string, double>> nonlinear_values(
    const smave::ModelIR& model) {
    std::vector<std::unordered_map<std::string, double>> values;
    values.reserve(64);
    for (std::size_t index = 0; index < 64; ++index) {
        const double parameter = 1.5 + 0.01 * static_cast<double>(index);
        std::unordered_map<std::string, double> item{{"p", parameter}};
        for (const auto& variable : model.variables) {
            if (!item.contains(variable.name)) item.emplace(variable.name, variable.start);
        }
        item.insert_or_assign("x", parameter + 1.0);
        item.insert_or_assign("y", 2.0 * parameter + 1.0);
        values.push_back(std::move(item));
    }
    auto rejected = values.front();
    rejected.at("x") += 1.0e-2;
    values.push_back(std::move(rejected));
    return values;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 5) {
        std::cerr << "usage: gate_architecture_evidence linear.mo scenarios nonlinear.mo output\n";
        return 2;
    }
    const auto linear_model = smave::compile_model(argv[1]);
    const auto nonlinear_model = smave::compile_model(argv[3]);
    const auto linear = measure(
        "operator-linear-100", linear_model,
        linear_values(
            linear_model, argv[2], std::filesystem::path(argv[4]).parent_path() / "traces"),
        100);
    const auto nonlinear = measure(
        "cubic-coupled-nonlinear", nonlinear_model,
        nonlinear_values(nonlinear_model), 100);
    std::ofstream output(argv[4]);
    if (!output) throw std::runtime_error("cannot write gate architecture evidence");
    output << std::setprecision(17)
           << "SMAVE_GATE_ARCHITECTURE 1\n"
           << "contract=\"strict-per-request-fp64-original-expression\"\n"
           << "workloads=2\n";
    for (const auto& result : {linear, nonlinear}) {
        const std::string prefix = result.workload + ".";
        output << prefix << "requests=" << result.requests << '\n'
               << prefix << "repetitions=" << result.repetitions << '\n'
               << prefix << "scalar_median_us=" << result.scalar_median_us << '\n'
               << prefix << "fused_median_us=" << result.fused_median_us << '\n'
               << prefix << "speedup="
               << result.scalar_median_us / result.fused_median_us << '\n'
               << prefix << "paired_speedup_ci95_lower="
               << result.paired_speedup_ci95_lower << '\n'
               << prefix << "decision_mismatches=" << result.decision_mismatches << '\n'
               << prefix << "residual_mismatches=" << result.residual_mismatches << '\n'
               << prefix << "false_accepts=" << result.false_accepts << '\n'
               << prefix << "false_rejects=" << result.false_rejects << '\n';
    }
    const bool safe = linear.decision_mismatches == 0 && linear.residual_mismatches == 0 &&
        nonlinear.decision_mismatches == 0 && nonlinear.residual_mismatches == 0 &&
        linear.paired_speedup_ci95_lower > 1.0 &&
        nonlinear.paired_speedup_ci95_lower > 1.0;
    output << "strict_equivalence=" << safe << '\n'
           << "END\n";
    std::cout << "SMAVE_GATE_ARCHITECTURE 1\n"
              << "LINEAR_SPEEDUP " << linear.scalar_median_us / linear.fused_median_us << '\n'
              << "NONLINEAR_SPEEDUP "
              << nonlinear.scalar_median_us / nonlinear.fused_median_us << '\n'
              << "STRICT_EQUIVALENCE " << safe << '\n';
    return safe ? 0 : 1;
}
