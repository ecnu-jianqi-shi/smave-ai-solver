#include "smave/compiler.hpp"
#include "smave/incremental_gate.hpp"
#include "smave/runtime.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {

using Values = std::unordered_map<std::string, double>;

struct SlimGate {
    smave::GateDecision decision{smave::GateDecision::reject};
    double residual_inf{};
};

double median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    const auto middle = values.size() / 2;
    return values.size() % 2 ? values[middle] : 0.5 * (values[middle - 1] + values[middle]);
}

double paired_bootstrap_lower(
    const std::vector<double>& strict,
    const std::vector<double>& adaptive,
    std::uint64_t seed) {
    if (strict.size() != adaptive.size() || strict.empty()) {
        throw std::invalid_argument("paired bootstrap requires equal non-empty samples");
    }
    std::vector<double> paired(strict.size());
    for (std::size_t index = 0; index < strict.size(); ++index) {
        paired[index] = strict[index] / adaptive[index];
    }
    std::mt19937_64 generator(seed);
    std::uniform_int_distribution<std::size_t> sample(0, paired.size() - 1);
    std::vector<double> bootstrap;
    bootstrap.reserve(10000);
    std::vector<double> draw(paired.size());
    for (std::size_t iteration = 0; iteration < 10000; ++iteration) {
        for (double& value : draw) value = paired[sample(generator)];
        bootstrap.push_back(median(draw));
    }
    std::sort(bootstrap.begin(), bootstrap.end());
    return bootstrap[249];
}

struct Measurement {
    std::string workload;
    std::size_t candidates{};
    std::size_t requests_per_repetition{};
    std::size_t repetitions{};
    double strict_median_us{};
    double adaptive_median_us{};
    double paired_speedup_ci95_lower{};
    std::vector<double> strict_samples;
    std::vector<double> adaptive_samples;
    smave::IncrementalGateCounters counters;
    std::size_t false_accepts{};
    std::size_t false_rejects{};
    std::size_t decision_mismatches{};
};

struct FullSolveMeasurement {
    std::string workload;
    std::size_t repetitions{};
    std::size_t solves_per_repetition{};
    double strict_total_median_us{};
    double adaptive_total_median_us{};
    double strict_gate_median_us{};
    double adaptive_gate_median_us{};
    double total_speedup_ci95_lower{};
    double gate_speedup_ci95_lower{};
    std::vector<double> strict_total_samples;
    std::vector<double> adaptive_total_samples;
    std::vector<double> strict_gate_samples;
    std::vector<double> adaptive_gate_samples;
    std::size_t result_mismatches{};
    smave::SolveGatePolicyCounters counters;
};

bool equivalent_outcome(
    const smave::SolveOutcome& left,
    const smave::SolveOutcome& right) {
    if (left.success != right.success || left.blocks.size() != right.blocks.size() ||
        left.values.size() != right.values.size()) return false;
    for (const auto& [name, value] : left.values) {
        const auto other = right.values.find(name);
        if (other == right.values.end() || value != other->second) return false;
    }
    for (std::size_t index = 0; index < left.blocks.size(); ++index) {
        const auto& lhs = left.blocks[index];
        const auto& rhs = right.blocks[index];
        if (lhs.path != rhs.path || lhs.plan_id != rhs.plan_id ||
            lhs.gate.decision != rhs.gate.decision ||
            lhs.gate.residual_inf != rhs.gate.residual_inf ||
            lhs.solution != rhs.solution) return false;
    }
    return true;
}

FullSolveMeasurement measure_full_solve(
    std::string workload,
    const smave::ModelIR& model,
    const Values& context,
    const std::filesystem::path& trace_root,
    std::size_t repetitions,
    std::size_t burst,
    std::size_t full_period) {
    const smave::Runtime strict_runtime(model);
    auto policy = std::make_shared<smave::ExperimentalExactCandidateGatePolicy>(full_period);
    const smave::Runtime adaptive_runtime(model, {}, {}, {}, policy);
    std::vector<double> strict_total;
    std::vector<double> adaptive_total;
    std::vector<double> strict_gate;
    std::vector<double> adaptive_gate;
    FullSolveMeasurement measurement{
        .workload = std::move(workload),
        .repetitions = repetitions,
        .solves_per_repetition = burst,
    };
    for (std::size_t repetition = 0; repetition < repetitions; ++repetition) {
        double strict_total_sample{};
        double adaptive_total_sample{};
        double strict_gate_sample{};
        double adaptive_gate_sample{};
        for (std::size_t request = 0; request < burst; ++request) {
            smave::SolveOutcome strict;
            smave::SolveOutcome adaptive;
            if ((repetition + request) % 2 == 0) {
                strict = strict_runtime.solve(
                    context, trace_root / "strict" / std::to_string(repetition));
                adaptive = adaptive_runtime.solve(
                    context, trace_root / "adaptive" / std::to_string(repetition));
            } else {
                adaptive = adaptive_runtime.solve(
                    context, trace_root / "adaptive" / std::to_string(repetition));
                strict = strict_runtime.solve(
                    context, trace_root / "strict" / std::to_string(repetition));
            }
            strict_total_sample += strict.timing.total_us;
            adaptive_total_sample += adaptive.timing.total_us;
            strict_gate_sample += strict.timing.gate_us;
            adaptive_gate_sample += adaptive.timing.gate_us;
            if (!equivalent_outcome(strict, adaptive)) ++measurement.result_mismatches;
        }
        strict_total.push_back(strict_total_sample);
        adaptive_total.push_back(adaptive_total_sample);
        strict_gate.push_back(strict_gate_sample);
        adaptive_gate.push_back(adaptive_gate_sample);
    }
    measurement.strict_total_median_us = median(strict_total);
    measurement.adaptive_total_median_us = median(adaptive_total);
    measurement.strict_gate_median_us = median(strict_gate);
    measurement.adaptive_gate_median_us = median(adaptive_gate);
    measurement.total_speedup_ci95_lower = paired_bootstrap_lower(
        strict_total, adaptive_total, UINT64_C(20260720));
    measurement.gate_speedup_ci95_lower = paired_bootstrap_lower(
        strict_gate, adaptive_gate, UINT64_C(20260720));
    measurement.strict_total_samples = strict_total;
    measurement.adaptive_total_samples = adaptive_total;
    measurement.strict_gate_samples = strict_gate;
    measurement.adaptive_gate_samples = adaptive_gate;
    measurement.counters = policy->counters();
    return measurement;
}

Measurement measure(
    std::string workload,
    const smave::ModelIR& model,
    const std::vector<Values>& values,
    std::size_t repetitions,
    std::size_t burst,
    std::size_t full_period) {
    const auto& block = model.blocks.front();
    const smave::Runtime runtime(model);
    std::vector<SlimGate> authority;
    authority.reserve(values.size());
    for (const auto& item : values) {
        const auto gate = runtime.evaluate_gate(block, item, true);
        authority.push_back({gate.decision, gate.residual_inf});
    }

    std::vector<double> strict_times;
    std::vector<double> adaptive_times;
    strict_times.reserve(repetitions);
    adaptive_times.reserve(repetitions);
    Measurement measurement{
        .workload = std::move(workload),
        .candidates = values.size(),
        .requests_per_repetition = values.size() * burst,
        .repetitions = repetitions,
    };
    std::uint64_t checksum{};
    for (std::size_t repetition = 0; repetition < repetitions; ++repetition) {
        auto run_strict = [&] {
            const auto started = std::chrono::steady_clock::now();
            for (std::size_t candidate = 0; candidate < values.size(); ++candidate) {
                for (std::size_t request = 0; request < burst; ++request) {
                    const auto gate = runtime.evaluate_gate(block, values[candidate], true);
                    checksum += static_cast<std::uint64_t>(gate.decision) +
                        static_cast<std::uint64_t>(gate.residual_inf > 1.0);
                }
            }
            return std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - started).count();
        };
        auto run_adaptive = [&] {
            smave::ExperimentalIncrementalGate policy(
                model, block.id, {}, full_period);
            std::vector<std::shared_ptr<const smave::ImmutableGateInput>> inputs;
            inputs.reserve(values.size());
            for (const auto& value : values) inputs.push_back(policy.issue_input(value));
            const auto started = std::chrono::steady_clock::now();
            for (std::size_t candidate = 0; candidate < values.size(); ++candidate) {
                for (std::size_t request = 0; request < burst; ++request) {
                    const auto gate = policy.evaluate(inputs[candidate], true);
                    checksum += static_cast<std::uint64_t>(gate.decision) +
                        static_cast<std::uint64_t>(gate.residual_inf > 1.0);
                    if (gate.decision != authority[candidate].decision) {
                        ++measurement.decision_mismatches;
                        if (gate.decision == smave::GateDecision::direct_accept) {
                            ++measurement.false_accepts;
                        } else if (authority[candidate].decision ==
                                   smave::GateDecision::direct_accept) {
                            ++measurement.false_rejects;
                        }
                    }
                }
            }
            const auto elapsed = std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - started).count();
            const auto counters = policy.counters();
            measurement.counters.requests += counters.requests;
            measurement.counters.high_risk_full_verifications +=
                counters.high_risk_full_verifications;
            measurement.counters.periodic_full_verifications +=
                counters.periodic_full_verifications;
            measurement.counters.certificate_reuses += counters.certificate_reuses;
            measurement.counters.low_cost_rejects += counters.low_cost_rejects;
            measurement.counters.periodic_mismatches += counters.periodic_mismatches;
            return elapsed;
        };
        if (repetition % 2 == 0) {
            strict_times.push_back(run_strict());
            adaptive_times.push_back(run_adaptive());
        } else {
            adaptive_times.push_back(run_adaptive());
            strict_times.push_back(run_strict());
        }
    }
    if (checksum == std::numeric_limits<std::uint64_t>::max()) {
        throw std::runtime_error("unreachable gate checksum");
    }
    measurement.strict_median_us = median(strict_times);
    measurement.adaptive_median_us = median(adaptive_times);
    measurement.paired_speedup_ci95_lower = paired_bootstrap_lower(
        strict_times, adaptive_times, UINT64_C(20260720));
    measurement.strict_samples = strict_times;
    measurement.adaptive_samples = adaptive_times;
    return measurement;
}

void write_raw_samples(
    const std::filesystem::path& path,
    const Measurement& linear,
    const Measurement& nonlinear,
    const FullSolveMeasurement& full_linear,
    const FullSolveMeasurement& full_nonlinear,
    const FullSolveMeasurement& full_scaled_nonlinear) {
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write risk-adaptive raw samples");
    output << std::setprecision(17)
           << "SMAVE_RISK_ADAPTIVE_GATE_SAMPLES 1\n"
           << "contract=counterbalanced-strict-adaptive-complete-cost-raw-samples\n"
           << "repetitions=100\n";
    for (const auto* result : {&linear, &nonlinear}) {
        for (std::size_t repetition = 0; repetition < result->repetitions; ++repetition) {
            output << result->workload << ".repetition_" << repetition + 1
                   << ".strict_us=" << result->strict_samples[repetition] << '\n'
                   << result->workload << ".repetition_" << repetition + 1
                   << ".adaptive_us=" << result->adaptive_samples[repetition] << '\n';
        }
    }
    for (const auto* result : {&full_linear, &full_nonlinear, &full_scaled_nonlinear}) {
        for (std::size_t repetition = 0; repetition < result->repetitions; ++repetition) {
            const auto prefix = result->workload + ".repetition_" +
                std::to_string(repetition + 1);
            output << prefix << ".strict_total_us="
                   << result->strict_total_samples[repetition] << '\n'
                   << prefix << ".adaptive_total_us="
                   << result->adaptive_total_samples[repetition] << '\n'
                   << prefix << ".strict_gate_us="
                   << result->strict_gate_samples[repetition] << '\n'
                   << prefix << ".adaptive_gate_us="
                   << result->adaptive_gate_samples[repetition] << '\n';
        }
    }
    output << "END\n";
}

bool revocation_probe(const smave::ModelIR& model, const Values& values) {
    smave::ExperimentalIncrementalGate policy(model, model.blocks.front().id);
    const auto input = policy.issue_input(values);
    const auto first = policy.evaluate(input, true);
    const auto reused = policy.evaluate(input, true);
    const auto before = policy.counters();
    policy.revoke();
    const auto after_revoke = policy.evaluate(input, true);
    const auto after = policy.counters();
    return first.decision == reused.decision && first.decision == after_revoke.decision &&
        before.certificate_reuses == 1 && after.revocations == 1 &&
        after.high_risk_full_verifications == before.high_risk_full_verifications + 1;
}

bool session_and_concurrency_probe(const smave::ModelIR& model, const Values& values) {
    smave::ExperimentalIncrementalGate issuer(model, model.blocks.front().id);
    smave::ExperimentalIncrementalGate verifier(model, model.blocks.front().id);
    const auto foreign_input = issuer.issue_input(values);
    const auto foreign_result = verifier.evaluate(foreign_input, true);
    const auto foreign_counters = verifier.counters();
    if (foreign_result.decision != smave::GateDecision::direct_accept ||
        foreign_counters.high_risk_full_verifications != 1 ||
        foreign_counters.certificate_reuses != 0) {
        return false;
    }

    constexpr std::size_t workers = 8;
    constexpr std::size_t requests_per_worker = 64;
    std::vector<std::thread> threads;
    std::vector<std::uint8_t> passed(workers, 1);
    threads.reserve(workers);
    for (std::size_t worker = 0; worker < workers; ++worker) {
        threads.emplace_back([&, worker] {
            for (std::size_t request = 0; request < requests_per_worker; ++request) {
                if (verifier.evaluate(foreign_input, true).decision !=
                    smave::GateDecision::direct_accept) {
                    passed[worker] = 0;
                }
            }
        });
    }
    for (auto& thread : threads) thread.join();
    const auto counters = verifier.counters();
    return std::all_of(passed.begin(), passed.end(), [](std::uint8_t value) {
               return value != 0;
           }) &&
        counters.requests == 1 + workers * requests_per_worker &&
        counters.periodic_mismatches == 0;
}

bool negative_contract_probe(const smave::ModelIR& model, const Values& values) {
    bool bad_period_rejected{};
    try {
        smave::ExperimentalIncrementalGate bad_period(
            model, model.blocks.front().id, {}, 1);
    } catch (const std::invalid_argument&) {
        bad_period_rejected = true;
    }
    bool bad_block_rejected{};
    try {
        smave::ExperimentalIncrementalGate bad_block(model, "missing-block");
    } catch (const std::invalid_argument&) {
        bad_block_rejected = true;
    }

    smave::ExperimentalIncrementalGate policy(model, model.blocks.front().id);
    const auto null_result = policy.evaluate(nullptr, true);
    const auto input = policy.issue_input(values);
    const auto direct = policy.evaluate(input, true);
    const auto corrected = policy.evaluate(input, false);
    const auto counters = policy.counters();
    return bad_period_rejected && bad_block_rejected &&
        null_result.decision == smave::GateDecision::reject &&
        direct.decision == smave::GateDecision::direct_accept &&
        corrected.decision == smave::GateDecision::need_correction &&
        counters.low_cost_rejects == 1 &&
        counters.high_risk_full_verifications == 2;
}

bool full_solve_policy_contract_probe(
    const smave::ModelIR& model,
    const Values& context,
    const std::filesystem::path& trace_root) {
    bool bad_period_rejected{};
    try {
        smave::ExperimentalExactCandidateGatePolicy invalid(1);
    } catch (const std::invalid_argument&) {
        bad_period_rejected = true;
    }
    auto policy = std::make_shared<smave::ExperimentalExactCandidateGatePolicy>(4);
    const smave::Runtime first_runtime(model, {}, {}, {}, policy);
    const auto first = first_runtime.solve(context, trace_root / "first");
    const auto reused = first_runtime.solve(context, trace_root / "reused");
    const auto after_reuse = policy->counters();
    Values changed = context;
    const auto parameter = std::find_if(
        model.variables.begin(), model.variables.end(),
        [](const smave::VariableIR& variable) { return variable.kind == "parameter"; });
    if (parameter != model.variables.end()) {
        changed.insert_or_assign(parameter->name, parameter->start + 0.125);
    } else {
        return false;
    }
    const auto changed_result = first_runtime.solve(changed, trace_root / "changed");
    const auto after_change = policy->counters();
    policy->revoke();
    const auto after_revoke = first_runtime.solve(context, trace_root / "revoked");
    const auto after_explicit_revoke = policy->counters();
    const smave::Runtime second_runtime(
        model, smave::Tolerance{.absolute = 1.0e-9, .relative = 1.0e-7},
        {}, {}, policy);
    const auto cross_runtime = second_runtime.solve(context, trace_root / "cross-runtime");
    const auto final_counters = policy->counters();
    return bad_period_rejected && first.success && reused.success && changed_result.success &&
        after_revoke.success && cross_runtime.success && equivalent_outcome(first, reused) &&
        after_reuse.strict_verifications == 1 && after_reuse.certificate_reuses == 1 &&
        after_change.strict_verifications == 2 &&
        after_explicit_revoke.strict_verifications == 3 &&
        final_counters.strict_verifications == 4 && final_counters.revocations >= 1;
}

bool full_solve_concurrency_revocation_probe(
    const smave::ModelIR& model,
    const Values& context,
    const std::filesystem::path& trace_root) {
    constexpr std::size_t workers = 8;
    constexpr std::size_t solves_per_worker = 128;
    constexpr std::size_t revocations = 64;
    const smave::Runtime authority(model);
    const auto expected = authority.solve(context, trace_root / "authority");
    if (!expected.success) return false;
    auto policy = std::make_shared<smave::ExperimentalExactCandidateGatePolicy>(16);
    const smave::Runtime adaptive(model, {}, {}, {}, policy);
    std::vector<std::thread> threads;
    std::vector<std::uint8_t> passed(workers, 1);
    threads.reserve(workers + 1);
    for (std::size_t worker = 0; worker < workers; ++worker) {
        threads.emplace_back([&, worker] {
            for (std::size_t request = 0; request < solves_per_worker; ++request) {
                const auto observed = adaptive.solve(
                    context, trace_root / "workers" / std::to_string(worker));
                if (!equivalent_outcome(expected, observed)) passed[worker] = 0;
            }
        });
    }
    threads.emplace_back([&] {
        for (std::size_t index = 0; index < revocations; ++index) {
            policy->revoke();
            std::this_thread::yield();
        }
    });
    for (auto& thread : threads) thread.join();
    const auto counters = policy->counters();
    const bool all_passed = std::all_of(
        passed.begin(), passed.end(), [](std::uint8_t value) { return value != 0; });
    return all_passed && counters.requests == workers * solves_per_worker &&
        counters.strict_verifications > 0 && counters.certificate_reuses > 0 &&
        counters.periodic_mismatches == 0 && counters.revocations > 0;
}

bool reconstructed_process_local_probe(
    const smave::ModelIR& model,
    const Values& context,
    const std::filesystem::path& trace_root) {
    auto first_policy = std::make_shared<smave::ExperimentalExactCandidateGatePolicy>();
    {
        const smave::Runtime first(model, {}, {}, {}, first_policy);
        if (!first.solve(context, trace_root / "first").success ||
            !first.solve(context, trace_root / "first-reuse").success) return false;
    }
    const auto first_counters = first_policy->counters();
    auto reconstructed_policy =
        std::make_shared<smave::ExperimentalExactCandidateGatePolicy>();
    const smave::Runtime reconstructed(model, {}, {}, {}, reconstructed_policy);
    const auto result = reconstructed.solve(context, trace_root / "reconstructed");
    const auto reconstructed_counters = reconstructed_policy->counters();
    return result.success && first_counters.strict_verifications == 1 &&
        first_counters.certificate_reuses == 1 &&
        reconstructed_counters.strict_verifications == 1 &&
        reconstructed_counters.certificate_reuses == 0;
}

bool bundle_version_change_probe(
    const smave::ModelIR& model,
    const Values& context,
    const std::filesystem::path& trace_root) {
    auto policy = std::make_shared<smave::ExperimentalExactCandidateGatePolicy>();
    auto first_bundle = smave::make_default_bundle(model);
    const smave::Runtime first(
        model, smave::make_default_registry(model), std::move(first_bundle),
        {}, {}, {}, policy);
    if (!first.solve(context, trace_root / "first").success ||
        !first.solve(context, trace_root / "first-reuse").success) return false;
    const auto before_change = policy->counters();
    auto second_bundle = smave::make_default_bundle(model);
    second_bundle.bundle_id += "-rotated";
    second_bundle.seal();
    const smave::Runtime second(
        model, smave::make_default_registry(model), std::move(second_bundle),
        {}, {}, {}, policy);
    const auto changed = second.solve(context, trace_root / "rotated");
    const auto after_change = policy->counters();
    return changed.success && before_change.strict_verifications == 1 &&
        before_change.certificate_reuses == 1 &&
        after_change.strict_verifications == 2 &&
        after_change.certificate_reuses == 1;
}

std::vector<Values> linear_values(
    const smave::ModelIR& model,
    const std::filesystem::path& scenario_directory,
    const std::filesystem::path& trace_directory) {
    std::vector<Values> values;
    const smave::Runtime runtime(model);
    for (const auto& entry : std::filesystem::directory_iterator(scenario_directory)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".conf") continue;
        const auto outcome = runtime.solve(
            smave::read_scenario(entry.path()),
            trace_directory / std::to_string(values.size()));
        if (!outcome.success) throw std::runtime_error("linear fixture solve failed");
        values.push_back(outcome.values);
    }
    auto residual_reject = values.front();
    residual_reject.at(model.blocks.front().unknowns.front()) += 1.0e-2;
    values.push_back(std::move(residual_reject));
    auto necessary_reject = values.front();
    necessary_reject.at(model.blocks.front().unknowns.front()) =
        std::numeric_limits<double>::quiet_NaN();
    values.push_back(std::move(necessary_reject));
    return values;
}

std::vector<Values> nonlinear_values(const smave::ModelIR& model) {
    std::vector<Values> values;
    for (std::size_t index = 0; index < 64; ++index) {
        const double parameter = 1.5 + 0.01 * static_cast<double>(index);
        Values item{{"p", parameter}};
        for (const auto& variable : model.variables) {
            if (!item.contains(variable.name)) item.emplace(variable.name, variable.start);
        }
        item.insert_or_assign("x", parameter + 1.0);
        item.insert_or_assign("y", 2.0 * parameter + 1.0);
        values.push_back(std::move(item));
    }
    auto residual_reject = values.front();
    residual_reject.at("x") += 1.0e-2;
    values.push_back(std::move(residual_reject));
    auto necessary_reject = values.front();
    necessary_reject.at("x") = std::numeric_limits<double>::infinity();
    values.push_back(std::move(necessary_reject));
    return values;
}

smave::ModelIR scaled_nonlinear_model(const std::filesystem::path& source) {
    constexpr std::size_t dimension = 64;
    std::ofstream model(source);
    if (!model) throw std::runtime_error("cannot write scaled nonlinear gate fixture");
    model << "model ScaledNonlinearGate\n";
    for (std::size_t index = 1; index <= dimension; ++index) {
        model << "Real x" << index << "(start=1.0, nominal=1.0);\n";
    }
    model << "equation\n";
    for (std::size_t index = 1; index <= dimension; ++index) {
        const std::size_t next = index == dimension ? 1 : index + 1;
        model << "(x" << index << "-1.0)+0.001*(x" << index
              << "-1.0)*(x" << index << "-1.0)*(x" << index
              << "-1.0)+0.01*(x" << next << "-1.0)=0.0;\n";
    }
    model << "end ScaledNonlinearGate;\n";
    model.close();
    auto compiled = smave::compile_model(source);
    if (compiled.blocks.size() != 1 || compiled.blocks.front().unknowns.size() != dimension ||
        compiled.blocks.front().linear) {
        throw std::runtime_error("scaled nonlinear gate fixture lost its single nonlinear SCC");
    }
    return compiled;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 5) {
        std::cerr << "usage: risk_adaptive_gate_evidence linear.mo scenarios nonlinear.mo output\n";
        return 2;
    }
    constexpr std::size_t repetitions = 100;
    constexpr std::size_t burst = 32;
    constexpr std::size_t full_period = 16;
    const auto linear_model = smave::compile_model(argv[1]);
    const auto nonlinear_model = smave::compile_model(argv[3]);
    const auto linear = measure(
        "operator-linear-100", linear_model,
        linear_values(linear_model, argv[2],
            std::filesystem::path(argv[4]).parent_path() / "traces"),
        repetitions, burst, full_period);
    const auto nonlinear = measure(
        "cubic-coupled-nonlinear", nonlinear_model,
        nonlinear_values(nonlinear_model), repetitions, burst, full_period);
    std::vector<std::filesystem::path> linear_scenarios;
    for (const auto& entry : std::filesystem::directory_iterator(argv[2])) {
        if (entry.is_regular_file() && entry.path().extension() == ".conf") {
            linear_scenarios.push_back(entry.path());
        }
    }
    std::sort(linear_scenarios.begin(), linear_scenarios.end());
    if (linear_scenarios.empty()) {
        throw std::runtime_error("full-solve gate evidence requires a linear scenario");
    }
    const auto linear_context = smave::read_scenario(linear_scenarios.front());
    const Values nonlinear_context{{"p", 2.0}};
    const auto full_linear = measure_full_solve(
        "full-solve-linear", linear_model, linear_context,
        std::filesystem::path(argv[4]).parent_path() / "full-solve-traces-linear",
        repetitions, burst, full_period);
    const auto full_nonlinear = measure_full_solve(
        "full-solve-nonlinear", nonlinear_model, nonlinear_context,
        std::filesystem::path(argv[4]).parent_path() / "full-solve-traces-nonlinear",
        repetitions, burst, full_period);
    const auto scaled_nonlinear = scaled_nonlinear_model(
        std::filesystem::path(argv[4]).parent_path() / "ScaledNonlinearGate.mo");
    const auto full_scaled_nonlinear = measure_full_solve(
        "full-solve-scaled-nonlinear", scaled_nonlinear, {},
        std::filesystem::path(argv[4]).parent_path() /
            "full-solve-traces-scaled-nonlinear",
        repetitions, burst, full_period);
    const bool revocation_recheck = revocation_probe(
        nonlinear_model, nonlinear_values(nonlinear_model).front());
    const bool session_and_concurrency_recheck = session_and_concurrency_probe(
        nonlinear_model, nonlinear_values(nonlinear_model).front());
    const bool negative_contract_recheck = negative_contract_probe(
        nonlinear_model, nonlinear_values(nonlinear_model).front());
    const bool full_solve_policy_contract_recheck = full_solve_policy_contract_probe(
        nonlinear_model, nonlinear_context,
        std::filesystem::path(argv[4]).parent_path() / "full-solve-contract-traces");
    const bool full_solve_concurrency_revocation_recheck =
        full_solve_concurrency_revocation_probe(
            scaled_nonlinear, {}, std::filesystem::path(argv[4]).parent_path() /
                "full-solve-race-traces");
    const bool reconstructed_process_local_recheck = reconstructed_process_local_probe(
        scaled_nonlinear, {}, std::filesystem::path(argv[4]).parent_path() /
            "full-solve-reconstruction-traces");
    const bool bundle_version_change_recheck = bundle_version_change_probe(
        scaled_nonlinear, {}, std::filesystem::path(argv[4]).parent_path() /
            "full-solve-bundle-version-traces");

    std::ofstream output(argv[4]);
    if (!output) throw std::runtime_error("cannot write risk-adaptive gate evidence");
    output << std::setprecision(17)
           << "SMAVE_RISK_ADAPTIVE_GATE 1\n"
           << "contract=experimental-immutable-input-token-incremental-certificate\n"
           << "authority_contract=strict-per-request-fp64-original-expression\n"
           << "deployment_promoted=0\n"
           << "assumptions=immutable-model-artifact-tolerance-and-direct-permission;library-owned-immutable-input-identity\n"
           << "low_cost_necessary_condition=finite-and-declared-bound-checks\n"
           << "high_risk_policy=full-strict-verification\n"
           << "low_risk_policy=immutable-input-token-certificate-with-periodic-full-verification\n"
           << "periodic_full_interval=" << full_period << '\n'
           << "certificate_drift_action=reject-and-invalidate\n"
           << "cached_result_contract=aggregate-decision-and-residual-inf\n"
           << "input_issuance_timing=excluded-from-gate-timing\n"
           << "library_issued_immutable_input=1\n"
           << "explicit_revocation_recheck=" << revocation_recheck << '\n'
           << "foreign_session_requires_first_full_verification=1\n"
           << "concurrent_serialized_recheck=" << session_and_concurrency_recheck << '\n'
           << "negative_contract_recheck=" << negative_contract_recheck << '\n'
           << "full_solve_policy_contract_recheck="
           << full_solve_policy_contract_recheck << '\n'
           << "full_solve_concurrency_revocation_recheck="
           << full_solve_concurrency_revocation_recheck << '\n'
           << "reconstructed_process_local_recheck="
           << reconstructed_process_local_recheck << '\n'
           << "bundle_version_change_recheck="
           << bundle_version_change_recheck << '\n'
           << "workloads=2\n"
           << "full_solve_workloads=3\n";
    for (const auto& result : {linear, nonlinear}) {
        const auto prefix = result.workload + ".";
        output << prefix << "candidates=" << result.candidates << '\n'
               << prefix << "requests_per_repetition=" << result.requests_per_repetition << '\n'
               << prefix << "repetitions=" << result.repetitions << '\n'
               << prefix << "strict_median_us=" << result.strict_median_us << '\n'
               << prefix << "adaptive_median_us=" << result.adaptive_median_us << '\n'
               << prefix << "paired_speedup="
               << result.strict_median_us / result.adaptive_median_us << '\n'
               << prefix << "paired_speedup_ci95_lower="
               << result.paired_speedup_ci95_lower << '\n'
               << prefix << "policy_requests=" << result.counters.requests << '\n'
               << prefix << "high_risk_full_verifications="
               << result.counters.high_risk_full_verifications << '\n'
               << prefix << "periodic_full_verifications="
               << result.counters.periodic_full_verifications << '\n'
               << prefix << "certificate_reuses="
               << result.counters.certificate_reuses << '\n'
               << prefix << "low_cost_rejects=" << result.counters.low_cost_rejects << '\n'
               << prefix << "periodic_mismatches="
               << result.counters.periodic_mismatches << '\n'
               << prefix << "decision_mismatches=" << result.decision_mismatches << '\n'
               << prefix << "false_accepts=" << result.false_accepts << '\n'
               << prefix << "false_rejects=" << result.false_rejects << '\n';
    }
    for (const auto& result : {full_linear, full_nonlinear, full_scaled_nonlinear}) {
        const auto prefix = result.workload + ".";
        output << prefix << "repetitions=" << result.repetitions << '\n'
               << prefix << "solves_per_repetition="
               << result.solves_per_repetition << '\n'
               << prefix << "strict_total_median_us="
               << result.strict_total_median_us << '\n'
               << prefix << "adaptive_total_median_us="
               << result.adaptive_total_median_us << '\n'
               << prefix << "total_speedup="
               << result.strict_total_median_us / result.adaptive_total_median_us << '\n'
               << prefix << "total_speedup_ci95_lower="
               << result.total_speedup_ci95_lower << '\n'
               << prefix << "strict_gate_median_us="
               << result.strict_gate_median_us << '\n'
               << prefix << "adaptive_gate_median_us="
               << result.adaptive_gate_median_us << '\n'
               << prefix << "gate_speedup="
               << result.strict_gate_median_us / result.adaptive_gate_median_us << '\n'
               << prefix << "gate_speedup_ci95_lower="
               << result.gate_speedup_ci95_lower << '\n'
               << prefix << "result_mismatches=" << result.result_mismatches << '\n'
               << prefix << "policy_requests=" << result.counters.requests << '\n'
               << prefix << "strict_verifications="
               << result.counters.strict_verifications << '\n'
               << prefix << "periodic_strict_verifications="
               << result.counters.periodic_strict_verifications << '\n'
               << prefix << "certificate_reuses="
               << result.counters.certificate_reuses << '\n'
               << prefix << "periodic_mismatches="
               << result.counters.periodic_mismatches << '\n';
    }
    const bool safe = linear.false_accepts == 0 && linear.false_rejects == 0 &&
        nonlinear.false_accepts == 0 && nonlinear.false_rejects == 0 &&
        linear.counters.periodic_mismatches == 0 &&
        nonlinear.counters.periodic_mismatches == 0;
    const bool full_solve_safe = full_linear.result_mismatches == 0 &&
        full_nonlinear.result_mismatches == 0 &&
        full_scaled_nonlinear.result_mismatches == 0 &&
        full_linear.counters.periodic_mismatches == 0 &&
        full_nonlinear.counters.periodic_mismatches == 0 &&
        full_scaled_nonlinear.counters.periodic_mismatches == 0;
    const std::size_t full_solve_total_significant_workloads =
        static_cast<std::size_t>(full_linear.total_speedup_ci95_lower > 1.0) +
        static_cast<std::size_t>(full_nonlinear.total_speedup_ci95_lower > 1.0) +
        static_cast<std::size_t>(
            full_scaled_nonlinear.total_speedup_ci95_lower > 1.0);
    const std::size_t full_solve_gate_significant_workloads =
        static_cast<std::size_t>(full_linear.gate_speedup_ci95_lower > 1.0) +
        static_cast<std::size_t>(full_nonlinear.gate_speedup_ci95_lower > 1.0) +
        static_cast<std::size_t>(
            full_scaled_nonlinear.gate_speedup_ci95_lower > 1.0);
    const bool complete = safe && full_solve_safe && revocation_recheck &&
        session_and_concurrency_recheck && negative_contract_recheck &&
        full_solve_policy_contract_recheck &&
        full_solve_concurrency_revocation_recheck &&
        reconstructed_process_local_recheck && bundle_version_change_recheck;
    output << "offline_strict_equivalence=" << safe << '\n'
           << "full_solve_strict_equivalence=" << full_solve_safe << '\n'
           << "full_solve_total_significant_workloads="
           << full_solve_total_significant_workloads << '\n'
           << "full_solve_gate_significant_workloads="
           << full_solve_gate_significant_workloads << '\n'
           << "END\n";
    write_raw_samples(
        std::filesystem::path(argv[4]).parent_path() / "raw-samples.txt",
        linear, nonlinear, full_linear, full_nonlinear, full_scaled_nonlinear);
    std::cout << "SMAVE_RISK_ADAPTIVE_GATE 1\n"
              << "LINEAR_SPEEDUP " << linear.strict_median_us / linear.adaptive_median_us << '\n'
              << "NONLINEAR_SPEEDUP "
              << nonlinear.strict_median_us / nonlinear.adaptive_median_us << '\n'
              << "OFFLINE_STRICT_EQUIVALENCE " << safe << '\n';
    return complete ? 0 : 1;
}
