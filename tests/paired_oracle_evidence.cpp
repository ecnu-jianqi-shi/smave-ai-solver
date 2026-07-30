#include "smave/competition.hpp"
#include "smave/expert.hpp"
#include "smave/family_routing.hpp"
#include "smave/runtime.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <memory>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

struct TimedOutcome {
    smave::SolveOutcome outcome;
    double wall_us{};
};

struct SelectorScenario {
    std::unordered_map<std::string, double> features;
    std::string oracle_winner;
    std::map<std::string, std::vector<double>> candidate_times;
    std::vector<double> calibrated_times;
    std::map<std::string, std::size_t> failures;
    std::map<std::string, std::size_t> gate_mismatches;
    std::map<std::string, double> maximum_errors;
};

struct CartNode {
    std::string prediction;
    std::string feature;
    double threshold{};
    std::unique_ptr<CartNode> lower;
    std::unique_ptr<CartNode> upper;
};

std::vector<std::filesystem::path> scenario_files(
    const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> paths;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".conf") {
            paths.push_back(entry.path());
        }
    }
    std::sort(paths.begin(), paths.end());
    if (paths.empty()) throw std::invalid_argument("paired oracle has no scenarios");
    return paths;
}

std::unordered_map<std::string, double> read_scenario(
    const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read paired oracle scenario");
    std::unordered_map<std::string, double> scenario;
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty() || line.front() == '#') continue;
        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            throw std::invalid_argument("invalid paired oracle scenario field");
        }
        scenario.emplace(line.substr(0, separator), std::stod(line.substr(separator + 1)));
    }
    return scenario;
}

void discard_trace(
    const std::filesystem::path& directory,
    const smave::SolveOutcome& outcome,
    bool retain) {
    if (retain || outcome.trace_id.empty()) return;
    std::error_code error;
    std::filesystem::remove(directory / (outcome.trace_id + ".trace"), error);
}

TimedOutcome timed_solve(
    const smave::Runtime& runtime,
    const std::unordered_map<std::string, double>& scenario,
    const std::filesystem::path& trace_directory,
    bool retain_trace) {
    const auto started = std::chrono::steady_clock::now();
    auto outcome = runtime.solve(scenario, trace_directory);
    const double wall_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - started).count();
    discard_trace(trace_directory, outcome, retain_trace);
    return {std::move(outcome), wall_us};
}

double quantile(const std::vector<double>& sorted, double probability) {
    const double position = probability * static_cast<double>(sorted.size() - 1);
    const auto lower = static_cast<std::size_t>(std::floor(position));
    const auto upper = static_cast<std::size_t>(std::ceil(position));
    const double fraction = position - static_cast<double>(lower);
    return sorted[lower] * (1.0 - fraction) + sorted[upper] * fraction;
}

double median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    return quantile(values, 0.5);
}

double scenario_distance(
    const std::unordered_map<std::string, double>& left,
    const std::unordered_map<std::string, double>& right) {
    if (left.size() != right.size()) return std::numeric_limits<double>::infinity();
    double distance{};
    for (const auto& [name, left_value] : left) {
        const auto found = right.find(name);
        if (found == right.end()) return std::numeric_limits<double>::infinity();
        const double scale = std::max({1.0, std::abs(left_value), std::abs(found->second)});
        const double delta = (left_value - found->second) / scale;
        distance += delta * delta;
    }
    return distance;
}

std::string majority_label(
    const std::vector<SelectorScenario>& scenarios,
    const std::vector<std::size_t>& indices) {
    std::map<std::string, std::size_t> counts;
    for (const auto index : indices) ++counts[scenarios[index].oracle_winner];
    std::string winner;
    std::size_t winner_count{};
    for (const auto& [label, count] : counts) {
        if (count > winner_count) {
            winner = label;
            winner_count = count;
        }
    }
    return winner;
}

double gini_impurity(
    const std::vector<SelectorScenario>& scenarios,
    const std::vector<std::size_t>& indices) {
    std::map<std::string, std::size_t> counts;
    for (const auto index : indices) ++counts[scenarios[index].oracle_winner];
    double squared_probability_sum{};
    for (const auto& [label, count] : counts) {
        static_cast<void>(label);
        const double probability = static_cast<double>(count) /
            static_cast<double>(indices.size());
        squared_probability_sum += probability * probability;
    }
    return 1.0 - squared_probability_sum;
}

std::unique_ptr<CartNode> build_cart(
    const std::vector<SelectorScenario>& scenarios,
    const std::vector<std::size_t>& indices,
    const std::vector<std::string>& features,
    std::size_t depth) {
    auto node = std::make_unique<CartNode>();
    node->prediction = majority_label(scenarios, indices);
    if (depth == 3 || gini_impurity(scenarios, indices) == 0.0) return node;

    double best_impurity = std::numeric_limits<double>::infinity();
    std::string best_feature;
    double best_threshold{};
    std::vector<std::size_t> best_lower;
    std::vector<std::size_t> best_upper;
    for (const auto& feature : features) {
        std::vector<double> values;
        values.reserve(indices.size());
        for (const auto index : indices) values.push_back(scenarios[index].features.at(feature));
        std::sort(values.begin(), values.end());
        values.erase(std::unique(values.begin(), values.end()), values.end());
        for (std::size_t split = 1; split < values.size(); ++split) {
            const double threshold = (values[split - 1] + values[split]) * 0.5;
            std::vector<std::size_t> lower;
            std::vector<std::size_t> upper;
            for (const auto index : indices) {
                (scenarios[index].features.at(feature) <= threshold ? lower : upper)
                    .push_back(index);
            }
            if (lower.size() < 2 || upper.size() < 2) continue;
            const double weighted_impurity =
                (static_cast<double>(lower.size()) * gini_impurity(scenarios, lower) +
                 static_cast<double>(upper.size()) * gini_impurity(scenarios, upper)) /
                static_cast<double>(indices.size());
            if (weighted_impurity < best_impurity ||
                (weighted_impurity == best_impurity &&
                 (best_feature.empty() || feature < best_feature ||
                  (feature == best_feature && threshold < best_threshold)))) {
                best_impurity = weighted_impurity;
                best_feature = feature;
                best_threshold = threshold;
                best_lower = std::move(lower);
                best_upper = std::move(upper);
            }
        }
    }
    if (best_feature.empty()) return node;
    node->feature = best_feature;
    node->threshold = best_threshold;
    node->lower = build_cart(scenarios, best_lower, features, depth + 1);
    node->upper = build_cart(scenarios, best_upper, features, depth + 1);
    return node;
}

std::string predict_cart(
    const CartNode& node,
    const std::unordered_map<std::string, double>& features) {
    if (!node.lower || !node.upper) return node.prediction;
    return predict_cart(
        features.at(node.feature) <= node.threshold ? *node.lower : *node.upper,
        features);
}

std::vector<double> structural_features(
    const std::unordered_map<std::string, double>& features) {
    if (features.empty()) throw std::invalid_argument("structural selector has no features");
    std::vector<double> values;
    values.reserve(features.size());
    for (const auto& [name, value] : features) {
        static_cast<void>(name);
        values.push_back(value);
    }
    const double count = static_cast<double>(values.size());
    const double sum = std::accumulate(values.begin(), values.end(), 0.0);
    const double mean = sum / count;
    double squared_sum{};
    double absolute_sum{};
    double centered_squared_sum{};
    for (const double value : values) {
        squared_sum += value * value;
        absolute_sum += std::abs(value);
        const double centered = value - mean;
        centered_squared_sum += centered * centered;
    }
    const auto [minimum, maximum] = std::minmax_element(values.begin(), values.end());
    return {
        std::log1p(count),
        mean,
        mean * mean,
        std::sqrt(centered_squared_sum / count),
        *minimum,
        *maximum,
        *maximum - *minimum,
        std::sqrt(squared_sum / count),
        absolute_sum / count,
    };
}

std::vector<double> solve_linear_system(
    std::vector<std::vector<double>> matrix,
    std::vector<double> right_hand_side) {
    const std::size_t dimension = right_hand_side.size();
    for (std::size_t column = 0; column < dimension; ++column) {
        std::size_t pivot = column;
        for (std::size_t row = column + 1; row < dimension; ++row) {
            if (std::abs(matrix[row][column]) > std::abs(matrix[pivot][column])) pivot = row;
        }
        if (std::abs(matrix[pivot][column]) < 1.0e-12) {
            throw std::runtime_error("structural selector regression is singular");
        }
        std::swap(matrix[column], matrix[pivot]);
        std::swap(right_hand_side[column], right_hand_side[pivot]);
        const double divisor = matrix[column][column];
        for (std::size_t entry = column; entry < dimension; ++entry) {
            matrix[column][entry] /= divisor;
        }
        right_hand_side[column] /= divisor;
        for (std::size_t row = 0; row < dimension; ++row) {
            if (row == column) continue;
            const double factor = matrix[row][column];
            for (std::size_t entry = column; entry < dimension; ++entry) {
                matrix[row][entry] -= factor * matrix[column][entry];
            }
            right_hand_side[row] -= factor * right_hand_side[column];
        }
    }
    return right_hand_side;
}

double predict_structural_log_cost(
    const std::vector<SelectorScenario>& scenarios,
    const std::vector<std::size_t>& training,
    const std::string& expert,
    const std::unordered_map<std::string, double>& query_features) {
    constexpr double ridge = 1.0e-2;
    std::vector<std::vector<double>> raw_training;
    raw_training.reserve(training.size());
    for (const auto index : training) {
        raw_training.push_back(structural_features(scenarios[index].features));
    }
    const std::vector<double> raw_query = structural_features(query_features);
    const std::size_t feature_count = raw_query.size();
    std::vector<double> means(feature_count);
    std::vector<double> scales(feature_count);
    for (std::size_t feature = 0; feature < feature_count; ++feature) {
        for (const auto& values : raw_training) means[feature] += values[feature];
        means[feature] /= static_cast<double>(raw_training.size());
        for (const auto& values : raw_training) {
            const double centered = values[feature] - means[feature];
            scales[feature] += centered * centered;
        }
        scales[feature] = std::sqrt(scales[feature] / static_cast<double>(raw_training.size()));
        if (scales[feature] < 1.0e-12) scales[feature] = 1.0;
    }

    const std::size_t dimension = feature_count + 1;
    std::vector<std::vector<double>> normal(
        dimension, std::vector<double>(dimension));
    std::vector<double> right_hand_side(dimension);
    for (std::size_t sample = 0; sample < training.size(); ++sample) {
        std::vector<double> design(dimension, 1.0);
        for (std::size_t feature = 0; feature < feature_count; ++feature) {
            design[feature + 1] =
                (raw_training[sample][feature] - means[feature]) / scales[feature];
        }
        const double target = std::log(std::max(
            median(scenarios[training[sample]].candidate_times.at(expert)), 1.0e-9));
        for (std::size_t row = 0; row < dimension; ++row) {
            right_hand_side[row] += design[row] * target;
            for (std::size_t column = 0; column < dimension; ++column) {
                normal[row][column] += design[row] * design[column];
            }
        }
    }
    for (std::size_t diagonal = 1; diagonal < dimension; ++diagonal) {
        normal[diagonal][diagonal] += ridge;
    }
    const auto coefficients = solve_linear_system(normal, right_hand_side);
    double prediction = coefficients.front();
    for (std::size_t feature = 0; feature < feature_count; ++feature) {
        prediction += coefficients[feature + 1] *
            (raw_query[feature] - means[feature]) / scales[feature];
    }
    return prediction;
}

double mixed_qoi_error(
    const smave::SolveOutcome& reference,
    const smave::SolveOutcome& candidate) {
    double maximum = 0.0;
    for (const auto& [name, value] : reference.values) {
        const auto found = candidate.values.find(name);
        if (found == candidate.values.end()) return std::numeric_limits<double>::infinity();
        const double denominator = 1.0e-10 + 1.0e-4 * std::abs(value);
        maximum = std::max(maximum, std::abs(found->second - value) / denominator);
    }
    return maximum;
}

bool native_safe(
    const std::string& expert,
    const std::string& terminal_fallback,
    const smave::SolveOutcome& outcome,
    const smave::SolveOutcome& reference) {
    if (!outcome.success || outcome.blocks.empty() || mixed_qoi_error(reference, outcome) > 1.0) {
        return false;
    }
    const auto& block = outcome.blocks.front();
    const bool attempted = std::find(
        block.attempted_experts.begin(), block.attempted_experts.end(), expert) !=
        block.attempted_experts.end();
    if (!attempted) return false;
    return expert == terminal_fallback
        ? block.path == smave::SolvePath::full_fallback
        : block.path != smave::SolvePath::full_fallback;
}

struct Interval {
    double estimate{};
    double lower{};
    double upper{};
};

Interval bootstrap_median(const std::vector<double>& ratios) {
    Interval interval{.estimate = median(ratios)};
    std::mt19937_64 generator(20260720ULL);
    std::uniform_int_distribution<std::size_t> sample(0, ratios.size() - 1);
    std::vector<double> estimates;
    estimates.reserve(10000);
    std::vector<double> resampled(ratios.size());
    for (std::size_t bootstrap = 0; bootstrap < 10000; ++bootstrap) {
        for (double& value : resampled) value = ratios[sample(generator)];
        estimates.push_back(median(resampled));
    }
    std::sort(estimates.begin(), estimates.end());
    interval.lower = quantile(estimates, 0.025);
    interval.upper = quantile(estimates, 0.975);
    return interval;
}

std::unique_ptr<smave::Runtime> forced_runtime(
    const smave::ModelIR& model,
    const smave::RuntimeBundle& bundle,
    const std::string& expert) {
    smave::RoutingConfig routing;
    routing.top_k = 1;
    if (expert == bundle.terminal_fallback) {
        routing.calibration_block_fingerprint = model.blocks.front().fingerprint;
        routing.calibration_winner = bundle.terminal_fallback;
    } else {
        routing.expert_allowlist.insert(expert);
    }
    return std::make_unique<smave::Runtime>(
        model, smave::make_default_registry(model), bundle, smave::Tolerance{}, routing);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 7) {
        throw std::invalid_argument(
            "usage: paired-oracle-evidence MODEL BUNDLE COMPETITION FAMILY_EVALUATION "
            "SCENARIOS OUTPUT_DIR");
    }
    const auto model = smave::ModelIR::read(argv[1]);
    const auto bundle = smave::RuntimeBundle::read(argv[2]);
    const auto competition = smave::read_competition_report(argv[3]);
    const auto family_evaluation = smave::read_family_router_evaluation(argv[4]);
    const std::filesystem::path scenarios = argv[5];
    const std::filesystem::path output_directory = argv[6];
    std::filesystem::remove_all(output_directory);
    std::filesystem::create_directories(output_directory);

    std::vector<std::string> experts;
    for (const auto& entry : competition.entries) {
        if (entry.attempts > 0 && entry.passes == entry.attempts &&
            entry.fallbacks == 0 && entry.failures == 0 && entry.erroneous_accepts == 0) {
            experts.push_back(entry.expert_version);
        }
    }
    if (std::find(experts.begin(), experts.end(), bundle.terminal_fallback) == experts.end()) {
        experts.push_back(bundle.terminal_fallback);
    }
    std::sort(experts.begin(), experts.end());
    experts.erase(std::unique(experts.begin(), experts.end()), experts.end());
    if (experts.size() < 2) throw std::runtime_error("paired oracle lacks safe candidates");

    smave::Runtime online(
        model, smave::make_default_registry(model), bundle, smave::Tolerance{}, {});
    smave::RoutingConfig calibrated_routing;
    smave::apply_family_router_evaluation(family_evaluation, calibrated_routing);
    smave::Runtime calibrated(
        model,
        smave::make_default_registry(model),
        bundle,
        smave::Tolerance{},
        calibrated_routing);
    std::map<std::string, std::unique_ptr<smave::Runtime>> forced;
    for (const auto& expert : experts) forced.emplace(expert, forced_runtime(model, bundle, expert));

    constexpr std::size_t repetitions = 100;
    std::vector<double> ratios;
    std::vector<double> calibrated_ratios;
    std::vector<double> online_over_calibrated;
    std::vector<SelectorScenario> selector_scenarios;
    std::map<std::string, std::size_t> winner_counts;
    std::size_t online_failures{};
    std::size_t online_gate_mismatches{};
    std::size_t calibrated_failures{};
    std::size_t calibrated_gate_mismatches{};
    std::size_t oracle_selection_failures{};
    double maximum_online_error{};
    double maximum_calibrated_error{};
    const auto paths = scenario_files(scenarios);
    for (std::size_t scenario_index = 0; scenario_index < paths.size(); ++scenario_index) {
        const auto scenario = read_scenario(paths[scenario_index]);
        for (std::size_t warmup = 0; warmup < 3; ++warmup) {
            const auto online_warmup = online.solve(scenario, output_directory / "warmup-online");
            discard_trace(output_directory / "warmup-online", online_warmup, false);
            const auto calibrated_warmup = calibrated.solve(
                scenario, output_directory / "warmup-calibrated");
            discard_trace(output_directory / "warmup-calibrated", calibrated_warmup, false);
            for (const auto& [expert, runtime] : forced) {
                const auto warmup_outcome = runtime->solve(
                    scenario, output_directory / ("warmup-" + expert));
                discard_trace(output_directory / ("warmup-" + expert), warmup_outcome, false);
            }
        }

        std::vector<TimedOutcome> online_samples(repetitions);
        std::vector<TimedOutcome> calibrated_samples(repetitions);
        std::map<std::string, std::vector<TimedOutcome>> candidate_samples;
        for (const auto& expert : experts) candidate_samples[expert].resize(repetitions);
        for (std::size_t repetition = 0; repetition < repetitions; ++repetition) {
            const bool retain = scenario_index == 0 && repetition == 0;
            if (repetition % 2 == 0) {
                online_samples[repetition] = timed_solve(
                    online, scenario, output_directory / "online-traces", retain);
            } else {
                calibrated_samples[repetition] = timed_solve(
                    calibrated,
                    scenario,
                    output_directory / "calibrated-traces",
                    retain);
            }
            for (const auto& expert : experts) {
                candidate_samples[expert][repetition] = timed_solve(
                    *forced.at(expert),
                    scenario,
                    output_directory / ("forced-" + expert + "-traces"),
                    retain);
            }
            if (repetition % 2 != 0) {
                online_samples[repetition] = timed_solve(
                    online, scenario, output_directory / "online-traces", retain);
            } else {
                calibrated_samples[repetition] = timed_solve(
                    calibrated,
                    scenario,
                    output_directory / "calibrated-traces",
                    retain);
            }
        }

        const auto& reference_samples = candidate_samples.at(bundle.terminal_fallback);
        std::string winner;
        double winner_median = std::numeric_limits<double>::infinity();
        for (const auto& expert : experts) {
            std::vector<double> times;
            times.reserve(repetitions);
            bool safe = true;
            for (std::size_t repetition = 0; repetition < repetitions; ++repetition) {
                const auto& sample = candidate_samples.at(expert)[repetition];
                if (!native_safe(
                        expert,
                        bundle.terminal_fallback,
                        sample.outcome,
                        reference_samples[repetition].outcome)) {
                    safe = false;
                    break;
                }
                times.push_back(sample.wall_us);
            }
            if (!safe) continue;
            const double candidate_median = median(times);
            if (candidate_median < winner_median ||
                (candidate_median == winner_median && expert < winner)) {
                winner = expert;
                winner_median = candidate_median;
            }
        }
        if (winner.empty()) {
            ++oracle_selection_failures;
            continue;
        }
        ++winner_counts[winner];
        SelectorScenario selector_scenario{
            .features = scenario,
            .oracle_winner = winner,
        };
        for (const auto& expert : experts) {
            selector_scenario.candidate_times[expert].reserve(repetitions);
        }
        selector_scenario.calibrated_times.reserve(repetitions);
        for (std::size_t repetition = 0; repetition < repetitions; ++repetition) {
            const auto& reference = reference_samples[repetition].outcome;
            const auto& online_sample = online_samples[repetition];
            const auto& calibrated_sample = calibrated_samples[repetition];
            if (!online_sample.outcome.success) ++online_failures;
            const double error = mixed_qoi_error(reference, online_sample.outcome);
            maximum_online_error = std::max(maximum_online_error, error);
            if (error > 1.0) ++online_gate_mismatches;
            if (!calibrated_sample.outcome.success) ++calibrated_failures;
            const double calibrated_error = mixed_qoi_error(
                reference, calibrated_sample.outcome);
            maximum_calibrated_error = std::max(
                maximum_calibrated_error, calibrated_error);
            if (calibrated_error > 1.0) ++calibrated_gate_mismatches;
            selector_scenario.calibrated_times.push_back(calibrated_sample.wall_us);
            for (const auto& expert : experts) {
                const auto& candidate = candidate_samples.at(expert)[repetition];
                const double candidate_error = mixed_qoi_error(reference, candidate.outcome);
                selector_scenario.candidate_times[expert].push_back(candidate.wall_us);
                if (!candidate.outcome.success) ++selector_scenario.failures[expert];
                if (candidate_error > 1.0) ++selector_scenario.gate_mismatches[expert];
                selector_scenario.maximum_errors[expert] = std::max(
                    selector_scenario.maximum_errors[expert], candidate_error);
            }
            ratios.push_back(
                online_sample.wall_us / candidate_samples.at(winner)[repetition].wall_us);
            calibrated_ratios.push_back(
                calibrated_sample.wall_us /
                candidate_samples.at(winner)[repetition].wall_us);
            online_over_calibrated.push_back(
                online_sample.wall_us / calibrated_sample.wall_us);
        }
        selector_scenarios.push_back(std::move(selector_scenario));
    }
    if (ratios.empty()) throw std::runtime_error("paired oracle produced no samples");
    const auto interval = bootstrap_median(ratios);
    const auto calibrated_interval = bootstrap_median(calibrated_ratios);
    const auto calibration_gain = bootstrap_median(online_over_calibrated);
    const double within_five_percent = static_cast<double>(std::count_if(
        ratios.begin(), ratios.end(), [](double ratio) { return ratio <= 1.05; })) /
        static_cast<double>(ratios.size());
    const double online_win_rate = static_cast<double>(std::count_if(
        ratios.begin(), ratios.end(), [](double ratio) { return ratio < 1.0; })) /
        static_cast<double>(ratios.size());
    const double calibrated_within_five_percent = static_cast<double>(std::count_if(
        calibrated_ratios.begin(),
        calibrated_ratios.end(),
        [](double ratio) { return ratio <= 1.05; })) /
        static_cast<double>(calibrated_ratios.size());

    std::vector<double> selector_over_oracle;
    std::vector<double> selector_over_calibrated;
    std::size_t selector_correct_scenarios{};
    std::size_t selector_failures{};
    std::size_t selector_gate_mismatches{};
    double maximum_selector_error{};
    std::map<std::string, std::size_t> selector_counts;
    for (std::size_t query = 0; query < selector_scenarios.size(); ++query) {
        std::size_t nearest = selector_scenarios.size();
        double nearest_distance = std::numeric_limits<double>::infinity();
        for (std::size_t training = 0; training < selector_scenarios.size(); ++training) {
            if (training == query) continue;
            const double distance = scenario_distance(
                selector_scenarios[query].features,
                selector_scenarios[training].features);
            if (distance < nearest_distance ||
                (distance == nearest_distance && training < nearest)) {
                nearest = training;
                nearest_distance = distance;
            }
        }
        if (nearest == selector_scenarios.size()) {
            throw std::runtime_error("leave-one-out selector lacks a training neighbor");
        }
        const std::string& selected = selector_scenarios[nearest].oracle_winner;
        const auto selected_times = selector_scenarios[query].candidate_times.find(selected);
        const auto oracle_times = selector_scenarios[query].candidate_times.find(
            selector_scenarios[query].oracle_winner);
        if (selected_times == selector_scenarios[query].candidate_times.end() ||
            oracle_times == selector_scenarios[query].candidate_times.end()) {
            throw std::runtime_error("leave-one-out selector chose an unavailable expert");
        }
        ++selector_counts[selected];
        if (selected == selector_scenarios[query].oracle_winner) {
            ++selector_correct_scenarios;
        }
        selector_failures += selector_scenarios[query].failures[selected];
        selector_gate_mismatches += selector_scenarios[query].gate_mismatches[selected];
        maximum_selector_error = std::max(
            maximum_selector_error, selector_scenarios[query].maximum_errors[selected]);
        for (std::size_t repetition = 0; repetition < repetitions; ++repetition) {
            selector_over_oracle.push_back(
                selected_times->second[repetition] / oracle_times->second[repetition]);
            selector_over_calibrated.push_back(
                selected_times->second[repetition] /
                selector_scenarios[query].calibrated_times[repetition]);
        }
    }
    const auto selector_oracle_interval = bootstrap_median(selector_over_oracle);
    const auto selector_calibrated_interval = bootstrap_median(selector_over_calibrated);
    const double selector_accuracy = static_cast<double>(selector_correct_scenarios) /
        static_cast<double>(selector_scenarios.size());
    const double selector_within_five_percent = static_cast<double>(std::count_if(
        selector_over_oracle.begin(),
        selector_over_oracle.end(),
        [](double ratio) { return ratio <= 1.05; })) /
        static_cast<double>(selector_over_oracle.size());

    std::vector<std::string> cart_features;
    cart_features.reserve(selector_scenarios.front().features.size());
    for (const auto& [name, value] : selector_scenarios.front().features) {
        static_cast<void>(value);
        cart_features.push_back(name);
    }
    std::sort(cart_features.begin(), cart_features.end());
    std::vector<double> cart_over_oracle;
    std::vector<double> cart_over_calibrated;
    std::size_t cart_correct_scenarios{};
    std::size_t cart_failures{};
    std::size_t cart_gate_mismatches{};
    double maximum_cart_error{};
    std::map<std::string, std::size_t> cart_counts;
    for (std::size_t query = 0; query < selector_scenarios.size(); ++query) {
        std::vector<std::size_t> training;
        training.reserve(selector_scenarios.size() - 1);
        for (std::size_t index = 0; index < selector_scenarios.size(); ++index) {
            if (index != query) training.push_back(index);
        }
        const auto tree = build_cart(selector_scenarios, training, cart_features, 0);
        const std::string selected = predict_cart(
            *tree, selector_scenarios[query].features);
        const auto selected_times = selector_scenarios[query].candidate_times.find(selected);
        const auto oracle_times = selector_scenarios[query].candidate_times.find(
            selector_scenarios[query].oracle_winner);
        if (selected_times == selector_scenarios[query].candidate_times.end() ||
            oracle_times == selector_scenarios[query].candidate_times.end()) {
            throw std::runtime_error("leave-one-out CART chose an unavailable expert");
        }
        ++cart_counts[selected];
        if (selected == selector_scenarios[query].oracle_winner) ++cart_correct_scenarios;
        cart_failures += selector_scenarios[query].failures[selected];
        cart_gate_mismatches += selector_scenarios[query].gate_mismatches[selected];
        maximum_cart_error = std::max(
            maximum_cart_error, selector_scenarios[query].maximum_errors[selected]);
        for (std::size_t repetition = 0; repetition < repetitions; ++repetition) {
            cart_over_oracle.push_back(
                selected_times->second[repetition] / oracle_times->second[repetition]);
            cart_over_calibrated.push_back(
                selected_times->second[repetition] /
                selector_scenarios[query].calibrated_times[repetition]);
        }
    }
    const auto cart_oracle_interval = bootstrap_median(cart_over_oracle);
    const auto cart_calibrated_interval = bootstrap_median(cart_over_calibrated);
    const double cart_accuracy = static_cast<double>(cart_correct_scenarios) /
        static_cast<double>(selector_scenarios.size());
    const double cart_within_five_percent = static_cast<double>(std::count_if(
        cart_over_oracle.begin(),
        cart_over_oracle.end(),
        [](double ratio) { return ratio <= 1.05; })) /
        static_cast<double>(cart_over_oracle.size());

    std::vector<double> structural_over_oracle;
    std::vector<double> structural_over_calibrated;
    std::size_t structural_correct_scenarios{};
    std::size_t structural_failures{};
    std::size_t structural_gate_mismatches{};
    double maximum_structural_error{};
    std::map<std::string, std::size_t> structural_counts;
    for (std::size_t query = 0; query < selector_scenarios.size(); ++query) {
        std::vector<std::size_t> training;
        training.reserve(selector_scenarios.size() - 1);
        for (std::size_t index = 0; index < selector_scenarios.size(); ++index) {
            if (index != query) training.push_back(index);
        }
        std::string selected;
        double predicted_log_cost = std::numeric_limits<double>::infinity();
        for (const auto& expert : experts) {
            const double prediction = predict_structural_log_cost(
                selector_scenarios,
                training,
                expert,
                selector_scenarios[query].features);
            if (prediction < predicted_log_cost ||
                (prediction == predicted_log_cost && (selected.empty() || expert < selected))) {
                selected = expert;
                predicted_log_cost = prediction;
            }
        }
        const auto selected_times = selector_scenarios[query].candidate_times.find(selected);
        const auto oracle_times = selector_scenarios[query].candidate_times.find(
            selector_scenarios[query].oracle_winner);
        if (selected_times == selector_scenarios[query].candidate_times.end() ||
            oracle_times == selector_scenarios[query].candidate_times.end()) {
            throw std::runtime_error("structural selector chose an unavailable expert");
        }
        ++structural_counts[selected];
        if (selected == selector_scenarios[query].oracle_winner) {
            ++structural_correct_scenarios;
        }
        structural_failures += selector_scenarios[query].failures[selected];
        structural_gate_mismatches += selector_scenarios[query].gate_mismatches[selected];
        maximum_structural_error = std::max(
            maximum_structural_error, selector_scenarios[query].maximum_errors[selected]);
        for (std::size_t repetition = 0; repetition < repetitions; ++repetition) {
            structural_over_oracle.push_back(
                selected_times->second[repetition] / oracle_times->second[repetition]);
            structural_over_calibrated.push_back(
                selected_times->second[repetition] /
                selector_scenarios[query].calibrated_times[repetition]);
        }
    }
    const auto structural_oracle_interval = bootstrap_median(structural_over_oracle);
    const auto structural_calibrated_interval = bootstrap_median(structural_over_calibrated);
    const double structural_accuracy =
        static_cast<double>(structural_correct_scenarios) /
        static_cast<double>(selector_scenarios.size());
    const double structural_within_five_percent = static_cast<double>(std::count_if(
        structural_over_oracle.begin(),
        structural_over_oracle.end(),
        [](double ratio) { return ratio <= 1.05; })) /
        static_cast<double>(structural_over_oracle.size());

    std::ofstream output(output_directory / "evidence.txt");
    if (!output) throw std::runtime_error("cannot write paired oracle evidence");
    output << std::setprecision(17)
           << "SMAVE_PAIRED_SCENARIO_ORACLE 1\n"
           << "contract=hindsight-per-scenario-best-safe-forced-complete-runtime-reference\n"
           << "post_hoc_scenario_reference=1\n"
           << "strict_lower_bound=0\n"
           << "search_cost_excluded=1\n"
           << "deployed_family_calibration_compared=1\n"
           << "feature_selector_compared=1\n"
           << "feature_selector_contract=leave-one-scenario-out-one-nearest-neighbor\n"
           << "feature_selector_query_label_excluded=1\n"
           << "feature_selector_label_generation_cost_excluded=1\n"
           << "feature_selector_lookup_cost_excluded=1\n"
           << "feature_selector_exact_winner_accuracy_is_telemetry=1\n"
           << "feature_selector_practical_equivalence_primary=1\n"
           << "literature_cart_selector_compared=1\n"
           << "literature_cart_selector_contract=leave-one-scenario-out-depth-3-gini-cart\n"
           << "literature_cart_selector_reimplementation=1\n"
           << "literature_cart_selector_public_code_used=0\n"
           << "literature_cart_selector_training_and_inference_cost_excluded=1\n"
           << "structural_cost_selector_compared=1\n"
           << "structural_cost_selector_contract=leave-one-scenario-out-per-expert-log-runtime-ridge\n"
           << "structural_cost_selector_query_label_excluded=1\n"
           << "structural_cost_selector_query_timing_excluded=1\n"
           << "structural_cost_selector_features=aggregate-count-mean-variance-range-rms-l1\n"
           << "structural_cost_selector_fixed_ridge=0.01\n"
           << "structural_cost_selector_training_and_inference_cost_excluded=1\n"
           << "calibrated_expert=" << family_evaluation.calibrated_expert << '\n'
           << "scenarios=" << paths.size() << '\n'
           << "repetitions=" << repetitions << '\n'
           << "samples=" << ratios.size() << '\n'
           << "safe_candidates=" << experts.size() << '\n'
           << "distinct_oracle_winners=" << winner_counts.size() << '\n'
           << "paired_median_online_over_oracle=" << interval.estimate << '\n'
           << "bootstrap_95_lower=" << interval.lower << '\n'
           << "bootstrap_95_upper=" << interval.upper << '\n'
           << "online_within_5_percent_rate=" << within_five_percent << '\n'
           << "online_win_rate=" << online_win_rate << '\n'
           << "paired_median_calibrated_over_oracle=" << calibrated_interval.estimate << '\n'
           << "calibrated_bootstrap_95_lower=" << calibrated_interval.lower << '\n'
           << "calibrated_bootstrap_95_upper=" << calibrated_interval.upper << '\n'
           << "calibrated_within_5_percent_rate=" << calibrated_within_five_percent << '\n'
           << "paired_median_online_over_calibrated=" << calibration_gain.estimate << '\n'
           << "calibration_gain_bootstrap_95_lower=" << calibration_gain.lower << '\n'
           << "calibration_gain_bootstrap_95_upper=" << calibration_gain.upper << '\n'
           << "selector_training_scenarios_per_query=" << paths.size() - 1 << '\n'
           << "selector_correct_scenarios=" << selector_correct_scenarios << '\n'
           << "selector_accuracy=" << selector_accuracy << '\n'
           << "selector_distinct_selected_experts=" << selector_counts.size() << '\n'
           << "paired_median_selector_over_oracle=" << selector_oracle_interval.estimate << '\n'
           << "selector_oracle_bootstrap_95_lower=" << selector_oracle_interval.lower << '\n'
           << "selector_oracle_bootstrap_95_upper=" << selector_oracle_interval.upper << '\n'
           << "selector_within_5_percent_rate=" << selector_within_five_percent << '\n'
           << "paired_median_selector_over_calibrated="
           << selector_calibrated_interval.estimate << '\n'
           << "selector_calibrated_bootstrap_95_lower="
           << selector_calibrated_interval.lower << '\n'
           << "selector_calibrated_bootstrap_95_upper="
           << selector_calibrated_interval.upper << '\n'
           << "maximum_selector_mixed_qoi_error=" << maximum_selector_error << '\n'
           << "selector_failures=" << selector_failures << '\n'
           << "selector_gate_mismatches=" << selector_gate_mismatches << '\n'
           << "cart_correct_scenarios=" << cart_correct_scenarios << '\n'
           << "cart_accuracy=" << cart_accuracy << '\n'
           << "cart_distinct_selected_experts=" << cart_counts.size() << '\n'
           << "paired_median_cart_over_oracle=" << cart_oracle_interval.estimate << '\n'
           << "cart_oracle_bootstrap_95_lower=" << cart_oracle_interval.lower << '\n'
           << "cart_oracle_bootstrap_95_upper=" << cart_oracle_interval.upper << '\n'
           << "cart_within_5_percent_rate=" << cart_within_five_percent << '\n'
           << "paired_median_cart_over_calibrated="
           << cart_calibrated_interval.estimate << '\n'
           << "cart_calibrated_bootstrap_95_lower="
           << cart_calibrated_interval.lower << '\n'
           << "cart_calibrated_bootstrap_95_upper="
           << cart_calibrated_interval.upper << '\n'
           << "maximum_cart_mixed_qoi_error=" << maximum_cart_error << '\n'
           << "cart_failures=" << cart_failures << '\n'
           << "cart_gate_mismatches=" << cart_gate_mismatches << '\n'
           << "structural_correct_scenarios=" << structural_correct_scenarios << '\n'
           << "structural_accuracy=" << structural_accuracy << '\n'
           << "structural_distinct_selected_experts=" << structural_counts.size() << '\n'
           << "paired_median_structural_over_oracle="
           << structural_oracle_interval.estimate << '\n'
           << "structural_oracle_bootstrap_95_lower="
           << structural_oracle_interval.lower << '\n'
           << "structural_oracle_bootstrap_95_upper="
           << structural_oracle_interval.upper << '\n'
           << "structural_within_5_percent_rate=" << structural_within_five_percent << '\n'
           << "paired_median_structural_over_calibrated="
           << structural_calibrated_interval.estimate << '\n'
           << "structural_calibrated_bootstrap_95_lower="
           << structural_calibrated_interval.lower << '\n'
           << "structural_calibrated_bootstrap_95_upper="
           << structural_calibrated_interval.upper << '\n'
           << "maximum_structural_mixed_qoi_error=" << maximum_structural_error << '\n'
           << "structural_failures=" << structural_failures << '\n'
           << "structural_gate_mismatches=" << structural_gate_mismatches << '\n'
           << "maximum_online_mixed_qoi_error=" << maximum_online_error << '\n'
           << "maximum_calibrated_mixed_qoi_error=" << maximum_calibrated_error << '\n'
           << "online_failures=" << online_failures << '\n'
           << "online_gate_mismatches=" << online_gate_mismatches << '\n'
           << "calibrated_failures=" << calibrated_failures << '\n'
           << "calibrated_gate_mismatches=" << calibrated_gate_mismatches << '\n'
           << "oracle_selection_failures=" << oracle_selection_failures << '\n';
    for (const auto& [expert, count] : winner_counts) {
        output << "WINNER \"" << expert << "\" scenarios=" << count << '\n';
    }
    for (const auto& [expert, count] : selector_counts) {
        output << "SELECTOR \"" << expert << "\" scenarios=" << count << '\n';
    }
    for (const auto& [expert, count] : cart_counts) {
        output << "CART_SELECTOR \"" << expert << "\" scenarios=" << count << '\n';
    }
    for (const auto& [expert, count] : structural_counts) {
        output << "STRUCTURAL_SELECTOR \"" << expert << "\" scenarios=" << count << '\n';
    }
    output << "END\n";
}
