#pragma once

#include "smave/ir.hpp"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace smave {

struct DeviceExecutionResult;

enum class EvidenceLevel { e0, e1, e2, e3, e4 };
enum class Permission { shadow, warm_start, corrected, direct };
enum class BackendRole {
    initializer,
    linear_solver,
    preconditioner,
    nonlinear_solver,
    operator_candidate,
    fallback,
};

struct Capability {
    bool linear{false};
    bool nonlinear{false};
    bool event_related{false};
    bool preconditioner{false};
    std::vector<BackendRole> backend_roles;
    std::vector<std::string> devices{"cpu"};
    EvidenceLevel evidence_level{EvidenceLevel::e0};
    Permission maximum_permission{Permission::shadow};
};

struct BlockContext {
    struct NumericProbe {
        bool available{false};
        bool symmetric{false};
        bool positive_definite{false};
        double diagonal_condition_estimate{0.0};
    } numeric_probe;
    std::unordered_map<std::string, double> values;
    std::unordered_map<std::string, double> previous_solution;
    std::string mode{"continuous"};
    double event_distance{1.0};
    double ood_score{0.0};
};

struct Estimate {
    double pass_probability{0.0};
    double expected_setup_time_us{0.0};
    double expected_solve_time_us{0.0};
    double expected_correction_time_us{0.0};
    double failure_cost_us{0.0};
    double risk_score{1.0};
    double ood_score{0.0};
};

struct SolveBudget {
    int work_iterations{8};
    std::chrono::microseconds timeout{1000};
};

struct ExpertResult {
    std::unordered_map<std::string, double> candidate;
    std::string status{"failure"};
    std::string branch_id{"default"};
    std::optional<double> residual_hint;
    double uncertainty{1.0};
    std::unordered_map<std::string, double> telemetry;
};

class Expert {
public:
    virtual ~Expert() = default;
    [[nodiscard]] virtual std::string version() const = 0;
    [[nodiscard]] virtual Capability match(const BlockIR& block) const = 0;
    [[nodiscard]] virtual Estimate estimate(
        const BlockIR& block, const BlockContext& context) const = 0;
    [[nodiscard]] virtual ExpertResult solve(
        const BlockIR& block,
        const BlockContext& context,
        const SolveBudget& budget) const = 0;
    [[nodiscard]] virtual bool apply_preconditioner(
        const BlockIR& block,
        const BlockContext& context,
        const std::vector<double>& residual,
        std::vector<double>& result) const;
    [[nodiscard]] virtual bool apply_preconditioner_batch(
        const BlockIR& block,
        const std::vector<BlockContext>& contexts,
        const std::vector<std::vector<double>>& residuals,
        std::vector<std::vector<double>>& results) const;
    [[nodiscard]] virtual bool apply_preconditioner_batch_on_device(
        const std::string& device,
        const BlockIR& block,
        const std::vector<BlockContext>& contexts,
        const std::vector<std::vector<double>>& residuals,
        std::vector<std::vector<double>>& results,
        DeviceExecutionResult* execution) const;
    [[nodiscard]] virtual bool device_batch_is_resident(
        const std::string& device,
        std::size_t batch,
        std::size_t width) const;
};

struct ExpertGrant {
    std::string expert_version;
    std::string block_family;
    std::string domain_version{"domain-v1"};
    std::string tolerance_profile{"default"};
    std::string hardware_profile{"cpu"};
    Permission permission{Permission::shadow};
    EvidenceLevel evidence_level{EvidenceLevel::e0};
    std::string evidence_bundle;
    std::string artifact_hash;
    std::size_t resident_bytes{1};
    std::int64_t expires_unix_seconds{0};
};

struct RuntimeBundle {
    std::string bundle_id;
    std::string model_source_hash;
    std::string ir_schema_version{kIrSchemaVersion};
    std::string domain_version{"domain-v1"};
    std::string tolerance_profile{"default"};
    std::string hardware_profile{"cpu"};
    std::vector<std::string> expert_versions;
    std::vector<std::string> expert_artifact_hashes;
    std::vector<std::string> expert_evidence_hashes;
    std::string terminal_fallback{"original-damped-newton"};
    std::string bundle_hash;

    void add_expert(
        std::string version,
        std::string artifact_hash,
        std::string evidence_hash);
    void seal();
    void write(const std::filesystem::path& path) const;
    static RuntimeBundle read(const std::filesystem::path& path);
};

class Registry {
public:
    void register_expert(std::shared_ptr<const Expert> expert, ExpertGrant grant);
    [[nodiscard]] const Expert& expert(const std::string& version) const;
    [[nodiscard]] const ExpertGrant& grant(const std::string& version) const;
    [[nodiscard]] bool compatible(
        const std::string& version,
        const BlockIR& block,
        const RuntimeBundle& bundle,
        Permission requested) const;
    void validate_bundle(const RuntimeBundle& bundle, const ModelIR& model) const;

private:
    struct Entry {
        std::shared_ptr<const Expert> expert;
        ExpertGrant grant;
    };
    std::unordered_map<std::string, Entry> entries_;
};

class ContinuationWarmStartExpert final : public Expert {
public:
    explicit ContinuationWarmStartExpert(const ModelIR& model);
    [[nodiscard]] std::string version() const override;
    [[nodiscard]] Capability match(const BlockIR& block) const override;
    [[nodiscard]] Estimate estimate(
        const BlockIR& block, const BlockContext& context) const override;
    [[nodiscard]] ExpertResult solve(
        const BlockIR& block,
        const BlockContext& context,
        const SolveBudget& budget) const override;

private:
    std::unordered_map<std::string, double> starts_;
};

[[nodiscard]] RuntimeBundle make_default_bundle(const ModelIR& model);
[[nodiscard]] Registry make_default_registry(const ModelIR& model);
[[nodiscard]] std::string to_string(EvidenceLevel level);
[[nodiscard]] std::string to_string(Permission permission);

}  // namespace smave
