#pragma once

#include "smave/runtime.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace smave {

class ImmutableGateInput {
public:
    [[nodiscard]] std::uint64_t id() const noexcept;
    [[nodiscard]] const std::unordered_map<std::string, double>& values() const noexcept;

private:
    friend class ExperimentalIncrementalGate;
    ImmutableGateInput(
        std::uint64_t id,
        std::unordered_map<std::string, double> values);

    std::uint64_t id_{};
    std::unordered_map<std::string, double> values_;
};

struct IncrementalGateCounters {
    std::size_t requests{};
    std::size_t high_risk_full_verifications{};
    std::size_t periodic_full_verifications{};
    std::size_t certificate_reuses{};
    std::size_t low_cost_rejects{};
    std::size_t periodic_mismatches{};
    std::size_t revocations{};
};

struct SolveGatePolicyCounters {
    std::size_t requests{};
    std::size_t strict_verifications{};
    std::size_t periodic_strict_verifications{};
    std::size_t certificate_reuses{};
    std::size_t periodic_mismatches{};
    std::size_t revocations{};
};

class ExperimentalExactCandidateGatePolicy final : public SolveGatePolicy {
public:
    explicit ExperimentalExactCandidateGatePolicy(
        std::size_t strict_verification_period = 16);

    [[nodiscard]] GateResult evaluate(
        const Runtime& runtime,
        const BlockIR& block,
        const std::unordered_map<std::string, double>& values,
        bool direct_permission) override;
    void revoke();
    [[nodiscard]] SolveGatePolicyCounters counters() const;

private:
    struct Certificate {
        std::shared_ptr<const void> runtime_identity;
        std::unordered_map<std::string, double> values;
        bool direct_permission{};
        GateResult gate;
        std::size_t reuses_since_strict{};
    };

    [[nodiscard]] static bool identical_values(
        const std::unordered_map<std::string, double>& left,
        const std::unordered_map<std::string, double>& right);
    [[nodiscard]] static bool identical_gate(
        const GateResult& left,
        const GateResult& right);

    const std::size_t strict_verification_period_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, Certificate> certificates_;
    SolveGatePolicyCounters counters_;
};

class ExperimentalIncrementalGate {
public:
    ExperimentalIncrementalGate(
        ModelIR model,
        std::string block_id,
        Tolerance tolerance = {},
        std::size_t full_verification_period = 16);

    [[nodiscard]] std::shared_ptr<const ImmutableGateInput> issue_input(
        std::unordered_map<std::string, double> values);
    [[nodiscard]] GateResult evaluate(
        const std::shared_ptr<const ImmutableGateInput>& input,
        bool direct_permission);
    void revoke();
    [[nodiscard]] IncrementalGateCounters counters() const;

private:
    [[nodiscard]] GateResult full_verify_locked(
        const std::shared_ptr<const ImmutableGateInput>& input,
        bool direct_permission,
        bool periodic);

    ModelIR model_;
    const BlockIR* block_{};
    Runtime runtime_;
    std::size_t full_verification_period_{};
    mutable std::mutex mutex_;
    std::uint64_t next_input_id_{1};
    std::shared_ptr<const ImmutableGateInput> cached_input_;
    bool cached_direct_permission_{};
    GateResult cached_gate_;
    std::size_t low_risk_hits_since_full_{};
    IncrementalGateCounters counters_;
};

}  // namespace smave
