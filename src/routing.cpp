#include "smave/routing.hpp"

#include "smave/dae_learning.hpp"
#include "smave/complementarity.hpp"
#include "smave/expression.hpp"
#include "smave/high_index_dae.hpp"
#include "smave/linear.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <tuple>

namespace smave {
namespace {

bool supported_routing_feature_name(const std::string& name) {
    if (name == "block:unknown_count" || name == "block:equation_count" ||
        name == "block:structural_nonzeros" || name == "block:structural_density" ||
        name == "context:ood_score" || name == "context:event_distance" ||
        name == "numeric:diagonal_condition_estimate" ||
        name == "sparse:log_rows" || name == "sparse:log_nonzeros" ||
        name == "sparse:log_average_row_nonzeros" ||
        name == "sparse:log_diagonal_condition" ||
        name == "sparse:log_coefficient_dynamic_range" ||
        name == "sparse:row_nonzero_coefficient_of_variation" ||
        name == "sparse:log_row_l1_condition" ||
        name == "sparse:diagonal_dominance_fraction" ||
        name == "sparse:mean_diagonal_row_l1_fraction" ||
        name == "sparse:normalized_mean_bandwidth" ||
        name == "sparse:structurally_symmetric" ||
        name == "sparse:numerically_positive_definite" ||
        name == "sparse:right_hand_side_roughness" ||
        name == "sparse:right_hand_side_sign_change_fraction" ||
        name == "sparse:requested_digits") {
        return true;
    }
    constexpr std::string_view context_prefix = "context:";
    return name.starts_with(context_prefix) && name.size() > context_prefix.size();
}

void validate_routing_feature_names(const std::vector<std::string>& feature_names) {
    if (feature_names.empty()) {
        throw std::invalid_argument("request-conditioned routing requires features");
    }
    std::set<std::string> unique;
    for (const auto& name : feature_names) {
        if (!supported_routing_feature_name(name) || !unique.insert(name).second) {
            throw std::invalid_argument("invalid request-conditioned routing feature: " + name);
        }
    }
}

void validate_request_conditioned_model(const RequestConditionedRoutingModel& model) {
    validate_routing_feature_names(model.feature_names);
    const std::size_t dimensions = model.feature_names.size();
    if (model.feature_means.size() != dimensions ||
        model.feature_scales.size() != dimensions || model.actions.empty()) {
        throw std::invalid_argument("invalid request-conditioned routing model dimensions");
    }
    for (std::size_t index = 0; index < dimensions; ++index) {
        if (!std::isfinite(model.feature_means[index]) ||
            !std::isfinite(model.feature_scales[index]) ||
            model.feature_scales[index] <= 0.0) {
            throw std::invalid_argument("invalid request-conditioned feature normalization");
        }
    }
    for (const auto& [expert, predictors] : model.actions) {
        if (expert.empty() || predictors.empty()) {
            throw std::invalid_argument("request-conditioned action set is empty");
        }
        std::set<int> budgets;
        for (const auto& predictor : predictors) {
            const auto valid_coefficients = [&](const std::vector<double>& coefficients) {
                return coefficients.size() == dimensions + 1 &&
                    std::all_of(
                        coefficients.begin(), coefficients.end(),
                        [](double value) { return std::isfinite(value); });
            };
            if (predictor.work_iterations < 0 || predictor.training_samples == 0 ||
                predictor.independent_training_groups == 0 ||
                predictor.independent_training_groups > predictor.training_samples ||
                predictor.independent_calibration_groups == 0 ||
                !budgets.insert(predictor.work_iterations).second ||
                !valid_coefficients(predictor.log_cost_coefficients) ||
                !valid_coefficients(predictor.pass_logit_coefficients) ||
                !std::isfinite(predictor.log_cost_calibration_offset) ||
                !std::isfinite(predictor.pass_logit_calibration_offset) ||
                !std::isfinite(predictor.cost_calibration_error) ||
                predictor.cost_calibration_error < 0.0 ||
                !std::isfinite(predictor.pass_calibration_error) ||
                predictor.pass_calibration_error < 0.0 ||
                predictor.pass_calibration_error > 1.0 ||
                !std::isfinite(predictor.cost_calibration_upper_error) ||
                predictor.cost_calibration_upper_error < 0.0 ||
                !std::isfinite(predictor.pass_calibration_upper_error) ||
                predictor.pass_calibration_upper_error < 0.0 ||
                predictor.pass_calibration_upper_error > 1.0 ||
                predictor.support_feature_minimums.size() !=
                    predictor.support_feature_maximums.size() ||
                (!predictor.support_feature_minimums.empty() &&
                 predictor.support_feature_minimums.size() != dimensions) ||
                predictor.joint_support_feature_indices.empty() !=
                    predictor.joint_support_group_centers.empty() ||
                (!predictor.joint_support_feature_indices.empty() &&
                 (!std::isfinite(predictor.joint_support_nearest_distance_upper) ||
                  predictor.joint_support_nearest_distance_upper < 0.0))) {
                throw std::invalid_argument(
                    "invalid request-conditioned action predictor for expert " + expert);
            }
            for (std::size_t index = 0;
                 index < predictor.support_feature_minimums.size(); ++index) {
                if (!std::isfinite(predictor.support_feature_minimums[index]) ||
                    !std::isfinite(predictor.support_feature_maximums[index]) ||
                    predictor.support_feature_minimums[index] >
                        predictor.support_feature_maximums[index]) {
                    throw std::invalid_argument(
                        "invalid request-conditioned action support for expert " + expert);
                }
            }
            std::set<std::size_t> joint_indices;
            for (const auto index : predictor.joint_support_feature_indices) {
                if (index >= dimensions || !joint_indices.insert(index).second) {
                    throw std::invalid_argument(
                        "invalid request-conditioned joint support index for expert " +
                        expert);
                }
            }
            for (const auto& center : predictor.joint_support_group_centers) {
                if (center.size() != predictor.joint_support_feature_indices.size() ||
                    !std::all_of(center.begin(), center.end(), [](double value) {
                        return std::isfinite(value);
                    })) {
                    throw std::invalid_argument(
                        "invalid request-conditioned joint support center for expert " +
                        expert);
                }
            }
            std::set<std::string> prior_families;
            for (const auto& prior : predictor.family_priors) {
                if (prior.routing_family.empty() ||
                    !prior_families.insert(prior.routing_family).second ||
                    prior.independent_training_groups == 0 ||
                    prior.independent_calibration_groups == 0 ||
                    !std::isfinite(prior.pooled_log_cost) ||
                    !std::isfinite(prior.pooled_pass_probability) ||
                    prior.pooled_pass_probability < 0.0 ||
                    prior.pooled_pass_probability > 1.0 ||
                    !std::isfinite(prior.cost_regression_weight) ||
                    prior.cost_regression_weight < 0.0 ||
                    prior.cost_regression_weight > 1.0 ||
                    !std::isfinite(prior.pass_regression_weight) ||
                    prior.pass_regression_weight < 0.0 ||
                    prior.pass_regression_weight > 1.0 ||
                    !std::isfinite(prior.cost_calibration_upper_error) ||
                    prior.cost_calibration_upper_error < 0.0 ||
                    !std::isfinite(prior.pass_calibration_upper_error) ||
                    prior.pass_calibration_upper_error < 0.0 ||
                    prior.pass_calibration_upper_error > 1.0) {
                    throw std::invalid_argument(
                        "invalid request-conditioned family prior for expert " + expert);
                }
            }
        }
    }
}

std::vector<double> solve_dense_system(
    std::vector<std::vector<double>> matrix,
    std::vector<double> right_hand_side) {
    const std::size_t size = matrix.size();
    if (size == 0 || right_hand_side.size() != size) {
        throw std::invalid_argument("invalid regression system");
    }
    for (const auto& row : matrix) {
        if (row.size() != size) throw std::invalid_argument("invalid regression matrix");
    }
    for (std::size_t column = 0; column < size; ++column) {
        std::size_t pivot = column;
        for (std::size_t row = column + 1; row < size; ++row) {
            if (std::abs(matrix[row][column]) > std::abs(matrix[pivot][column])) {
                pivot = row;
            }
        }
        if (!std::isfinite(matrix[pivot][column]) ||
            std::abs(matrix[pivot][column]) < 1.0e-14) {
            throw std::invalid_argument("singular request-conditioned regression system");
        }
        if (pivot != column) {
            std::swap(matrix[pivot], matrix[column]);
            std::swap(right_hand_side[pivot], right_hand_side[column]);
        }
        const double diagonal = matrix[column][column];
        for (std::size_t entry = column; entry < size; ++entry) {
            matrix[column][entry] /= diagonal;
        }
        right_hand_side[column] /= diagonal;
        for (std::size_t row = 0; row < size; ++row) {
            if (row == column) continue;
            const double factor = matrix[row][column];
            if (factor == 0.0) continue;
            for (std::size_t entry = column; entry < size; ++entry) {
                matrix[row][entry] -= factor * matrix[column][entry];
            }
            right_hand_side[row] -= factor * right_hand_side[column];
        }
    }
    return right_hand_side;
}

std::vector<double> normalized_design_row(
    const RequestConditionedRoutingModel& model,
    const std::vector<double>& features) {
    if (features.size() != model.feature_names.size()) {
        throw std::invalid_argument("request-conditioned feature dimension mismatch");
    }
    std::vector<double> row(features.size() + 1, 1.0);
    for (std::size_t index = 0; index < features.size(); ++index) {
        if (!std::isfinite(features[index])) {
            throw std::invalid_argument("nonfinite request-conditioned routing feature");
        }
        row[index + 1] =
            (features[index] - model.feature_means[index]) / model.feature_scales[index];
    }
    return row;
}

double linear_response(
    const std::vector<double>& coefficients,
    const std::vector<double>& row) {
    return std::inner_product(coefficients.begin(), coefficients.end(), row.begin(), 0.0);
}

double logistic(double value) {
    if (value >= 0.0) {
        const double inverse = std::exp(-std::min(value, 40.0));
        return 1.0 / (1.0 + inverse);
    }
    const double exponential = std::exp(std::max(value, -40.0));
    return exponential / (1.0 + exponential);
}

double calibrated_logit_offset(
    const std::vector<double>& logits,
    double observed_probability) {
    if (logits.empty() || !std::isfinite(observed_probability) ||
        observed_probability < 0.0 || observed_probability > 1.0) {
        throw std::invalid_argument("invalid probability calibration samples");
    }
    double lower = -40.0;
    double upper = 40.0;
    for (std::size_t iteration = 0; iteration < 100; ++iteration) {
        const double offset = 0.5 * (lower + upper);
        double predicted{};
        for (const double logit : logits) predicted += logistic(logit + offset);
        predicted /= static_cast<double>(logits.size());
        if (predicted < observed_probability) lower = offset;
        else upper = offset;
    }
    return 0.5 * (lower + upper);
}

double conservative_expected_cascade_cost(
    const std::vector<SolveStep>& steps,
    double terminal_cost_us,
    const std::vector<RouteConditionalCostCalibration>&
        conditional_cost_calibrations = {}) {
    if (!std::isfinite(terminal_cost_us) || terminal_cost_us < 0.0) {
        throw std::invalid_argument("invalid conservative terminal cost");
    }
    using ActionKey = std::pair<std::string, int>;
    using TransitionKey = std::pair<ActionKey, ActionKey>;
    std::map<TransitionKey, double> conditional_multipliers;
    for (const auto& calibration : conditional_cost_calibrations) {
        const ActionKey previous{
            calibration.previous.expert_version,
            calibration.previous.work_iterations};
        const ActionKey next{
            calibration.next.expert_version,
            calibration.next.work_iterations};
        if (previous.first.empty() || next.first.empty() || previous.second < 0 ||
            next.second < 0 || calibration.independent_training_groups == 0 ||
            calibration.independent_calibration_groups == 0 ||
            !std::isfinite(calibration.conditional_cost_multiplier) ||
            calibration.conditional_cost_multiplier <= 0.0 ||
            !std::isfinite(calibration.conditional_cost_multiplier_upper) ||
            calibration.conditional_cost_multiplier_upper <
                calibration.conditional_cost_multiplier ||
            !conditional_multipliers.emplace(
                TransitionKey{previous, next},
                calibration.conditional_cost_multiplier_upper).second) {
            throw std::invalid_argument(
                "invalid conservative conditional routing cost calibration");
        }
    }
    double reach_probability = 1.0;
    double expected_cost{};
    std::optional<ActionKey> previous;
    for (const auto& step : steps) {
        if (!std::isfinite(step.estimated_cost_us) || step.estimated_cost_us < 0.0 ||
            !std::isfinite(step.pass_probability) || step.pass_probability < 0.0 ||
            step.pass_probability > 1.0 || !std::isfinite(step.risk_score) ||
            step.risk_score < 0.0 ||
            !std::isfinite(step.cost_relative_uncertainty) ||
            step.cost_relative_uncertainty < 0.0 ||
            !std::isfinite(step.pass_probability_uncertainty) ||
            step.pass_probability_uncertainty < 0.0 ||
            step.pass_probability_uncertainty > 1.0 ||
            !std::isfinite(step.support_extrapolation) ||
            step.support_extrapolation < 0.0) {
            throw std::invalid_argument("invalid conservative solve step");
        }
        const double support_multiplier = std::exp(std::min(
            step.support_extrapolation, std::log(1.0e6)));
        const double inflated_cost = step.estimated_cost_us *
            (1.0 + step.cost_relative_uncertainty) * support_multiplier;
        const double lower_pass_probability = std::clamp(
            step.pass_probability - step.pass_probability_uncertainty -
                std::min(1.0, step.support_extrapolation),
            0.0, 1.0);
        const ActionKey current{
            step.expert_version, step.budget.work_iterations};
        double conditional_multiplier = 1.0;
        if (previous.has_value()) {
            const auto found = conditional_multipliers.find({*previous, current});
            if (found != conditional_multipliers.end()) {
                conditional_multiplier = found->second;
            }
        }
        expected_cost += reach_probability * inflated_cost * conditional_multiplier;
        reach_probability *= 1.0 - lower_pass_probability;
        previous = current;
    }
    return expected_cost + reach_probability * terminal_cost_us;
}

double median_value(std::vector<double> values) {
    if (values.empty()) throw std::invalid_argument("median requires samples");
    std::sort(values.begin(), values.end());
    const std::size_t middle = values.size() / 2;
    return values.size() % 2 == 0
        ? 0.5 * (values[middle - 1] + values[middle])
        : values[middle];
}

double upper_empirical_quantile(std::vector<double> values, double probability) {
    if (values.empty() || !std::isfinite(probability) ||
        probability <= 0.0 || probability > 1.0) {
        throw std::invalid_argument("invalid empirical quantile");
    }
    std::sort(values.begin(), values.end());
    const double finite_sample_rank = std::ceil(
        probability * static_cast<double>(values.size() + 1));
    const std::size_t index = static_cast<std::size_t>(std::clamp(
        finite_sample_rank - 1.0, 0.0,
        static_cast<double>(values.size() - 1)));
    return values[index];
}

bool square_five_point_sparsity(const BlockIR& block) {
    const auto size = block.unknowns.size();
    if (size < 16 || block.jacobian_sparsity.row_count != size ||
        block.jacobian_sparsity.column_count != size) return false;
    const auto width = static_cast<std::size_t>(std::llround(std::sqrt(
        static_cast<double>(size))));
    if (width * width != size) return false;
    for (std::size_t row = 0; row < size; ++row) {
        bool diagonal = false;
        for (const auto column : block.jacobian_sparsity.row(row)) {
            const bool west = row % width != 0 && column + 1 == row;
            const bool east = row % width + 1 < width && column == row + 1;
            const bool south = row >= width && column + width == row;
            const bool north = row + width < size && column == row + width;
            if (column == row) diagonal = true;
            else if (!west && !east && !south && !north) return false;
        }
        if (!diagonal) return false;
    }
    return true;
}

std::string plan_id(const BlockIR& block, const std::vector<SolveStep>& steps) {
    std::uint64_t hash = 1469598103934665603ULL;
    std::string contract = block.fingerprint;
    for (const auto& step : steps) {
        contract += step.expert_version + to_string(step.permission) +
            to_string(step.backend_role) +
            std::to_string(step.budget.work_iterations) + step.selection_reason;
        for (const auto& stage : step.backend_chain) contract += stage;
    }
    for (const unsigned char character : contract) {
        hash ^= character;
        hash *= 1099511628211ULL;
    }
    std::ostringstream output;
    output << "plan-" << std::hex << std::setfill('0') << std::setw(16) << hash;
    return output.str();
}

std::string plan_id(
    const std::string& fingerprint,
    const std::vector<SolveStep>& steps) {
    std::uint64_t hash = 1469598103934665603ULL;
    std::string contract = fingerprint;
    for (const auto& step : steps) {
        contract += step.expert_version + to_string(step.permission) +
            to_string(step.backend_role) +
            std::to_string(step.budget.work_iterations) + step.selection_reason;
        for (const auto& stage : step.backend_chain) contract += stage;
    }
    for (const unsigned char character : contract) {
        hash ^= character;
        hash *= 1099511628211ULL;
    }
    std::ostringstream output;
    output << "plan-" << std::hex << std::setfill('0') << std::setw(16) << hash;
    return output.str();
}

std::string implicit_fingerprint(const FullyImplicitDaeIR& model) {
    std::ostringstream value;
    value << "fully-implicit-" << model.source_hash << "-events-" << model.events.size();
    for (const auto& event : model.events) {
        value << '-' << event.id << ':' << event.direction << ':' << event.priority
              << ':' << event.source_order << ':' << event.guard;
        for (const auto& reset : event.resets) {
            value << ':' << reset.variable << '=' << reset.expression;
        }
    }
    return value.str();
}

std::size_t implicit_structural_nonzeros(const FullyImplicitDaeIR& model) {
    std::unordered_map<std::string, std::size_t> positions;
    for (std::size_t index = 0; index < model.states.size(); ++index) {
        positions[model.states[index].name] = index;
        positions["__smave_der_" + model.states[index].name] = index;
    }
    for (std::size_t index = 0; index < model.algebraics.size(); ++index) {
        positions[model.algebraics[index].name] = model.states.size() + index;
    }
    std::size_t nonzeros{};
    for (const auto& equation : model.equations) {
        std::set<std::size_t> columns;
        for (const auto& name : equation.variables) {
            const auto position = positions.find(name);
            if (position != positions.end()) columns.insert(position->second);
        }
        nonzeros += columns.size();
    }
    return nonzeros;
}

}  // namespace

std::string to_string(BackendRole role) {
    switch (role) {
        case BackendRole::initializer: return "initializer";
        case BackendRole::linear_solver: return "linear_solver";
        case BackendRole::preconditioner: return "preconditioner";
        case BackendRole::nonlinear_solver: return "nonlinear_solver";
        case BackendRole::operator_candidate: return "operator_candidate";
        case BackendRole::fallback: return "fallback";
    }
    throw std::invalid_argument("unknown backend role");
}

namespace {

std::vector<std::string> backend_chain(const CandidateExpert& candidate) {
    if (candidate.expert_version == "newton-krylov-jfnk-cpu-v1") {
        return {
            "damped-newton-corrector",
            "directional-ad-jacobian-vector-product",
            "matrix-free-diagonal-preconditioner",
            "restarted-gmres-true-residual",
            "runtime-residual-constraint-gate",
        };
    }
    if (candidate.expert_version == "newton-krylov-csr-cpu-v1") {
        return {
            "damped-newton-corrector",
            "colored-forward-ad-csr-jacobian",
            "ic0-or-ilu0-preconditioner",
            "pcg-or-restarted-gmres",
            "runtime-residual-constraint-gate",
        };
    }
    switch (candidate.backend_role) {
        case BackendRole::initializer:
            return {
                candidate.expert_version,
                "original-damped-newton-corrector",
                "runtime-residual-constraint-gate",
            };
        case BackendRole::preconditioner:
            return {
                candidate.expert_version,
                "pcg-krylov-corrector",
                "runtime-residual-constraint-gate",
            };
        case BackendRole::operator_candidate:
            return {
                candidate.expert_version,
                "original-damped-newton-corrector",
                "runtime-residual-constraint-gate",
            };
        case BackendRole::linear_solver:
        case BackendRole::nonlinear_solver:
            return {candidate.expert_version, "runtime-residual-constraint-gate"};
        case BackendRole::fallback:
            return {candidate.expert_version};
    }
    throw std::invalid_argument("unknown backend role");
}

}  // namespace

EquationAssessment assess_equation(const BlockIR& block) {
    EquationAssessment assessment;
    assessment.unknown_count = block.unknowns.size();
    assessment.equation_count = block.equation_ids.size();
    assessment.linear = block.linear;
    assessment.smooth = block.smooth;
    assessment.event_related = block.event_related;
    assessment.structurally_square =
        assessment.unknown_count != 0 &&
        assessment.equation_count == assessment.unknown_count &&
        block.jacobian_sparsity.row_count == assessment.equation_count &&
        block.jacobian_sparsity.column_count == assessment.unknown_count;

    bool symmetric = assessment.structurally_square;
    assessment.structural_nonzeros = block.jacobian_sparsity.nonzeros();
    for (std::size_t row = 0; row < block.jacobian_sparsity.row_count; ++row) {
        for (const auto column : block.jacobian_sparsity.row(row)) {
            if (symmetric && !block.jacobian_sparsity.contains(column, row)) {
                symmetric = false;
            }
        }
    }
    assessment.structurally_symmetric = symmetric;
    if (assessment.unknown_count != 0 && assessment.equation_count != 0) {
        assessment.structural_density = static_cast<double>(assessment.structural_nonzeros) /
            static_cast<double>(assessment.unknown_count * assessment.equation_count);
    }
    assessment.scale_class = assessment.unknown_count <= 8
        ? "tiny"
        : (assessment.unknown_count <= 64
            ? "small"
            : (assessment.unknown_count <= 1024 ? "medium" : "large"));
    if (assessment.unknown_count != 0 &&
        assessment.unknown_count <=
            std::numeric_limits<std::size_t>::max() / assessment.unknown_count / sizeof(double)) {
        assessment.estimated_dense_bytes = assessment.unknown_count *
            assessment.unknown_count * sizeof(double);
    } else {
        assessment.estimated_dense_bytes = std::numeric_limits<std::size_t>::max();
    }
    constexpr std::size_t sparse_entry_bytes = sizeof(double) + 2 * sizeof(std::size_t);
    if (assessment.structural_nonzeros <=
        std::numeric_limits<std::size_t>::max() / sparse_entry_bytes) {
        assessment.estimated_sparse_bytes =
            assessment.structural_nonzeros * sparse_entry_bytes;
    } else {
        assessment.estimated_sparse_bytes = std::numeric_limits<std::size_t>::max();
    }
    constexpr std::size_t maximum_dense_direct_bytes = 64U * 1024U * 1024U;
    assessment.dense_direct_eligible = assessment.structurally_square &&
        assessment.estimated_dense_bytes <= maximum_dense_direct_bytes &&
        (assessment.unknown_count <= 64 || assessment.structural_density >= 0.25);

    if (block.event_related || !block.smooth) {
        assessment.equation_family = "event-or-nonsmooth";
        assessment.admissible_backend_roles = {
            BackendRole::nonlinear_solver, BackendRole::fallback};
        assessment.forbidden_backend_roles = {
            BackendRole::operator_candidate, BackendRole::preconditioner};
        assessment.reasons.push_back(
            "event or nonsmooth semantics require the authoritative event-aware path");
    } else if (block.dae_index > 1) {
        assessment.equation_family = "high-index-dae";
        assessment.admissible_backend_roles = {BackendRole::fallback};
        assessment.forbidden_backend_roles = {
            BackendRole::initializer, BackendRole::linear_solver,
            BackendRole::preconditioner, BackendRole::operator_candidate};
        assessment.reasons.push_back("high-index DAE requires reduction before expert routing");
    } else if (block.linear) {
        assessment.admissible_backend_roles = {
            BackendRole::linear_solver, BackendRole::preconditioner,
            BackendRole::operator_candidate, BackendRole::fallback};
        if (assessment.structurally_symmetric) {
            assessment.equation_family = "linear-structurally-symmetric";
            assessment.runtime_positive_definite_check_required = true;
            assessment.reasons.push_back(
                "symmetric sparsity admits PCG candidates but numerical SPD remains runtime-gated");
        } else {
            assessment.equation_family = assessment.structural_density <= 0.35
                ? "linear-sparse-nonsymmetric" : "linear-dense-nonsymmetric";
            assessment.reasons.push_back(
                "nonsymmetric sparsity excludes CG-family backends at compile time");
        }
    } else {
        assessment.equation_family = block.smooth
            ? "nonlinear-smooth" : "nonlinear-nonsmooth";
        assessment.admissible_backend_roles = {
            BackendRole::initializer, BackendRole::nonlinear_solver,
            BackendRole::operator_candidate, BackendRole::fallback};
        assessment.reasons.push_back(
            "nonlinear block retains initializer, corrector, and original solver composition");
    }
    return assessment;
}

EquationAssessment assess_equation(const SparseLinearProfile& profile) {
    EquationAssessment assessment;
    assessment.unknown_count = profile.columns;
    assessment.equation_count = profile.rows;
    assessment.structural_nonzeros = profile.nonzeros;
    assessment.linear = true;
    assessment.smooth = true;
    assessment.structurally_square = profile.rows != 0 && profile.rows == profile.columns;
    assessment.structurally_symmetric = profile.structurally_symmetric;
    assessment.numeric_probe_available = true;
    assessment.numerically_symmetric = profile.numerically_symmetric;
    assessment.numerically_positive_definite = profile.numerically_positive_definite;
    assessment.diagonal_condition_estimate = profile.diagonal_condition_estimate;
    if (profile.rows != 0 && profile.columns != 0) {
        assessment.structural_density = static_cast<double>(profile.nonzeros) /
            static_cast<double>(profile.rows * profile.columns);
    }
    assessment.scale_class = profile.columns <= 8 ? "tiny" :
        (profile.columns <= 64 ? "small" : (profile.columns <= 1024 ? "medium" : "large"));
    assessment.estimated_dense_bytes = profile.columns != 0 && profile.columns <=
            std::numeric_limits<std::size_t>::max() / profile.columns / sizeof(double)
        ? profile.columns * profile.columns * sizeof(double)
        : std::numeric_limits<std::size_t>::max();
    constexpr std::size_t sparse_entry_bytes = sizeof(double) + 2 * sizeof(std::size_t);
    assessment.estimated_sparse_bytes = profile.nonzeros <=
            std::numeric_limits<std::size_t>::max() / sparse_entry_bytes
        ? profile.nonzeros * sparse_entry_bytes
        : std::numeric_limits<std::size_t>::max();
    assessment.dense_direct_eligible = assessment.structurally_square &&
        assessment.estimated_dense_bytes <= 64U * 1024U * 1024U &&
        (profile.columns <= 64 || assessment.structural_density >= 0.25);
    assessment.admissible_backend_roles = {
        BackendRole::linear_solver, BackendRole::preconditioner,
        BackendRole::operator_candidate, BackendRole::fallback};
    assessment.mandatory_fallback = "terminal-numerical-linear-cascade-v1";
    if (profile.structurally_symmetric) {
        assessment.equation_family = "linear-structurally-symmetric";
        assessment.runtime_positive_definite_check_required = true;
        assessment.reasons.push_back(profile.numerically_positive_definite
            ? "numeric probe admits SPD Krylov candidates"
            : "numeric probe rejects SPD-only candidates");
    } else {
        assessment.equation_family = assessment.structural_density <= 0.35
            ? "linear-sparse-nonsymmetric" : "linear-dense-nonsymmetric";
        assessment.reasons.push_back("nonsymmetric structure requires GMRES or direct fallback");
    }
    return assessment;
}

SolvePlan route_sparse_linear_system(
    const SparseLinearProfile& profile, const RoutingConfig& routing) {
    if (routing.top_k == 0 || !routing.require_original_fallback) {
        throw std::invalid_argument("invalid sparse linear routing configuration");
    }
    if (!routing.request_conditioned_family_anchors.empty() &&
        !routing.request_conditioned_model.has_value()) {
        throw std::invalid_argument(
            "sparse family anchors require a request-conditioned model");
    }
    if (!std::isfinite(routing.minimum_family_anchor_gain_fraction) ||
        routing.minimum_family_anchor_gain_fraction < 0.0 ||
        routing.minimum_family_anchor_gain_fraction >= 1.0) {
        throw std::invalid_argument("invalid sparse family-anchor gain fraction");
    }
    if (profile.maximum_work_iterations <= 0 || profile.restart_dimension <= 0) {
        throw std::invalid_argument("invalid sparse linear work budget");
    }
    SolvePlan plan;
    plan.block_fingerprint = profile.fingerprint;
    plan.assessment = assess_equation(profile);
    plan.terminal_fallback = plan.assessment.mandatory_fallback;
    auto append = [&](std::string version, int work_iterations,
                      double pass_probability, double risk,
                      double estimated_cost_us, std::string reason,
                      std::vector<std::string> backend_chain = {}) {
        if (!routing.expert_allowlist.empty() && !routing.expert_allowlist.contains(version)) return;
        if (backend_chain.empty()) {
            backend_chain = {
                "equation-expert", "sparse-linear-solver",
                "original-residual-gate"};
        }
        plan.steps.push_back(SolveStep{
            .expert_version = std::move(version),
            .permission = Permission::direct,
            .budget = SolveBudget{.work_iterations = work_iterations},
            .estimated_cost_us = estimated_cost_us,
            .pass_probability = pass_probability,
            .risk_score = risk,
            .cost_relative_uncertainty = 0.0,
            .pass_probability_uncertainty = 0.0,
            .support_extrapolation = 0.0,
            .backend_role = BackendRole::linear_solver,
            .backend_chain = std::move(backend_chain),
            .selection_reason = std::move(reason),
            .builtin = true,
        });
    };
    const auto nonzeros = static_cast<double>(profile.nonzeros);
    const auto unknowns = static_cast<double>(profile.columns);
    const auto batch = static_cast<double>(std::max<std::size_t>(1, profile.batch_size));
    const auto reuses = static_cast<double>(std::max<std::size_t>(1, profile.expected_reuses));
    const bool many_query = profile.batch_size >= 8 || profile.expected_reuses >= 32;
    const bool request_conditioned = routing.request_conditioned_model.has_value();
    const auto request_features = request_conditioned
        ? extract_sparse_routing_features(
            routing.request_conditioned_model->feature_names, profile)
        : std::vector<double>{};
    const std::string numeric_family = profile.numerically_positive_definite
        ? "spd"
        : (profile.numerically_symmetric
            ? "symmetric-indefinite" : "nonsymmetric");
    double conditioned_terminal_attempt_cost =
        routing.calibrated_terminal_fallback_cost_us;
    double conditioned_terminal_cost = conditioned_terminal_attempt_cost;
    if (request_conditioned) {
        const auto terminal_predictors =
            routing.request_conditioned_model->actions.find(plan.terminal_fallback);
        if (terminal_predictors != routing.request_conditioned_model->actions.end()) {
            const auto terminal = predict_request_conditioned_action(
                *routing.request_conditioned_model, plan.terminal_fallback, 0,
                request_features, 0.0, numeric_family);
            conditioned_terminal_attempt_cost = terminal.attempt_wall_us;
            conditioned_terminal_cost = conditioned_terminal_attempt_cost +
                routing.risk_weight * terminal.risk_score;
        }
    }
    auto append_action_family = [&](const std::string& version,
                                    int default_work_iterations,
                                    double pass_probability, double risk,
                                    double estimated_cost_us, const std::string& reason,
                                    std::vector<std::string> backend_chain = {}) {
        if (!request_conditioned) {
            append(version, default_work_iterations, pass_probability, risk,
                   estimated_cost_us, reason, std::move(backend_chain));
            return;
        }
        const auto actions = routing.request_conditioned_model->actions.find(version);
        if (actions == routing.request_conditioned_model->actions.end()) return;
        for (const auto& predictor : actions->second) {
            const bool direct_action = default_work_iterations == 0;
            if (direct_action && predictor.work_iterations != 0) {
                throw std::invalid_argument(
                    "direct sparse action requires zero work budget: " + version);
            }
            if (!direct_action && (predictor.work_iterations <= 0 ||
                                   predictor.work_iterations >
                                       profile.maximum_work_iterations)) {
                throw std::invalid_argument(
                    "iterative sparse action work budget is outside request limit: " +
                    version);
            }
            const auto prediction = predict_request_conditioned_action(
                *routing.request_conditioned_model, version,
                predictor.work_iterations, request_features,
                conditioned_terminal_attempt_cost, numeric_family);
            if (prediction.pass_probability <= 0.0 ||
                prediction.pass_probability < routing.minimum_pass_probability) {
                continue;
            }
            const double action_risk = std::max(risk, prediction.risk_score);
            const std::size_t previous_size = plan.steps.size();
            append(version, predictor.work_iterations,
                   prediction.pass_probability, action_risk,
                   prediction.attempt_wall_us + routing.risk_weight * action_risk,
                   reason + "; request-conditioned work budget=" +
                       std::to_string(predictor.work_iterations),
                   backend_chain);
            if (plan.steps.size() == previous_size) continue;
            auto& appended = plan.steps.back();
            appended.cost_relative_uncertainty =
                prediction.cost_relative_uncertainty;
            appended.pass_probability_uncertainty =
                prediction.pass_probability_uncertainty;
            appended.support_extrapolation = prediction.support_extrapolation;
        }
    };
    if (!profile.structured_direct_backend.empty()) {
        append(profile.structured_direct_backend, 0, 1.0, 0.0, unknowns,
               "numeric structure probe admits an exact structured direct solve");
    }
    if (profile.learned_expert_available && profile.learned_expert_resident && many_query) {
        append(
            "learned-tensor-operator-resident-v1", profile.maximum_work_iterations,
            0.97, 0.002,
            unknowns * batch * 0.004 + nonzeros * 0.01 / reuses,
            "resident learned operator amortizes model setup across a many-query batch",
            {"resident-learned-tensor-operator", "cpu-iterative-refinement",
             "original-residual-gate", "original-solver-fallback"});
    }
    if (profile.regular_grid && profile.grid_dimension >= 2 &&
        profile.apple_accelerate_available && profile.numerically_positive_definite) {
        append(
            "accelerate-vdsp-spectral-plan-fp64-v1", 0, 0.9999, 0.0002,
            unknowns * std::max(1.0, std::log2(std::max(2.0, unknowns))) /
                std::min(batch, 10.0),
            "regular SPD grid admits a reusable Apple Accelerate spectral plan",
            {"structured-spectrum-proof", "accelerate-vdsp-persistent-plan",
             "original-residual-gate", "krylov-fallback"});
    }
    if (profile.regular_grid && profile.grid_dimension >= 2 &&
        profile.metal_available && profile.batch_size >= 16) {
        append(
            "metal-gpu-batched-stencil-v1", profile.maximum_work_iterations,
            0.985, 0.0015,
            nonzeros * batch * 0.015,
            "large regular-grid batch amortizes Metal command and unified-memory overhead",
            {"metal-resident-stencil", "cpu-iterative-refinement",
             "original-residual-gate", "cpu-krylov-fallback"});
    }
    if (profile.numerically_positive_definite) {
        if (profile.regular_grid && profile.grid_dimension == 2 && profile.columns >= 16) {
            append_action_family(
                   "pcg-aggregation-amg-cpu-v1", profile.maximum_work_iterations,
                   0.998, 0.00025, nonzeros * 0.8,
                   "square five-point SPD grid admits aggregation AMG-PCG");
        }
        append_action_family("pcg-ic0-cpu-v1", profile.maximum_work_iterations,
               0.995, 0.0003, nonzeros,
               "numeric probe confirms SPD; prefer IC(0)-PCG");
        append_action_family("pcg-jacobi-cpu-v1", profile.maximum_work_iterations,
               0.98, 0.0005, nonzeros * 1.5,
               "low-setup SPD fallback");
    } else {
        if (plan.assessment.scale_class != "large") {
            append_action_family("gmres-ilut-cpu-v1", profile.maximum_work_iterations,
                   0.995, 0.00035, nonzeros * 1.5,
                   "nonsymmetric sparse system; prefer ILUT-GMRES");
        }
        append_action_family("gmres-ilu0-cpu-v1", profile.maximum_work_iterations,
               0.99, 0.0004, nonzeros * 2.0,
               "bounded-memory nonsymmetric Krylov fallback");
    }
    if (industrial_sparse_direct_available()) {
        append_action_family(industrial_sparse_direct_backend(), 0, 0.9995, 0.0008,
               nonzeros * 3.0,
               "independent platform sparse direct fallback with residual gate");
    }
    if (superlu_sparse_direct_available()) {
        append_action_family(superlu_sparse_direct_backend(), 0, 0.9996, 0.0006,
               nonzeros * 2.5,
               "SuperLU sparse direct fallback with original residual gate");
    }
    append_action_family("sparse-ordered-threshold-pivot-cpu-v2", 0, 0.999, 0.001,
           nonzeros * 4.0,
           "terminal built-in sparse direct fallback");
    if (profile.dense_direct_available && plan.assessment.dense_direct_eligible) {
        append_action_family("dense-direct-cpu-v1", 0, 0.9999, 0.0005,
               unknowns * unknowns * unknowns,
               "small or dense square system admits a bounded dense direct fallback");
    }
    if (request_conditioned) {
        const auto alternatives = plan.steps;
        const double terminal_cost = conditioned_terminal_cost;
        plan.steps = optimize_interaction_aware_calibrated_cascade(
            alternatives, routing.conditional_cost_calibrations,
            routing.top_k, terminal_cost,
            routing.maximum_joint_states);
        const auto anchor = routing.request_conditioned_family_anchors.find(numeric_family);
        if (anchor != routing.request_conditioned_family_anchors.end()) {
            const auto locate_anchor = [&](const RouteActionReference& reference) {
                if (reference.expert_version.empty()) return alternatives.end();
                return std::find_if(
                    alternatives.begin(), alternatives.end(), [&](const SolveStep& step) {
                        return step.expert_version == reference.expert_version &&
                            step.budget.work_iterations == reference.work_iterations;
                    });
            };
            const auto conservative_anchor_cost = [&](const auto step) {
                return step == alternatives.end()
                    ? terminal_cost
                    : conservative_expected_cascade_cost(
                        std::vector<SolveStep>{*step}, terminal_cost,
                        routing.conditional_cost_calibrations);
            };

            auto anchor_step = locate_anchor(anchor->second);
            bool terminal_anchor = anchor_step == alternatives.end();
            double anchor_upper = conservative_anchor_cost(anchor_step);
            if (routing.request_conditioned_global_fixed_anchor.has_value()) {
                const auto global_step = locate_anchor(
                    *routing.request_conditioned_global_fixed_anchor);
                const bool terminal_global = global_step == alternatives.end();
                const double global_upper = conservative_anchor_cost(global_step);
                const double severe_joint_support_extrapolation = std::log(4.0);
                const bool anchor_out_of_support = !terminal_anchor &&
                    anchor_step->support_extrapolation >=
                        severe_joint_support_extrapolation;
                const double required_global_upper = global_upper *
                    (1.0 - routing.minimum_family_anchor_gain_fraction);
                if (anchor_out_of_support || !(anchor_upper < required_global_upper)) {
                    anchor_step = global_step;
                    terminal_anchor = terminal_global;
                    anchor_upper = global_upper;
                }
            }
            if (routing.request_conditioned_anchor_only_families.contains(
                    numeric_family)) {
                if (terminal_anchor) {
                    plan.steps.clear();
                } else {
                    auto guarded_anchor = *anchor_step;
                    guarded_anchor.selection_reason +=
                        "; control-aware family anchor retained by calibration gate";
                    plan.steps = {std::move(guarded_anchor)};
                }
                plan.plan_id = plan_id(profile.fingerprint, plan.steps);
                return plan;
            }
            const double conditioned_upper = conservative_expected_cascade_cost(
                plan.steps, terminal_cost,
                routing.conditional_cost_calibrations);
            const double severe_joint_support_extrapolation = std::log(4.0);
            if (!terminal_anchor &&
                anchor_step->support_extrapolation >=
                    severe_joint_support_extrapolation &&
                !(anchor_upper < terminal_cost)) {
                terminal_anchor = true;
                anchor_upper = terminal_cost;
            }
            const double required_upper = anchor_upper *
                (1.0 - routing.minimum_family_anchor_gain_fraction);
            if (!(conditioned_upper < required_upper)) {
                if (terminal_anchor) {
                    plan.steps.clear();
                } else {
                    auto guarded_anchor = *anchor_step;
                    guarded_anchor.selection_reason +=
                        "; family anchor retained by support-and-tail-aware abstention";
                    plan.steps = {std::move(guarded_anchor)};
                }
            }
        }
    } else if (plan.steps.size() > routing.top_k) {
        plan.steps.resize(routing.top_k);
    }
    plan.plan_id = plan_id(profile.fingerprint, plan.steps);
    return plan;
}

EquationAssessment assess_equation(const NonlinearAlgebraicProfile& profile) {
    EquationAssessment assessment;
    assessment.equation_family = profile.smooth
        ? "nonlinear-algebraic-smooth" : "nonlinear-algebraic-nonsmooth";
    assessment.unknown_count = profile.dimension;
    assessment.equation_count = profile.dimension;
    assessment.structural_nonzeros = profile.jacobian_nonzeros;
    assessment.structural_density = profile.dimension == 0 ? 0.0 :
        static_cast<double>(profile.jacobian_nonzeros) /
            static_cast<double>(profile.dimension * profile.dimension);
    assessment.scale_class = profile.dimension <= 8 ? "tiny" :
        (profile.dimension <= 64 ? "small" :
         (profile.dimension <= 1024 ? "medium" : "large"));
    assessment.linear = false;
    assessment.smooth = profile.smooth;
    assessment.structurally_square = profile.dimension != 0;
    assessment.numeric_probe_available = profile.jacobian_available;
    assessment.admissible_backend_roles = {
        BackendRole::nonlinear_solver, BackendRole::fallback};
    assessment.mandatory_fallback = "finite-difference-damped-newton-fallback-v1";
    assessment.reasons.push_back(profile.jacobian_available
        ? "caller Jacobian is a candidate; original residual remains correctness authority"
        : "missing caller Jacobian requires finite-difference damped Newton");
    return assessment;
}

SolvePlan route_nonlinear_algebraic_system(
    const NonlinearAlgebraicProfile& profile,
    const RoutingConfig& routing) {
    if (profile.dimension == 0 || routing.top_k == 0 ||
        !routing.require_original_fallback) {
        throw std::invalid_argument("invalid nonlinear algebraic routing configuration");
    }
    SolvePlan plan;
    plan.block_fingerprint = profile.fingerprint;
    plan.assessment = assess_equation(profile);
    plan.terminal_fallback = plan.assessment.mandatory_fallback;
    const auto append = [&](std::string version, Permission permission, std::string reason) {
        if (!routing.expert_allowlist.empty() &&
            !routing.expert_allowlist.contains(version)) return;
        plan.steps.push_back(SolveStep{
            .expert_version = std::move(version),
            .permission = permission,
            .pass_probability = permission == Permission::direct ? 0.999 : 0.995,
            .risk_score = permission == Permission::direct ? 0.0005 : 0.001,
            .backend_role = BackendRole::nonlinear_solver,
            .backend_chain = {
                "nonlinear-candidate", "damped-line-search",
                "original-nonlinear-residual-gate"},
            .selection_reason = std::move(reason),
            .builtin = true,
        });
    };
    if (profile.jacobian_available) {
        append("callback-jacobian-damped-newton-v1", Permission::corrected,
               "caller Jacobian is eligible only with independent residual acceptance");
    }
    append("finite-difference-damped-newton-fallback-v1", Permission::direct,
           "finite-difference fallback is rebuilt from the original initial state");
    if (plan.steps.size() > routing.top_k) plan.steps.resize(routing.top_k);
    plan.plan_id = plan_id(profile.fingerprint, plan.steps);
    return plan;
}

EquationAssessment assess_equation(const ExplicitOdeProfile& profile) {
    EquationAssessment assessment;
    assessment.equation_family = profile.events
        ? "explicit-ode-with-events" : "explicit-ode-smooth";
    assessment.unknown_count = profile.state_dimension;
    assessment.equation_count = profile.state_dimension;
    assessment.scale_class = profile.state_dimension <= 8 ? "tiny" :
        (profile.state_dimension <= 64 ? "small" :
         (profile.state_dimension <= 1024 ? "medium" : "large"));
    assessment.linear = false;
    assessment.smooth = profile.smooth;
    assessment.event_related = profile.events;
    assessment.structurally_square = profile.state_dimension != 0;
    assessment.admissible_backend_roles = {
        BackendRole::nonlinear_solver, BackendRole::fallback};
    assessment.mandatory_fallback = "adaptive-heun-euler-fallback-v1";
    assessment.reasons.push_back(profile.events
        ? "event callbacks require a separate event-location contract"
        : "embedded local-error control remains the acceptance authority");
    return assessment;
}

SolvePlan route_explicit_ode(
    const ExplicitOdeProfile& profile,
    const RoutingConfig& routing) {
    if (profile.state_dimension == 0 || routing.top_k == 0 ||
        !routing.require_original_fallback) {
        throw std::invalid_argument("invalid explicit ODE routing configuration");
    }
    SolvePlan plan;
    plan.block_fingerprint = profile.fingerprint;
    plan.assessment = assess_equation(profile);
    plan.terminal_fallback = plan.assessment.mandatory_fallback;
    const auto append = [&](std::string version, Permission permission, std::string reason) {
        if (!routing.expert_allowlist.empty() &&
            !routing.expert_allowlist.contains(version)) return;
        plan.steps.push_back(SolveStep{
            .expert_version = std::move(version),
            .permission = permission,
            .pass_probability = permission == Permission::direct ? 0.999 : 0.995,
            .risk_score = permission == Permission::direct ? 0.0005 : 0.001,
            .backend_role = BackendRole::nonlinear_solver,
            .backend_chain = profile.events
                ? std::vector<std::string>{
                    "explicit-ode-step", "embedded-local-error-estimate",
                    "directional-guard-crossing", "bracketed-root-localizer",
                    "priority-ordered-atomic-reset", "original-rhs-guard-finite-gate"}
                : std::vector<std::string>{
                    "explicit-ode-step", "embedded-local-error-estimate",
                    "original-rhs-finite-gate"},
            .selection_reason = std::move(reason),
            .builtin = true,
        });
    };
    append(
        profile.events ? "adaptive-rk4-event-localization-v1"
                       : "adaptive-rk4-step-doubling-v1",
        Permission::direct,
        profile.events
            ? "primary event ODE path uses step doubling, bracketed roots, and atomic reset"
            : "primary explicit ODE path uses step doubling and local-error acceptance");
    append(
        profile.events ? "adaptive-rk4-event-localization-retry-v1"
                       : "adaptive-heun-euler-fallback-v1",
        Permission::direct,
        profile.events
            ? "event fallback restarts from the original initial state with a reduced maximum step"
            : "lower-order embedded fallback restarts from the original initial state");
    if (plan.steps.size() > routing.top_k) plan.steps.resize(routing.top_k);
    plan.plan_id = plan_id(profile.fingerprint, plan.steps);
    return plan;
}

EquationAssessment assess_equation(const FullyImplicitDaeIR& model) {
    model.validate();
    EquationAssessment assessment;
    assessment.unknown_count = model.states.size() + model.algebraics.size();
    assessment.equation_count = model.equations.size();
    assessment.structural_nonzeros = implicit_structural_nonzeros(model);
    assessment.structurally_square = assessment.unknown_count != 0 &&
        assessment.equation_count == assessment.unknown_count;
    assessment.linear = false;
    assessment.event_related = !model.events.empty();
    assessment.smooth = std::none_of(
        model.equations.begin(), model.equations.end(), [](const DaeEquationIR& equation) {
            return equation.residual.find("abs(") != std::string::npos;
        });
    assessment.structurally_symmetric = false;
    assessment.runtime_positive_definite_check_required = true;
    assessment.mandatory_fallback = "fully-implicit-dense-newton-cpu-v1";
    if (assessment.unknown_count != 0) {
        assessment.structural_density =
            static_cast<double>(assessment.structural_nonzeros) /
            static_cast<double>(assessment.unknown_count * assessment.equation_count);
    }
    assessment.scale_class = assessment.unknown_count <= 8
        ? "tiny"
        : (assessment.unknown_count <= 64
            ? "small"
            : (assessment.unknown_count <= 1024 ? "medium" : "large"));
    if (assessment.unknown_count <=
        std::numeric_limits<std::size_t>::max() /
            std::max<std::size_t>(1, assessment.unknown_count) / sizeof(double)) {
        assessment.estimated_dense_bytes = assessment.unknown_count *
            assessment.unknown_count * sizeof(double);
    } else {
        assessment.estimated_dense_bytes = std::numeric_limits<std::size_t>::max();
    }
    constexpr std::size_t sparse_entry_bytes = sizeof(double) + 2 * sizeof(std::size_t);
    assessment.estimated_sparse_bytes = assessment.structural_nonzeros <=
            std::numeric_limits<std::size_t>::max() / sparse_entry_bytes
        ? assessment.structural_nonzeros * sparse_entry_bytes
        : std::numeric_limits<std::size_t>::max();
    assessment.dense_direct_eligible = assessment.structurally_square &&
        assessment.unknown_count <= 64;
    assessment.equation_family = assessment.smooth
        ? "dae-fully-implicit-first-order-smooth"
        : "dae-fully-implicit-first-order-nonsmooth";
    assessment.admissible_backend_roles = {
        BackendRole::initializer, BackendRole::nonlinear_solver,
        BackendRole::linear_solver, BackendRole::preconditioner,
        BackendRole::fallback};
    assessment.forbidden_backend_roles = {BackendRole::operator_candidate};
    assessment.reasons = {
        "fixed-state derivative/algebraic initialization must pass the original residual gate",
        "step and initialization incidence both have structural perfect matchings",
        "colored directional AD permits CSR Newton-Krylov without flattening to an explicit RHS",
        model.events.empty()
            ? "no event partition is present in this residual system"
            : "directional root localization, atomic reinit, and fixed-state derivative/algebraic projection are required",
        "index reduction and learned fully implicit DAE operators remain forbidden",
    };
    return assessment;
}

EquationAssessment assess_equation(const ComplementarityIR& model) {
    model.validate();
    EquationAssessment assessment;
    assessment.equation_family = "strongly-monotone-linear-complementarity";
    assessment.unknown_count = model.variables.size();
    assessment.equation_count = model.variables.size();
    assessment.linear = true;
    assessment.smooth = false;
    assessment.structurally_square = true;
    assessment.structurally_symmetric = true;
    assessment.mandatory_fallback = "enumerated-active-set-terminal-cpu-v1";
    for (const auto& row : model.matrix) {
        assessment.structural_nonzeros += static_cast<std::size_t>(std::count_if(
            row.begin(), row.end(), [](double value) { return value != 0.0; }));
    }
    assessment.structural_density = static_cast<double>(assessment.structural_nonzeros) /
        static_cast<double>(assessment.unknown_count * assessment.unknown_count);
    assessment.scale_class = assessment.unknown_count <= 8
        ? "tiny"
        : (assessment.unknown_count <= 64
            ? "small"
            : (assessment.unknown_count <= 1024 ? "medium" : "large"));
    assessment.estimated_dense_bytes = assessment.unknown_count *
        assessment.unknown_count * sizeof(double);
    assessment.estimated_sparse_bytes = assessment.structural_nonzeros *
        (sizeof(double) + 2 * sizeof(std::size_t));
    assessment.admissible_backend_roles = {
        BackendRole::nonlinear_solver, BackendRole::fallback};
    assessment.forbidden_backend_roles = {
        BackendRole::operator_candidate, BackendRole::preconditioner};
    assessment.reasons = {
        "positive-definite symmetric part proves a unique strongly monotone LCP solution",
        "all candidates remain subject to original gap, inequality, and complementarity gates",
        "enumerated active-set terminal fallback is eligible only through 20 variables",
    };
    return assessment;
}

SolvePlan route_complementarity(
    const ComplementarityIR& model,
    const RoutingConfig& routing) {
    const auto assessment = assess_equation(model);
    SolvePlan plan;
    plan.block_fingerprint = model.source_hash;
    plan.assessment = assessment;
    plan.terminal_fallback = assessment.mandatory_fallback;
    const auto append = [&](const std::string& version,
                            double pass_probability,
                            double risk,
                            double cost,
                            BackendRole role,
                            const std::string& reason) {
        if (!routing.expert_allowlist.empty() &&
            !routing.expert_allowlist.contains(version)) return;
        plan.steps.push_back(SolveStep{
            .expert_version = version,
            .permission = Permission::direct,
            .budget = {},
            .estimated_cost_us = cost,
            .pass_probability = pass_probability,
            .risk_score = risk,
            .backend_role = role,
            .backend_chain = {
                version,
                "original-gap-equation-gate",
                "primal-dual-inequality-gate",
                "complementarity-product-gate",
            },
            .selection_reason = reason,
            .builtin = true,
        });
    };
    const double size = static_cast<double>(assessment.unknown_count);
    const double nonzeros = static_cast<double>(assessment.structural_nonzeros);
    append(
        "projected-gauss-seidel-cpu-v1", 0.995, 0.0008,
        nonzeros * 0.08 + size * 0.1, BackendRole::nonlinear_solver,
        "strong monotonicity and positive diagonal admit projected splitting");
    append(
        "fischer-burmeister-newton-cpu-v1", 0.999, 0.001,
        size * size * size * 0.002, BackendRole::nonlinear_solver,
        "semismooth complementarity reformulation with damped Newton correction");
    if (assessment.unknown_count <= 20) {
        plan.steps.push_back(SolveStep{
            .expert_version = "enumerated-active-set-terminal-cpu-v1",
            .permission = Permission::direct,
            .budget = {},
            .estimated_cost_us =
                std::ldexp(1.0, static_cast<int>(assessment.unknown_count)) * size,
            .pass_probability = 1.0,
            .risk_score = 0.002,
            .backend_role = BackendRole::fallback,
            .backend_chain = {
                "enumerated-active-set-terminal-cpu-v1",
                "original-gap-equation-gate",
                "primal-dual-inequality-gate",
                "complementarity-product-gate",
            },
            .selection_reason =
                "authoritative small-system active-set enumeration terminal fallback",
            .builtin = true,
        });
    }
    plan.plan_id = plan_id(plan.block_fingerprint, plan.steps);
    return plan;
}

EquationAssessment assess_equation(const IndexTwoDaeIR& model) {
    model.validate();
    EquationAssessment assessment;
    assessment.equation_family = "hessenberg-index2-affine-constraint-dae";
    assessment.unknown_count = model.states.size() + model.multipliers.size();
    assessment.equation_count = assessment.unknown_count;
    assessment.structurally_square = true;
    assessment.smooth = true;
    assessment.mandatory_fallback = "index2-dense-kkt-terminal-cpu-v1";
    std::set<std::string> unknowns;
    for (const auto& state : model.states) unknowns.insert(state.name);
    for (const auto& multiplier : model.multipliers) unknowns.insert(multiplier.name);
    for (const auto& source : model.dynamics) {
        const Expression expression(source);
        assessment.structural_nonzeros += static_cast<std::size_t>(std::count_if(
            expression.names().begin(), expression.names().end(),
            [&](const std::string& name) { return unknowns.contains(name); }));
        ++assessment.structural_nonzeros;
    }
    for (const auto& constraint : model.constraints) {
        const Expression expression(constraint.residual);
        assessment.structural_nonzeros += static_cast<std::size_t>(std::count_if(
            expression.names().begin(), expression.names().end(),
            [&](const std::string& name) { return unknowns.contains(name); }));
    }
    assessment.structural_density = static_cast<double>(assessment.structural_nonzeros) /
        static_cast<double>(assessment.unknown_count * assessment.unknown_count);
    assessment.scale_class = assessment.unknown_count <= 8
        ? "tiny"
        : (assessment.unknown_count <= 64
            ? "small"
            : (assessment.unknown_count <= 1024 ? "medium" : "large"));
    assessment.estimated_dense_bytes = assessment.unknown_count *
        assessment.unknown_count * sizeof(double);
    assessment.estimated_sparse_bytes = assessment.structural_nonzeros *
        (sizeof(double) + 2 * sizeof(std::size_t));
    assessment.admissible_backend_roles = {
        BackendRole::initializer, BackendRole::nonlinear_solver,
        BackendRole::fallback};
    assessment.forbidden_backend_roles = {BackendRole::operator_candidate};
    assessment.reasons = {
        "affine state constraints are differentiated exactly once into g_x*f(x,lambda)=0",
        "hidden Jacobian g_x*f_lambda must pass a numerical rank gate before every commit",
        "original dynamics, original constraints, and differentiated hidden constraints remain authoritative",
    };
    return assessment;
}

SolvePlan route_index_two_dae(
    const IndexTwoDaeIR& model,
    const RoutingConfig& routing) {
    SolvePlan plan;
    plan.block_fingerprint = model.source_hash;
    plan.assessment = assess_equation(model);
    plan.terminal_fallback = plan.assessment.mandatory_fallback;
    if (routing.expert_allowlist.empty() || routing.expert_allowlist.contains(
            "index2-differentiated-constraint-newton-cpu-v1")) {
        plan.steps.push_back(SolveStep{
            .expert_version = "index2-differentiated-constraint-newton-cpu-v1",
            .permission = Permission::corrected,
            .budget = {},
            .estimated_cost_us = static_cast<double>(
                plan.assessment.structural_nonzeros) * 0.4,
            .pass_probability = 0.995,
            .risk_score = 0.001,
            .backend_role = BackendRole::nonlinear_solver,
            .backend_chain = {
                "affine-constraint-state-projector",
                "symbolic-first-constraint-differentiation",
                "hidden-jacobian-rank-gate",
                "directional-ad-kkt-newton",
                "original-dynamics-constraint-hidden-residual-gate",
            },
            .selection_reason =
                "Hessenberg index-2 structure admits one exact constraint differentiation",
            .builtin = true,
        });
    }
    plan.steps.push_back(SolveStep{
        .expert_version = "index2-dense-kkt-terminal-cpu-v1",
        .permission = Permission::direct,
        .budget = {},
        .estimated_cost_us = std::pow(
            static_cast<double>(plan.assessment.unknown_count), 3.0) * 0.002,
        .pass_probability = 1.0,
        .risk_score = 0.002,
        .backend_role = BackendRole::fallback,
        .backend_chain = {
            "original-candidate-restart",
            "finite-difference-dense-kkt-newton",
            "hidden-jacobian-rank-gate",
            "original-dynamics-constraint-hidden-residual-gate",
        },
        .selection_reason =
            "mandatory dense KKT fallback restarts from the original candidate",
        .builtin = true,
    });
    plan.plan_id = plan_id(plan.block_fingerprint, plan.steps);
    return plan;
}

SolvePlan route_fully_implicit_dae(
    const FullyImplicitDaeIR& model,
    const RoutingConfig& routing,
    const DaeMultigridArtifact* artifact) {
    if (!routing.require_original_fallback) {
        throw std::invalid_argument("fully implicit Router requires terminal fallback");
    }
    SolvePlan plan;
    plan.block_fingerprint = implicit_fingerprint(model);
    plan.assessment = assess_equation(model);
    plan.terminal_fallback = plan.assessment.mandatory_fallback;
    bool learned_eligible = false;
    if (artifact != nullptr) {
        try {
            artifact->validate();
            learned_eligible = artifact->model_source_hash == model.source_hash &&
                artifact->residual_family == "fully-implicit-first-order-step" &&
                artifact->unknown_count == plan.assessment.unknown_count;
        } catch (const std::exception&) {
            learned_eligible = false;
        }
    }
    if (learned_eligible) {
        plan.block_fingerprint += "-dae-mg-" + artifact->artifact_hash;
    }
    const std::string version = learned_eligible
        ? "fully-implicit-learned-multigrid-pcg-cpu-v1"
        : "fully-implicit-csr-newton-krylov-cpu-v1";
    if (routing.expert_allowlist.empty() || routing.expert_allowlist.contains(version)) {
        const double unknowns = static_cast<double>(plan.assessment.unknown_count);
        const double nonzeros = static_cast<double>(plan.assessment.structural_nonzeros);
        plan.steps.push_back(SolveStep{
            .expert_version = version,
            .permission = Permission::corrected,
            .budget = {},
            .estimated_cost_us = unknowns * 0.3 + nonzeros * 0.65,
            .pass_probability = 0.98,
            .risk_score = 0.0008,
            .backend_role = BackendRole::nonlinear_solver,
            .backend_chain = {
                "fixed-state-derivative-algebraic-initializer",
                "colored-directional-ad-csr-jacobian",
                learned_eligible
                    ? "verified-learned-multigrid-preconditioner"
                    : "classic-ic0-or-ilu0-preconditioner",
                learned_eligible
                    ? "pcg-learned-multigrid-linear-solver"
                    : "pcg-ic0-or-gmres-ilu0-linear-solver",
                "damped-newton-corrector",
                "directional-root-localizer",
                "atomic-reinit-consistency-projector",
                "original-dae-residual-gate",
            },
            .selection_reason =
                std::string(learned_eligible
                    ? "verified learned multigrid artifact matches source, family, and dimension; "
                    : "first-order fully implicit residual with matched initialization/step incidence; ") +
                "scale=" + plan.assessment.scale_class +
                "; density=" + std::to_string(plan.assessment.structural_density),
            .builtin = true,
        });
    }
    if (plan.steps.size() > routing.top_k) plan.steps.resize(routing.top_k);
    plan.plan_id = plan_id(plan.block_fingerprint, plan.steps);
    return plan;
}

std::vector<CandidateExpert> CompileRouter::lookup(
    const BlockIR& block,
    const Registry& registry,
    const RuntimeBundle& bundle) const {
    std::vector<CandidateExpert> candidates;
    const auto assessment = assess_equation(block);
    if (block.event_related || !block.smooth || block.dae_index > 1) return candidates;
    if (block.linear) {
        if (block.unknowns.size() >= 4) {
            candidates.push_back(CandidateExpert{
                .expert_version = "structured-tridiagonal-direct-cpu-v1",
                .permission = Permission::direct,
                .backend_role = BackendRole::linear_solver,
                .selection_reason =
                    "runtime topology probe may prove tridiagonal or cyclic tridiagonal structure",
                .builtin = true,
            });
            candidates.push_back(CandidateExpert{
                .expert_version = "gmres-ilut-cpu-v1",
                .permission = Permission::direct,
                .backend_role = BackendRole::linear_solver,
                .selection_reason = "general sparse nonsymmetric Krylov candidate",
                .builtin = true,
            });
            candidates.push_back(CandidateExpert{
                .expert_version = "gmres-ilu0-cpu-v1",
                .permission = Permission::direct,
                .backend_role = BackendRole::linear_solver,
                .selection_reason = "low-setup sparse nonsymmetric Krylov candidate",
                .builtin = true,
            });
            if (assessment.structurally_symmetric) {
                if (square_five_point_sparsity(block)) {
                    candidates.push_back(CandidateExpert{
                        .expert_version = "pcg-aggregation-amg-cpu-v1",
                        .permission = Permission::direct,
                        .backend_role = BackendRole::linear_solver,
                        .selection_reason =
                            "square five-point sparsity; numerical SPD checked at runtime",
                        .builtin = true,
                    });
                }
                candidates.push_back(CandidateExpert{
                    .expert_version = "pcg-ic0-cpu-v1",
                    .permission = Permission::direct,
                    .backend_role = BackendRole::linear_solver,
                    .selection_reason = "structural symmetry; numerical SPD checked at runtime",
                    .builtin = true,
                });
                candidates.push_back(CandidateExpert{
                    .expert_version = "pcg-jacobi-cpu-v1",
                    .permission = Permission::direct,
                    .backend_role = BackendRole::linear_solver,
                    .selection_reason = "structural symmetry; numerical SPD checked at runtime",
                    .builtin = true,
                });
            }
            if (industrial_sparse_direct_available()) {
                candidates.push_back(CandidateExpert{
                    .expert_version = industrial_sparse_direct_backend(),
                    .permission = Permission::direct,
                    .backend_role = BackendRole::linear_solver,
                    .selection_reason =
                        "platform industrial sparse QR with deterministic rank and original residual gates",
                    .builtin = true,
                });
            }
            if (superlu_sparse_direct_available()) {
                candidates.push_back(CandidateExpert{
                    .expert_version = superlu_sparse_direct_backend(),
                    .permission = Permission::direct,
                    .backend_role = BackendRole::linear_solver,
                    .selection_reason =
                        "SuperLU sparse LU with independent original residual gate",
                    .builtin = true,
                });
            }
            candidates.push_back(CandidateExpert{
                .expert_version = "sparse-ordered-threshold-pivot-cpu-v2",
                .permission = Permission::direct,
                .backend_role = BackendRole::linear_solver,
                .selection_reason = "robust sparse direct cascade",
                .builtin = true,
            });
        }
        if (assessment.dense_direct_eligible) {
            candidates.push_back(CandidateExpert{
                .expert_version = "dense-direct-cpu-v1",
                .permission = Permission::direct,
                .backend_role = BackendRole::linear_solver,
                .selection_reason = "dense direct is eligible for this scale and density",
                .builtin = true,
            });
        }
    } else if (block.smooth && assessment.scale_class == "large") {
        if (assessment.structural_density >= 0.05 ||
            assessment.estimated_sparse_bytes >= 64U * 1024U * 1024U) {
            candidates.push_back(CandidateExpert{
                .expert_version = "newton-krylov-jfnk-cpu-v1",
                .permission = Permission::corrected,
                .backend_role = BackendRole::nonlinear_solver,
                .selection_reason =
                    "large dense/high-memory nonlinear block requires matrix-free Jacobian-vector products",
                .builtin = true,
            });
        }
        candidates.push_back(CandidateExpert{
            .expert_version = "newton-krylov-csr-cpu-v1",
            .permission = Permission::corrected,
            .backend_role = BackendRole::nonlinear_solver,
            .selection_reason =
                "large smooth nonlinear block requires sparse Jacobian and adaptive Krylov",
            .builtin = true,
        });
    }
    for (const auto& version : bundle.expert_versions) {
        const auto& grant = registry.grant(version);
        if (registry.compatible(version, block, bundle, grant.permission)) {
            const auto capability = registry.expert(version).match(block);
            if (capability.backend_roles.empty()) continue;
            candidates.push_back(CandidateExpert{
                .expert_version = version,
                .permission = grant.permission,
                .backend_role = capability.backend_roles.front(),
                .selection_reason = "registry capability and evidence grant are compatible",
                .builtin = false,
            });
        }
    }
    return candidates;
}

std::vector<double> extract_routing_features(
    const std::vector<std::string>& feature_names,
    const BlockIR& block,
    const BlockContext& context) {
    validate_routing_feature_names(feature_names);
    const auto assessment = assess_equation(block);
    std::vector<double> features;
    features.reserve(feature_names.size());
    for (const auto& name : feature_names) {
        if (name == "block:unknown_count") {
            features.push_back(static_cast<double>(assessment.unknown_count));
        } else if (name == "block:equation_count") {
            features.push_back(static_cast<double>(assessment.equation_count));
        } else if (name == "block:structural_nonzeros") {
            features.push_back(static_cast<double>(assessment.structural_nonzeros));
        } else if (name == "block:structural_density") {
            features.push_back(assessment.structural_density);
        } else if (name == "context:ood_score") {
            features.push_back(context.ood_score);
        } else if (name == "context:event_distance") {
            features.push_back(context.event_distance);
        } else if (name == "numeric:diagonal_condition_estimate") {
            if (!context.numeric_probe.available) {
                throw std::invalid_argument(
                    "numeric routing feature requires an available numeric probe");
            }
            features.push_back(context.numeric_probe.diagonal_condition_estimate);
        } else if (name.starts_with("context:")) {
            constexpr std::string_view prefix = "context:";
            const std::string parameter = name.substr(prefix.size());
            const auto value = context.values.find(parameter);
            if (value == context.values.end()) {
                throw std::invalid_argument(
                    "missing request-conditioned context value: " + parameter);
            }
            features.push_back(value->second);
        } else {
            throw std::invalid_argument(
                "routing feature is unavailable for BlockIR context: " + name);
        }
    }
    if (!std::all_of(features.begin(), features.end(), [](double value) {
            return std::isfinite(value);
        })) {
        throw std::invalid_argument("nonfinite request-conditioned routing feature");
    }
    return features;
}

std::vector<double> extract_sparse_routing_features(
    const std::vector<std::string>& feature_names,
    const SparseLinearProfile& profile) {
    validate_routing_feature_names(feature_names);
    if (profile.rows == 0 || profile.columns == 0 || profile.nonzeros == 0 ||
        profile.maximum_work_iterations <= 0 || profile.restart_dimension <= 0 ||
        profile.coefficient_dynamic_range < 1.0 ||
        profile.row_nonzero_coefficient_of_variation < 0.0 ||
        profile.row_l1_condition_estimate < 1.0 ||
        profile.diagonal_dominance_fraction < 0.0 ||
        profile.diagonal_dominance_fraction > 1.0 ||
        profile.mean_diagonal_row_l1_fraction < 0.0 ||
        profile.mean_diagonal_row_l1_fraction > 1.0 ||
        profile.normalized_mean_bandwidth < 0.0 ||
        profile.normalized_mean_bandwidth > 1.0 ||
        !std::isfinite(profile.right_hand_side_inf) ||
        !std::isfinite(profile.right_hand_side_roughness) ||
        !std::isfinite(profile.right_hand_side_sign_change_fraction) ||
        !std::isfinite(profile.absolute_tolerance) ||
        !std::isfinite(profile.relative_tolerance) ||
        profile.absolute_tolerance < 0.0 || profile.relative_tolerance <= 0.0) {
        throw std::invalid_argument("invalid sparse request-conditioned routing profile");
    }
    std::vector<double> features;
    features.reserve(feature_names.size());
    for (const auto& name : feature_names) {
        if (name == "sparse:log_rows") {
            features.push_back(std::log1p(static_cast<double>(profile.rows)));
        } else if (name == "sparse:log_nonzeros") {
            features.push_back(std::log1p(static_cast<double>(profile.nonzeros)));
        } else if (name == "sparse:log_average_row_nonzeros") {
            features.push_back(std::log1p(
                static_cast<double>(profile.nonzeros) /
                static_cast<double>(profile.rows)));
        } else if (name == "sparse:log_diagonal_condition") {
            const double condition = std::isfinite(profile.diagonal_condition_estimate)
                ? std::clamp(profile.diagonal_condition_estimate, 0.0, 1.0e16)
                : 1.0e16;
            features.push_back(std::log1p(condition));
        } else if (name == "sparse:log_coefficient_dynamic_range") {
            const double dynamic_range =
                std::isfinite(profile.coefficient_dynamic_range)
                ? std::clamp(profile.coefficient_dynamic_range, 1.0, 1.0e32)
                : 1.0e32;
            features.push_back(std::log1p(dynamic_range));
        } else if (name == "sparse:row_nonzero_coefficient_of_variation") {
            features.push_back(
                std::isfinite(profile.row_nonzero_coefficient_of_variation)
                ? std::clamp(
                    profile.row_nonzero_coefficient_of_variation, 0.0, 1.0e16)
                : 1.0e16);
        } else if (name == "sparse:log_row_l1_condition") {
            const double row_l1_condition =
                std::isfinite(profile.row_l1_condition_estimate)
                ? std::clamp(profile.row_l1_condition_estimate, 1.0, 1.0e32)
                : 1.0e32;
            features.push_back(std::log1p(row_l1_condition));
        } else if (name == "sparse:diagonal_dominance_fraction") {
            features.push_back(std::isfinite(profile.diagonal_dominance_fraction)
                ? std::clamp(profile.diagonal_dominance_fraction, 0.0, 1.0)
                : 0.0);
        } else if (name == "sparse:mean_diagonal_row_l1_fraction") {
            features.push_back(std::isfinite(profile.mean_diagonal_row_l1_fraction)
                ? std::clamp(profile.mean_diagonal_row_l1_fraction, 0.0, 1.0)
                : 0.0);
        } else if (name == "sparse:normalized_mean_bandwidth") {
            features.push_back(std::isfinite(profile.normalized_mean_bandwidth)
                ? std::clamp(profile.normalized_mean_bandwidth, 0.0, 1.0)
                : 0.0);
        } else if (name == "sparse:structurally_symmetric") {
            features.push_back(profile.structurally_symmetric ? 1.0 : 0.0);
        } else if (name == "sparse:numerically_positive_definite") {
            features.push_back(profile.numerically_positive_definite ? 1.0 : 0.0);
        } else if (name == "sparse:right_hand_side_roughness") {
            features.push_back(profile.right_hand_side_roughness);
        } else if (name == "sparse:right_hand_side_sign_change_fraction") {
            features.push_back(profile.right_hand_side_sign_change_fraction);
        } else if (name == "sparse:requested_digits") {
            features.push_back(-std::log10(std::max(profile.relative_tolerance, 1.0e-16)));
        } else {
            throw std::invalid_argument(
                "routing feature is unavailable for sparse profile: " + name);
        }
    }
    return features;
}

RequestConditionedRoutingModel train_request_conditioned_routing_model(
    const std::vector<std::string>& feature_names,
    const std::vector<RouteActionTrainingSample>& training_samples,
    const std::vector<RouteActionTrainingSample>& calibration_samples,
    double cost_ridge_regularization,
    double pass_logistic_regularization,
    std::size_t maximum_logistic_iterations,
    double maximum_absolute_log_cost_calibration_offset,
    double maximum_absolute_pass_logit_calibration_offset) {
    validate_routing_feature_names(feature_names);
    if (training_samples.empty() || calibration_samples.empty() ||
        !std::isfinite(cost_ridge_regularization) || cost_ridge_regularization <= 0.0 ||
        !std::isfinite(pass_logistic_regularization) ||
        pass_logistic_regularization <= 0.0 || maximum_logistic_iterations == 0 ||
        !std::isfinite(maximum_absolute_log_cost_calibration_offset) ||
        maximum_absolute_log_cost_calibration_offset < 0.0 ||
        !std::isfinite(maximum_absolute_pass_logit_calibration_offset) ||
        maximum_absolute_pass_logit_calibration_offset < 0.0) {
        throw std::invalid_argument("invalid request-conditioned routing training contract");
    }
    const std::size_t dimensions = feature_names.size();
    const auto validate_sample = [&](const RouteActionTrainingSample& sample) {
        if (sample.expert_version.empty() || sample.work_iterations < 0 ||
            sample.independent_group.empty() || sample.routing_family.empty() ||
            sample.features.size() != dimensions ||
            !std::isfinite(sample.attempt_wall_us) || sample.attempt_wall_us <= 0.0 ||
            (sample.cost_relative_to_terminal &&
             (!std::isfinite(sample.terminal_reference_wall_us) ||
              sample.terminal_reference_wall_us <= 0.0)) ||
            !std::all_of(sample.features.begin(), sample.features.end(), [](double value) {
                return std::isfinite(value);
            })) {
            throw std::invalid_argument("invalid request-conditioned routing sample");
        }
    };
    for (const auto& sample : training_samples) validate_sample(sample);
    for (const auto& sample : calibration_samples) validate_sample(sample);

    RequestConditionedRoutingModel model;
    model.feature_names = feature_names;
    model.feature_means.assign(dimensions, 0.0);
    model.feature_scales.assign(dimensions, 0.0);
    for (const auto& sample : training_samples) {
        for (std::size_t index = 0; index < dimensions; ++index) {
            model.feature_means[index] += sample.features[index];
        }
    }
    for (double& mean : model.feature_means) {
        mean /= static_cast<double>(training_samples.size());
    }
    for (const auto& sample : training_samples) {
        for (std::size_t index = 0; index < dimensions; ++index) {
            const double delta = sample.features[index] - model.feature_means[index];
            model.feature_scales[index] += delta * delta;
        }
    }
    for (double& scale : model.feature_scales) {
        scale = std::sqrt(scale / static_cast<double>(training_samples.size()));
        if (scale < 1.0e-12) scale = 1.0;
    }

    using Action = std::pair<std::string, int>;
    std::map<Action, std::vector<const RouteActionTrainingSample*>> training_by_action;
    std::map<Action, std::vector<const RouteActionTrainingSample*>> calibration_by_action;
    for (const auto& sample : training_samples) {
        training_by_action[{sample.expert_version, sample.work_iterations}]
            .push_back(&sample);
    }
    for (const auto& sample : calibration_samples) {
        const Action action{sample.expert_version, sample.work_iterations};
        if (!training_by_action.contains(action)) {
            throw std::invalid_argument("calibration contains an unseen routing action");
        }
        calibration_by_action[action].push_back(&sample);
    }
    if (calibration_by_action.size() != training_by_action.size()) {
        throw std::invalid_argument("request-conditioned action lacks calibration samples");
    }

    const std::size_t coefficient_count = dimensions + 1;
    for (const auto& [action, samples] : training_by_action) {
        if (samples.size() < coefficient_count) {
            throw std::invalid_argument(
                "insufficient request-conditioned samples for action " + action.first);
        }
        std::vector<std::vector<double>> design;
        design.reserve(samples.size());
        for (const auto* sample : samples) {
            design.push_back(normalized_design_row(model, sample->features));
        }
        const bool relative_cost_target = samples.front()->cost_relative_to_terminal;
        if (std::any_of(samples.begin(), samples.end(), [&](const auto* sample) {
                return sample->cost_relative_to_terminal != relative_cost_target;
            }) || std::any_of(
                calibration_by_action.at(action).begin(),
                calibration_by_action.at(action).end(), [&](const auto* sample) {
                    return sample->cost_relative_to_terminal != relative_cost_target;
                })) {
            throw std::invalid_argument(
                "request-conditioned action mixes absolute and relative cost targets");
        }
        std::set<std::string> training_groups;
        std::set<std::string> calibration_groups;
        for (const auto* sample : samples) training_groups.insert(sample->independent_group);
        for (const auto* sample : calibration_by_action.at(action)) {
            calibration_groups.insert(sample->independent_group);
        }

        std::vector<std::vector<double>> normal(
            coefficient_count, std::vector<double>(coefficient_count, 0.0));
        std::vector<double> right_hand_side(coefficient_count, 0.0);
        for (std::size_t sample_index = 0; sample_index < samples.size(); ++sample_index) {
            const double response = std::log(
                relative_cost_target
                ? samples[sample_index]->attempt_wall_us /
                    samples[sample_index]->terminal_reference_wall_us
                : samples[sample_index]->attempt_wall_us);
            for (std::size_t row = 0; row < coefficient_count; ++row) {
                right_hand_side[row] += design[sample_index][row] * response;
                for (std::size_t column = 0; column < coefficient_count; ++column) {
                    normal[row][column] +=
                        design[sample_index][row] * design[sample_index][column];
                }
            }
        }
        for (std::size_t index = 1; index < coefficient_count; ++index) {
            normal[index][index] += cost_ridge_regularization;
        }
        auto log_cost_coefficients = solve_dense_system(normal, right_hand_side);

        std::vector<double> pass_coefficients(coefficient_count, 0.0);
        for (std::size_t iteration = 0; iteration < maximum_logistic_iterations;
             ++iteration) {
            std::vector<std::vector<double>> hessian(
                coefficient_count, std::vector<double>(coefficient_count, 0.0));
            std::vector<double> gradient(coefficient_count, 0.0);
            for (std::size_t sample_index = 0; sample_index < samples.size();
                 ++sample_index) {
                const double probability = logistic(
                    linear_response(pass_coefficients, design[sample_index]));
                const double response = samples[sample_index]->passed ? 1.0 : 0.0;
                const double weight = std::max(
                    probability * (1.0 - probability), 1.0e-9);
                for (std::size_t row = 0; row < coefficient_count; ++row) {
                    gradient[row] += design[sample_index][row] * (probability - response);
                    for (std::size_t column = 0; column < coefficient_count; ++column) {
                        hessian[row][column] += weight * design[sample_index][row] *
                            design[sample_index][column];
                    }
                }
            }
            for (std::size_t index = 0; index < coefficient_count; ++index) {
                hessian[index][index] += pass_logistic_regularization;
                gradient[index] += pass_logistic_regularization * pass_coefficients[index];
            }
            const auto update = solve_dense_system(hessian, gradient);
            double maximum_update = 0.0;
            for (std::size_t index = 0; index < coefficient_count; ++index) {
                pass_coefficients[index] -= update[index];
                maximum_update = std::max(maximum_update, std::abs(update[index]));
            }
            if (maximum_update < 1.0e-10) break;
        }

        std::vector<double> cost_log_residuals;
        std::vector<double> pass_logits;
        double observed_passes = 0.0;
        for (const auto* sample : calibration_by_action.at(action)) {
            const auto row = normalized_design_row(model, sample->features);
            cost_log_residuals.push_back(
                std::log(
                    relative_cost_target
                    ? sample->attempt_wall_us / sample->terminal_reference_wall_us
                    : sample->attempt_wall_us) -
                linear_response(log_cost_coefficients, row));
            pass_logits.push_back(linear_response(pass_coefficients, row));
            observed_passes += sample->passed ? 1.0 : 0.0;
        }
        const double calibration_count =
            static_cast<double>(calibration_by_action.at(action).size());
        const double log_cost_calibration_offset = std::clamp(
            median_value(std::move(cost_log_residuals)),
            -maximum_absolute_log_cost_calibration_offset,
            maximum_absolute_log_cost_calibration_offset);
        const double pass_logit_calibration_offset = std::clamp(
            calibrated_logit_offset(
                pass_logits, observed_passes / calibration_count),
            -maximum_absolute_pass_logit_calibration_offset,
            maximum_absolute_pass_logit_calibration_offset);
        const auto observed_log_cost = [&](const RouteActionTrainingSample& sample) {
            return std::log(
                relative_cost_target
                ? sample.attempt_wall_us / sample.terminal_reference_wall_us
                : sample.attempt_wall_us);
        };
        std::map<std::string, std::vector<const RouteActionTrainingSample*>>
            training_by_family;
        std::map<std::string, std::vector<const RouteActionTrainingSample*>>
            calibration_by_family;
        for (const auto* sample : samples) {
            training_by_family[sample->routing_family].push_back(sample);
        }
        for (const auto* sample : calibration_by_action.at(action)) {
            calibration_by_family[sample->routing_family].push_back(sample);
        }
        std::vector<RouteFamilyPrior> family_priors;
        constexpr std::array<double, 5> regression_weights{
            0.0, 0.25, 0.5, 0.75, 1.0};
        for (const auto& [family, family_training_samples] : training_by_family) {
            const auto family_calibration = calibration_by_family.find(family);
            if (family_calibration == calibration_by_family.end()) continue;
            std::map<std::string, std::vector<double>> training_group_costs;
            std::map<std::string, std::pair<double, std::size_t>> training_group_passes;
            for (const auto* sample : family_training_samples) {
                training_group_costs[sample->independent_group].push_back(
                    observed_log_cost(*sample));
                auto& [passes, count] =
                    training_group_passes[sample->independent_group];
                passes += sample->passed ? 1.0 : 0.0;
                ++count;
            }
            std::vector<double> group_log_costs;
            double pooled_pass_probability{};
            for (auto& [group, values] : training_group_costs) {
                group_log_costs.push_back(median_value(std::move(values)));
                const auto& [passes, count] = training_group_passes.at(group);
                pooled_pass_probability += passes / static_cast<double>(count);
            }
            const double pooled_log_cost = median_value(std::move(group_log_costs));
            pooled_pass_probability /=
                static_cast<double>(training_group_passes.size());
            double best_cost_weight = 0.0;
            double best_cost_objective = std::numeric_limits<double>::infinity();
            double best_pass_weight = 0.0;
            double best_pass_objective = std::numeric_limits<double>::infinity();
            for (const double weight : regression_weights) {
                std::map<std::string, std::vector<double>> cost_errors_by_group;
                std::map<std::string, std::pair<double, std::size_t>>
                    pass_errors_by_group;
                for (const auto* sample : family_calibration->second) {
                    const auto row = normalized_design_row(model, sample->features);
                    const double regression_log_cost = std::clamp(
                        linear_response(log_cost_coefficients, row) +
                            log_cost_calibration_offset,
                        -40.0, 40.0);
                    const double blended_log_cost = weight * regression_log_cost +
                        (1.0 - weight) * pooled_log_cost;
                    cost_errors_by_group[sample->independent_group].push_back(
                        std::abs(blended_log_cost - observed_log_cost(*sample)));
                    const double regression_pass = logistic(
                        linear_response(pass_coefficients, row) +
                        pass_logit_calibration_offset);
                    const double blended_pass = weight * regression_pass +
                        (1.0 - weight) * pooled_pass_probability;
                    auto& [error, count] =
                        pass_errors_by_group[sample->independent_group];
                    error += std::abs(
                        blended_pass - (sample->passed ? 1.0 : 0.0));
                    ++count;
                }
                std::vector<double> group_cost_errors;
                std::vector<double> group_pass_errors;
                for (auto& [group, values] : cost_errors_by_group) {
                    (void)group;
                    group_cost_errors.push_back(median_value(std::move(values)));
                }
                for (const auto& [group, error_count] : pass_errors_by_group) {
                    (void)group;
                    group_pass_errors.push_back(
                        error_count.first /
                        static_cast<double>(error_count.second));
                }
                const double cost_objective = upper_empirical_quantile(
                    group_cost_errors, 0.95);
                const double pass_objective = upper_empirical_quantile(
                    group_pass_errors, 0.95);
                constexpr double objective_tolerance = 1.0e-12;
                if (cost_objective + objective_tolerance < best_cost_objective) {
                    best_cost_objective = cost_objective;
                    best_cost_weight = weight;
                }
                if (pass_objective + objective_tolerance < best_pass_objective) {
                    best_pass_objective = pass_objective;
                    best_pass_weight = weight;
                }
            }
            std::set<std::string> family_calibration_groups;
            for (const auto* sample : family_calibration->second) {
                family_calibration_groups.insert(sample->independent_group);
            }
            family_priors.push_back(RouteFamilyPrior{
                .routing_family = family,
                .independent_training_groups = training_group_passes.size(),
                .independent_calibration_groups = family_calibration_groups.size(),
                .pooled_log_cost = pooled_log_cost,
                .pooled_pass_probability = pooled_pass_probability,
                .cost_regression_weight = best_cost_weight,
                .pass_regression_weight = best_pass_weight,
                .cost_calibration_upper_error = std::min(
                    1.0e6, std::expm1(std::min(best_cost_objective,
                                               std::log(1.0e6 + 1.0)))),
                .pass_calibration_upper_error = std::min(
                    1.0, best_pass_objective +
                        std::sqrt(std::log(40.0) /
                            (2.0 * static_cast<double>(
                                family_calibration_groups.size())))),
            });
        }
        std::vector<double> cost_errors;
        double predicted_passes{};
        for (const auto* sample : calibration_by_action.at(action)) {
            const auto row = normalized_design_row(model, sample->features);
            const double predicted_cost = std::exp(std::clamp(
                linear_response(log_cost_coefficients, row) +
                    log_cost_calibration_offset,
                -40.0, 40.0));
            const double observed_cost = relative_cost_target
                ? sample->attempt_wall_us / sample->terminal_reference_wall_us
                : sample->attempt_wall_us;
            cost_errors.push_back(
                std::abs(predicted_cost - observed_cost) / observed_cost);
            const double predicted_pass = logistic(
                linear_response(pass_coefficients, row) +
                pass_logit_calibration_offset);
            predicted_passes += predicted_pass;
        }
        std::vector<double> support_minimums(
            dimensions, std::numeric_limits<double>::infinity());
        std::vector<double> support_maximums(
            dimensions, -std::numeric_limits<double>::infinity());
        const auto update_support = [&](const RouteActionTrainingSample& sample) {
            const auto row = normalized_design_row(model, sample.features);
            for (std::size_t index = 0; index < dimensions; ++index) {
                support_minimums[index] = std::min(
                    support_minimums[index], row[index + 1]);
                support_maximums[index] = std::max(
                    support_maximums[index], row[index + 1]);
            }
        };
        for (const auto* sample : samples) update_support(*sample);
        for (const auto* sample : calibration_by_action.at(action)) {
            update_support(*sample);
        }
        using GroupRows = std::map<std::string, std::vector<std::vector<double>>>;
        GroupRows training_group_rows;
        GroupRows calibration_group_rows;
        for (const auto* sample : samples) {
            const auto row = normalized_design_row(model, sample->features);
            training_group_rows[sample->independent_group].emplace_back(
                row.begin() + 1, row.end());
        }
        for (const auto* sample : calibration_by_action.at(action)) {
            const auto row = normalized_design_row(model, sample->features);
            calibration_group_rows[sample->independent_group].emplace_back(
                row.begin() + 1, row.end());
        }
        std::vector<std::size_t> joint_support_feature_indices;
        for (std::size_t feature = 0; feature < dimensions; ++feature) {
            bool group_invariant = true;
            const auto inspect_groups = [&](const GroupRows& groups) {
                for (const auto& [group, rows] : groups) {
                    (void)group;
                    double minimum = std::numeric_limits<double>::infinity();
                    double maximum = -std::numeric_limits<double>::infinity();
                    for (const auto& row : rows) {
                        minimum = std::min(minimum, row[feature]);
                        maximum = std::max(maximum, row[feature]);
                    }
                    const double scale = std::max(
                        {1.0, std::abs(minimum), std::abs(maximum)});
                    if (maximum - minimum > 1.0e-9 * scale) return false;
                }
                return true;
            };
            group_invariant = inspect_groups(training_group_rows) &&
                inspect_groups(calibration_group_rows);
            if (group_invariant) joint_support_feature_indices.push_back(feature);
        }
        const auto group_centers = [&](const GroupRows& groups) {
            std::vector<std::vector<double>> centers;
            centers.reserve(groups.size());
            for (const auto& [group, rows] : groups) {
                (void)group;
                std::vector<double> center(
                    joint_support_feature_indices.size(), 0.0);
                for (const auto& row : rows) {
                    for (std::size_t index = 0;
                         index < joint_support_feature_indices.size(); ++index) {
                        center[index] += row[joint_support_feature_indices[index]];
                    }
                }
                for (double& value : center) {
                    value /= static_cast<double>(rows.size());
                }
                centers.push_back(std::move(center));
            }
            return centers;
        };
        auto joint_support_training_centers = group_centers(training_group_rows);
        auto joint_support_calibration_centers = group_centers(calibration_group_rows);
        double joint_support_nearest_distance_upper{};
        if (!joint_support_feature_indices.empty()) {
            std::vector<double> calibration_distances;
            calibration_distances.reserve(joint_support_calibration_centers.size());
            for (const auto& calibration_center :
                 joint_support_calibration_centers) {
                double nearest = std::numeric_limits<double>::infinity();
                for (const auto& training_center : joint_support_training_centers) {
                    double squared_distance{};
                    for (std::size_t index = 0;
                         index < calibration_center.size(); ++index) {
                        const double delta =
                            calibration_center[index] - training_center[index];
                        squared_distance += delta * delta;
                    }
                    nearest = std::min(
                        nearest, std::sqrt(
                            squared_distance /
                            static_cast<double>(calibration_center.size())));
                }
                calibration_distances.push_back(nearest);
            }
            joint_support_nearest_distance_upper = upper_empirical_quantile(
                calibration_distances, 0.95);
            joint_support_training_centers.insert(
                joint_support_training_centers.end(),
                joint_support_calibration_centers.begin(),
                joint_support_calibration_centers.end());
        } else {
            joint_support_training_centers.clear();
        }
        const double cost_calibration_error = median_value(cost_errors);
        const double cost_calibration_upper_error =
            upper_empirical_quantile(cost_errors, 0.95);
        const double pass_calibration_error = std::abs(
            predicted_passes / calibration_count - observed_passes / calibration_count);
        const double pass_calibration_upper_error = std::min(
            1.0, pass_calibration_error +
                std::sqrt(std::log(40.0) /
                    (2.0 * static_cast<double>(calibration_groups.size()))));
        model.actions[action.first].push_back(RouteActionPredictor{
            .work_iterations = action.second,
            .training_samples = samples.size(),
            .independent_training_groups = training_groups.size(),
            .independent_calibration_groups = calibration_groups.size(),
            .cost_relative_to_terminal = relative_cost_target,
            .log_cost_coefficients = std::move(log_cost_coefficients),
            .pass_logit_coefficients = std::move(pass_coefficients),
            .log_cost_calibration_offset = log_cost_calibration_offset,
            .pass_logit_calibration_offset = pass_logit_calibration_offset,
            .cost_calibration_error = cost_calibration_error,
            .pass_calibration_error = pass_calibration_error,
            .cost_calibration_upper_error = cost_calibration_upper_error,
            .pass_calibration_upper_error = pass_calibration_upper_error,
            .support_feature_minimums = std::move(support_minimums),
            .support_feature_maximums = std::move(support_maximums),
            .joint_support_feature_indices =
                std::move(joint_support_feature_indices),
            .joint_support_group_centers =
                std::move(joint_support_training_centers),
            .joint_support_nearest_distance_upper =
                joint_support_nearest_distance_upper,
            .family_priors = std::move(family_priors),
        });
    }
    validate_request_conditioned_model(model);
    return model;
}

RouteActionPrediction predict_request_conditioned_action(
    const RequestConditionedRoutingModel& model,
    const std::string& expert_version,
    int work_iterations,
    const std::vector<double>& features,
    double terminal_reference_wall_us,
    const std::string& routing_family) {
    validate_request_conditioned_model(model);
    const auto expert = model.actions.find(expert_version);
    if (expert == model.actions.end()) {
        throw std::invalid_argument("request-conditioned expert is not modeled");
    }
    const auto predictor = std::find_if(
        expert->second.begin(), expert->second.end(), [&](const auto& candidate) {
            return candidate.work_iterations == work_iterations;
        });
    if (predictor == expert->second.end()) {
        throw std::invalid_argument("request-conditioned expert budget is not modeled");
    }
    if (predictor->cost_relative_to_terminal &&
        (!std::isfinite(terminal_reference_wall_us) ||
         terminal_reference_wall_us <= 0.0)) {
        throw std::invalid_argument(
            "relative request-conditioned cost requires terminal reference");
    }
    const auto row = normalized_design_row(model, features);
    double log_cost = std::clamp(
        linear_response(predictor->log_cost_coefficients, row) +
            predictor->log_cost_calibration_offset,
        -40.0, 40.0);
    double pass_probability = logistic(
        linear_response(predictor->pass_logit_coefficients, row) +
        predictor->pass_logit_calibration_offset);
    double support_extrapolation{};
    if (!predictor->support_feature_minimums.empty()) {
        double squared_extrapolation{};
        for (std::size_t index = 0; index < model.feature_names.size(); ++index) {
            const double below = predictor->support_feature_minimums[index] - row[index + 1];
            const double above = row[index + 1] - predictor->support_feature_maximums[index];
            const double extrapolation = std::max({0.0, below, above});
            squared_extrapolation += extrapolation * extrapolation;
        }
        support_extrapolation = std::sqrt(squared_extrapolation);
    }
    if (!predictor->joint_support_feature_indices.empty()) {
        double nearest = std::numeric_limits<double>::infinity();
        for (const auto& center : predictor->joint_support_group_centers) {
            double squared_distance{};
            for (std::size_t index = 0;
                 index < predictor->joint_support_feature_indices.size(); ++index) {
                const double delta = row[
                    predictor->joint_support_feature_indices[index] + 1] - center[index];
                squared_distance += delta * delta;
            }
            nearest = std::min(
                nearest, std::sqrt(
                    squared_distance /
                    static_cast<double>(
                        predictor->joint_support_feature_indices.size())));
        }
        support_extrapolation = std::max(
            support_extrapolation,
            std::max(
                0.0, nearest - predictor->joint_support_nearest_distance_upper));
    }
    double cost_uncertainty = std::max(
        predictor->cost_calibration_error,
        predictor->cost_calibration_upper_error);
    double pass_uncertainty = std::max(
        predictor->pass_calibration_error,
        predictor->pass_calibration_upper_error);
    if (!routing_family.empty()) {
        const auto prior = std::find_if(
            predictor->family_priors.begin(), predictor->family_priors.end(),
            [&](const auto& candidate) {
                return candidate.routing_family == routing_family;
            });
        if (prior != predictor->family_priors.end()) {
            log_cost = prior->cost_regression_weight * log_cost +
                (1.0 - prior->cost_regression_weight) * prior->pooled_log_cost;
            pass_probability = prior->pass_regression_weight * pass_probability +
                (1.0 - prior->pass_regression_weight) *
                    prior->pooled_pass_probability;
            cost_uncertainty = std::max(
                cost_uncertainty, prior->cost_calibration_upper_error);
            pass_uncertainty = std::max(
                pass_uncertainty, prior->pass_calibration_upper_error);
        }
    }
    return RouteActionPrediction{
        .attempt_wall_us = std::exp(log_cost) *
            (predictor->cost_relative_to_terminal ? terminal_reference_wall_us : 1.0),
        .pass_probability = pass_probability,
        .risk_score = std::max({
            cost_uncertainty, pass_uncertainty, support_extrapolation}),
        .cost_relative_uncertainty = cost_uncertainty,
        .pass_probability_uncertainty = pass_uncertainty,
        .support_extrapolation = support_extrapolation,
    };
}

RuntimeRouter::RuntimeRouter(RoutingConfig config) : config_(config) {
    if (config_.top_k == 0) throw std::invalid_argument("routing top_k must be positive");
    if (!config_.require_original_fallback) {
        throw std::invalid_argument("original fallback is a mandatory routing invariant");
    }
    if (!std::isfinite(config_.minimum_family_anchor_gain_fraction) ||
        config_.minimum_family_anchor_gain_fraction < 0.0 ||
        config_.minimum_family_anchor_gain_fraction >= 1.0) {
        throw std::invalid_argument("invalid family-anchor gain fraction");
    }
    if (config_.maximum_joint_states == 0) {
        throw std::invalid_argument("joint routing state limit must be positive");
    }
    bool has_joint_calibration = false;
    for (const auto& [expert, calibration] : config_.calibrations) {
        if (calibration.work_iterations < 0) {
            throw std::invalid_argument(
                "negative calibrated correction budget for expert " + expert);
        }
        std::set<int> budgets;
        for (const auto& option : calibration.budget_options) {
            has_joint_calibration = true;
            if (option.work_iterations < 0 || option.attempts == 0 ||
                option.passes + option.fallbacks + option.failures != option.attempts ||
                option.failures != 0 || option.erroneous_accepts != 0 ||
                !std::isfinite(option.pass_probability) ||
                option.pass_probability < 0.0 || option.pass_probability > 1.0 ||
                !std::isfinite(option.calibration_error) ||
                option.calibration_error < 0.0 || option.calibration_error > 1.0 ||
                !std::isfinite(option.median_attempt_wall_us) ||
                option.median_attempt_wall_us < 0.0 ||
                !budgets.insert(option.work_iterations).second) {
                throw std::invalid_argument(
                    "invalid joint budget calibration for expert " + expert);
            }
        }
    }
    if (config_.request_conditioned_model.has_value()) {
        validate_request_conditioned_model(*config_.request_conditioned_model);
        has_joint_calibration = true;
    }
    for (const auto& [family, anchor] :
         config_.request_conditioned_family_anchors) {
        if ((family != "spd" && family != "symmetric-indefinite" &&
             family != "nonsymmetric") ||
            anchor.work_iterations < 0 ||
            (anchor.expert_version.empty() && anchor.work_iterations != 0)) {
            throw std::invalid_argument(
                "invalid request-conditioned family anchor: " + family);
        }
    }
    if (!config_.request_conditioned_family_anchors.empty() &&
        !config_.request_conditioned_model.has_value()) {
        throw std::invalid_argument(
            "request-conditioned family anchors require a conditioned model");
    }
    if (config_.request_conditioned_global_fixed_anchor.has_value() &&
        !config_.request_conditioned_model.has_value()) {
        throw std::invalid_argument(
            "request-conditioned global fixed anchor requires a conditioned model");
    }
    for (const auto& family : config_.request_conditioned_anchor_only_families) {
        if (!config_.request_conditioned_family_anchors.contains(family)) {
            throw std::invalid_argument(
                "anchor-only routing family lacks a conditioned anchor: " + family);
        }
    }
    if (config_.request_conditioned_model.has_value()) {
        for (const auto& [family, anchor] :
             config_.request_conditioned_family_anchors) {
            if (anchor.expert_version.empty()) continue;
            const auto expert = config_.request_conditioned_model->actions.find(
                anchor.expert_version);
            const bool modeled = expert != config_.request_conditioned_model->actions.end() &&
                std::any_of(
                    expert->second.begin(), expert->second.end(), [&](const auto& predictor) {
                        return predictor.work_iterations == anchor.work_iterations;
                    });
            if (!modeled) {
                throw std::invalid_argument(
                    "request-conditioned family anchor is not modeled: " + family);
            }
        }
        if (config_.request_conditioned_global_fixed_anchor.has_value()) {
            const auto& anchor = *config_.request_conditioned_global_fixed_anchor;
            if (anchor.work_iterations < 0 ||
                (anchor.expert_version.empty() && anchor.work_iterations != 0)) {
                throw std::invalid_argument(
                    "invalid request-conditioned global fixed anchor");
            }
            if (!anchor.expert_version.empty()) {
                const auto expert = config_.request_conditioned_model->actions.find(
                    anchor.expert_version);
                const bool modeled =
                    expert != config_.request_conditioned_model->actions.end() &&
                    std::any_of(
                        expert->second.begin(), expert->second.end(),
                        [&](const auto& predictor) {
                            return predictor.work_iterations == anchor.work_iterations;
                        });
                if (!modeled) {
                    throw std::invalid_argument(
                        "request-conditioned global fixed anchor is not modeled");
                }
            }
        }
    }
    if (has_joint_calibration &&
        (!std::isfinite(config_.calibrated_terminal_fallback_cost_us) ||
         config_.calibrated_terminal_fallback_cost_us < 0.0)) {
        throw std::invalid_argument("invalid calibrated terminal fallback cost");
    }
}

double cascade_ordering_index(const SolveStep& step) {
    if (!std::isfinite(step.estimated_cost_us) || step.estimated_cost_us < 0.0 ||
        !std::isfinite(step.pass_probability) || step.pass_probability <= 0.0 ||
        step.pass_probability > 1.0) {
        throw std::invalid_argument("invalid solve step for cascade ordering");
    }
    return step.estimated_cost_us / step.pass_probability;
}

double expected_cascade_cost(
    const std::vector<SolveStep>& steps, double terminal_cost_us) {
    if (!std::isfinite(terminal_cost_us) || terminal_cost_us < 0.0) {
        throw std::invalid_argument("invalid terminal cost for cascade evaluation");
    }
    double reach_probability = 1.0;
    double expected_cost = 0.0;
    for (const auto& step : steps) {
        (void)cascade_ordering_index(step);
        expected_cost += reach_probability * step.estimated_cost_us;
        reach_probability *= 1.0 - step.pass_probability;
    }
    return expected_cost + reach_probability * terminal_cost_us;
}

double expected_interaction_aware_cascade_cost(
    const std::vector<SolveStep>& steps,
    double terminal_cost_us,
    const std::vector<RouteConditionalCostCalibration>& conditional_cost_calibrations) {
    if (!std::isfinite(terminal_cost_us) || terminal_cost_us < 0.0) {
        throw std::invalid_argument(
            "invalid terminal cost for interaction-aware cascade evaluation");
    }
    using ActionKey = std::pair<std::string, int>;
    using TransitionKey = std::pair<ActionKey, ActionKey>;
    std::map<TransitionKey, double> multipliers;
    for (const auto& calibration : conditional_cost_calibrations) {
        const ActionKey previous{
            calibration.previous.expert_version,
            calibration.previous.work_iterations};
        const ActionKey next{
            calibration.next.expert_version,
            calibration.next.work_iterations};
        if (previous.first.empty() || next.first.empty() || previous.second < 0 ||
            next.second < 0 || calibration.independent_training_groups == 0 ||
            calibration.independent_calibration_groups == 0 ||
            !std::isfinite(calibration.conditional_cost_multiplier) ||
            calibration.conditional_cost_multiplier <= 0.0 ||
            !std::isfinite(calibration.conditional_cost_multiplier_upper) ||
            calibration.conditional_cost_multiplier_upper <
                calibration.conditional_cost_multiplier ||
            !multipliers.emplace(
                TransitionKey{previous, next},
                calibration.conditional_cost_multiplier_upper).second) {
            throw std::invalid_argument(
                "invalid conditional routing cost calibration");
        }
    }
    double reach_probability = 1.0;
    double expected_cost = 0.0;
    std::optional<ActionKey> previous;
    for (const auto& step : steps) {
        (void)cascade_ordering_index(step);
        const ActionKey current{
            step.expert_version, step.budget.work_iterations};
        double multiplier = 1.0;
        if (previous.has_value()) {
            const auto found = multipliers.find({*previous, current});
            if (found != multipliers.end()) multiplier = found->second;
        }
        expected_cost += reach_probability * step.estimated_cost_us * multiplier;
        reach_probability *= 1.0 - step.pass_probability;
        previous = current;
    }
    return expected_cost + reach_probability * terminal_cost_us;
}

void order_cascade_steps(std::vector<SolveStep>& steps) {
    std::stable_sort(
        steps.begin(), steps.end(), [](const SolveStep& left, const SolveStep& right) {
            const double left_index = cascade_ordering_index(left);
            const double right_index = cascade_ordering_index(right);
            if (left_index != right_index) return left_index < right_index;
            if (left.risk_score != right.risk_score) return left.risk_score < right.risk_score;
            return left.estimated_cost_us < right.estimated_cost_us;
        });
}

std::vector<SolveStep> optimize_joint_calibrated_cascade(
    const std::vector<SolveStep>& alternatives,
    std::size_t top_k,
    double terminal_cost_us,
    std::size_t maximum_states,
    CascadeOptimizationDiagnostics* diagnostics) {
    if (diagnostics != nullptr) *diagnostics = {};
    if (top_k == 0) throw std::invalid_argument("joint routing top_k must be positive");
    if (!std::isfinite(terminal_cost_us) || terminal_cost_us < 0.0) {
        throw std::invalid_argument("invalid joint routing terminal cost");
    }
    if (maximum_states == 0) {
        throw std::invalid_argument("joint routing state limit must be positive");
    }
    if (alternatives.empty()) return {};

    std::vector<SolveStep> ordered = alternatives;
    for (const auto& alternative : ordered) {
        if (alternative.expert_version.empty() ||
            alternative.budget.work_iterations < 0) {
            throw std::invalid_argument("invalid joint routing alternative");
        }
        (void)cascade_ordering_index(alternative);
    }
    order_cascade_steps(ordered);

    std::map<std::string, std::size_t> expert_bits;
    std::set<std::pair<std::string, int>> actions;
    for (const auto& alternative : ordered) {
        if (!actions.emplace(
                alternative.expert_version,
                alternative.budget.work_iterations).second) {
            throw std::invalid_argument("duplicate expert-budget routing alternative");
        }
        if (!expert_bits.contains(alternative.expert_version)) {
            if (expert_bits.size() >= 63) {
                throw std::invalid_argument(
                    "joint routing supports at most 63 calibrated experts");
            }
            expert_bits.emplace(alternative.expert_version, expert_bits.size());
        }
    }

    struct Solution {
        double expected_cost{};
        std::vector<SolveStep> steps;
    };
    using State = std::tuple<std::size_t, std::uint64_t, std::size_t>;
    std::map<State, Solution> memo;
    std::size_t visited_states = 0;
    const auto better = [](const Solution& left, const Solution& right) {
        constexpr double tolerance = 1.0e-12;
        if (left.expected_cost + tolerance < right.expected_cost) return true;
        if (right.expected_cost + tolerance < left.expected_cost) return false;
        if (left.steps.size() != right.steps.size()) {
            return left.steps.size() < right.steps.size();
        }
        for (std::size_t index = 0; index < left.steps.size(); ++index) {
            const auto left_key = std::tie(
                left.steps[index].expert_version,
                left.steps[index].budget.work_iterations);
            const auto right_key = std::tie(
                right.steps[index].expert_version,
                right.steps[index].budget.work_iterations);
            if (left_key != right_key) return left_key < right_key;
        }
        return false;
    };
    std::function<Solution(std::size_t, std::uint64_t, std::size_t)> solve =
        [&](std::size_t index, std::uint64_t used_experts, std::size_t selected) {
            const State state{index, used_experts, selected};
            if (const auto found = memo.find(state); found != memo.end()) {
                if (diagnostics != nullptr) ++diagnostics->memo_hits;
                return found->second;
            }
            if (++visited_states > maximum_states) {
                if (diagnostics != nullptr) {
                    diagnostics->visited_states = visited_states;
                    diagnostics->state_limit_exceeded = true;
                }
                throw std::invalid_argument("joint routing state limit exceeded");
            }
            if (diagnostics != nullptr) diagnostics->visited_states = visited_states;
            if (index == ordered.size() || selected == top_k) {
                Solution terminal{.expected_cost = terminal_cost_us};
                if (diagnostics != nullptr) ++diagnostics->terminal_states;
                memo.emplace(state, terminal);
                return terminal;
            }
            if (diagnostics != nullptr) ++diagnostics->recursive_transitions;
            Solution best = solve(index + 1, used_experts, selected);
            const auto& action = ordered[index];
            const std::uint64_t expert_bit =
                std::uint64_t{1} << expert_bits.at(action.expert_version);
            if ((used_experts & expert_bit) == 0) {
                if (diagnostics != nullptr) ++diagnostics->recursive_transitions;
                Solution tail = solve(
                    index + 1, used_experts | expert_bit, selected + 1);
                Solution take{
                    .expected_cost = action.estimated_cost_us +
                        (1.0 - action.pass_probability) * tail.expected_cost,
                    .steps = std::move(tail.steps),
                };
                take.steps.insert(take.steps.begin(), action);
                if (better(take, best)) best = std::move(take);
            }
            memo.emplace(state, best);
            return best;
        };
    auto result = solve(0, 0, 0).steps;
    if (diagnostics != nullptr) diagnostics->estimated_states = visited_states;
    return result;
}

std::vector<SolveStep> optimize_interaction_aware_calibrated_cascade(
    const std::vector<SolveStep>& alternatives,
    const std::vector<RouteConditionalCostCalibration>& conditional_cost_calibrations,
    std::size_t top_k,
    double terminal_cost_us,
    std::size_t maximum_states,
    CascadeOptimizationDiagnostics* diagnostics) {
    if (diagnostics != nullptr) *diagnostics = {};
    if (top_k == 0) throw std::invalid_argument("joint routing top_k must be positive");
    if (!std::isfinite(terminal_cost_us) || terminal_cost_us < 0.0) {
        throw std::invalid_argument("invalid joint routing terminal cost");
    }
    if (maximum_states == 0) {
        throw std::invalid_argument("joint routing state limit must be positive");
    }
    if (conditional_cost_calibrations.empty()) {
        return optimize_joint_calibrated_cascade(
            alternatives, top_k, terminal_cost_us, maximum_states, diagnostics);
    }
    if (alternatives.empty()) return {};

    std::vector<SolveStep> ordered = alternatives;
    for (const auto& alternative : ordered) {
        if (alternative.expert_version.empty() ||
            alternative.budget.work_iterations < 0) {
            throw std::invalid_argument("invalid joint routing alternative");
        }
        (void)cascade_ordering_index(alternative);
    }
    std::stable_sort(
        ordered.begin(), ordered.end(), [](const SolveStep& left, const SolveStep& right) {
            return std::tie(left.expert_version, left.budget.work_iterations) <
                std::tie(right.expert_version, right.budget.work_iterations);
        });

    std::map<std::string, std::size_t> expert_bits;
    std::set<std::pair<std::string, int>> actions;
    for (const auto& alternative : ordered) {
        if (!actions.emplace(
                alternative.expert_version,
                alternative.budget.work_iterations).second) {
            throw std::invalid_argument("duplicate expert-budget routing alternative");
        }
        if (!expert_bits.contains(alternative.expert_version)) {
            if (expert_bits.size() >= 63) {
                throw std::invalid_argument(
                    "joint routing supports at most 63 calibrated experts");
            }
            expert_bits.emplace(alternative.expert_version, expert_bits.size());
        }
    }

    using ActionKey = std::pair<std::string, int>;
    using TransitionKey = std::pair<ActionKey, ActionKey>;
    std::map<TransitionKey, double> conditional_multipliers;
    for (const auto& calibration : conditional_cost_calibrations) {
        const ActionKey previous{
            calibration.previous.expert_version,
            calibration.previous.work_iterations};
        const ActionKey next{
            calibration.next.expert_version,
            calibration.next.work_iterations};
        if (previous.first.empty() || next.first.empty() || previous.second < 0 ||
            next.second < 0 || calibration.independent_training_groups == 0 ||
            calibration.independent_calibration_groups == 0 ||
            !std::isfinite(calibration.conditional_cost_multiplier) ||
            calibration.conditional_cost_multiplier <= 0.0 ||
            !std::isfinite(calibration.conditional_cost_multiplier_upper) ||
            calibration.conditional_cost_multiplier_upper <
                calibration.conditional_cost_multiplier ||
            !conditional_multipliers.emplace(
                TransitionKey{previous, next},
                calibration.conditional_cost_multiplier_upper).second) {
            throw std::invalid_argument(
                "invalid conditional routing cost calibration");
        }
    }

    const auto saturated_add = [](std::size_t left, std::size_t right) {
        const auto maximum = std::numeric_limits<std::size_t>::max();
        return right > maximum - left ? maximum : left + right;
    };
    const auto saturated_multiply = [](std::size_t left, std::size_t right) {
        const auto maximum = std::numeric_limits<std::size_t>::max();
        return left != 0 && right > maximum / left ? maximum : left * right;
    };
    const std::size_t selected_limit = std::min(top_k, expert_bits.size());
    std::vector<std::size_t> subset_counts(selected_limit + 1);
    std::vector<std::size_t> previous_action_counts(selected_limit + 1);
    subset_counts[0] = 1;
    std::map<std::string, std::size_t> actions_per_expert;
    for (const auto& action : ordered) ++actions_per_expert[action.expert_version];
    std::size_t processed_experts = 0;
    for (const auto& [expert, action_count] : actions_per_expert) {
        (void)expert;
        ++processed_experts;
        const std::size_t upper = std::min(selected_limit, processed_experts);
        for (std::size_t selected = upper; selected > 0; --selected) {
            const std::size_t added_previous_actions = saturated_add(
                previous_action_counts[selected - 1],
                saturated_multiply(action_count, subset_counts[selected - 1]));
            previous_action_counts[selected] = saturated_add(
                previous_action_counts[selected], added_previous_actions);
            subset_counts[selected] = saturated_add(
                subset_counts[selected], subset_counts[selected - 1]);
        }
    }
    std::size_t estimated_states = 1;
    for (std::size_t selected = 1; selected <= selected_limit; ++selected) {
        estimated_states = saturated_add(
            estimated_states, previous_action_counts[selected]);
    }
    if (diagnostics != nullptr) diagnostics->estimated_states = estimated_states;
    if (estimated_states > maximum_states) {
        if (diagnostics != nullptr) diagnostics->state_limit_exceeded = true;
        throw std::invalid_argument("joint routing state limit exceeded");
    }

    struct Solution {
        double expected_cost{};
        std::vector<SolveStep> steps;
    };
    const std::size_t no_previous = ordered.size();
    using State = std::tuple<std::uint64_t, std::size_t, std::size_t>;
    std::map<State, Solution> memo;
    std::size_t visited_states = 0;

    const auto better = [](const Solution& left, const Solution& right) {
        constexpr double tolerance = 1.0e-12;
        if (left.expected_cost + tolerance < right.expected_cost) return true;
        if (right.expected_cost + tolerance < left.expected_cost) return false;
        if (left.steps.size() != right.steps.size()) {
            return left.steps.size() < right.steps.size();
        }
        for (std::size_t index = 0; index < left.steps.size(); ++index) {
            const auto left_key = std::tie(
                left.steps[index].expert_version,
                left.steps[index].budget.work_iterations);
            const auto right_key = std::tie(
                right.steps[index].expert_version,
                right.steps[index].budget.work_iterations);
            if (left_key != right_key) return left_key < right_key;
        }
        return false;
    };

    std::function<Solution(std::uint64_t, std::size_t, std::size_t)> solve =
        [&](std::uint64_t used_experts, std::size_t previous_index,
            std::size_t selected) {
            const State state{used_experts, previous_index, selected};
            if (const auto found = memo.find(state); found != memo.end()) {
                if (diagnostics != nullptr) ++diagnostics->memo_hits;
                return found->second;
            }
            if (++visited_states > maximum_states) {
                if (diagnostics != nullptr) {
                    diagnostics->visited_states = visited_states;
                    diagnostics->state_limit_exceeded = true;
                }
                throw std::invalid_argument("joint routing state limit exceeded");
            }
            if (diagnostics != nullptr) diagnostics->visited_states = visited_states;
            if (selected == top_k) {
                Solution terminal{.expected_cost = terminal_cost_us};
                if (diagnostics != nullptr) ++diagnostics->terminal_states;
                memo.emplace(state, terminal);
                return terminal;
            }

            Solution best{.expected_cost = terminal_cost_us};
            for (std::size_t action_index = 0; action_index < ordered.size();
                 ++action_index) {
                const auto& action = ordered[action_index];
                const std::uint64_t expert_bit =
                    std::uint64_t{1} << expert_bits.at(action.expert_version);
                if ((used_experts & expert_bit) != 0) continue;
                if (diagnostics != nullptr) ++diagnostics->recursive_transitions;
                double conditional_multiplier = 1.0;
                if (previous_index != no_previous) {
                    const auto& previous = ordered[previous_index];
                    const auto found = conditional_multipliers.find({
                        ActionKey{previous.expert_version,
                                  previous.budget.work_iterations},
                        ActionKey{action.expert_version,
                                  action.budget.work_iterations}});
                    if (found != conditional_multipliers.end()) {
                        conditional_multiplier = found->second;
                    }
                }
                Solution tail = solve(
                    used_experts | expert_bit, action_index, selected + 1);
                auto selected_action = action;
                if (conditional_multiplier != 1.0) {
                    selected_action.selection_reason +=
                        "; conditional-cost upper multiplier=" +
                        std::to_string(conditional_multiplier);
                }
                Solution take{
                    .expected_cost = action.estimated_cost_us * conditional_multiplier +
                        (1.0 - action.pass_probability) * tail.expected_cost,
                    .steps = std::move(tail.steps),
                };
                take.steps.insert(take.steps.begin(), std::move(selected_action));
                if (better(take, best)) best = std::move(take);
            }
            memo.emplace(state, best);
            return best;
        };

    auto result = solve(0, no_previous, 0).steps;
    if (diagnostics != nullptr && diagnostics->visited_states != estimated_states) {
        throw std::logic_error(
            "interaction-aware routing state estimate disagrees with recurrence");
    }
    return result;
}

SolvePlan RuntimeRouter::route(
    const BlockIR& block,
    const BlockContext& context,
    const std::vector<CandidateExpert>& candidates,
    const Registry& registry,
    const RuntimeBundle& bundle) const {
    SolvePlan plan;
    plan.block_fingerprint = block.fingerprint;
    plan.assessment = assess_equation(block);
    plan.assessment.numeric_probe_available = context.numeric_probe.available;
    plan.assessment.numerically_symmetric = context.numeric_probe.symmetric;
    plan.assessment.numerically_positive_definite =
        context.numeric_probe.positive_definite;
    plan.assessment.diagonal_condition_estimate =
        context.numeric_probe.diagonal_condition_estimate;
    plan.terminal_fallback = bundle.terminal_fallback;
    const bool calibrated = !config_.calibration_block_fingerprint.empty() &&
        config_.calibration_block_fingerprint == block.fingerprint;
    const bool joint_calibrated = calibrated && std::any_of(
        config_.calibrations.begin(), config_.calibrations.end(),
        [](const auto& item) { return !item.second.budget_options.empty(); });
    const bool request_conditioned = config_.request_conditioned_model.has_value();
    const bool joint_optimized = joint_calibrated || request_conditioned;
    const std::vector<double> request_features = request_conditioned
        ? extract_routing_features(
            config_.request_conditioned_model->feature_names, block, context)
        : std::vector<double>{};
    if (calibrated && config_.calibration_winner == bundle.terminal_fallback) {
        plan.plan_id = plan_id(block, plan.steps);
        return plan;
    }
    for (const auto& candidate : candidates) {
        if (!config_.expert_allowlist.empty() &&
            !config_.expert_allowlist.contains(candidate.expert_version)) continue;
        if (context.numeric_probe.available) {
            const bool pcg = candidate.expert_version == "pcg-aggregation-amg-cpu-v1" ||
                candidate.expert_version == "pcg-ic0-cpu-v1" ||
                candidate.expert_version == "pcg-jacobi-cpu-v1";
            const bool gmres = candidate.expert_version == "gmres-ilut-cpu-v1" ||
                candidate.expert_version == "gmres-ilu0-cpu-v1";
            if (pcg && !context.numeric_probe.positive_definite) continue;
            if (gmres && context.numeric_probe.symmetric) continue;
            if (candidate.expert_version == "gmres-ilut-cpu-v1" &&
                plan.assessment.scale_class == "large") continue;
            if (candidate.backend_role == BackendRole::preconditioner &&
                block.linear && !context.numeric_probe.positive_definite) continue;
        }
        std::string selection_reason = candidate.selection_reason +
            "; scale=" + plan.assessment.scale_class +
            "; density=" + std::to_string(plan.assessment.structural_density);
        if (context.numeric_probe.available && block.linear) {
            selection_reason += context.numeric_probe.positive_definite
                ? "; numeric probe confirms SPD"
                : (context.numeric_probe.symmetric
                    ? "; numeric probe confirms symmetric non-SPD"
                    : "; numeric probe confirms nonsymmetric matrix");
        }
        Estimate estimate;
        if (candidate.builtin) {
            const double size = static_cast<double>(std::max<std::size_t>(1, block.unknowns.size()));
            const double nonzeros = static_cast<double>(std::max<std::size_t>(
                block.unknowns.size(), plan.assessment.structural_nonzeros));
            if (candidate.expert_version == "gmres-ilut-cpu-v1") {
                estimate = Estimate{
                    .pass_probability = 0.995,
                    .expected_setup_time_us = nonzeros * 0.12 + size * 0.2,
                    .expected_solve_time_us = nonzeros * 0.08,
                    .expected_correction_time_us = 0.0,
                    .failure_cost_us = 50.0,
                    .risk_score = 0.00035,
                    .ood_score = 0.0,
                };
            } else if (candidate.expert_version == "gmres-ilu0-cpu-v1") {
                estimate = Estimate{
                    .pass_probability = 0.99,
                    .expected_setup_time_us = nonzeros * 0.07 + size * 0.15,
                    .expected_solve_time_us = nonzeros * 0.11,
                    .expected_correction_time_us = 0.0,
                    .failure_cost_us = 50.0,
                    .risk_score = 0.0004,
                    .ood_score = 0.0,
                };
            } else if (candidate.expert_version == "pcg-aggregation-amg-cpu-v1") {
                estimate = Estimate{
                    .pass_probability = 0.998,
                    .expected_setup_time_us = nonzeros * 0.1 + size * 0.12,
                    .expected_solve_time_us = nonzeros * 0.035,
                    .expected_correction_time_us = 0.0,
                    .failure_cost_us = 50.0,
                    .risk_score = 0.00025,
                    .ood_score = 0.0,
                };
            } else if (candidate.expert_version == "pcg-ic0-cpu-v1") {
                estimate = Estimate{
                    .pass_probability = 0.995,
                    .expected_setup_time_us = nonzeros * 0.08 + size * 0.1,
                    .expected_solve_time_us = nonzeros * 0.05,
                    .expected_correction_time_us = 0.0,
                    .failure_cost_us = 50.0,
                    .risk_score = 0.0003,
                    .ood_score = 0.0,
                };
            } else if (candidate.expert_version ==
                       "structured-tridiagonal-direct-cpu-v1") {
                estimate = Estimate{
                    .pass_probability = 0.9999,
                    .expected_setup_time_us = size * 0.003,
                    .expected_solve_time_us = size * 0.006,
                    .expected_correction_time_us = 0.0,
                    .failure_cost_us = size * 0.002,
                    .risk_score = 0.0001,
                    .ood_score = 0.0,
                };
            } else if (candidate.expert_version == "pcg-jacobi-cpu-v1") {
                estimate = Estimate{
                    .pass_probability = 0.98,
                    .expected_setup_time_us = size * 0.02,
                    .expected_solve_time_us = nonzeros * 0.12,
                    .expected_correction_time_us = 0.0,
                    .failure_cost_us = 50.0,
                    .risk_score = 0.0005,
                    .ood_score = 0.0,
                };
            } else if (candidate.expert_version == "newton-krylov-csr-cpu-v1") {
                estimate = Estimate{
                    .pass_probability = 0.99,
                    .expected_setup_time_us = nonzeros * 0.15,
                    .expected_solve_time_us = nonzeros * 0.5,
                    .expected_correction_time_us = nonzeros * 0.2,
                    .failure_cost_us = nonzeros * 0.1,
                    .risk_score = 0.0008,
                    .ood_score = 0.0,
                };
            } else if (candidate.expert_version == "newton-krylov-jfnk-cpu-v1") {
                estimate = Estimate{
                    .pass_probability = 0.98,
                    .expected_setup_time_us = size * 0.05,
                    .expected_solve_time_us = nonzeros * 0.25,
                    .expected_correction_time_us = size * 0.2,
                    .failure_cost_us = size * 0.1,
                    .risk_score = 0.0007,
                    .ood_score = 0.0,
                };
            } else if (candidate.expert_version ==
                       "accelerate-sparse-qr-cpu-v1") {
                estimate = Estimate{
                    .pass_probability = 0.9995,
                    .expected_setup_time_us =
                        nonzeros * std::log2(size + 1.0) * 0.055,
                    .expected_solve_time_us = nonzeros * 0.025,
                    .expected_correction_time_us = 0.0,
                    .failure_cost_us = 50.0,
                    .risk_score = 0.0008,
                    .ood_score = 0.0,
                };
            } else if (candidate.expert_version == "superlu-dgssv-cpu-v1") {
                estimate = Estimate{
                    .pass_probability = 0.9996,
                    .expected_setup_time_us =
                        nonzeros * std::log2(size + 1.0) * 0.05,
                    .expected_solve_time_us = nonzeros * 0.02,
                    .expected_correction_time_us = 0.0,
                    .failure_cost_us = 40.0,
                    .risk_score = 0.0006,
                    .ood_score = 0.0,
                };
            } else if (candidate.expert_version ==
                       "sparse-ordered-threshold-pivot-cpu-v2") {
                estimate = Estimate{
                    .pass_probability = 0.999,
                    .expected_setup_time_us = nonzeros * std::log2(size + 1.0) * 0.08,
                    .expected_solve_time_us = nonzeros *
                        (1.0 + plan.assessment.structural_density * size) * 0.05,
                    .expected_correction_time_us = 0.0,
                    .failure_cost_us = 50.0,
                    .risk_score = 0.001,
                    .ood_score = 0.0,
                };
            } else {
                estimate = Estimate{
                    .pass_probability = 0.999,
                    .expected_setup_time_us = size * size * 0.01,
                    .expected_solve_time_us = size * size * size * 0.002,
                    .expected_correction_time_us = 0.0,
                    .failure_cost_us = 50.0,
                    .risk_score = 0.001,
                    .ood_score = 0.0,
                };
            }
        } else {
            if (!registry.compatible(
                    candidate.expert_version, block, bundle, candidate.permission)) continue;
            estimate = registry.expert(candidate.expert_version).estimate(block, context);
        }
        SolveBudget budget;
        if (request_conditioned) {
            const auto modeled_actions =
                config_.request_conditioned_model->actions.find(candidate.expert_version);
            if (modeled_actions == config_.request_conditioned_model->actions.end()) {
                continue;
            }
            for (const auto& predictor : modeled_actions->second) {
                const auto prediction = predict_request_conditioned_action(
                    *config_.request_conditioned_model,
                    candidate.expert_version,
                    predictor.work_iterations,
                    request_features);
                if (prediction.pass_probability <= 0.0 ||
                    prediction.pass_probability < config_.minimum_pass_probability) {
                    continue;
                }
                const double risk = std::max(
                    estimate.risk_score, prediction.risk_score) + estimate.ood_score;
                plan.steps.push_back(SolveStep{
                    .expert_version = candidate.expert_version,
                    .permission = candidate.permission,
                    .budget = SolveBudget{
                        .work_iterations = predictor.work_iterations},
                    .estimated_cost_us = prediction.attempt_wall_us +
                        config_.risk_weight * risk,
                    .pass_probability = prediction.pass_probability,
                    .risk_score = risk,
                    .backend_role = candidate.backend_role,
                    .backend_chain = backend_chain(candidate),
                    .selection_reason = selection_reason +
                        "; request-conditioned correction budget=" +
                        std::to_string(predictor.work_iterations),
                    .builtin = candidate.builtin,
                });
            }
            continue;
        }
        if (calibrated && !config_.calibrations.empty()) {
            const auto calibration = config_.calibrations.find(candidate.expert_version);
            if (calibration == config_.calibrations.end()) {
                continue;
            }
            if (!calibration->second.budget_options.empty()) {
                for (const auto& option : calibration->second.budget_options) {
                    if (option.pass_probability <= 0.0 ||
                        option.pass_probability < config_.minimum_pass_probability) {
                        continue;
                    }
                    const double risk = std::max(
                        estimate.risk_score, option.calibration_error) +
                        estimate.ood_score;
                    const double cost = option.median_attempt_wall_us +
                        config_.risk_weight * risk;
                    plan.steps.push_back(SolveStep{
                        .expert_version = candidate.expert_version,
                        .permission = candidate.permission,
                        .budget = SolveBudget{
                            .work_iterations = option.work_iterations},
                        .estimated_cost_us = cost,
                        .pass_probability = option.pass_probability,
                        .risk_score = risk,
                        .backend_role = candidate.backend_role,
                        .backend_chain = backend_chain(candidate),
                        .selection_reason = selection_reason +
                            "; joint calibrated correction budget=" +
                            std::to_string(option.work_iterations),
                        .builtin = candidate.builtin,
                    });
                }
                continue;
            }
            if (calibration->second.attempts == 0 ||
                calibration->second.failures != 0 ||
                calibration->second.fallbacks != 0 ||
                calibration->second.passes != calibration->second.attempts) {
                continue;
            }
            estimate.pass_probability = calibration->second.pass_probability;
            estimate.expected_setup_time_us = 0.0;
            estimate.expected_solve_time_us = calibration->second.median_wall_us;
            estimate.expected_correction_time_us = 0.0;
            estimate.risk_score = std::max(
                estimate.risk_score, calibration->second.calibration_error);
            budget.work_iterations = calibration->second.work_iterations;
            selection_reason += "; calibrated correction budget=" +
                std::to_string(budget.work_iterations);
        }
        if (estimate.pass_probability <= 0.0 ||
            estimate.pass_probability < config_.minimum_pass_probability) continue;
        const double cost = estimate.expected_setup_time_us +
            estimate.expected_solve_time_us + estimate.expected_correction_time_us +
            (1.0 - estimate.pass_probability) * estimate.failure_cost_us +
            config_.risk_weight * (estimate.risk_score + estimate.ood_score);
        plan.steps.push_back(SolveStep{
            .expert_version = candidate.expert_version,
            .permission = candidate.permission,
            .budget = budget,
            .estimated_cost_us = cost,
            .pass_probability = estimate.pass_probability,
            .risk_score = estimate.risk_score + estimate.ood_score,
            .backend_role = candidate.backend_role,
            .backend_chain = backend_chain(candidate),
            .selection_reason = std::move(selection_reason),
            .builtin = candidate.builtin,
        });
    }
    if (joint_optimized) {
        plan.steps = optimize_interaction_aware_calibrated_cascade(
            plan.steps,
            config_.conditional_cost_calibrations,
            config_.top_k,
            config_.calibrated_terminal_fallback_cost_us,
            config_.maximum_joint_states);
        plan.plan_id = plan_id(block, plan.steps);
        return plan;
    }
    order_cascade_steps(plan.steps);
    std::vector<SolveStep> direct_fallbacks;
    for (const std::string version : {
             "accelerate-sparse-qr-cpu-v1",
             "superlu-dgssv-cpu-v1",
             "sparse-ordered-threshold-pivot-cpu-v2",
             "dense-direct-cpu-v1"}) {
        const auto fallback = std::find_if(
            plan.steps.begin(), plan.steps.end(), [&](const SolveStep& step) {
                return step.builtin && step.expert_version == version;
            });
        if (fallback != plan.steps.end()) {
            direct_fallbacks.push_back(*fallback);
            plan.steps.erase(fallback);
        }
    }
    if (plan.steps.size() > config_.top_k) plan.steps.resize(config_.top_k);
    plan.steps.insert(plan.steps.end(), direct_fallbacks.begin(), direct_fallbacks.end());
    plan.plan_id = plan_id(block, plan.steps);
    return plan;
}

void write_equation_assessment_report(
    const ModelIR& model,
    const BlockIR& block,
    const BlockContext& context,
    const Registry& registry,
    const RuntimeBundle& bundle,
    const RoutingConfig& routing,
    const std::filesystem::path& path) {
    if (std::find_if(
            model.blocks.begin(), model.blocks.end(), [&](const BlockIR& candidate) {
                return candidate.id == block.id && candidate.fingerprint == block.fingerprint;
            }) == model.blocks.end()) {
        throw std::invalid_argument("assessment block does not belong to the model");
    }
    registry.validate_bundle(bundle, model);
    BlockContext routed_context = context;
    if (block.linear) {
        std::unordered_map<std::string, Expression> residuals;
        for (const auto& equation : model.equations) {
            residuals.emplace(equation.id, Expression(equation.residual));
        }
        const auto system = assemble_linear_system(
            model, block, residuals, routed_context.values);
        routed_context.numeric_probe = BlockContext::NumericProbe{
            .available = true,
            .symmetric = system.symmetric,
            .positive_definite = system.positive_definite,
            .diagonal_condition_estimate = system.diagonal_condition_estimate,
        };
    }
    const auto candidates = CompileRouter{}.lookup(block, registry, bundle);
    const auto plan = RuntimeRouter(routing).route(
        block, routed_context, candidates, registry, bundle);
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write equation assessment report");
    output << std::setprecision(17)
           << "SMAVE_EQUATION_ASSESSMENT 2\n"
           << "MODEL " << std::quoted(model.model_id) << '\n'
           << "SOURCE_HASH " << std::quoted(model.source_hash) << '\n'
           << "BLOCK " << std::quoted(block.id) << '\n'
           << "FINGERPRINT " << std::quoted(block.fingerprint) << '\n'
           << "FAMILY " << std::quoted(plan.assessment.equation_family) << '\n'
           << "UNKNOWNS " << plan.assessment.unknown_count << '\n'
           << "EQUATIONS " << plan.assessment.equation_count << '\n'
           << "STRUCTURAL_NONZEROS " << plan.assessment.structural_nonzeros << '\n'
           << "STRUCTURAL_DENSITY " << plan.assessment.structural_density << '\n'
           << "SCALE_CLASS " << std::quoted(plan.assessment.scale_class) << '\n'
           << "ESTIMATED_DENSE_BYTES " << plan.assessment.estimated_dense_bytes << '\n'
           << "ESTIMATED_SPARSE_BYTES " << plan.assessment.estimated_sparse_bytes << '\n'
           << "DENSE_DIRECT_ELIGIBLE " << plan.assessment.dense_direct_eligible << '\n'
           << "LINEAR " << plan.assessment.linear << '\n'
           << "SMOOTH " << plan.assessment.smooth << '\n'
           << "EVENT_RELATED " << plan.assessment.event_related << '\n'
           << "STRUCTURALLY_SQUARE " << plan.assessment.structurally_square << '\n'
           << "STRUCTURALLY_SYMMETRIC "
           << plan.assessment.structurally_symmetric << '\n'
           << "RUNTIME_SPD_GATE_REQUIRED "
           << plan.assessment.runtime_positive_definite_check_required << '\n'
           << "NUMERIC_PROBE_AVAILABLE "
           << plan.assessment.numeric_probe_available << '\n'
           << "NUMERICALLY_SYMMETRIC "
           << plan.assessment.numerically_symmetric << '\n'
           << "NUMERICALLY_POSITIVE_DEFINITE "
           << plan.assessment.numerically_positive_definite << '\n'
           << "DIAGONAL_CONDITION_ESTIMATE "
           << plan.assessment.diagonal_condition_estimate << '\n';
    for (const auto role : plan.assessment.admissible_backend_roles) {
        output << "ADMISSIBLE_ROLE " << std::quoted(to_string(role)) << '\n';
    }
    for (const auto role : plan.assessment.forbidden_backend_roles) {
        output << "FORBIDDEN_ROLE " << std::quoted(to_string(role)) << '\n';
    }
    for (const auto& reason : plan.assessment.reasons) {
        output << "ASSESSMENT_REASON " << std::quoted(reason) << '\n';
    }
    output << "PLAN_ID " << std::quoted(plan.plan_id) << '\n'
           << "PLAN_STEPS " << plan.steps.size() << '\n';
    for (std::size_t index = 0; index < plan.steps.size(); ++index) {
        const auto& step = plan.steps[index];
        output << "PLAN_STEP " << index
               << " EXPERT " << std::quoted(step.expert_version)
               << " ROLE " << std::quoted(to_string(step.backend_role))
               << " PERMISSION " << std::quoted(to_string(step.permission))
               << " ESTIMATED_COST_US " << step.estimated_cost_us
               << " PASS_PROBABILITY " << step.pass_probability
               << " RISK_SCORE " << step.risk_score
               << " WORK_ITERATIONS " << step.budget.work_iterations
               << " BUILTIN " << step.builtin
               << " REASON " << std::quoted(step.selection_reason) << '\n';
        for (std::size_t stage = 0; stage < step.backend_chain.size(); ++stage) {
            output << "BACKEND_CHAIN " << index << ' ' << stage << ' '
                   << std::quoted(step.backend_chain[stage]) << '\n';
        }
    }
    output << "TERMINAL_FALLBACK " << std::quoted(plan.terminal_fallback) << '\n'
           << "END\n";
}

void write_equation_assessment_report(
    const FullyImplicitDaeIR& model,
    const RoutingConfig& routing,
    const std::filesystem::path& path,
    const DaeMultigridArtifact* artifact) {
    const auto plan = route_fully_implicit_dae(model, routing, artifact);
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("cannot write fully implicit equation assessment report");
    }
    output << std::setprecision(17)
           << "SMAVE_EQUATION_ASSESSMENT 1\n"
           << "MODEL " << std::quoted(model.model_id) << '\n'
           << "SOURCE_HASH " << std::quoted(model.source_hash) << '\n'
           << "BLOCK " << std::quoted("fully-implicit-system") << '\n'
           << "FINGERPRINT " << std::quoted(plan.block_fingerprint) << '\n'
           << "FAMILY " << std::quoted(plan.assessment.equation_family) << '\n'
           << "UNKNOWNS " << plan.assessment.unknown_count << '\n'
           << "EQUATIONS " << plan.assessment.equation_count << '\n'
           << "STRUCTURAL_NONZEROS " << plan.assessment.structural_nonzeros << '\n'
           << "STRUCTURAL_DENSITY " << plan.assessment.structural_density << '\n'
           << "SCALE_CLASS " << std::quoted(plan.assessment.scale_class) << '\n'
           << "ESTIMATED_DENSE_BYTES " << plan.assessment.estimated_dense_bytes << '\n'
           << "ESTIMATED_SPARSE_BYTES " << plan.assessment.estimated_sparse_bytes << '\n'
           << "DENSE_DIRECT_ELIGIBLE " << plan.assessment.dense_direct_eligible << '\n'
           << "LINEAR " << plan.assessment.linear << '\n'
           << "SMOOTH " << plan.assessment.smooth << '\n'
           << "EVENT_RELATED " << plan.assessment.event_related << '\n'
           << "STRUCTURALLY_SQUARE " << plan.assessment.structurally_square << '\n'
           << "STRUCTURALLY_SYMMETRIC " << plan.assessment.structurally_symmetric << '\n'
           << "RUNTIME_SPD_GATE_REQUIRED "
           << plan.assessment.runtime_positive_definite_check_required << '\n';
    for (const auto role : plan.assessment.admissible_backend_roles) {
        output << "ADMISSIBLE_ROLE " << std::quoted(to_string(role)) << '\n';
    }
    for (const auto role : plan.assessment.forbidden_backend_roles) {
        output << "FORBIDDEN_ROLE " << std::quoted(to_string(role)) << '\n';
    }
    for (const auto& reason : plan.assessment.reasons) {
        output << "ASSESSMENT_REASON " << std::quoted(reason) << '\n';
    }
    output << "PLAN_ID " << std::quoted(plan.plan_id) << '\n'
           << "PLAN_STEPS " << plan.steps.size() << '\n';
    for (std::size_t index = 0; index < plan.steps.size(); ++index) {
        const auto& step = plan.steps[index];
        output << "PLAN_STEP " << index
               << " EXPERT " << std::quoted(step.expert_version)
               << " ROLE " << std::quoted(to_string(step.backend_role))
               << " PERMISSION " << std::quoted(to_string(step.permission))
               << " ESTIMATED_COST_US " << step.estimated_cost_us
               << " PASS_PROBABILITY " << step.pass_probability
               << " RISK_SCORE " << step.risk_score
               << " BUILTIN " << step.builtin
               << " REASON " << std::quoted(step.selection_reason) << '\n';
        for (std::size_t stage = 0; stage < step.backend_chain.size(); ++stage) {
            output << "BACKEND_CHAIN " << index << ' ' << stage << ' '
                   << std::quoted(step.backend_chain[stage]) << '\n';
        }
    }
    output << "TERMINAL_FALLBACK " << std::quoted(plan.terminal_fallback) << '\n'
           << "END\n";
}

}  // namespace smave
