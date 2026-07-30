#include "smave/incremental_gate.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace smave {

ExperimentalExactCandidateGatePolicy::ExperimentalExactCandidateGatePolicy(
    std::size_t strict_verification_period)
    : strict_verification_period_(strict_verification_period) {
    if (strict_verification_period_ < 2) {
        throw std::invalid_argument(
            "exact-candidate gate strict verification period must be at least two");
    }
}

bool ExperimentalExactCandidateGatePolicy::identical_values(
    const std::unordered_map<std::string, double>& left,
    const std::unordered_map<std::string, double>& right) {
    if (left.size() != right.size()) return false;
    for (const auto& [name, value] : left) {
        const auto other = right.find(name);
        if (other == right.end() ||
            std::bit_cast<std::uint64_t>(value) != std::bit_cast<std::uint64_t>(other->second)) {
            return false;
        }
    }
    return true;
}

bool ExperimentalExactCandidateGatePolicy::identical_gate(
    const GateResult& left,
    const GateResult& right) {
    if (left.decision != right.decision || left.reason != right.reason ||
        std::bit_cast<std::uint64_t>(left.residual_inf) !=
            std::bit_cast<std::uint64_t>(right.residual_inf) ||
        left.scaled_residuals.size() != right.scaled_residuals.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.scaled_residuals.size(); ++index) {
        if (std::bit_cast<std::uint64_t>(left.scaled_residuals[index]) !=
            std::bit_cast<std::uint64_t>(right.scaled_residuals[index])) {
            return false;
        }
    }
    return true;
}

GateResult ExperimentalExactCandidateGatePolicy::evaluate(
    const Runtime& runtime,
    const BlockIR& block,
    const std::unordered_map<std::string, double>& values,
    bool direct_permission) {
    std::scoped_lock lock(mutex_);
    ++counters_.requests;
    auto certificate = certificates_.find(block.id);
    const auto runtime_identity = runtime.solve_gate_identity();
    const bool exact_repeat = certificate != certificates_.end() &&
        certificate->second.runtime_identity == runtime_identity &&
        certificate->second.direct_permission == direct_permission &&
        identical_values(certificate->second.values, values);
    if (exact_repeat) {
        if (certificate->second.reuses_since_strict + 1 < strict_verification_period_) {
            ++certificate->second.reuses_since_strict;
            ++counters_.certificate_reuses;
            return certificate->second.gate;
        }
        ++counters_.strict_verifications;
        ++counters_.periodic_strict_verifications;
        const GateResult observed = runtime.evaluate_gate(block, values, direct_permission);
        if (!identical_gate(certificate->second.gate, observed)) {
            certificates_.erase(certificate);
            ++counters_.periodic_mismatches;
            ++counters_.revocations;
            return {.reason = "exact-candidate gate periodic verification drift"};
        }
        certificate->second.gate = observed;
        certificate->second.reuses_since_strict = 0;
        return observed;
    }

    ++counters_.strict_verifications;
    const GateResult observed = runtime.evaluate_gate(block, values, direct_permission);
    if (observed.decision == GateDecision::direct_accept) {
        certificates_.insert_or_assign(block.id, Certificate{
            .runtime_identity = runtime_identity,
            .values = values,
            .direct_permission = direct_permission,
            .gate = observed,
            .reuses_since_strict = 0,
        });
    } else if (certificate != certificates_.end()) {
        certificates_.erase(certificate);
        ++counters_.revocations;
    }
    return observed;
}

void ExperimentalExactCandidateGatePolicy::revoke() {
    std::scoped_lock lock(mutex_);
    if (!certificates_.empty()) ++counters_.revocations;
    certificates_.clear();
}

SolveGatePolicyCounters ExperimentalExactCandidateGatePolicy::counters() const {
    std::scoped_lock lock(mutex_);
    return counters_;
}

ImmutableGateInput::ImmutableGateInput(
    std::uint64_t id,
    std::unordered_map<std::string, double> values)
    : id_(id), values_(std::move(values)) {}

std::uint64_t ImmutableGateInput::id() const noexcept { return id_; }

const std::unordered_map<std::string, double>& ImmutableGateInput::values() const noexcept {
    return values_;
}

ExperimentalIncrementalGate::ExperimentalIncrementalGate(
    ModelIR model,
    std::string block_id,
    Tolerance tolerance,
    std::size_t full_verification_period)
    : model_(std::move(model)),
      runtime_(model_, tolerance),
      full_verification_period_(full_verification_period) {
    if (full_verification_period_ < 2) {
        throw std::invalid_argument("incremental gate full verification period must be at least two");
    }
    const auto block = std::find_if(
        model_.blocks.begin(), model_.blocks.end(),
        [&](const BlockIR& item) { return item.id == block_id; });
    if (block == model_.blocks.end()) {
        throw std::invalid_argument("incremental gate block does not exist");
    }
    block_ = &*block;
}

std::shared_ptr<const ImmutableGateInput> ExperimentalIncrementalGate::issue_input(
    std::unordered_map<std::string, double> values) {
    std::scoped_lock lock(mutex_);
    if (next_input_id_ == 0) throw std::overflow_error("incremental gate input id exhausted");
    return std::shared_ptr<const ImmutableGateInput>(
        new ImmutableGateInput(next_input_id_++, std::move(values)));
}

GateResult ExperimentalIncrementalGate::evaluate(
    const std::shared_ptr<const ImmutableGateInput>& input,
    bool direct_permission) {
    std::scoped_lock lock(mutex_);
    ++counters_.requests;
    if (!input) {
        ++counters_.low_cost_rejects;
        return {.reason = "incremental gate input is null"};
    }
    const bool exact_repeat = cached_input_ == input &&
        cached_direct_permission_ == direct_permission;
    if (exact_repeat) {
        if (low_risk_hits_since_full_ + 1 >= full_verification_period_) {
            ++counters_.periodic_full_verifications;
            return full_verify_locked(input, direct_permission, true);
        }
        ++low_risk_hits_since_full_;
        ++counters_.certificate_reuses;
        return cached_gate_;
    }

    for (const auto& unknown : block_->unknowns) {
        const auto variable = std::find_if(
            model_.variables.begin(), model_.variables.end(),
            [&](const VariableIR& item) { return item.name == unknown; });
        const auto value = input->values().find(unknown);
        if (variable == model_.variables.end() || value == input->values().end() ||
            !std::isfinite(value->second) ||
            (variable->minimum && value->second < *variable->minimum) ||
            (variable->maximum && value->second > *variable->maximum)) {
            ++counters_.low_cost_rejects;
            return {.reason = "incremental gate necessary condition failed"};
        }
    }
    ++counters_.high_risk_full_verifications;
    return full_verify_locked(input, direct_permission, false);
}

void ExperimentalIncrementalGate::revoke() {
    std::scoped_lock lock(mutex_);
    cached_input_.reset();
    low_risk_hits_since_full_ = 0;
    ++counters_.revocations;
}

IncrementalGateCounters ExperimentalIncrementalGate::counters() const {
    std::scoped_lock lock(mutex_);
    return counters_;
}

GateResult ExperimentalIncrementalGate::full_verify_locked(
    const std::shared_ptr<const ImmutableGateInput>& input,
    bool direct_permission,
    bool periodic) {
    auto observed = runtime_.evaluate_gate(*block_, input->values(), direct_permission);
    observed.scaled_residuals.clear();
    if (periodic &&
        (observed.decision != cached_gate_.decision ||
         observed.residual_inf != cached_gate_.residual_inf)) {
        ++counters_.periodic_mismatches;
        cached_input_.reset();
        low_risk_hits_since_full_ = 0;
        return {.reason = "incremental gate periodic verification drift"};
    }
    cached_input_ = input;
    cached_direct_permission_ = direct_permission;
    cached_gate_ = observed;
    low_risk_hits_since_full_ = 0;
    return observed;
}

}  // namespace smave
