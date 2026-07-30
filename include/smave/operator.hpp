#pragma once

#include "smave/expert.hpp"
#include "smave/runtime.hpp"
#include "smave/verification.hpp"

#include <filesystem>
#include <optional>

namespace smave {

struct LatentOperatorArtifact {
    std::string schema_version{"smave.latent-operator.v1"};
    std::string training_dataset_id;
    std::string training_dataset_version;
    std::string training_dataset_manifest_hash;
    std::string expert_version;
    std::string model_source_hash;
    std::string block_fingerprint;
    std::vector<std::string> features;
    std::vector<std::string> outputs;
    std::vector<std::string> qoi_outputs;
    std::vector<double> feature_minimum;
    std::vector<double> feature_maximum;
    std::vector<double> state_mean;
    std::vector<std::vector<double>> state_basis;
    std::vector<std::vector<double>> latent_coefficients;
    std::size_t training_samples{};
    double retained_energy{};
    double training_rmse{};
    double training_wall_us{};
    std::string output_permission{"full-state-corrected"};
    std::string artifact_hash;

    void seal();
    void validate() const;
    void write(const std::filesystem::path& path) const;
    static LatentOperatorArtifact read(const std::filesystem::path& path);
};

class LatentOperatorExpert final : public Expert {
public:
    explicit LatentOperatorExpert(
        LatentOperatorArtifact artifact,
        std::optional<VerificationCertificate> certificate = std::nullopt);
    [[nodiscard]] std::string version() const override;
    [[nodiscard]] Capability match(const BlockIR& block) const override;
    [[nodiscard]] Estimate estimate(
        const BlockIR& block, const BlockContext& context) const override;
    [[nodiscard]] ExpertResult solve(
        const BlockIR& block,
        const BlockContext& context,
        const SolveBudget& budget) const override;
    [[nodiscard]] std::vector<ExpertResult> solve_batch(
        const BlockIR& block,
        const std::vector<BlockContext>& contexts) const;
    [[nodiscard]] std::vector<ExpertResult> solve_batch_on_device(
        const std::string& device,
        const BlockIR& block,
        const std::vector<BlockContext>& contexts,
        DeviceExecutionResult* execution = nullptr) const;
    [[nodiscard]] bool operator_batch_is_resident(
        const std::string& device,
        std::size_t batch) const;
    [[nodiscard]] std::size_t in_domain_batch_size(
        const std::vector<BlockContext>& contexts) const;

private:
    [[nodiscard]] double ood_score(const BlockContext& context) const;
    LatentOperatorArtifact artifact_;
    std::optional<VerificationCertificate> certificate_;
    std::vector<float> affine_weights_;
    std::vector<float> affine_bias_;
};

[[nodiscard]] LatentOperatorArtifact train_latent_operator(
    const ModelIR& model,
    const std::string& block_id,
    const std::filesystem::path& scenario_directory,
    const std::filesystem::path& trace_directory,
    std::size_t maximum_rank = 8,
    std::vector<std::string> qoi_outputs = {});

void register_latent_operator(
    Registry& registry,
    const LatentOperatorArtifact& artifact,
    const std::string& domain_version = "domain-v1",
    const std::string& tolerance_profile = "default",
    const std::string& hardware_profile = "cpu",
    std::optional<VerificationCertificate> certificate = std::nullopt);

[[nodiscard]] VerificationCertificate verify_latent_operator(
    const ModelIR& model,
    const LatentOperatorArtifact& artifact,
    std::size_t maximum_depth = 4,
    const std::filesystem::path& trace_directory = ".smave/operator-verification-probes");

struct OperatorBenchmarkReport {
    int schema_version{1};
    std::string dataset_id;
    std::string dataset_version;
    std::string dataset_manifest_hash;
    std::size_t requests{};
    std::size_t repetitions{};
    std::size_t batches{};
    double average_batch{};
    std::size_t accepted{};
    std::size_t fallbacks{};
    std::size_t failures{};
    double acceptance_rate{};
    double baseline_median_us{};
    double operator_median_us{};
    double online_speedup{};
    double paired_speedup_ci95_lower{};
    double paired_speedup_ci95_upper{};
    double paired_median_saving_us{};
    double training_wall_us{};
    std::size_t break_even_queries{};
    std::size_t projected_queries{};
    double amortized_speedup{};
    double maximum_full_state_error{};
    double maximum_qoi_error{};
    double maximum_candidate_full_state_error{};
    double maximum_candidate_qoi_error{};
    bool candidate_qoi_within_tolerance{};
    bool same_accuracy{};
    bool break_even_met{};
    std::string artifact_hash;
    std::string certificate_hash;
    std::string report_hash;

    void seal();
    void validate() const;
};

[[nodiscard]] OperatorBenchmarkReport benchmark_latent_operator(
    const ModelIR& model,
    const Registry& registry,
    const RuntimeBundle& bundle,
    const LatentOperatorArtifact& artifact,
    const std::filesystem::path& scenario_directory,
    const std::filesystem::path& trace_directory,
    std::size_t repetitions,
    std::size_t projected_queries,
    Tolerance tolerance = {},
    std::string device = "cpu");

void write_operator_benchmark_report(
    const OperatorBenchmarkReport& report,
    const std::filesystem::path& path);
[[nodiscard]] OperatorBenchmarkReport read_operator_benchmark_report(
    const std::filesystem::path& path);

}  // namespace smave
