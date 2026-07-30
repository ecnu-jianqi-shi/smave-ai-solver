#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace smave {

struct VerifiedCell {
    std::vector<std::string> features;
    std::vector<double> lower;
    std::vector<double> upper;
    double worst_residual{};
    double worst_risk{};
    std::size_t probes{};
};

struct Counterexample {
    std::unordered_map<std::string, double> context;
    double residual{};
    double risk{};
    std::string reason;
};

struct VerificationCertificate {
    std::string schema_version{"smave.verified-cells.v1"};
    std::string training_dataset_id;
    std::string training_dataset_version;
    std::string training_dataset_manifest_hash;
    std::string expert_version;
    std::string artifact_hash;
    std::string block_fingerprint;
    std::string domain_version{"domain-v1"};
    std::vector<VerifiedCell> cells;
    std::vector<Counterexample> counterexamples;
    std::size_t total_probes{};
    std::string certificate_hash;

    void seal();
    void validate() const;
    void write(const std::filesystem::path& path) const;
    static VerificationCertificate read(const std::filesystem::path& path);
    void export_counterexamples(const std::filesystem::path& directory) const;

    [[nodiscard]] bool contains(
        const std::unordered_map<std::string, double>& context) const;
};

struct ProbeResult {
    bool accepted{false};
    double residual{};
    double risk{};
    std::string reason;
};

using VerificationProbe = std::function<ProbeResult(
    const std::unordered_map<std::string, double>& context)>;

[[nodiscard]] VerificationCertificate verify_cells(
    const std::string& expert_version,
    const std::string& artifact_hash,
    const std::string& block_fingerprint,
    const std::vector<std::string>& features,
    const std::vector<double>& lower,
    const std::vector<double>& upper,
    const VerificationProbe& probe,
    std::size_t maximum_depth = 4,
    double minimum_width = 1.0e-6);

}  // namespace smave
