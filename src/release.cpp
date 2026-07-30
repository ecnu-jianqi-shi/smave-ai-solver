#include "smave/release.hpp"

#include "smave/expert.hpp"
#include "smave/data_registry.hpp"
#include "smave/ir.hpp"
#include "smave/validation.hpp"
#include "smave/learning.hpp"
#include "smave/operator.hpp"
#include "smave/verification.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace smave {
namespace {

using Hash = std::array<std::uint8_t, 32>;

constexpr std::array<std::uint32_t, 64> kRoundConstants{
    0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,
    0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,
    0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
    0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,
    0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,
    0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
    0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,
    0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U};

Hash sha256(std::string_view input) {
    std::vector<std::uint8_t> message(input.begin(), input.end());
    const std::uint64_t bit_length = static_cast<std::uint64_t>(message.size()) * 8U;
    message.push_back(0x80U);
    while (message.size() % 64 != 56) message.push_back(0U);
    for (int shift = 56; shift >= 0; shift -= 8) {
        message.push_back(static_cast<std::uint8_t>(bit_length >> shift));
    }
    std::array<std::uint32_t, 8> state{
        0x6a09e667U,0xbb67ae85U,0x3c6ef372U,0xa54ff53aU,
        0x510e527fU,0x9b05688cU,0x1f83d9abU,0x5be0cd19U};
    for (std::size_t offset = 0; offset < message.size(); offset += 64) {
        std::array<std::uint32_t, 64> words{};
        for (std::size_t index = 0; index < 16; ++index) {
            const auto base = offset + index * 4;
            words[index] = (static_cast<std::uint32_t>(message[base]) << 24U) |
                (static_cast<std::uint32_t>(message[base + 1]) << 16U) |
                (static_cast<std::uint32_t>(message[base + 2]) << 8U) |
                message[base + 3];
        }
        for (std::size_t index = 16; index < 64; ++index) {
            const auto small0 = std::rotr(words[index - 15], 7) ^
                std::rotr(words[index - 15], 18) ^ (words[index - 15] >> 3U);
            const auto small1 = std::rotr(words[index - 2], 17) ^
                std::rotr(words[index - 2], 19) ^ (words[index - 2] >> 10U);
            words[index] = words[index - 16] + small0 + words[index - 7] + small1;
        }
        auto [a,b,c,d,e,f,g,h] = state;
        for (std::size_t index = 0; index < 64; ++index) {
            const auto big1 = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
            const auto choice = (e & f) ^ (~e & g);
            const auto temporary1 = h + big1 + choice + kRoundConstants[index] + words[index];
            const auto big0 = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
            const auto majority = (a & b) ^ (a & c) ^ (b & c);
            const auto temporary2 = big0 + majority;
            h=g; g=f; f=e; e=d+temporary1; d=c; c=b; b=a; a=temporary1+temporary2;
        }
        state[0]+=a; state[1]+=b; state[2]+=c; state[3]+=d;
        state[4]+=e; state[5]+=f; state[6]+=g; state[7]+=h;
    }
    Hash result{};
    for (std::size_t index = 0; index < state.size(); ++index) {
        for (std::size_t byte = 0; byte < 4; ++byte) {
            result[index * 4 + byte] = static_cast<std::uint8_t>(
                state[index] >> (24U - static_cast<unsigned>(byte) * 8U));
        }
    }
    return result;
}

std::string hexadecimal(const Hash& hash) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const auto byte : hash) output << std::setw(2) << static_cast<unsigned>(byte);
    return output.str();
}

std::string read_bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot read release artifact: " + path.string());
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
}

std::string read_key(const std::filesystem::path& path) {
    auto key = read_bytes(path);
    while (!key.empty() && (key.back() == '\n' || key.back() == '\r')) key.pop_back();
    if (key.size() < 32) throw std::invalid_argument("release signing key must contain at least 256 bits");
    return key;
}

std::string hmac_sha256(const std::string& key_input, std::string_view message) {
    std::string key = key_input;
    if (key.size() > 64) {
        const auto compressed = sha256(key);
        key.assign(reinterpret_cast<const char*>(compressed.data()), compressed.size());
    }
    key.resize(64, '\0');
    std::string inner(64, '\0');
    std::string outer(64, '\0');
    for (std::size_t index = 0; index < 64; ++index) {
        inner[index] = static_cast<char>(static_cast<unsigned char>(key[index]) ^ 0x36U);
        outer[index] = static_cast<char>(static_cast<unsigned char>(key[index]) ^ 0x5cU);
    }
    inner.append(message);
    const auto inner_hash = sha256(inner);
    outer.append(reinterpret_cast<const char*>(inner_hash.data()), inner_hash.size());
    return hexadecimal(sha256(outer));
}

bool constant_time_equal(std::string_view left, std::string_view right) {
    if (left.size() != right.size()) return false;
    unsigned difference{};
    for (std::size_t index = 0; index < left.size(); ++index) {
        difference |= static_cast<unsigned char>(left[index]) ^
            static_cast<unsigned char>(right[index]);
    }
    return difference == 0;
}

std::map<std::string, std::string> key_values(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read report: " + path.string());
    std::map<std::string, std::string> values;
    std::string line;
    while (std::getline(input, line)) {
        const auto separator = line.find('=');
        if (separator != std::string::npos) {
            values[line.substr(0, separator)] = line.substr(separator + 1);
        }
    }
    return values;
}

template <typename Number>
Number number(const std::map<std::string, std::string>& values, const std::string& name) {
    const auto iterator = values.find(name);
    if (iterator == values.end()) throw std::invalid_argument("report lacks required field: " + name);
    std::istringstream input(iterator->second);
    Number result{};
    input >> result;
    if (!input || input.peek() != std::char_traits<char>::eof()) {
        throw std::invalid_argument("invalid report field: " + name);
    }
    return result;
}

bool performance_passed(const std::filesystem::path& path) {
    std::ifstream input(path);
    std::string magic;
    int version{};
    input >> magic >> version;
    if (!input ||
        (magic == "SMAVE_OPERATOR_BENCHMARK" &&
         version != 1 && version != 2 && version != 3) ||
        (magic == "SMAVE_PERFORMANCE" && version != 1)) {
        throw std::invalid_argument("unsupported performance report");
    }
    const auto values = key_values(path);
    if (magic == "SMAVE_OPERATOR_BENCHMARK") {
        const auto report = read_operator_benchmark_report(path);
        const bool candidate_gate = report.schema_version >= 3 ||
            report.candidate_qoi_within_tolerance;
        return report.failures == 0 && report.same_accuracy &&
            candidate_gate && report.break_even_met &&
            report.acceptance_rate >= 0.95 && report.online_speedup > 1.0 &&
            report.amortized_speedup > 1.0;
    }
    if (magic == "SMAVE_PERFORMANCE") {
        return number<int>(values, "baseline_failures") == 0 &&
            number<int>(values, "accelerated_failures") == 0 &&
            number<int>(values, "gate_mismatches") == 0 &&
            number<int>(values, "same_accuracy") == 1 &&
            number<int>(values, "p99_not_regressed") == 1 &&
            number<double>(values, "median_speedup") > 1.0;
    }
    throw std::invalid_argument("performance report is not a recognized benchmark artifact");
}

std::string audit_contract(const ReleaseAudit& audit) {
    std::ostringstream output;
    output << "SMAVE_RELEASE_AUDIT_" << audit.schema_version << '|'
           << audit.stage << '|'
           << audit.model_source_hash << '|' << audit.bundle_hash << '|'
           << audit.validation_sha256 << '|' << audit.performance_sha256 << '|'
           << audit.parent_audit_hash << '|' << audit.parent_audit_sha256 << '|';
    if (audit.schema_version >= 3) {
        output << audit.dataset_id << '|' << audit.dataset_version << '|'
               << audit.dataset_manifest_hash << '|'
               << audit.dataset_manifest_sha256 << '|';
    }
    output << audit.requests << '|' << audit.minimum_requests << '|'
           << audit.successful_requests << '|'
           << audit.erroneous_accepts << '|' << audit.safety_evaluations << '|'
           << audit.original_solver_failures << '|'
           << audit.safety_confidence_level << '|'
           << audit.erroneous_accept_rate_upper_bound << '|'
           << audit.maximum_erroneous_accept_rate << '|'
           << std::setprecision(17) << audit.top_k_pass_rate << '|'
           << audit.minimum_top_k_pass_rate << '|' << audit.observed_hours << '|'
           << audit.minimum_observed_hours << '|' << audit.traffic_fraction << '|'
           << audit.safety_met << '|' << audit.performance_met << '|'
           << audit.promotion_ready;
    return output.str();
}

std::string manifest_contract(const ReleaseManifest& manifest) {
    std::ostringstream output;
    output << "SMAVE_RELEASE_MANIFEST_" << manifest.schema_version << '|'
           << manifest.release_id << '|'
           << manifest.created_utc << '|' << manifest.model_source_hash << '|'
           << manifest.bundle_id << '|' << manifest.bundle_hash << '|'
           << manifest.bundle_sha256 << '|' << manifest.audit_hash << '|'
           << manifest.audit_sha256 << '|' << manifest.model_ir_sha256 << '|'
           << manifest.expert_sha256 << '|'
           << manifest.certificate_sha256 << '|';
    if (manifest.schema_version >= 2) {
        output << manifest.dataset_id << '|' << manifest.dataset_version << '|'
               << manifest.dataset_manifest_hash << '|'
               << manifest.dataset_manifest_sha256 << '|';
    }
    output << manifest.key_id;
    return output.str();
}

std::string state_contract(const ReleaseState& state) {
    std::ostringstream output;
    output << "SMAVE_RELEASE_STATE_" << state.schema_version << '|'
           << state.generation << '|' << state.current_release << '|'
           << state.previous_release;
    if (state.schema_version >= 2) output << '|' << state.key_id;
    return output.str();
}

void tag(std::istream& input, std::string_view expected, std::string_view kind) {
    std::string actual;
    input >> actual;
    if (!input || actual != expected) {
        throw std::runtime_error("invalid " + std::string(kind) + ": expected " + std::string(expected));
    }
}

std::string utc_now() {
    const auto now = std::chrono::system_clock::now();
    const auto seconds = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &seconds);
#else
    gmtime_r(&seconds, &utc);
#endif
    std::ostringstream output;
    output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return output.str();
}

class StoreLock {
public:
    explicit StoreLock(const std::filesystem::path& root) : path_(root / ".release-lock-v2") {
        std::filesystem::create_directories(root);
#if defined(_WIN32)
        while (handle_ == INVALID_HANDLE_VALUE) {
            handle_ = CreateFileW(
                path_.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                FILE_ATTRIBUTE_NORMAL, nullptr);
            if (handle_ != INVALID_HANDLE_VALUE) break;
            const auto error = GetLastError();
            if (error != ERROR_SHARING_VIOLATION && error != ERROR_LOCK_VIOLATION) {
                throw std::runtime_error("cannot lock release store");
            }
            Sleep(10);
        }
#else
        descriptor_ = ::open(path_.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0600);
        if (descriptor_ < 0) {
            throw std::runtime_error(
                "cannot open release store lock: " + std::string(std::strerror(errno)));
        }
        if (::flock(descriptor_, LOCK_EX) != 0) {
            const auto message = std::string(std::strerror(errno));
            ::close(descriptor_);
            descriptor_ = -1;
            throw std::runtime_error("cannot lock release store: " + message);
        }
#endif
    }
    ~StoreLock() {
#if defined(_WIN32)
        if (handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_);
#else
        if (descriptor_ >= 0) {
            (void)::flock(descriptor_, LOCK_UN);
            (void)::close(descriptor_);
        }
#endif
    }
    StoreLock(const StoreLock&) = delete;
    StoreLock& operator=(const StoreLock&) = delete;
private:
    std::filesystem::path path_;
#if defined(_WIN32)
    HANDLE handle_{INVALID_HANDLE_VALUE};
#else
    int descriptor_{-1};
#endif
};

void sync_file(const std::filesystem::path& path) {
#if defined(_WIN32)
    const auto handle = CreateFileW(
        path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE || !FlushFileBuffers(handle)) {
        if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
        throw std::runtime_error("cannot persist release state file");
    }
    CloseHandle(handle);
#else
    const auto descriptor = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (descriptor < 0) throw std::runtime_error("cannot open release state for sync");
    if (::fsync(descriptor) != 0) {
        const auto message = std::string(std::strerror(errno));
        ::close(descriptor);
        throw std::runtime_error("cannot persist release state: " + message);
    }
    ::close(descriptor);
#endif
}

void replace_file(const std::filesystem::path& source, const std::filesystem::path& destination) {
#if defined(_WIN32)
    if (!MoveFileExW(
            source.c_str(), destination.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        throw std::runtime_error("cannot atomically replace release state");
    }
#else
    std::filesystem::rename(source, destination);
#endif
}

void sync_directory(const std::filesystem::path& path) {
#if !defined(_WIN32)
    const auto descriptor = ::open(path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (descriptor < 0) throw std::runtime_error("cannot open release store for sync");
    if (::fsync(descriptor) != 0) {
        const auto message = std::string(std::strerror(errno));
        ::close(descriptor);
        throw std::runtime_error("cannot persist release store directory: " + message);
    }
    ::close(descriptor);
#else
    (void)path;
#endif
}

void sync_tree(const std::filesystem::path& root) {
    std::vector<std::filesystem::path> directories{root};
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (entry.is_regular_file()) sync_file(entry.path());
        else if (entry.is_directory()) directories.push_back(entry.path());
    }
    for (auto directory = directories.rbegin(); directory != directories.rend(); ++directory) {
        sync_directory(*directory);
    }
}

void verify_release_files(
    const ReleaseManifest& manifest, const ReleaseAudit& audit,
    const RuntimeBundle& bundle, const std::string& key,
    const std::filesystem::path& bundle_path,
    const std::filesystem::path& audit_path) {
    manifest.validate(key);
    audit.validate();
    if (!audit.promotion_ready) throw std::invalid_argument("release audit has not passed promotion gates");
    if (manifest.model_source_hash != bundle.model_source_hash ||
        manifest.model_source_hash != audit.model_source_hash ||
        manifest.bundle_id != bundle.bundle_id || manifest.bundle_hash != bundle.bundle_hash ||
        manifest.bundle_hash != audit.bundle_hash) {
        throw std::invalid_argument("release model/bundle/audit bindings differ");
    }
    if (manifest.bundle_sha256 != sha256_file(bundle_path) ||
        manifest.audit_sha256 != sha256_file(audit_path) ||
        manifest.audit_hash != audit.audit_hash) {
        throw std::invalid_argument("release artifact digest binding failed");
    }
    if (manifest.dataset_id != audit.dataset_id ||
        manifest.dataset_version != audit.dataset_version ||
        manifest.dataset_manifest_hash != audit.dataset_manifest_hash ||
        manifest.dataset_manifest_sha256 != audit.dataset_manifest_sha256) {
        throw std::invalid_argument("release dataset lineage differs from audit");
    }
}

void verify_payload_files(
    const ReleaseManifest& manifest,
    const std::filesystem::path& bundle_path,
    const std::filesystem::path& model_path,
    const std::filesystem::path& expert_path,
    const std::filesystem::path& certificate_path,
    const std::filesystem::path& dataset_manifest_path) {
    if (manifest.model_ir_sha256.empty() != model_path.empty()) {
        throw std::invalid_argument("release model payload presence differs from signed manifest");
    }
    if (manifest.expert_sha256.empty() != expert_path.empty() ||
        manifest.certificate_sha256.empty() != certificate_path.empty()) {
        throw std::invalid_argument("release payload presence differs from signed manifest");
    }
    if (!expert_path.empty() && manifest.expert_sha256 != sha256_file(expert_path)) {
        throw std::invalid_argument("release expert payload digest binding failed");
    }
    if (!model_path.empty()) {
        const auto model = ModelIR::read(model_path);
        if (manifest.model_ir_sha256 != sha256_file(model_path) ||
            manifest.model_source_hash != model.source_hash) {
            throw std::invalid_argument("release model IR payload digest/source binding failed");
        }
    }
    if (!certificate_path.empty() &&
        manifest.certificate_sha256 != sha256_file(certificate_path)) {
        throw std::invalid_argument("release certificate payload digest binding failed");
    }
    if (manifest.dataset_manifest_sha256.empty() != dataset_manifest_path.empty()) {
        throw std::invalid_argument(
            "release dataset manifest presence differs from signed manifest");
    }
    if (!dataset_manifest_path.empty()) {
        const auto dataset = DatasetManifest::read(dataset_manifest_path);
        if (manifest.dataset_manifest_sha256 != sha256_file(dataset_manifest_path) ||
            manifest.dataset_id != dataset.dataset_id ||
            manifest.dataset_version != dataset.version ||
            manifest.dataset_manifest_hash != dataset.manifest_hash) {
            throw std::invalid_argument("release dataset manifest digest/identity binding failed");
        }
    }
    if (!expert_path.empty()) {
        const auto model = ModelIR::read(model_path);
        const auto bundle = RuntimeBundle::read(bundle_path);
        const auto certificate = VerificationCertificate::read(certificate_path);
        Registry registry = make_default_registry(model);
        std::ifstream input(expert_path);
        std::string magic;
        input >> magic;
        std::string version;
        std::string artifact_hash;
        std::string fingerprint;
        if (magic == "SMAVE_AFFINE") {
            const auto artifact = AffineWarmStartArtifact::read(expert_path);
            version = artifact.expert_version;
            artifact_hash = artifact.artifact_hash;
            fingerprint = artifact.block_fingerprint;
            register_affine_expert(registry, artifact, "domain-v1", "default", "cpu", certificate);
        } else if (magic == "SMAVE_LINEAR_PC") {
            const auto artifact = LinearPreconditionerArtifact::read(expert_path);
            version = artifact.expert_version;
            artifact_hash = artifact.artifact_hash;
            fingerprint = artifact.block_fingerprint;
            register_linear_preconditioner(registry, artifact, "domain-v1", "default", "cpu", certificate);
        } else if (magic == "SMAVE_LEARNED_MULTIGRID") {
            const auto artifact = LearnedMultigridArtifact::read(expert_path);
            version = artifact.expert_version;
            artifact_hash = artifact.artifact_hash;
            fingerprint = artifact.block_fingerprint;
            register_learned_multigrid(
                registry, artifact, "domain-v1", "default", "cpu", certificate);
        } else if (magic == "SMAVE_LATENT_OPERATOR") {
            const auto artifact = LatentOperatorArtifact::read(expert_path);
            version = artifact.expert_version;
            artifact_hash = artifact.artifact_hash;
            fingerprint = artifact.block_fingerprint;
            register_latent_operator(registry, artifact, "domain-v1", "default", "cpu", certificate);
        } else {
            throw std::invalid_argument("unsupported release expert payload type");
        }
        if (certificate.expert_version != version ||
            certificate.artifact_hash != artifact_hash ||
            certificate.block_fingerprint != fingerprint) {
            throw std::invalid_argument("release certificate does not bind the expert payload");
        }
        registry.validate_bundle(bundle, model);
    }
}

void verify_parent_audit(
    const ReleaseAudit& audit, const std::filesystem::path& parent_path) {
    if (audit.stage == "shadow") {
        if (!parent_path.empty()) throw std::invalid_argument("shadow release cannot have parent audit");
        return;
    }
    if (parent_path.empty()) throw std::invalid_argument("canary release requires shadow parent artifact");
    const auto parent = ReleaseAudit::read(parent_path);
    if (parent.stage != "shadow" || !parent.promotion_ready ||
        parent.audit_hash != audit.parent_audit_hash ||
        sha256_file(parent_path) != audit.parent_audit_sha256 ||
        parent.model_source_hash != audit.model_source_hash ||
        parent.bundle_hash != audit.bundle_hash ||
        parent.dataset_id != audit.dataset_id ||
        parent.dataset_version != audit.dataset_version ||
        parent.dataset_manifest_hash != audit.dataset_manifest_hash ||
        parent.dataset_manifest_sha256 != audit.dataset_manifest_sha256) {
        throw std::invalid_argument("canary shadow parent artifact binding failed");
    }
}

}  // namespace

std::string sha256_file(const std::filesystem::path& path) {
    return hexadecimal(sha256(read_bytes(path)));
}

std::string sha256_text(std::string_view text) {
    return hexadecimal(sha256(text));
}

std::string hmac_sha256_text(std::string_view key, std::string_view text) {
    if (key.size() < 32) {
        throw std::invalid_argument("HMAC key must contain at least 256 bits");
    }
    return hmac_sha256(std::string(key), text);
}

bool verify_hmac_sha256_text(
    std::string_view key,
    std::string_view text,
    std::string_view expected) {
    if (key.size() < 32) return false;
    return constant_time_equal(
        expected, hmac_sha256(std::string(key), text));
}

void ReleaseAudit::seal() { audit_hash = hexadecimal(sha256(audit_contract(*this))); }

void ReleaseAudit::validate() const {
    if (schema_version != 2 && schema_version != 3) {
        throw std::invalid_argument("unsupported release audit schema");
    }
    const bool has_dataset = !dataset_id.empty() || !dataset_version.empty() ||
        !dataset_manifest_hash.empty() || !dataset_manifest_sha256.empty();
    if (schema_version == 2 && has_dataset) {
        throw std::invalid_argument("release audit v2 cannot contain dataset lineage");
    }
    if (schema_version >= 3 && has_dataset &&
        (dataset_id.empty() || dataset_version.empty() ||
         dataset_manifest_hash.empty() || dataset_manifest_sha256.empty())) {
        throw std::invalid_argument("release dataset lineage must be complete");
    }
    if (stage != "shadow" && stage != "canary") throw std::invalid_argument("release stage must be shadow or canary");
    if (requests == 0 || successful_requests > requests ||
        safety_evaluations == 0 || erroneous_accepts > safety_evaluations) {
        throw std::invalid_argument("invalid release request/safety counts");
    }
    if (!(safety_confidence_level > 0.0 && safety_confidence_level < 1.0) ||
        !(maximum_erroneous_accept_rate > 0.0 &&
          maximum_erroneous_accept_rate < 1.0) ||
        !(erroneous_accept_rate_upper_bound >= 0.0 &&
          erroneous_accept_rate_upper_bound <= 1.0)) {
        throw std::invalid_argument("invalid release safety confidence evidence");
    }
    if (minimum_top_k_pass_rate < 0.95 || minimum_top_k_pass_rate > 1.0) {
        throw std::invalid_argument("release pass-rate threshold must be within [0.95,1]");
    }
    if (observed_hours < 0.0 || minimum_observed_hours < 0.0 ||
        traffic_fraction < 0.0 || traffic_fraction > 1.0) {
        throw std::invalid_argument("invalid release duration or traffic fraction");
    }
    if ((stage == "shadow" && traffic_fraction != 0.0) ||
        (stage == "canary" && !(traffic_fraction > 0.0 && traffic_fraction <= 0.5))) {
        throw std::invalid_argument("release traffic fraction is incompatible with stage");
    }
    if ((stage == "shadow" && (!parent_audit_hash.empty() || !parent_audit_sha256.empty())) ||
        (stage == "canary" && (parent_audit_hash.empty() || parent_audit_sha256.empty()))) {
        throw std::invalid_argument("release audit parent chain is incompatible with stage");
    }
    const bool expected_safety = requests >= minimum_requests &&
        successful_requests == requests && erroneous_accepts == 0 &&
        erroneous_accept_rate_upper_bound <= maximum_erroneous_accept_rate &&
        original_solver_failures == 0 && top_k_pass_rate >= minimum_top_k_pass_rate;
    const bool expected_ready = expected_safety && performance_met &&
        observed_hours >= minimum_observed_hours;
    if (safety_met != expected_safety || promotion_ready != expected_ready) {
        throw std::invalid_argument("release gate booleans do not match evidence");
    }
    if (audit_hash != hexadecimal(sha256(audit_contract(*this)))) {
        throw std::invalid_argument("release audit integrity check failed");
    }
}

void ReleaseAudit::write(const std::filesystem::path& path) const {
    validate();
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write release audit: " + path.string());
    output << "SMAVE_RELEASE_AUDIT " << schema_version << '\n'
           << "STAGE " << std::quoted(stage) << '\n'
           << "MODEL " << std::quoted(model_source_hash) << '\n'
           << "BUNDLE " << std::quoted(bundle_hash) << '\n'
           << "VALIDATION_SHA256 " << std::quoted(validation_sha256) << '\n'
           << "PERFORMANCE_SHA256 " << std::quoted(performance_sha256) << '\n'
           << "PARENT_AUDIT_HASH " << std::quoted(parent_audit_hash) << '\n'
           << "PARENT_AUDIT_SHA256 " << std::quoted(parent_audit_sha256) << '\n'
           << (schema_version >= 3
               ? "DATASET_ID " + std::string{"\""} + dataset_id + "\"\n"
                 "DATASET_VERSION \"" + dataset_version + "\"\n"
                 "DATASET_MANIFEST_HASH \"" + dataset_manifest_hash + "\"\n"
                 "DATASET_MANIFEST_SHA256 \"" + dataset_manifest_sha256 + "\"\n"
               : "")
           << "REQUESTS " << requests << '\n'
           << "MIN_REQUESTS " << minimum_requests << '\n'
           << "SUCCESSFUL " << successful_requests << '\n'
           << "ERRONEOUS_ACCEPTS " << erroneous_accepts << '\n'
           << "SAFETY_EVALUATIONS " << safety_evaluations << '\n'
           << "SAFETY_CONFIDENCE_LEVEL " << std::setprecision(17)
           << safety_confidence_level << '\n'
           << "ERRONEOUS_ACCEPT_RATE_UPPER_BOUND "
           << erroneous_accept_rate_upper_bound << '\n'
           << "MAX_ERRONEOUS_ACCEPT_RATE " << maximum_erroneous_accept_rate << '\n'
           << "SOLVER_FAILURES " << original_solver_failures << '\n'
           << "TOP_K_PASS_RATE " << std::setprecision(17) << top_k_pass_rate << '\n'
           << "MIN_TOP_K_PASS_RATE " << minimum_top_k_pass_rate << '\n'
           << "OBSERVED_HOURS " << observed_hours << '\n'
           << "MIN_OBSERVED_HOURS " << minimum_observed_hours << '\n'
           << "TRAFFIC_FRACTION " << traffic_fraction << '\n'
           << "SAFETY_MET " << safety_met << '\n'
           << "PERFORMANCE_MET " << performance_met << '\n'
           << "PROMOTION_READY " << promotion_ready << '\n'
           << "HASH " << std::quoted(audit_hash) << "\nEND\n";
}

ReleaseAudit ReleaseAudit::read(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read release audit: " + path.string());
    tag(input, "SMAVE_RELEASE_AUDIT", "release audit");
    int version{}; input >> version;
    if (version != 2 && version != 3) {
        throw std::invalid_argument("unsupported release audit schema");
    }
    ReleaseAudit audit;
    audit.schema_version = version;
    tag(input,"STAGE","release audit"); input >> std::quoted(audit.stage);
    tag(input,"MODEL","release audit"); input >> std::quoted(audit.model_source_hash);
    tag(input,"BUNDLE","release audit"); input >> std::quoted(audit.bundle_hash);
    tag(input,"VALIDATION_SHA256","release audit"); input >> std::quoted(audit.validation_sha256);
    tag(input,"PERFORMANCE_SHA256","release audit"); input >> std::quoted(audit.performance_sha256);
    tag(input,"PARENT_AUDIT_HASH","release audit"); input >> std::quoted(audit.parent_audit_hash);
    tag(input,"PARENT_AUDIT_SHA256","release audit"); input >> std::quoted(audit.parent_audit_sha256);
    if (version >= 3) {
        tag(input,"DATASET_ID","release audit"); input >> std::quoted(audit.dataset_id);
        tag(input,"DATASET_VERSION","release audit"); input >> std::quoted(audit.dataset_version);
        tag(input,"DATASET_MANIFEST_HASH","release audit");
        input >> std::quoted(audit.dataset_manifest_hash);
        tag(input,"DATASET_MANIFEST_SHA256","release audit");
        input >> std::quoted(audit.dataset_manifest_sha256);
    }
    tag(input,"REQUESTS","release audit"); input >> audit.requests;
    tag(input,"MIN_REQUESTS","release audit"); input >> audit.minimum_requests;
    tag(input,"SUCCESSFUL","release audit"); input >> audit.successful_requests;
    tag(input,"ERRONEOUS_ACCEPTS","release audit"); input >> audit.erroneous_accepts;
    tag(input,"SAFETY_EVALUATIONS","release audit"); input >> audit.safety_evaluations;
    tag(input,"SAFETY_CONFIDENCE_LEVEL","release audit"); input >> audit.safety_confidence_level;
    tag(input,"ERRONEOUS_ACCEPT_RATE_UPPER_BOUND","release audit");
    input >> audit.erroneous_accept_rate_upper_bound;
    tag(input,"MAX_ERRONEOUS_ACCEPT_RATE","release audit");
    input >> audit.maximum_erroneous_accept_rate;
    tag(input,"SOLVER_FAILURES","release audit"); input >> audit.original_solver_failures;
    tag(input,"TOP_K_PASS_RATE","release audit"); input >> audit.top_k_pass_rate;
    tag(input,"MIN_TOP_K_PASS_RATE","release audit"); input >> audit.minimum_top_k_pass_rate;
    tag(input,"OBSERVED_HOURS","release audit"); input >> audit.observed_hours;
    tag(input,"MIN_OBSERVED_HOURS","release audit"); input >> audit.minimum_observed_hours;
    tag(input,"TRAFFIC_FRACTION","release audit"); input >> audit.traffic_fraction;
    tag(input,"SAFETY_MET","release audit"); input >> audit.safety_met;
    tag(input,"PERFORMANCE_MET","release audit"); input >> audit.performance_met;
    tag(input,"PROMOTION_READY","release audit"); input >> audit.promotion_ready;
    tag(input,"HASH","release audit"); input >> std::quoted(audit.audit_hash);
    tag(input,"END","release audit");
    audit.validate();
    return audit;
}

ReleaseAudit audit_release(
    const std::filesystem::path& model_path,
    const std::filesystem::path& bundle_path,
    const std::filesystem::path& validation_path,
    const std::filesystem::path& performance_path,
    std::string stage,
    double traffic_fraction,
    double observed_hours,
    std::size_t minimum_requests,
    double minimum_observed_hours,
    double minimum_top_k_pass_rate,
    const std::filesystem::path& parent_audit_path,
    const std::filesystem::path& dataset_manifest_path) {
    const auto model = ModelIR::read(model_path);
    const auto bundle = RuntimeBundle::read(bundle_path);
    if (bundle.model_source_hash != model.source_hash) {
        throw std::invalid_argument("release bundle targets another model");
    }
    std::ifstream validation_input(validation_path);
    std::string magic;
    int version{};
    validation_input >> magic >> version;
    if (!validation_input || magic != "SMAVE_VALIDATION" ||
        (version != 2 && version != 3)) {
        throw std::invalid_argument("release validation artifact has unsupported schema");
    }
    const auto validation = key_values(validation_path);
    ReleaseAudit audit;
    if (!dataset_manifest_path.empty()) {
        const auto dataset = DatasetManifest::read(dataset_manifest_path);
        if (version < 3 || validation.find("dataset_id") == validation.end() ||
            validation.find("dataset_version") == validation.end() ||
            validation.find("dataset_manifest_hash") == validation.end() ||
            validation.at("dataset_id") != dataset.dataset_id ||
            validation.at("dataset_version") != dataset.version ||
            validation.at("dataset_manifest_hash") != dataset.manifest_hash) {
            throw std::invalid_argument(
                "release validation report dataset lineage differs from manifest");
        }
        std::ifstream performance_input(performance_path);
        std::string performance_magic;
        int performance_version{};
        performance_input >> performance_magic >> performance_version;
        const auto performance_fields = key_values(performance_path);
        if (performance_magic != "SMAVE_OPERATOR_BENCHMARK" ||
            performance_version < 2 ||
            performance_fields.find("dataset_id") == performance_fields.end() ||
            performance_fields.find("dataset_version") == performance_fields.end() ||
            performance_fields.find("dataset_manifest_hash") == performance_fields.end() ||
            performance_fields.at("dataset_id") != dataset.dataset_id ||
            performance_fields.at("dataset_version") != dataset.version ||
            performance_fields.at("dataset_manifest_hash") != dataset.manifest_hash) {
            throw std::invalid_argument(
                "release performance report dataset lineage differs from manifest");
        }
        audit.dataset_id = dataset.dataset_id;
        audit.dataset_version = dataset.version;
        audit.dataset_manifest_hash = dataset.manifest_hash;
        audit.dataset_manifest_sha256 = sha256_file(dataset_manifest_path);
    }
    audit.stage = std::move(stage);
    audit.model_source_hash = model.source_hash;
    audit.bundle_hash = bundle.bundle_hash;
    audit.validation_sha256 = sha256_file(validation_path);
    audit.performance_sha256 = sha256_file(performance_path);
    if (audit.stage == "canary") {
        if (parent_audit_path.empty()) {
            throw std::invalid_argument("canary audit requires a passed shadow parent");
        }
        const auto parent = ReleaseAudit::read(parent_audit_path);
        if (parent.stage != "shadow" || !parent.promotion_ready ||
            parent.model_source_hash != audit.model_source_hash ||
            parent.bundle_hash != audit.bundle_hash ||
            parent.dataset_id != audit.dataset_id ||
            parent.dataset_version != audit.dataset_version ||
            parent.dataset_manifest_hash != audit.dataset_manifest_hash ||
            parent.dataset_manifest_sha256 != audit.dataset_manifest_sha256) {
            throw std::invalid_argument("canary parent is not a compatible passed shadow audit");
        }
        audit.parent_audit_hash = parent.audit_hash;
        audit.parent_audit_sha256 = sha256_file(parent_audit_path);
    } else if (!parent_audit_path.empty()) {
        throw std::invalid_argument("shadow audit cannot have a parent audit");
    }
    audit.requests = number<std::size_t>(validation, "scenarios");
    audit.minimum_requests = minimum_requests;
    audit.successful_requests = number<std::size_t>(validation, "successful_scenarios");
    audit.erroneous_accepts = number<std::size_t>(validation, "erroneous_accepts");
    audit.safety_evaluations = number<std::size_t>(validation, "safety_evaluations");
    audit.safety_confidence_level = number<double>(validation, "safety_confidence_level");
    audit.erroneous_accept_rate_upper_bound =
        number<double>(validation, "erroneous_accept_rate_upper_bound");
    audit.maximum_erroneous_accept_rate =
        number<double>(validation, "maximum_erroneous_accept_rate");
    const double recomputed_upper_bound = binomial_proportion_upper_bound(
        audit.erroneous_accepts, audit.safety_evaluations,
        audit.safety_confidence_level);
    if (std::abs(recomputed_upper_bound -
            audit.erroneous_accept_rate_upper_bound) > 1.0e-12) {
        throw std::invalid_argument(
            "release validation safety confidence bound does not match counts");
    }
    audit.original_solver_failures = number<std::size_t>(validation, "original_solver_failures");
    audit.top_k_pass_rate = number<double>(validation, "top_k_pass_rate");
    audit.minimum_top_k_pass_rate = minimum_top_k_pass_rate;
    audit.observed_hours = observed_hours;
    audit.minimum_observed_hours = minimum_observed_hours;
    audit.traffic_fraction = traffic_fraction;
    audit.safety_met = audit.requests >= minimum_requests &&
        audit.successful_requests == audit.requests && audit.erroneous_accepts == 0 &&
        audit.erroneous_accept_rate_upper_bound <=
            audit.maximum_erroneous_accept_rate &&
        audit.original_solver_failures == 0 && audit.top_k_pass_rate >= minimum_top_k_pass_rate;
    audit.performance_met = performance_passed(performance_path);
    audit.promotion_ready = audit.safety_met && audit.performance_met &&
        observed_hours >= minimum_observed_hours;
    audit.seal();
    audit.validate();
    return audit;
}

void ReleaseManifest::seal_and_sign(const std::string& key) {
    if (schema_version != 1 && schema_version != 2) {
        throw std::invalid_argument("unsupported release manifest schema");
    }
    const bool has_dataset = !dataset_id.empty() || !dataset_version.empty() ||
        !dataset_manifest_hash.empty() || !dataset_manifest_sha256.empty();
    if (schema_version == 1 && has_dataset) {
        throw std::invalid_argument("release manifest v1 cannot contain dataset lineage");
    }
    if (schema_version >= 2 && has_dataset &&
        (dataset_id.empty() || dataset_version.empty() ||
         dataset_manifest_hash.empty() || dataset_manifest_sha256.empty())) {
        throw std::invalid_argument("release manifest dataset lineage must be complete");
    }
    if (release_id.empty() || release_id.find('/') != std::string::npos ||
        release_id.find("..") != std::string::npos) {
        throw std::invalid_argument("release id is empty or unsafe");
    }
    if (created_utc.empty()) created_utc = utc_now();
    key_id = hexadecimal(sha256(key)).substr(0, 16);
    manifest_hash = hexadecimal(sha256(manifest_contract(*this)));
    signature = hmac_sha256(key, manifest_hash);
}

void ReleaseManifest::validate(const std::string& key) const {
    if (schema_version != 1 && schema_version != 2) {
        throw std::invalid_argument("unsupported release manifest schema");
    }
    const bool has_dataset = !dataset_id.empty() || !dataset_version.empty() ||
        !dataset_manifest_hash.empty() || !dataset_manifest_sha256.empty();
    if (schema_version == 1 && has_dataset) {
        throw std::invalid_argument("release manifest v1 cannot contain dataset lineage");
    }
    if (schema_version >= 2 && has_dataset &&
        (dataset_id.empty() || dataset_version.empty() ||
         dataset_manifest_hash.empty() || dataset_manifest_sha256.empty())) {
        throw std::invalid_argument("release manifest dataset lineage must be complete");
    }
    if (key.size() < 32) throw std::invalid_argument("release signing key must contain at least 256 bits");
    if (key_id != hexadecimal(sha256(key)).substr(0, 16) ||
        manifest_hash != hexadecimal(sha256(manifest_contract(*this))) ||
        !constant_time_equal(signature, hmac_sha256(key, manifest_hash))) {
        throw std::invalid_argument("release manifest signature verification failed");
    }
}

void ReleaseManifest::write(const std::filesystem::path& path) const {
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write release manifest: " + path.string());
    output << "SMAVE_RELEASE_MANIFEST " << schema_version << '\n'
           << "RELEASE " << std::quoted(release_id) << '\n'
           << "CREATED " << std::quoted(created_utc) << '\n'
           << "MODEL " << std::quoted(model_source_hash) << '\n'
           << "BUNDLE_ID " << std::quoted(bundle_id) << '\n'
           << "BUNDLE_HASH " << std::quoted(bundle_hash) << '\n'
           << "BUNDLE_SHA256 " << std::quoted(bundle_sha256) << '\n'
           << "AUDIT_HASH " << std::quoted(audit_hash) << '\n'
           << "AUDIT_SHA256 " << std::quoted(audit_sha256) << '\n'
           << "MODEL_IR_SHA256 " << std::quoted(model_ir_sha256) << '\n'
           << "EXPERT_SHA256 " << std::quoted(expert_sha256) << '\n'
           << "CERTIFICATE_SHA256 " << std::quoted(certificate_sha256) << '\n'
           << (schema_version >= 2
               ? "DATASET_ID " + std::string{"\""} + dataset_id + "\"\n"
                 "DATASET_VERSION \"" + dataset_version + "\"\n"
                 "DATASET_MANIFEST_HASH \"" + dataset_manifest_hash + "\"\n"
                 "DATASET_MANIFEST_SHA256 \"" + dataset_manifest_sha256 + "\"\n"
               : "")
           << "KEY_ID " << std::quoted(key_id) << '\n'
           << "MANIFEST_HASH " << std::quoted(manifest_hash) << '\n'
           << "SIGNATURE " << std::quoted(signature) << "\nEND\n";
}

ReleaseManifest ReleaseManifest::read(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read release manifest: " + path.string());
    tag(input,"SMAVE_RELEASE_MANIFEST","release manifest");
    int version{}; input >> version;
    if (version != 1 && version != 2) {
        throw std::invalid_argument("unsupported release manifest schema");
    }
    ReleaseManifest manifest;
    manifest.schema_version = version;
    tag(input,"RELEASE","release manifest"); input >> std::quoted(manifest.release_id);
    tag(input,"CREATED","release manifest"); input >> std::quoted(manifest.created_utc);
    tag(input,"MODEL","release manifest"); input >> std::quoted(manifest.model_source_hash);
    tag(input,"BUNDLE_ID","release manifest"); input >> std::quoted(manifest.bundle_id);
    tag(input,"BUNDLE_HASH","release manifest"); input >> std::quoted(manifest.bundle_hash);
    tag(input,"BUNDLE_SHA256","release manifest"); input >> std::quoted(manifest.bundle_sha256);
    tag(input,"AUDIT_HASH","release manifest"); input >> std::quoted(manifest.audit_hash);
    tag(input,"AUDIT_SHA256","release manifest"); input >> std::quoted(manifest.audit_sha256);
    tag(input,"MODEL_IR_SHA256","release manifest"); input >> std::quoted(manifest.model_ir_sha256);
    tag(input,"EXPERT_SHA256","release manifest"); input >> std::quoted(manifest.expert_sha256);
    tag(input,"CERTIFICATE_SHA256","release manifest"); input >> std::quoted(manifest.certificate_sha256);
    if (version >= 2) {
        tag(input,"DATASET_ID","release manifest"); input >> std::quoted(manifest.dataset_id);
        tag(input,"DATASET_VERSION","release manifest"); input >> std::quoted(manifest.dataset_version);
        tag(input,"DATASET_MANIFEST_HASH","release manifest");
        input >> std::quoted(manifest.dataset_manifest_hash);
        tag(input,"DATASET_MANIFEST_SHA256","release manifest");
        input >> std::quoted(manifest.dataset_manifest_sha256);
    }
    tag(input,"KEY_ID","release manifest"); input >> std::quoted(manifest.key_id);
    tag(input,"MANIFEST_HASH","release manifest"); input >> std::quoted(manifest.manifest_hash);
    tag(input,"SIGNATURE","release manifest"); input >> std::quoted(manifest.signature);
    tag(input,"END","release manifest");
    std::string trailing;
    if (input >> trailing) {
        throw std::invalid_argument("release manifest has trailing content");
    }
    return manifest;
}

ReleaseManifest create_release_manifest(
    const std::filesystem::path& bundle_path,
    const std::filesystem::path& audit_path,
    const std::filesystem::path& key_path,
    std::string release_id,
    const std::filesystem::path& model_path,
    const std::filesystem::path& expert_path,
    const std::filesystem::path& certificate_path,
    const std::filesystem::path& dataset_manifest_path) {
    const auto bundle = RuntimeBundle::read(bundle_path);
    const auto audit = ReleaseAudit::read(audit_path);
    if (!audit.promotion_ready || audit.model_source_hash != bundle.model_source_hash ||
        audit.bundle_hash != bundle.bundle_hash) {
        throw std::invalid_argument("release audit does not authorize this bundle");
    }
    ReleaseManifest manifest;
    manifest.release_id = std::move(release_id);
    manifest.model_source_hash = bundle.model_source_hash;
    manifest.bundle_id = bundle.bundle_id;
    manifest.bundle_hash = bundle.bundle_hash;
    manifest.bundle_sha256 = sha256_file(bundle_path);
    manifest.audit_hash = audit.audit_hash;
    manifest.audit_sha256 = sha256_file(audit_path);
    if (model_path.empty()) throw std::invalid_argument("release manifest requires model IR payload");
    const auto model = ModelIR::read(model_path);
    if (model.source_hash != manifest.model_source_hash) {
        throw std::invalid_argument("release model IR targets another source model");
    }
    manifest.model_ir_sha256 = sha256_file(model_path);
    if (expert_path.empty() != certificate_path.empty()) {
        throw std::invalid_argument("release expert and certificate payloads must be supplied together");
    }
    if (!expert_path.empty()) {
        manifest.expert_sha256 = sha256_file(expert_path);
        manifest.certificate_sha256 = sha256_file(certificate_path);
    }
    if (!dataset_manifest_path.empty()) {
        const auto dataset = DatasetManifest::read(dataset_manifest_path);
        manifest.dataset_id = dataset.dataset_id;
        manifest.dataset_version = dataset.version;
        manifest.dataset_manifest_hash = dataset.manifest_hash;
        manifest.dataset_manifest_sha256 = sha256_file(dataset_manifest_path);
    }
    if (manifest.dataset_id != audit.dataset_id ||
        manifest.dataset_version != audit.dataset_version ||
        manifest.dataset_manifest_hash != audit.dataset_manifest_hash ||
        manifest.dataset_manifest_sha256 != audit.dataset_manifest_sha256) {
        throw std::invalid_argument("release manifest dataset lineage differs from audit");
    }
    verify_payload_files(
        manifest, bundle_path, model_path, expert_path, certificate_path,
        dataset_manifest_path);
    manifest.seal_and_sign(read_key(key_path));
    return manifest;
}

void ReleaseState::seal() {
    if (schema_version != 1 && schema_version != 2) {
        throw std::invalid_argument("unsupported release state schema");
    }
    if (schema_version == 1 && (!key_id.empty() || !signature.empty())) {
        throw std::invalid_argument("release state v1 cannot contain authentication fields");
    }
    state_hash = hexadecimal(sha256(state_contract(*this)));
}

void ReleaseState::seal_and_sign(const std::string& key) {
    if (key.size() < 32) {
        throw std::invalid_argument("release signing key must contain at least 256 bits");
    }
    schema_version = 2;
    key_id = hexadecimal(sha256(key)).substr(0, 16);
    signature.clear();
    seal();
    signature = hmac_sha256(key, state_hash);
}

void ReleaseState::validate() const {
    if (schema_version != 1 && schema_version != 2) {
        throw std::invalid_argument("unsupported release state schema");
    }
    if ((schema_version == 1 && (!key_id.empty() || !signature.empty())) ||
        (schema_version == 2 && (key_id.empty() || signature.empty()))) {
        throw std::invalid_argument("release state authentication fields are invalid");
    }
    if (state_hash != hexadecimal(sha256(state_contract(*this)))) {
        throw std::invalid_argument("release state integrity check failed");
    }
}

void ReleaseState::validate(const std::string& key) const {
    validate();
    if (schema_version != 2 || key.size() < 32 ||
        key_id != hexadecimal(sha256(key)).substr(0, 16) ||
        !constant_time_equal(signature, hmac_sha256(key, state_hash))) {
        throw std::invalid_argument("release state signature verification failed");
    }
}

void ReleaseState::write_atomic(const std::filesystem::path& path) const {
    validate();
    std::filesystem::create_directories(path.parent_path());
    const auto temporary = path.string() + ".tmp." +
        hexadecimal(sha256(path.string() + state_hash));
    {
        std::ofstream output(temporary, std::ios::trunc);
        if (!output) throw std::runtime_error("cannot write release state");
        output << "SMAVE_RELEASE_STATE " << schema_version
               << "\nGENERATION " << generation
               << "\nCURRENT " << std::quoted(current_release)
               << "\nPREVIOUS " << std::quoted(previous_release)
               << (schema_version >= 2
                   ? "\nKEY_ID \"" + key_id + "\""
                   : "")
               << "\nHASH " << std::quoted(state_hash)
               << (schema_version >= 2
                   ? "\nSIGNATURE \"" + signature + "\""
                   : "")
               << "\nEND\n";
        output.flush();
        if (!output) throw std::runtime_error("cannot flush release state");
    }
    sync_file(temporary);
    replace_file(temporary, path);
    sync_directory(path.parent_path());
}

ReleaseState ReleaseState::read(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("release store has no active state");
    tag(input,"SMAVE_RELEASE_STATE","release state");
    int version{}; input >> version;
    if (version != 1 && version != 2) {
        throw std::invalid_argument("unsupported release state schema");
    }
    ReleaseState state;
    state.schema_version = version;
    tag(input,"GENERATION","release state"); input >> state.generation;
    tag(input,"CURRENT","release state"); input >> std::quoted(state.current_release);
    tag(input,"PREVIOUS","release state"); input >> std::quoted(state.previous_release);
    if (version >= 2) {
        tag(input,"KEY_ID","release state"); input >> std::quoted(state.key_id);
    }
    tag(input,"HASH","release state"); input >> std::quoted(state.state_hash);
    if (version >= 2) {
        tag(input,"SIGNATURE","release state"); input >> std::quoted(state.signature);
    }
    tag(input,"END","release state");
    state.validate();
    return state;
}

namespace {

std::optional<ReleaseState> read_release_state_if_valid(
    const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) return std::nullopt;
    try {
        return ReleaseState::read(path);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::filesystem::path state_history_path(
    const std::filesystem::path& root, std::size_t generation) {
    std::ostringstream name;
    name << std::setfill('0') << std::setw(20) << generation << ".state";
    return root / "state-history" / name.str();
}

bool same_release_state(const ReleaseState& left, const ReleaseState& right) {
    return left.schema_version == right.schema_version &&
        left.generation == right.generation &&
        left.current_release == right.current_release &&
        left.previous_release == right.previous_release &&
        left.key_id == right.key_id && left.state_hash == right.state_hash &&
        left.signature == right.signature;
}

std::optional<ReleaseState> load_release_state(
    const std::filesystem::path& root, bool repair_primary) {
    auto selected = read_release_state_if_valid(root / "state");
    const auto history = root / "state-history";
    if (std::filesystem::exists(history)) {
        for (const auto& entry : std::filesystem::directory_iterator(history)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".state") continue;
            const auto candidate = read_release_state_if_valid(entry.path());
            if (!candidate.has_value()) {
                throw std::invalid_argument(
                    "release state history contains a corrupt committed generation");
            }
            if (!selected.has_value() || candidate->generation > selected->generation) {
                selected = candidate;
            } else if (candidate->generation == selected->generation &&
                       !same_release_state(*candidate, *selected)) {
                throw std::invalid_argument(
                    "release state history contains divergent generations");
            }
        }
    }
    if (selected.has_value() && repair_primary) {
        const auto primary = read_release_state_if_valid(root / "state");
        if (!primary.has_value() || !same_release_state(*primary, *selected)) {
            selected->write_atomic(root / "state");
        }
    }
    return selected;
}

void commit_release_state(
    const std::filesystem::path& root, const ReleaseState& state) {
    const auto history_path = state_history_path(root, state.generation);
    if (std::filesystem::exists(history_path)) {
        const auto existing = ReleaseState::read(history_path);
        if (!same_release_state(existing, state)) {
            throw std::invalid_argument(
                "release state generation is immutable and already differs");
        }
    } else {
        state.write_atomic(history_path);
    }
    state.write_atomic(root / "state");
}

void verify_stored_release(
    const std::filesystem::path& directory,
    const ReleaseManifest& expected_manifest,
    const std::string& key) {
    const auto manifest = ReleaseManifest::read(directory / "release.manifest");
    const auto audit = ReleaseAudit::read(directory / "release.audit");
    const auto bundle = RuntimeBundle::read(directory / "runtime.bundle");
    verify_release_files(
        manifest, audit, bundle, key,
        directory / "runtime.bundle", directory / "release.audit");
    verify_parent_audit(
        audit, std::filesystem::exists(directory / "parent.audit")
            ? directory / "parent.audit" : std::filesystem::path{});
    verify_payload_files(
        manifest,
        directory / "runtime.bundle",
        directory / "model.ir",
        std::filesystem::exists(directory / "expert.artifact")
            ? directory / "expert.artifact" : std::filesystem::path{},
        std::filesystem::exists(directory / "certificate.verify")
            ? directory / "certificate.verify" : std::filesystem::path{},
        std::filesystem::exists(directory / "dataset.manifest")
            ? directory / "dataset.manifest" : std::filesystem::path{});
    if (manifest.manifest_hash != expected_manifest.manifest_hash ||
        manifest.signature != expected_manifest.signature) {
        throw std::invalid_argument("release id is immutable and already differs");
    }
}

void verify_state_authority(
    const std::filesystem::path& root,
    const ReleaseState& state,
    const std::string& key,
    bool allow_legacy_migration) {
    if (state.schema_version >= 2) {
        state.validate(key);
        return;
    }
    if (!allow_legacy_migration || state.current_release.empty()) {
        throw std::invalid_argument(
            "legacy release state is readable but not authorized for this operation");
    }
    const auto directory = root / "releases" / state.current_release;
    const auto manifest = ReleaseManifest::read(directory / "release.manifest");
    verify_stored_release(directory, manifest, key);
}

}  // namespace

ReleaseStore::ReleaseStore(std::filesystem::path root) : root_(std::move(root)) {
    if (root_.empty()) throw std::invalid_argument("release store path is empty");
}

ReleaseState ReleaseStore::activate(
    const std::filesystem::path& manifest_path,
    const std::filesystem::path& bundle_path,
    const std::filesystem::path& audit_path,
    const std::filesystem::path& parent_audit_path,
    const std::filesystem::path& model_path,
    const std::filesystem::path& expert_path,
    const std::filesystem::path& certificate_path,
    const std::filesystem::path& key_path,
    const std::filesystem::path& dataset_manifest_path) {
    StoreLock lock(root_);
    const auto key = read_key(key_path);
    const auto manifest = ReleaseManifest::read(manifest_path);
    const auto audit = ReleaseAudit::read(audit_path);
    const auto bundle = RuntimeBundle::read(bundle_path);
    verify_release_files(manifest, audit, bundle, key, bundle_path, audit_path);
    verify_parent_audit(audit, parent_audit_path);
    verify_payload_files(
        manifest, bundle_path, model_path, expert_path, certificate_path,
        dataset_manifest_path);
    const auto releases = root_ / "releases";
    const auto destination = releases / manifest.release_id;
    std::filesystem::create_directories(releases);
    if (std::filesystem::exists(destination)) {
        verify_stored_release(destination, manifest, key);
    } else {
        const auto staging = releases / ("." + manifest.release_id + ".staging");
        std::filesystem::remove_all(staging);
        std::filesystem::create_directories(staging);
        std::filesystem::copy_file(manifest_path, staging / "release.manifest");
        std::filesystem::copy_file(bundle_path, staging / "runtime.bundle");
        std::filesystem::copy_file(audit_path, staging / "release.audit");
        std::filesystem::copy_file(model_path, staging / "model.ir");
        if (!parent_audit_path.empty()) {
            std::filesystem::copy_file(parent_audit_path, staging / "parent.audit");
        }
        if (!expert_path.empty()) {
            std::filesystem::copy_file(expert_path, staging / "expert.artifact");
            std::filesystem::copy_file(certificate_path, staging / "certificate.verify");
        }
        if (!dataset_manifest_path.empty()) {
            std::filesystem::copy_file(
                dataset_manifest_path, staging / "dataset.manifest");
        }
        verify_stored_release(staging, manifest, key);
        sync_tree(staging);
        std::filesystem::rename(staging, destination);
        sync_directory(releases);
    }
    const auto old = load_release_state(root_, true);
    if (old.has_value()) verify_state_authority(root_, *old, key, true);
    if (old.has_value() && old->current_release == manifest.release_id) {
        throw std::invalid_argument("release id is immutable and already active");
    }
    ReleaseState state;
    if (old.has_value()) {
        state.generation = old->generation + 1;
        state.previous_release = old->current_release;
    } else {
        state.generation = 1;
    }
    state.current_release = manifest.release_id;
    state.seal_and_sign(key);
    commit_release_state(root_, state);
    return state;
}

ReleaseState ReleaseStore::rollback(const std::filesystem::path& key_path) {
    StoreLock lock(root_);
    const auto loaded = load_release_state(root_, true);
    if (!loaded.has_value()) throw std::runtime_error("release store has no active state");
    const auto old = *loaded;
    const auto key = read_key(key_path);
    verify_state_authority(root_, old, key, false);
    if (old.previous_release.empty()) throw std::invalid_argument("release store has no rollback target");
    const auto target = root_ / "releases" / old.previous_release;
    const auto manifest = ReleaseManifest::read(target / "release.manifest");
    const auto audit = ReleaseAudit::read(target / "release.audit");
    const auto bundle = RuntimeBundle::read(target / "runtime.bundle");
    verify_release_files(manifest, audit, bundle, key,
        target / "runtime.bundle", target / "release.audit");
    verify_parent_audit(
        audit, std::filesystem::exists(target / "parent.audit")
            ? target / "parent.audit"
            : std::filesystem::path{});
    verify_payload_files(
        manifest,
        target / "runtime.bundle",
        target / "model.ir",
        std::filesystem::exists(target / "expert.artifact")
            ? target / "expert.artifact" : std::filesystem::path{},
        std::filesystem::exists(target / "certificate.verify")
            ? target / "certificate.verify" : std::filesystem::path{},
        std::filesystem::exists(target / "dataset.manifest")
            ? target / "dataset.manifest" : std::filesystem::path{});
    ReleaseState state;
    state.generation = old.generation + 1;
    state.current_release = old.previous_release;
    state.previous_release = old.current_release;
    state.seal_and_sign(key);
    commit_release_state(root_, state);
    return state;
}

ReleaseState ReleaseStore::status() const {
    StoreLock lock(root_);
    const auto state = load_release_state(root_, true);
    if (!state.has_value()) throw std::runtime_error("release store has no active state");
    return *state;
}

VerifiedRelease ReleaseStore::verified_active(
    const std::filesystem::path& key_path) const {
    StoreLock lock(root_);
    const auto loaded = load_release_state(root_, true);
    if (!loaded.has_value()) throw std::runtime_error("release store has no active state");
    const auto state = *loaded;
    if (state.current_release.empty()) throw std::invalid_argument("release store has no active release");
    const auto directory = root_ / "releases" / state.current_release;
    const auto key = read_key(key_path);
    verify_state_authority(root_, state, key, false);
    const auto manifest = ReleaseManifest::read(directory / "release.manifest");
    const auto audit = ReleaseAudit::read(directory / "release.audit");
    const auto bundle = RuntimeBundle::read(directory / "runtime.bundle");
    verify_release_files(manifest, audit, bundle, key,
        directory / "runtime.bundle", directory / "release.audit");
    verify_parent_audit(
        audit, std::filesystem::exists(directory / "parent.audit")
            ? directory / "parent.audit"
            : std::filesystem::path{});
    verify_payload_files(
        manifest,
        directory / "runtime.bundle",
        directory / "model.ir",
        std::filesystem::exists(directory / "expert.artifact")
            ? directory / "expert.artifact" : std::filesystem::path{},
        std::filesystem::exists(directory / "certificate.verify")
            ? directory / "certificate.verify" : std::filesystem::path{},
        std::filesystem::exists(directory / "dataset.manifest")
            ? directory / "dataset.manifest" : std::filesystem::path{});
    if (manifest.release_id != state.current_release) {
        throw std::invalid_argument("active release state and signed manifest differ");
    }
    return {.state = state, .manifest = manifest, .directory = directory};
}

ReleaseManifest ReleaseStore::verify_active(
    const std::filesystem::path& key_path) const {
    return verified_active(key_path).manifest;
}

std::filesystem::path ReleaseStore::active_directory() const {
    const auto state = status();
    if (state.current_release.empty()) throw std::invalid_argument("release store has no active release");
    return root_ / "releases" / state.current_release;
}

}  // namespace smave
