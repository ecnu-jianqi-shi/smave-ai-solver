#pragma once

#include "smave/expert.hpp"
#include "smave/ir.hpp"
#include "smave/verification.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace smave {

struct AffineWarmStartArtifact {
    std::string schema_version{"smave.affine-warm-start.v1"};
    std::string training_dataset_id;
    std::string training_dataset_version;
    std::string training_dataset_manifest_hash;
    std::string expert_version;
    std::string model_source_hash;
    std::string block_fingerprint;
    std::vector<std::string> features;
    std::vector<std::string> outputs;
    std::vector<double> feature_minimum;
    std::vector<double> feature_maximum;
    std::vector<std::vector<double>> coefficients;
    std::size_t training_samples{};
    double training_rmse{};
    std::string artifact_hash;

    void seal();
    void validate() const;
    void write(const std::filesystem::path& path) const;
    static AffineWarmStartArtifact read(const std::filesystem::path& path);
};

struct LinearPreconditionerArtifact {
    std::string schema_version{"smave.linear-preconditioner.v1"};
    std::string training_dataset_id;
    std::string training_dataset_version;
    std::string training_dataset_manifest_hash;
    std::string expert_version;
    std::string model_source_hash;
    std::string block_fingerprint;
    std::vector<std::string> features;
    std::vector<double> feature_minimum;
    std::vector<double> feature_maximum;
    std::vector<std::vector<double>> inverse_operator;
    std::size_t training_samples{};
    double maximum_matrix_drift{};
    std::string artifact_hash;

    void seal();
    void validate() const;
    void write(const std::filesystem::path& path) const;
    static LinearPreconditionerArtifact read(const std::filesystem::path& path);
};

struct LearnedMultigridArtifact {
    std::string schema_version{"smave.learned-multigrid.v2"};
    std::string training_dataset_id;
    std::string training_dataset_version;
    std::string training_dataset_manifest_hash;
    std::string expert_version;
    std::string model_source_hash;
    std::string block_fingerprint;
    std::vector<std::string> features;
    std::vector<double> feature_minimum;
    std::vector<double> feature_maximum;
    std::vector<std::vector<double>> fine_operator;
    std::vector<std::vector<double>> prolongation;
    std::vector<std::vector<double>> coarse_inverse;
    std::vector<std::vector<std::vector<double>>> level_operators;
    std::vector<std::vector<std::vector<double>>> level_prolongations;
    std::size_t training_samples{};
    bool jacobian_mode{false};
    std::size_t pre_smoothing_steps{1};
    std::size_t post_smoothing_steps{1};
    double smoothing_weight{2.0 / 3.0};
    double maximum_matrix_drift{};
    double maximum_probe_contraction{1.0};
    std::string artifact_hash;

    void seal();
    void validate() const;
    void write(const std::filesystem::path& path) const;
    static LearnedMultigridArtifact read(const std::filesystem::path& path);
};

class LearnedLinearPreconditionerExpert final : public Expert {
public:
    explicit LearnedLinearPreconditionerExpert(
        LinearPreconditionerArtifact artifact,
        std::optional<VerificationCertificate> certificate = std::nullopt);
    [[nodiscard]] std::string version() const override;
    [[nodiscard]] Capability match(const BlockIR& block) const override;
    [[nodiscard]] Estimate estimate(
        const BlockIR& block, const BlockContext& context) const override;
    [[nodiscard]] ExpertResult solve(
        const BlockIR& block,
        const BlockContext& context,
        const SolveBudget& budget) const override;
    [[nodiscard]] bool apply_preconditioner(
        const BlockIR& block,
        const BlockContext& context,
        const std::vector<double>& residual,
        std::vector<double>& result) const override;
    [[nodiscard]] bool apply_preconditioner_batch(
        const BlockIR& block,
        const std::vector<BlockContext>& contexts,
        const std::vector<std::vector<double>>& residuals,
        std::vector<std::vector<double>>& results) const override;
    [[nodiscard]] bool apply_preconditioner_batch_on_device(
        const std::string& device,
        const BlockIR& block,
        const std::vector<BlockContext>& contexts,
        const std::vector<std::vector<double>>& residuals,
        std::vector<std::vector<double>>& results,
        DeviceExecutionResult* execution) const override;
    [[nodiscard]] bool device_batch_is_resident(
        const std::string& device,
        std::size_t batch,
        std::size_t width) const override;

private:
    [[nodiscard]] double ood_score(const BlockContext& context) const;
    LinearPreconditionerArtifact artifact_;
    std::optional<VerificationCertificate> certificate_;
    std::vector<double> contiguous_inverse_operator_;
};

class LearnedMultigridExpert final : public Expert {
public:
    explicit LearnedMultigridExpert(
        LearnedMultigridArtifact artifact,
        std::optional<VerificationCertificate> certificate = std::nullopt);
    [[nodiscard]] std::string version() const override;
    [[nodiscard]] Capability match(const BlockIR& block) const override;
    [[nodiscard]] Estimate estimate(
        const BlockIR& block, const BlockContext& context) const override;
    [[nodiscard]] ExpertResult solve(
        const BlockIR& block,
        const BlockContext& context,
        const SolveBudget& budget) const override;
    [[nodiscard]] bool apply_preconditioner(
        const BlockIR& block,
        const BlockContext& context,
        const std::vector<double>& residual,
        std::vector<double>& result) const override;

private:
    [[nodiscard]] double ood_score(const BlockContext& context) const;
    LearnedMultigridArtifact artifact_;
    std::optional<VerificationCertificate> certificate_;
};

class AffineWarmStartExpert final : public Expert {
public:
    explicit AffineWarmStartExpert(
        AffineWarmStartArtifact artifact,
        std::optional<VerificationCertificate> certificate = std::nullopt);
    [[nodiscard]] std::string version() const override;
    [[nodiscard]] Capability match(const BlockIR& block) const override;
    [[nodiscard]] Estimate estimate(
        const BlockIR& block, const BlockContext& context) const override;
    [[nodiscard]] ExpertResult solve(
        const BlockIR& block,
        const BlockContext& context,
        const SolveBudget& budget) const override;

private:
    AffineWarmStartArtifact artifact_;
    std::optional<VerificationCertificate> certificate_;
};

[[nodiscard]] AffineWarmStartArtifact train_affine_warm_start(
    const ModelIR& model,
    const std::string& block_id,
    const std::filesystem::path& scenario_directory,
    const std::filesystem::path& trace_directory);
void register_affine_expert(
    Registry& registry,
    const AffineWarmStartArtifact& artifact,
    const std::string& domain_version = "domain-v1",
    const std::string& tolerance_profile = "default",
    const std::string& hardware_profile = "cpu",
    std::optional<VerificationCertificate> certificate = std::nullopt);

[[nodiscard]] LinearPreconditionerArtifact train_linear_preconditioner(
    const ModelIR& model,
    const std::string& block_id,
    const std::filesystem::path& scenario_directory);
void register_linear_preconditioner(
    Registry& registry,
    const LinearPreconditionerArtifact& artifact,
    const std::string& domain_version = "domain-v1",
    const std::string& tolerance_profile = "default",
    const std::string& hardware_profile = "cpu",
    std::optional<VerificationCertificate> certificate = std::nullopt);

[[nodiscard]] LearnedMultigridArtifact train_learned_multigrid(
    const ModelIR& model,
    const std::string& block_id,
    const std::filesystem::path& scenario_directory);
[[nodiscard]] LearnedMultigridArtifact build_learned_multigrid_artifact(
    const std::string& model_source_hash,
    const std::string& block_fingerprint,
    const std::vector<std::string>& features,
    const std::vector<double>& feature_minimum,
    const std::vector<double>& feature_maximum,
    const std::vector<std::vector<std::vector<double>>>& matrices,
    bool jacobian_mode = true);
[[nodiscard]] bool apply_learned_multigrid(
    const LearnedMultigridArtifact& artifact,
    const std::vector<double>& residual,
    std::vector<double>& result);
void register_learned_multigrid(
    Registry& registry,
    const LearnedMultigridArtifact& artifact,
    const std::string& domain_version = "domain-v1",
    const std::string& tolerance_profile = "default",
    const std::string& hardware_profile = "cpu",
    std::optional<VerificationCertificate> certificate = std::nullopt);

[[nodiscard]] VerificationCertificate verify_affine_warm_start(
    const ModelIR& model,
    const AffineWarmStartArtifact& artifact,
    std::size_t maximum_depth = 4,
    const std::filesystem::path& trace_directory = ".smave/verification-probes");

[[nodiscard]] VerificationCertificate verify_linear_preconditioner(
    const ModelIR& model,
    const LinearPreconditionerArtifact& artifact,
    std::size_t maximum_depth = 4,
    const std::filesystem::path& trace_directory = ".smave/verification-probes");

[[nodiscard]] VerificationCertificate verify_learned_multigrid(
    const ModelIR& model,
    const LearnedMultigridArtifact& artifact,
    std::size_t maximum_depth = 4,
    const std::filesystem::path& trace_directory = ".smave/verification-probes");

}  // namespace smave
