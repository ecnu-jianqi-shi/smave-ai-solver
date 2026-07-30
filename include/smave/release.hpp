#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>

namespace smave {

inline constexpr const char* kReleaseAuditSchemaVersion = "SMAVE_RELEASE_AUDIT_3";
inline constexpr const char* kReleaseManifestSchemaVersion = "SMAVE_RELEASE_MANIFEST_2";
inline constexpr const char* kReleaseStateSchemaVersion = "SMAVE_RELEASE_STATE_2";

struct ReleaseAudit {
    int schema_version{3};
    std::string stage;
    std::string model_source_hash;
    std::string bundle_hash;
    std::string validation_sha256;
    std::string performance_sha256;
    std::string parent_audit_hash;
    std::string parent_audit_sha256;
    std::string dataset_id;
    std::string dataset_version;
    std::string dataset_manifest_hash;
    std::string dataset_manifest_sha256;
    std::size_t requests{0};
    std::size_t minimum_requests{0};
    std::size_t successful_requests{0};
    std::size_t erroneous_accepts{0};
    std::size_t safety_evaluations{0};
    std::size_t original_solver_failures{0};
    double safety_confidence_level{0.95};
    double erroneous_accept_rate_upper_bound{1.0};
    double maximum_erroneous_accept_rate{0.05};
    double top_k_pass_rate{0.0};
    double minimum_top_k_pass_rate{0.95};
    double observed_hours{0.0};
    double minimum_observed_hours{0.0};
    double traffic_fraction{0.0};
    bool safety_met{false};
    bool performance_met{false};
    bool promotion_ready{false};
    std::string audit_hash;

    void seal();
    void validate() const;
    void write(const std::filesystem::path& path) const;
    static ReleaseAudit read(const std::filesystem::path& path);
};

[[nodiscard]] ReleaseAudit audit_release(
    const std::filesystem::path& model_path,
    const std::filesystem::path& bundle_path,
    const std::filesystem::path& validation_path,
    const std::filesystem::path& performance_path,
    std::string stage,
    double traffic_fraction,
    double observed_hours,
    std::size_t minimum_requests,
    double minimum_observed_hours,
    double minimum_top_k_pass_rate = 0.95,
    const std::filesystem::path& parent_audit_path = {},
    const std::filesystem::path& dataset_manifest_path = {});

struct ReleaseManifest {
    int schema_version{2};
    std::string release_id;
    std::string created_utc;
    std::string model_source_hash;
    std::string bundle_id;
    std::string bundle_hash;
    std::string bundle_sha256;
    std::string audit_hash;
    std::string audit_sha256;
    std::string model_ir_sha256;
    std::string expert_sha256;
    std::string certificate_sha256;
    std::string dataset_id;
    std::string dataset_version;
    std::string dataset_manifest_hash;
    std::string dataset_manifest_sha256;
    std::string key_id;
    std::string manifest_hash;
    std::string signature;

    void seal_and_sign(const std::string& key);
    void validate(const std::string& key) const;
    void write(const std::filesystem::path& path) const;
    static ReleaseManifest read(const std::filesystem::path& path);
};

[[nodiscard]] ReleaseManifest create_release_manifest(
    const std::filesystem::path& bundle_path,
    const std::filesystem::path& audit_path,
    const std::filesystem::path& key_path,
    std::string release_id,
    const std::filesystem::path& model_path = {},
    const std::filesystem::path& expert_path = {},
    const std::filesystem::path& certificate_path = {},
    const std::filesystem::path& dataset_manifest_path = {});

struct ReleaseState {
    int schema_version{2};
    std::size_t generation{0};
    std::string current_release;
    std::string previous_release;
    std::string key_id;
    std::string state_hash;
    std::string signature;

    void seal();
    void seal_and_sign(const std::string& key);
    void validate() const;
    void validate(const std::string& key) const;
    void write_atomic(const std::filesystem::path& path) const;
    static ReleaseState read(const std::filesystem::path& path);
};

struct VerifiedRelease {
    ReleaseState state;
    ReleaseManifest manifest;
    std::filesystem::path directory;
};

class ReleaseStore {
public:
    explicit ReleaseStore(std::filesystem::path root);
    [[nodiscard]] ReleaseState activate(
        const std::filesystem::path& manifest_path,
        const std::filesystem::path& bundle_path,
        const std::filesystem::path& audit_path,
        const std::filesystem::path& parent_audit_path,
        const std::filesystem::path& model_path,
        const std::filesystem::path& expert_path,
        const std::filesystem::path& certificate_path,
        const std::filesystem::path& key_path,
        const std::filesystem::path& dataset_manifest_path = {});
    [[nodiscard]] ReleaseState rollback(const std::filesystem::path& key_path);
    [[nodiscard]] ReleaseState status() const;
    [[nodiscard]] VerifiedRelease verified_active(
        const std::filesystem::path& key_path) const;
    [[nodiscard]] ReleaseManifest verify_active(
        const std::filesystem::path& key_path) const;
    [[nodiscard]] std::filesystem::path active_directory() const;

private:
    std::filesystem::path root_;
};

[[nodiscard]] std::string sha256_file(const std::filesystem::path& path);
[[nodiscard]] std::string sha256_text(std::string_view text);
[[nodiscard]] std::string hmac_sha256_text(
    std::string_view key,
    std::string_view text);
[[nodiscard]] bool verify_hmac_sha256_text(
    std::string_view key,
    std::string_view text,
    std::string_view expected);

}  // namespace smave
