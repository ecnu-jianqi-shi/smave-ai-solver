#include "smave/validation.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <stdexcept>

namespace smave {

namespace {

double binomial_cdf(
    std::size_t events,
    std::size_t trials,
    double probability) {
    if (probability <= 0.0) return 1.0;
    if (probability >= 1.0) return events == trials ? 1.0 : 0.0;
    std::vector<double> logarithms;
    logarithms.reserve(events + 1);
    const double log_probability = std::log(probability);
    const double log_complement = std::log1p(-probability);
    double maximum = -std::numeric_limits<double>::infinity();
    for (std::size_t count = 0; count <= events; ++count) {
        const double term = std::lgamma(static_cast<double>(trials + 1)) -
            std::lgamma(static_cast<double>(count + 1)) -
            std::lgamma(static_cast<double>(trials - count + 1)) +
            static_cast<double>(count) * log_probability +
            static_cast<double>(trials - count) * log_complement;
        logarithms.push_back(term);
        maximum = std::max(maximum, term);
    }
    double sum = 0.0;
    for (const double term : logarithms) sum += std::exp(term - maximum);
    return std::exp(maximum) * sum;
}

}  // namespace

double binomial_proportion_upper_bound(
    std::size_t events,
    std::size_t trials,
    double confidence_level) {
    if (trials == 0 || events > trials || !(confidence_level > 0.0) ||
        !(confidence_level < 1.0)) {
        throw std::invalid_argument("invalid binomial confidence-bound inputs");
    }
    if (events == trials) return 1.0;
    const double alpha = 1.0 - confidence_level;
    double lower = 0.0;
    double upper = 1.0;
    for (int iteration = 0; iteration < 80; ++iteration) {
        const double middle = 0.5 * (lower + upper);
        if (binomial_cdf(events, trials, middle) > alpha) {
            lower = middle;
        } else {
            upper = middle;
        }
    }
    return upper;
}

ValidationReport validate_scenarios(
    const Runtime& runtime,
    const std::filesystem::path& scenario_directory,
    const std::filesystem::path& trace_directory) {
    if (!std::filesystem::is_directory(scenario_directory)) {
        throw std::invalid_argument("scenario suite is not a directory");
    }
    std::vector<std::filesystem::path> scenarios;
    for (const auto& entry : std::filesystem::directory_iterator(scenario_directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".conf") {
            scenarios.push_back(entry.path());
        }
    }
    std::sort(scenarios.begin(), scenarios.end());
    if (scenarios.empty()) throw std::invalid_argument("scenario suite contains no .conf files");
    ValidationReport report;
    report.scenarios = scenarios.size();
    for (const auto& scenario : scenarios) {
        const auto outcome = runtime.solve(
            read_scenario(scenario), trace_directory / scenario.stem());
        if (outcome.success) {
            ++report.successful_scenarios;
        } else {
            ++report.original_solver_failures;
            report.failed_scenarios.push_back(scenario.filename().string());
        }
        for (const auto& block : outcome.blocks) {
            const bool admitted = !block.plan_id.empty() &&
                std::any_of(
                    block.estimated_costs_us.begin(), block.estimated_costs_us.end(),
                    [](double cost) { return cost >= 0.0; });
            if (admitted) {
                ++report.admitted_invocations;
                if (block.path == SolvePath::full_fallback) {
                    ++report.full_fallbacks;
                } else {
                    ++report.top_k_passes;
                }
            }
            if (block.path != SolvePath::full_fallback &&
                block.gate.decision == GateDecision::direct_accept) {
                ++report.safety_evaluations;
            } else if (block.path != SolvePath::full_fallback) {
                ++report.safety_evaluations;
                ++report.erroneous_accepts;
            }
        }
    }
    if (report.admitted_invocations > 0) {
        report.top_k_pass_rate = static_cast<double>(report.top_k_passes) /
            static_cast<double>(report.admitted_invocations);
        report.fallback_rate = static_cast<double>(report.full_fallbacks) /
            static_cast<double>(report.admitted_invocations);
    }
    report.top_k_target_met = report.admitted_invocations > 0 &&
        report.top_k_pass_rate >= 0.95;
    report.safety_target_met = report.erroneous_accepts == 0;
    if (report.safety_evaluations > 0) {
        report.erroneous_accept_rate_upper_bound = binomial_proportion_upper_bound(
            report.erroneous_accepts, report.safety_evaluations,
            report.safety_confidence_level);
    }
    report.confidence_target_met = report.safety_evaluations > 0 &&
        report.erroneous_accept_rate_upper_bound <=
            report.maximum_erroneous_accept_rate;
    return report;
}

void write_validation_report(
    const ValidationReport& report,
    const std::filesystem::path& path) {
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write validation report");
    output << std::setprecision(17)
           << "SMAVE_VALIDATION " << report.schema_version << '\n';
    if (report.schema_version >= 3) {
        output << "dataset_id=" << report.dataset_id << '\n'
               << "dataset_version=" << report.dataset_version << '\n'
               << "dataset_manifest_hash=" << report.dataset_manifest_hash << '\n';
    }
    output << "scenarios=" << report.scenarios << '\n'
           << "successful_scenarios=" << report.successful_scenarios << '\n'
           << "admitted_invocations=" << report.admitted_invocations << '\n'
           << "top_k_passes=" << report.top_k_passes << '\n'
           << "full_fallbacks=" << report.full_fallbacks << '\n'
           << "original_solver_failures=" << report.original_solver_failures << '\n'
           << "erroneous_accepts=" << report.erroneous_accepts << '\n'
           << "safety_evaluations=" << report.safety_evaluations << '\n'
           << "safety_confidence_level=" << report.safety_confidence_level << '\n'
           << "erroneous_accept_rate_upper_bound="
           << report.erroneous_accept_rate_upper_bound << '\n'
           << "maximum_erroneous_accept_rate="
           << report.maximum_erroneous_accept_rate << '\n'
           << "top_k_pass_rate=" << report.top_k_pass_rate << '\n'
           << "fallback_rate=" << report.fallback_rate << '\n'
           << "top_k_target_met=" << report.top_k_target_met << '\n'
           << "safety_target_met=" << report.safety_target_met << '\n';
    output << "confidence_target_met=" << report.confidence_target_met << '\n';
    for (const auto& scenario : report.failed_scenarios) {
        output << "failed_scenario=" << std::quoted(scenario) << '\n';
    }
    output << "END\n";
}

}  // namespace smave
