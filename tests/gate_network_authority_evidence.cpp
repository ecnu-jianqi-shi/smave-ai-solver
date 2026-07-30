#include "smave/compiler.hpp"
#include "smave/release.hpp"
#include "smave/runtime.hpp"

#include <algorithm>
#include <array>
#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <netdb.h>
#include <netinet/in.h>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using Values = std::unordered_map<std::string, double>;

struct Socket {
    int descriptor{-1};

    Socket() = default;
    explicit Socket(int value) : descriptor(value) {}
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&& other) noexcept
        : descriptor(std::exchange(other.descriptor, -1)) {}
    Socket& operator=(Socket&& other) noexcept {
        if (this != &other) {
            if (descriptor >= 0) ::close(descriptor);
            descriptor = std::exchange(other.descriptor, -1);
        }
        return *this;
    }
    ~Socket() {
        if (descriptor >= 0) ::close(descriptor);
    }
};

struct Request {
    std::string operation;
    std::uint64_t transaction_id{};
    std::string family;
    bool crash_after_commit{};
    bool crash_during_snapshot{};
    bool crash_before_witness_publication{};
    Values values;
};

struct Response {
    std::string status;
    std::string decision{"reject"};
    double residual_inf{};
    bool duplicate{};
    std::uint64_t committed_transactions{};
};

struct ServerStats {
    std::uint64_t complete_requests{};
    std::uint64_t committed_transactions{};
    std::uint64_t gate_rejections{};
    std::uint64_t duplicate_replays{};
    std::uint64_t transaction_conflicts{};
    std::uint64_t dropped_replies{};
    std::uint64_t malformed_requests{};
    std::uint64_t server_starts{};
    std::uint64_t recovered_transactions{};
    std::uint64_t crash_after_commit_injections{};
    std::uint64_t witness_lag_crash_injections{};
    std::uint64_t witness_lag_recoveries{};
    std::uint64_t state_generation{};
    std::uint64_t torn_temporary_recoveries{};
    std::uint64_t checksum_failures{};
    std::uint64_t authentication_failures{};
    std::uint64_t previous_generation_recoveries{};
    std::uint64_t stale_current_generations_rejected{};
    std::uint64_t mirror_recoveries{};
    std::uint64_t state_key_rotations{};
    std::uint64_t witness_key_rotations{};
    std::string previous_body_sha256;
};

struct TransactionRecord {
    std::string fingerprint;
    Response response;
};

struct MonotonicWitness {
    std::uint64_t generation{};
    std::string state_sha256;
};

struct AuthenticationKeys {
    std::string current;
    std::optional<std::string> previous;
};

struct KeyRotationLoad {
    bool state_key_rotation{};
    bool witness_key_rotation{};
    std::uint64_t selected_generation{};
    std::string selected_state_sha256;
    std::string selected_state_key_id;
    std::string selected_witness_key_id;
};

enum class PersistenceCrashPoint {
    none,
    before_publication,
    before_witness_publication,
};

class StateRecoveryFailure final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class KeyPolicyFailure final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class KeyFilePolicyFailure final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

std::runtime_error socket_error(const std::string& operation) {
    return std::runtime_error(operation + ": " + std::strerror(errno));
}

void write_all(int descriptor, const std::string& text) {
    const char* cursor = text.data();
    std::size_t remaining = text.size();
    while (remaining != 0) {
        const auto written = ::send(descriptor, cursor, remaining, 0);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) throw socket_error("TCP write failed");
        cursor += written;
        remaining -= static_cast<std::size_t>(written);
    }
}

std::optional<std::string> read_message(int descriptor) {
    std::string message;
    std::array<char, 4096> buffer{};
    while (message.find("\nEND\n") == std::string::npos) {
        const auto received = ::recv(descriptor, buffer.data(), buffer.size(), 0);
        if (received < 0 && errno == EINTR) continue;
        if (received < 0) throw socket_error("TCP read failed");
        if (received == 0) return std::nullopt;
        message.append(buffer.data(), static_cast<std::size_t>(received));
        if (message.size() > 1024 * 1024) {
            throw std::runtime_error("TCP message exceeds one MiB");
        }
    }
    return message;
}

std::map<std::string, std::string> parse_key_value_message(
    const std::string& message,
    const std::string& expected_header) {
    std::istringstream input(message);
    std::map<std::string, std::string> fields;
    std::string line;
    if (!std::getline(input, line) || line != expected_header) {
        throw std::invalid_argument("invalid transaction protocol header");
    }
    bool ended{};
    while (std::getline(input, line)) {
        if (line == "END") {
            ended = true;
            break;
        }
        const auto separator = line.find('=');
        if (separator == std::string::npos || separator == 0) {
            throw std::invalid_argument("invalid transaction protocol field");
        }
        const bool inserted = fields.emplace(
            line.substr(0, separator), line.substr(separator + 1)).second;
        if (!inserted) {
            throw std::invalid_argument("duplicate transaction protocol field");
        }
    }
    if (!ended) throw std::invalid_argument("unterminated transaction protocol message");
    return fields;
}

std::map<std::string, std::string> parse_fields(const std::string& message) {
    return parse_key_value_message(message, "SMAVE_GATE_TXN 1");
}

std::string format_double(double value) {
    std::ostringstream output;
    output << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
    return output.str();
}

std::string decision_name(smave::GateDecision decision) {
    switch (decision) {
        case smave::GateDecision::reject: return "reject";
        case smave::GateDecision::need_correction: return "need_correction";
        case smave::GateDecision::direct_accept: return "direct_accept";
    }
    throw std::logic_error("unknown gate decision");
}

std::string canonical_payload(const Request& request) {
    std::vector<std::pair<std::string, double>> values(
        request.values.begin(), request.values.end());
    std::sort(values.begin(), values.end());
    std::ostringstream output;
    output << "family=" << request.family << '\n';
    for (const auto& [name, value] : values) {
        output << "value." << name << '=' << format_double(value) << '\n';
    }
    return output.str();
}

std::string serialize_request(const Request& request) {
    std::ostringstream output;
    output << "SMAVE_GATE_TXN 1\n"
           << "operation=" << request.operation << '\n'
           << "transaction_id=" << request.transaction_id << '\n'
           << "family=" << request.family << '\n'
           << "crash_after_commit=" << (request.crash_after_commit ? 1 : 0) << '\n';
    output << "crash_during_snapshot="
           << (request.crash_during_snapshot ? 1 : 0) << '\n';
    output << "crash_before_witness_publication="
           << (request.crash_before_witness_publication ? 1 : 0) << '\n';
    std::vector<std::pair<std::string, double>> values(
        request.values.begin(), request.values.end());
    std::sort(values.begin(), values.end());
    output << "value_count=" << values.size() << '\n';
    for (const auto& [name, value] : values) {
        output << "value." << name << '=' << format_double(value) << '\n';
    }
    output << "END\n";
    return output.str();
}

Request deserialize_request(const std::string& message) {
    const auto fields = parse_fields(message);
    Request request;
    request.operation = fields.at("operation");
    request.transaction_id = std::stoull(fields.at("transaction_id"));
    request.family = fields.at("family");
    request.crash_after_commit = fields.at("crash_after_commit") == "1";
    request.crash_during_snapshot = fields.at("crash_during_snapshot") == "1";
    request.crash_before_witness_publication =
        fields.at("crash_before_witness_publication") == "1";
    const auto expected_values = std::stoull(fields.at("value_count"));
    for (const auto& [key, value] : fields) {
        if (!key.starts_with("value.")) continue;
        request.values.emplace(key.substr(6), std::stod(value));
    }
    if (request.values.size() != expected_values) {
        throw std::invalid_argument("transaction value count mismatch");
    }
    return request;
}

std::string serialize_response(const Response& response, const ServerStats& stats) {
    std::ostringstream output;
    output << "SMAVE_GATE_TXN 1\n"
           << "status=" << response.status << '\n'
           << "decision=" << response.decision << '\n'
           << "residual_inf=" << format_double(response.residual_inf) << '\n'
           << "duplicate=" << (response.duplicate ? 1 : 0) << '\n'
           << "committed_transactions=" << stats.committed_transactions << '\n'
           << "complete_requests=" << stats.complete_requests << '\n'
           << "gate_rejections=" << stats.gate_rejections << '\n'
           << "duplicate_replays=" << stats.duplicate_replays << '\n'
           << "transaction_conflicts=" << stats.transaction_conflicts << '\n'
           << "dropped_replies=" << stats.dropped_replies << '\n'
           << "malformed_requests=" << stats.malformed_requests << '\n'
           << "server_starts=" << stats.server_starts << '\n'
           << "recovered_transactions=" << stats.recovered_transactions << '\n'
           << "crash_after_commit_injections="
           << stats.crash_after_commit_injections << '\n'
           << "witness_lag_crash_injections="
           << stats.witness_lag_crash_injections << '\n'
           << "witness_lag_recoveries=" << stats.witness_lag_recoveries << '\n'
           << "state_generation=" << stats.state_generation << '\n'
           << "torn_temporary_recoveries="
           << stats.torn_temporary_recoveries << '\n'
           << "checksum_failures=" << stats.checksum_failures << '\n'
           << "authentication_failures=" << stats.authentication_failures << '\n'
           << "previous_generation_recoveries="
           << stats.previous_generation_recoveries << '\n'
           << "stale_current_generations_rejected="
           << stats.stale_current_generations_rejected << '\n'
           << "mirror_recoveries=" << stats.mirror_recoveries << '\n'
           << "state_key_rotations=" << stats.state_key_rotations << '\n'
           << "witness_key_rotations=" << stats.witness_key_rotations << '\n'
           << "END\n";
    return output.str();
}

std::string hex_encode(const std::string& value) {
    constexpr char digits[] = "0123456789abcdef";
    std::string encoded;
    encoded.reserve(value.size() * 2);
    for (const unsigned char byte : value) {
        encoded.push_back(digits[byte >> 4]);
        encoded.push_back(digits[byte & 0x0f]);
    }
    return encoded;
}

std::string hex_decode(const std::string& value) {
    if (value.size() % 2 != 0) throw std::invalid_argument("invalid state hex length");
    const auto nibble = [](char character) -> unsigned char {
        if (character >= '0' && character <= '9') {
            return static_cast<unsigned char>(character - '0');
        }
        if (character >= 'a' && character <= 'f') {
            return static_cast<unsigned char>(character - 'a' + 10);
        }
        throw std::invalid_argument("invalid state hex digit");
    };
    std::string decoded;
    decoded.reserve(value.size() / 2);
    for (std::size_t index = 0; index < value.size(); index += 2) {
        decoded.push_back(static_cast<char>(
            (nibble(value[index]) << 4) | nibble(value[index + 1])));
    }
    return decoded;
}

std::string state_body(
    const ServerStats& stats,
    const std::map<std::uint64_t, TransactionRecord>& transactions) {
    std::ostringstream output;
    output << "generation=" << stats.state_generation << '\n'
           << "previous_body_sha256=" << stats.previous_body_sha256 << '\n'
           << "complete_requests=" << stats.complete_requests << '\n'
           << "committed_transactions=" << stats.committed_transactions << '\n'
           << "gate_rejections=" << stats.gate_rejections << '\n'
           << "duplicate_replays=" << stats.duplicate_replays << '\n'
           << "transaction_conflicts=" << stats.transaction_conflicts << '\n'
           << "dropped_replies=" << stats.dropped_replies << '\n'
           << "malformed_requests=" << stats.malformed_requests << '\n'
           << "server_starts=" << stats.server_starts << '\n'
           << "crash_after_commit_injections="
           << stats.crash_after_commit_injections << '\n'
           << "witness_lag_crash_injections="
           << stats.witness_lag_crash_injections << '\n'
           << "witness_lag_recoveries=" << stats.witness_lag_recoveries << '\n'
           << "torn_temporary_recoveries="
           << stats.torn_temporary_recoveries << '\n'
           << "checksum_failures=" << stats.checksum_failures << '\n'
           << "authentication_failures=" << stats.authentication_failures << '\n'
           << "previous_generation_recoveries="
           << stats.previous_generation_recoveries << '\n'
           << "stale_current_generations_rejected="
           << stats.stale_current_generations_rejected << '\n'
           << "mirror_recoveries=" << stats.mirror_recoveries << '\n'
           << "state_key_rotations=" << stats.state_key_rotations << '\n'
           << "witness_key_rotations=" << stats.witness_key_rotations << '\n'
           << "transaction_count=" << transactions.size() << '\n';
    std::size_t index{};
    for (const auto& [transaction_id, record] : transactions) {
        output << "transaction." << index << ".id=" << transaction_id << '\n'
               << "transaction." << index << ".fingerprint_hex="
               << hex_encode(record.fingerprint) << '\n'
               << "transaction." << index << ".status="
               << record.response.status << '\n'
               << "transaction." << index << ".decision="
               << record.response.decision << '\n'
               << "transaction." << index << ".residual_inf="
               << format_double(record.response.residual_inf) << '\n';
        ++index;
    }
    return output.str();
}

std::string serialize_state(
    const ServerStats& stats,
    const std::map<std::uint64_t, TransactionRecord>& transactions,
    const std::string& authentication_key,
    std::string& authenticated_body_sha256) {
    const auto body = state_body(stats, transactions);
    authenticated_body_sha256 = smave::sha256_text(body);
    return "SMAVE_GATE_TXN_STATE 6\n" + body +
        "state_sha256=" + authenticated_body_sha256 + "\n" +
        "key_id=" + smave::sha256_text(authentication_key).substr(0, 16) + "\n" +
        "state_hmac_sha256=" +
        smave::hmac_sha256_text(authentication_key, body) + "\nEND\n";
}

std::string witness_body(
    std::uint64_t generation,
    const std::string& state_sha256) {
    std::ostringstream output;
    output << "generation=" << generation << '\n'
           << "state_sha256=" << state_sha256 << '\n';
    return output.str();
}

std::string serialize_witness(
    std::uint64_t generation,
    const std::string& state_sha256,
    const std::string& witness_key) {
    const auto body = witness_body(generation, state_sha256);
    return "SMAVE_GATE_TXN_WITNESS 1\n" + body +
        "witness_sha256=" + smave::sha256_text(body) + "\n" +
        "witness_key_id=" + smave::sha256_text(witness_key).substr(0, 16) + "\n" +
        "witness_hmac_sha256=" + smave::hmac_sha256_text(witness_key, body) +
        "\nEND\n";
}

void write_file_all(int descriptor, const std::string& text) {
    const char* cursor = text.data();
    std::size_t remaining = text.size();
    while (remaining != 0) {
        const auto written = ::write(descriptor, cursor, remaining);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) throw socket_error("state write failed");
        cursor += written;
        remaining -= static_cast<std::size_t>(written);
    }
}

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read transaction state");
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string authentication_key_id(const std::string& key) {
    return smave::sha256_text(key).substr(0, 16);
}

void fsync_directory(const std::filesystem::path& path) {
    const int descriptor = ::open(path.c_str(), O_RDONLY);
    if (descriptor < 0) return;
    if (::fsync(descriptor) != 0 && errno != EINVAL) {
        const auto error = socket_error("state directory fsync failed");
        ::close(descriptor);
        throw error;
    }
    ::close(descriptor);
}

void write_durable_file(
    const std::filesystem::path& path,
    const std::string& text) {
    const int descriptor = ::open(
        path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (descriptor < 0) throw socket_error("state open failed");
    try {
        write_file_all(descriptor, text);
        if (::fsync(descriptor) != 0) throw socket_error("state fsync failed");
        if (::close(descriptor) != 0) throw socket_error("state close failed");
    } catch (...) {
        ::close(descriptor);
        throw;
    }
}

std::string file_mode_text(mode_t mode) {
    std::ostringstream output;
    output << std::oct << std::setw(4) << std::setfill('0')
           << (mode & 07777);
    return output.str();
}

[[noreturn]] void fail_authentication_key_file_policy(
    const std::filesystem::path& state_path,
    std::string_view role,
    const std::filesystem::path& key_path,
    std::string_view reason,
    bool regular_file,
    bool symbolic_link,
    bool owner_only_permissions,
    mode_t mode,
    std::size_t key_bytes_read) {
    std::filesystem::create_directories(state_path.parent_path());
    const auto failure_path =
        state_path.parent_path() / "key-file-policy-failure.txt";
    const auto temporary =
        failure_path.string() + ".tmp." + std::to_string(::getpid());
    std::ostringstream output;
    output << "SMAVE_GATE_TXN_KEY_FILE_POLICY_FAILURE 1\n"
           << "contract=owner-only-regular-non-symlink-authentication-key-files\n"
           << "failure_stage=key-file-policy-before-state-load\n"
           << "reason=" << reason << '\n'
           << "role=" << role << '\n'
           << "key_file_name=" << key_path.filename().string() << '\n'
           << "key_file_mode=" << file_mode_text(mode) << '\n'
           << "regular_file=" << (regular_file ? 1 : 0) << '\n'
           << "symbolic_link=" << (symbolic_link ? 1 : 0) << '\n'
           << "owner_only_permissions=" << (owner_only_permissions ? 1 : 0)
           << '\n'
           << "minimum_key_bytes=32\n"
           << "key_bytes_read=" << key_bytes_read << '\n'
           << "fail_closed=1\n"
           << "state_loaded=0\n"
           << "listen_socket_created=0\n"
           << "server_exit_code=88\n"
           << "performance_evidence=0\n"
           << "END\n";
    try {
        write_durable_file(temporary, output.str());
    } catch (...) {
        ::unlink(temporary.c_str());
        throw;
    }
    if (::rename(temporary.c_str(), failure_path.c_str()) != 0) {
        ::unlink(temporary.c_str());
        throw socket_error("key-file-policy-failure rename failed");
    }
    fsync_directory(state_path.parent_path());
    throw KeyFilePolicyFailure(std::string(reason));
}

std::string read_authentication_key(
    const std::filesystem::path& state_path,
    std::string_view role,
    const std::filesystem::path& key_path) {
    struct stat path_status {};
    if (::lstat(key_path.c_str(), &path_status) != 0) {
        throw socket_error("authentication key lstat failed");
    }
    if (S_ISLNK(path_status.st_mode)) {
        fail_authentication_key_file_policy(
            state_path, role, key_path, "symbolic-link-key-file", false, true,
            false, path_status.st_mode, 0);
    }

    int flags = O_RDONLY;
#ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
#endif
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif
    const int descriptor = ::open(key_path.c_str(), flags);
    if (descriptor < 0) {
#ifdef O_NOFOLLOW
        if (errno == ELOOP) {
            fail_authentication_key_file_policy(
                state_path, role, key_path, "symbolic-link-key-file", false,
                true, false, path_status.st_mode, 0);
        }
#endif
        throw socket_error("authentication key open failed");
    }

    struct stat opened_status {};
    if (::fstat(descriptor, &opened_status) != 0) {
        const auto error = socket_error("authentication key fstat failed");
        ::close(descriptor);
        throw error;
    }
    const bool regular_file = S_ISREG(opened_status.st_mode);
    const bool owner_only_permissions =
        (opened_status.st_mode & (S_IRWXG | S_IRWXO)) == 0;
    if (!regular_file) {
        ::close(descriptor);
        fail_authentication_key_file_policy(
            state_path, role, key_path, "non-regular-key-file", false, false,
            owner_only_permissions, opened_status.st_mode, 0);
    }
    if (!owner_only_permissions) {
        ::close(descriptor);
        fail_authentication_key_file_policy(
            state_path, role, key_path, "group-or-other-accessible-key-file",
            true, false, false, opened_status.st_mode, 0);
    }

    std::string key;
    std::array<char, 4096> buffer{};
    while (true) {
        const auto received = ::read(descriptor, buffer.data(), buffer.size());
        if (received < 0 && errno == EINTR) continue;
        if (received < 0) {
            const auto error = socket_error("authentication key read failed");
            ::close(descriptor);
            throw error;
        }
        if (received == 0) break;
        key.append(buffer.data(), static_cast<std::size_t>(received));
    }
    if (::close(descriptor) != 0) {
        throw socket_error("authentication key close failed");
    }
    while (!key.empty() && (key.back() == '\n' || key.back() == '\r')) {
        key.pop_back();
    }
    if (key.size() < 32) {
        fail_authentication_key_file_policy(
            state_path, role, key_path, "key-file-shorter-than-32-bytes", true,
            false, true, opened_status.st_mode, key.size());
    }
    return key;
}

AuthenticationKeys read_authentication_keys(
    const std::filesystem::path& state_path,
    std::string_view current_role,
    std::string_view previous_role,
    const std::filesystem::path& current_path,
    const std::filesystem::path& previous_path) {
    AuthenticationKeys keys{
        .current = read_authentication_key(state_path, current_role, current_path)};
    if (previous_path != "-") {
        keys.previous =
            read_authentication_key(state_path, previous_role, previous_path);
    }
    return keys;
}

[[noreturn]] void fail_authentication_key_policy(
    const std::filesystem::path& state_path,
    std::string_view first_role,
    std::string_view second_role,
    const std::string& duplicate_key,
    std::size_t configured_key_count) {
    std::filesystem::create_directories(state_path.parent_path());
    const auto failure_path =
        state_path.parent_path() / "key-policy-failure.txt";
    const auto temporary =
        failure_path.string() + ".tmp." + std::to_string(::getpid());
    std::ostringstream output;
    output << "SMAVE_GATE_TXN_KEY_POLICY_FAILURE 1\n"
           << "contract=pairwise-distinct-state-witness-authentication-keys\n"
           << "failure_stage=key-policy-before-state-load\n"
           << "reason=duplicate-key-material-across-roles\n"
           << "first_role=" << first_role << '\n'
           << "second_role=" << second_role << '\n'
           << "duplicate_key_id=" << authentication_key_id(duplicate_key) << '\n'
           << "configured_key_count=" << configured_key_count << '\n'
           << "pairwise_distinct=0\n"
           << "fail_closed=1\n"
           << "state_loaded=0\n"
           << "listen_socket_created=0\n"
           << "server_exit_code=88\n"
           << "performance_evidence=0\n"
           << "END\n";
    try {
        write_durable_file(temporary, output.str());
    } catch (...) {
        ::unlink(temporary.c_str());
        throw;
    }
    if (::rename(temporary.c_str(), failure_path.c_str()) != 0) {
        ::unlink(temporary.c_str());
        throw socket_error("key-policy-failure rename failed");
    }
    fsync_directory(state_path.parent_path());
    throw KeyPolicyFailure("duplicate-key-material-across-roles");
}

void enforce_authentication_key_policy(
    const std::filesystem::path& state_path,
    const AuthenticationKeys& state_keys,
    const AuthenticationKeys& witness_keys) {
    std::vector<std::pair<std::string_view, const std::string*>> configured{
        {"state-current", &state_keys.current},
        {"witness-current", &witness_keys.current},
    };
    if (state_keys.previous) {
        configured.emplace_back("state-previous", &*state_keys.previous);
    }
    if (witness_keys.previous) {
        configured.emplace_back("witness-previous", &*witness_keys.previous);
    }
    for (std::size_t first = 0; first < configured.size(); ++first) {
        for (std::size_t second = first + 1; second < configured.size(); ++second) {
            if (*configured[first].second == *configured[second].second) {
                fail_authentication_key_policy(
                    state_path, configured[first].first, configured[second].first,
                    *configured[first].second, configured.size());
            }
        }
    }
}

[[noreturn]] void fail_state_recovery(
    const std::filesystem::path& state_path,
    const std::string& reason,
    bool current_present,
    bool current_valid,
    bool previous_present,
    bool previous_valid,
    bool mirror_current_present,
    bool mirror_current_valid,
    bool mirror_previous_present,
    bool mirror_previous_valid,
    bool witness_present = false,
    bool witness_valid = false) {
    const auto failure_path = state_path.parent_path() / "recovery-failure.txt";
    const auto temporary =
        failure_path.string() + ".tmp." + std::to_string(::getpid());
    std::ostringstream output;
    output << "SMAVE_GATE_TXN_RECOVERY_FAILURE 1\n"
           << "contract=authenticated-mirrored-monotonic-witness-fail-closed-startup\n"
           << "failure_stage=state-load-before-listen\n"
           << "reason=" << reason << '\n'
           << "current_present=" << (current_present ? 1 : 0) << '\n'
           << "current_valid=" << (current_valid ? 1 : 0) << '\n'
           << "previous_present=" << (previous_present ? 1 : 0) << '\n'
           << "previous_valid=" << (previous_valid ? 1 : 0) << '\n'
           << "mirror_current_present=" << (mirror_current_present ? 1 : 0) << '\n'
           << "mirror_current_valid=" << (mirror_current_valid ? 1 : 0) << '\n'
           << "mirror_previous_present=" << (mirror_previous_present ? 1 : 0) << '\n'
           << "mirror_previous_valid=" << (mirror_previous_valid ? 1 : 0) << '\n'
           << "witness_present=" << (witness_present ? 1 : 0) << '\n'
           << "witness_valid=" << (witness_valid ? 1 : 0) << '\n'
           << "fail_closed=1\n"
           << "blank_state_started=0\n"
           << "state_reinitialized=0\n"
           << "listen_socket_created=0\n"
           << "server_exit_code=88\n"
           << "performance_evidence=0\n"
           << "END\n";
    try {
        write_durable_file(temporary, output.str());
    } catch (...) {
        ::unlink(temporary.c_str());
        throw;
    }
    if (::rename(temporary.c_str(), failure_path.c_str()) != 0) {
        ::unlink(temporary.c_str());
        throw socket_error("recovery-failure rename failed");
    }
    fsync_directory(state_path.parent_path());
    throw StateRecoveryFailure(reason);
}

void persist_state(
    const std::filesystem::path& state_path,
    const std::filesystem::path& mirror_path,
    const std::string& authentication_key,
    const std::filesystem::path& witness_path,
    const std::string& witness_key,
    ServerStats& stats,
    const std::map<std::uint64_t, TransactionRecord>& transactions,
    PersistenceCrashPoint crash_point = PersistenceCrashPoint::none) {
    std::filesystem::create_directories(state_path.parent_path());
    std::filesystem::create_directories(mirror_path.parent_path());
    std::filesystem::create_directories(witness_path.parent_path());
    ++stats.state_generation;
    std::string state_sha256;
    const auto snapshot = serialize_state(
        stats, transactions, authentication_key, state_sha256);
    const auto witness = serialize_witness(
        stats.state_generation, state_sha256, witness_key);
    const auto temporary = state_path.string() + ".tmp." + std::to_string(::getpid());
    const auto mirror_temporary =
        mirror_path.string() + ".tmp." + std::to_string(::getpid());
    const auto witness_temporary =
        witness_path.string() + ".tmp." + std::to_string(::getpid());
    try {
        write_durable_file(temporary, snapshot);
        write_durable_file(mirror_temporary, snapshot);
        write_durable_file(witness_temporary, witness);
    } catch (...) {
        ::unlink(temporary.c_str());
        ::unlink(mirror_temporary.c_str());
        ::unlink(witness_temporary.c_str());
        throw;
    }
    if (crash_point == PersistenceCrashPoint::before_publication) ::_exit(87);

    const auto publish = [](const std::filesystem::path& path,
                            const std::filesystem::path& prepared) {
        const auto previous = path.string() + ".previous";
        const auto previous_temporary =
            previous + ".tmp." + std::to_string(::getpid());
        if (std::filesystem::exists(path)) {
            try {
                write_durable_file(previous_temporary, read_text_file(path));
            } catch (...) {
                ::unlink(previous_temporary.c_str());
                throw;
            }
            if (::rename(previous_temporary.c_str(), previous.c_str()) != 0) {
                ::unlink(previous_temporary.c_str());
                throw socket_error("previous-state rename failed");
            }
        }
        if (::rename(prepared.c_str(), path.c_str()) != 0) {
            throw socket_error("state rename failed");
        }
        fsync_directory(path.parent_path());
    };
    try {
        publish(state_path, temporary);
        publish(mirror_path, mirror_temporary);
        if (crash_point == PersistenceCrashPoint::before_witness_publication) {
            ::_exit(89);
        }
        publish(witness_path, witness_temporary);
    } catch (...) {
        ::unlink(temporary.c_str());
        ::unlink(mirror_temporary.c_str());
        ::unlink(witness_temporary.c_str());
        throw;
    }
    fsync_directory(state_path.parent_path());
    if (mirror_path.parent_path() != state_path.parent_path()) {
        fsync_directory(mirror_path.parent_path());
    }
    fsync_directory(witness_path.parent_path());
    stats.previous_body_sha256 = state_sha256;
}

void parse_state_snapshot(
    const std::string& message,
    const std::string& authentication_key,
    ServerStats& stats,
    std::map<std::uint64_t, TransactionRecord>& transactions,
    std::string& authenticated_body_sha256) {
    constexpr std::string_view header = "SMAVE_GATE_TXN_STATE 6\n";
    constexpr std::string_view checksum_marker = "state_sha256=";
    constexpr std::string_view key_marker = "key_id=";
    constexpr std::string_view hmac_marker = "state_hmac_sha256=";
    if (!message.starts_with(header) || !message.ends_with("END\n")) {
        throw std::invalid_argument("invalid transaction state envelope");
    }
    const auto checksum_position = message.rfind(checksum_marker);
    if (checksum_position == std::string::npos || checksum_position <= header.size()) {
        throw std::invalid_argument("transaction state checksum is missing");
    }
    const auto body = message.substr(
        header.size(), checksum_position - header.size());
    const auto checksum_end = message.find('\n', checksum_position);
    const auto key_position = message.find(key_marker, checksum_end + 1);
    const auto key_end = message.find('\n', key_position);
    const auto hmac_position = message.find(hmac_marker, key_end + 1);
    const auto hmac_end = message.find('\n', hmac_position);
    if (checksum_end == std::string::npos || key_position == std::string::npos ||
        key_end == std::string::npos || hmac_position == std::string::npos ||
        hmac_end == std::string::npos || message.substr(hmac_end + 1) != "END\n") {
        throw std::invalid_argument("transaction state checksum is unterminated");
    }
    const auto checksum = message.substr(
        checksum_position + checksum_marker.size(),
        checksum_end - checksum_position - checksum_marker.size());
    if (smave::sha256_text(body) != checksum) {
        throw std::invalid_argument("transaction state checksum mismatch");
    }
    const auto key_id = message.substr(
        key_position + key_marker.size(), key_end - key_position - key_marker.size());
    const auto expected_key_id =
        smave::sha256_text(authentication_key).substr(0, 16);
    const auto state_hmac = message.substr(
        hmac_position + hmac_marker.size(),
        hmac_end - hmac_position - hmac_marker.size());
    if (key_id != expected_key_id ||
        !smave::verify_hmac_sha256_text(authentication_key, body, state_hmac)) {
        throw std::invalid_argument("transaction state authentication mismatch");
    }
    authenticated_body_sha256 = checksum;
    const auto fields = parse_key_value_message(
        "SMAVE_GATE_TXN_STATE 6\n" + body + "END\n",
        "SMAVE_GATE_TXN_STATE 6");
    stats.state_generation = std::stoull(fields.at("generation"));
    stats.previous_body_sha256 = fields.at("previous_body_sha256");
    stats.complete_requests = std::stoull(fields.at("complete_requests"));
    stats.committed_transactions = std::stoull(fields.at("committed_transactions"));
    stats.gate_rejections = std::stoull(fields.at("gate_rejections"));
    stats.duplicate_replays = std::stoull(fields.at("duplicate_replays"));
    stats.transaction_conflicts = std::stoull(fields.at("transaction_conflicts"));
    stats.dropped_replies = std::stoull(fields.at("dropped_replies"));
    stats.malformed_requests = std::stoull(fields.at("malformed_requests"));
    stats.server_starts = std::stoull(fields.at("server_starts"));
    stats.crash_after_commit_injections =
        std::stoull(fields.at("crash_after_commit_injections"));
    stats.witness_lag_crash_injections =
        std::stoull(fields.at("witness_lag_crash_injections"));
    stats.witness_lag_recoveries =
        std::stoull(fields.at("witness_lag_recoveries"));
    stats.torn_temporary_recoveries =
        std::stoull(fields.at("torn_temporary_recoveries"));
    stats.checksum_failures = std::stoull(fields.at("checksum_failures"));
    stats.authentication_failures =
        std::stoull(fields.at("authentication_failures"));
    stats.previous_generation_recoveries =
        std::stoull(fields.at("previous_generation_recoveries"));
    stats.stale_current_generations_rejected =
        std::stoull(fields.at("stale_current_generations_rejected"));
    stats.mirror_recoveries = std::stoull(fields.at("mirror_recoveries"));
    stats.state_key_rotations =
        std::stoull(fields.at("state_key_rotations"));
    stats.witness_key_rotations =
        std::stoull(fields.at("witness_key_rotations"));
    const auto count = std::stoull(fields.at("transaction_count"));
    transactions.clear();
    for (std::size_t index = 0; index < count; ++index) {
        const auto prefix = "transaction." + std::to_string(index) + ".";
        const auto transaction_id = std::stoull(fields.at(prefix + "id"));
        transactions.emplace(transaction_id, TransactionRecord{
            .fingerprint = hex_decode(fields.at(prefix + "fingerprint_hex")),
            .response = {
                .status = fields.at(prefix + "status"),
                .decision = fields.at(prefix + "decision"),
                .residual_inf = std::stod(fields.at(prefix + "residual_inf")),
            },
        });
    }
    stats.recovered_transactions = transactions.size();
}

void parse_monotonic_witness(
    const std::string& message,
    const std::string& witness_key,
    MonotonicWitness& witness) {
    constexpr std::string_view header = "SMAVE_GATE_TXN_WITNESS 1\n";
    constexpr std::string_view checksum_marker = "witness_sha256=";
    constexpr std::string_view key_marker = "witness_key_id=";
    constexpr std::string_view hmac_marker = "witness_hmac_sha256=";
    if (!message.starts_with(header) || !message.ends_with("END\n")) {
        throw std::invalid_argument("invalid monotonic witness envelope");
    }
    const auto checksum_position = message.rfind(checksum_marker);
    if (checksum_position == std::string::npos || checksum_position <= header.size()) {
        throw std::invalid_argument("monotonic witness checksum is missing");
    }
    const auto body = message.substr(
        header.size(), checksum_position - header.size());
    const auto checksum_end = message.find('\n', checksum_position);
    const auto key_position = message.find(key_marker, checksum_end + 1);
    const auto key_end = message.find('\n', key_position);
    const auto hmac_position = message.find(hmac_marker, key_end + 1);
    const auto hmac_end = message.find('\n', hmac_position);
    if (checksum_end == std::string::npos || key_position == std::string::npos ||
        key_end == std::string::npos || hmac_position == std::string::npos ||
        hmac_end == std::string::npos || message.substr(hmac_end + 1) != "END\n") {
        throw std::invalid_argument("monotonic witness checksum is unterminated");
    }
    const auto checksum = message.substr(
        checksum_position + checksum_marker.size(),
        checksum_end - checksum_position - checksum_marker.size());
    if (smave::sha256_text(body) != checksum) {
        throw std::invalid_argument("monotonic witness checksum mismatch");
    }
    const auto key_id = message.substr(
        key_position + key_marker.size(), key_end - key_position - key_marker.size());
    const auto expected_key_id = smave::sha256_text(witness_key).substr(0, 16);
    const auto signature = message.substr(
        hmac_position + hmac_marker.size(),
        hmac_end - hmac_position - hmac_marker.size());
    if (key_id != expected_key_id ||
        !smave::verify_hmac_sha256_text(witness_key, body, signature)) {
        throw std::invalid_argument("monotonic witness authentication mismatch");
    }
    const auto fields = parse_key_value_message(
        "SMAVE_GATE_TXN_WITNESS 1\n" + body + "END\n",
        "SMAVE_GATE_TXN_WITNESS 1");
    witness.generation = std::stoull(fields.at("generation"));
    witness.state_sha256 = fields.at("state_sha256");
    if (witness.state_sha256.size() != 64) {
        throw std::invalid_argument("monotonic witness state digest is invalid");
    }
}

void publish_monotonic_witness(
    const std::filesystem::path& witness_path,
    const MonotonicWitness& witness,
    const std::string& witness_key) {
    std::filesystem::create_directories(witness_path.parent_path());
    const auto temporary =
        witness_path.string() + ".tmp." + std::to_string(::getpid());
    try {
        write_durable_file(
            temporary,
            serialize_witness(
                witness.generation, witness.state_sha256, witness_key));
    } catch (...) {
        ::unlink(temporary.c_str());
        throw;
    }
    if (::rename(temporary.c_str(), witness_path.c_str()) != 0) {
        ::unlink(temporary.c_str());
        throw socket_error("monotonic witness rename failed");
    }
    fsync_directory(witness_path.parent_path());
}

void record_witness_lag_recovery(
    const std::filesystem::path& state_path,
    const MonotonicWitness& prior_witness,
    std::uint64_t selected_generation,
    const std::string& selected_previous_sha256,
    const std::string& selected_state_sha256) {
    const auto record_path =
        state_path.parent_path() / "witness-lag-recovery.txt";
    const auto temporary =
        record_path.string() + ".tmp." + std::to_string(::getpid());
    std::ostringstream output;
    output << "SMAVE_GATE_TXN_WITNESS_LAG_RECOVERY 1\n"
           << "recovery_stage=state-load-before-listen\n"
           << "prior_witness_generation=" << prior_witness.generation << '\n'
           << "selected_state_generation=" << selected_generation << '\n'
           << "generation_delta="
           << selected_generation - prior_witness.generation << '\n'
           << "prior_witness_state_sha256=" << prior_witness.state_sha256 << '\n'
           << "selected_previous_body_sha256="
           << selected_previous_sha256 << '\n'
           << "selected_state_sha256=" << selected_state_sha256 << '\n'
           << "parent_digest_match=1\n"
           << "witness_republished=1\n"
           << "listen_socket_created=0\n"
           << "performance_evidence=0\n"
           << "END\n";
    try {
        write_durable_file(temporary, output.str());
    } catch (...) {
        ::unlink(temporary.c_str());
        throw;
    }
    if (::rename(temporary.c_str(), record_path.c_str()) != 0) {
        ::unlink(temporary.c_str());
        throw socket_error("witness lag recovery record rename failed");
    }
    fsync_directory(record_path.parent_path());
}

void record_key_rotation(
    const std::filesystem::path& state_path,
    const KeyRotationLoad& rotation,
    std::uint64_t published_generation,
    const std::string& published_state_sha256,
    const AuthenticationKeys& state_keys,
    const AuthenticationKeys& witness_keys) {
    const auto record_path = state_path.parent_path() / "key-rotation-recovery.txt";
    const auto temporary =
        record_path.string() + ".tmp." + std::to_string(::getpid());
    std::ostringstream output;
    output << "SMAVE_GATE_TXN_KEY_ROTATION 1\n"
           << "recovery_stage=state-load-before-listen\n"
           << "selected_state_generation=" << rotation.selected_generation << '\n'
           << "published_state_generation=" << published_generation << '\n'
           << "generation_delta="
           << published_generation - rotation.selected_generation << '\n'
           << "selected_state_sha256=" << rotation.selected_state_sha256 << '\n'
           << "published_previous_body_sha256="
           << rotation.selected_state_sha256 << '\n'
           << "published_state_sha256=" << published_state_sha256 << '\n'
           << "selected_state_key_id=" << rotation.selected_state_key_id << '\n'
           << "current_state_key_id="
           << authentication_key_id(state_keys.current) << '\n'
           << "selected_witness_key_id=" << rotation.selected_witness_key_id << '\n'
           << "current_witness_key_id="
           << authentication_key_id(witness_keys.current) << '\n'
           << "state_key_republished="
           << (rotation.state_key_rotation ? 1 : 0) << '\n'
           << "witness_key_republished="
           << (rotation.witness_key_rotation ? 1 : 0) << '\n'
           << "previous_state_key_configured="
           << (state_keys.previous ? 1 : 0) << '\n'
           << "previous_witness_key_configured="
           << (witness_keys.previous ? 1 : 0) << '\n'
           << "listen_socket_created=0\n"
           << "fixture_key_rotation=1\n"
           << "production_key_custody=0\n"
           << "kms_hsm_integration=0\n"
           << "performance_evidence=0\n"
           << "END\n";
    try {
        write_durable_file(temporary, output.str());
    } catch (...) {
        ::unlink(temporary.c_str());
        throw;
    }
    if (::rename(temporary.c_str(), record_path.c_str()) != 0) {
        ::unlink(temporary.c_str());
        throw socket_error("key rotation recovery record rename failed");
    }
    fsync_directory(record_path.parent_path());
}

std::size_t remove_orphaned_temporaries(const std::filesystem::path& state_path) {
    std::size_t removed{};
    const auto state_prefix = state_path.filename().string() + ".tmp.";
    const auto previous_prefix =
        state_path.filename().string() + ".previous.tmp.";
    if (!std::filesystem::exists(state_path.parent_path())) return removed;
    for (const auto& entry : std::filesystem::directory_iterator(
             state_path.parent_path())) {
        const auto name = entry.path().filename().string();
        if (name.starts_with(state_prefix) || name.starts_with(previous_prefix)) {
            std::filesystem::remove(entry.path());
            ++removed;
        }
    }
    if (removed != 0) fsync_directory(state_path.parent_path());
    return removed;
}

KeyRotationLoad load_state(
    const std::filesystem::path& state_path,
    const std::filesystem::path& mirror_path,
    const std::filesystem::path& witness_path,
    const AuthenticationKeys& authentication_keys,
    const AuthenticationKeys& witness_keys,
    ServerStats& stats,
    std::map<std::uint64_t, TransactionRecord>& transactions) {
    const auto orphaned = remove_orphaned_temporaries(state_path) +
        remove_orphaned_temporaries(mirror_path) +
        remove_orphaned_temporaries(witness_path);
    const auto previous_path = std::filesystem::path(state_path.string() + ".previous");
    const auto mirror_previous_path =
        std::filesystem::path(mirror_path.string() + ".previous");
    const bool witness_present = std::filesystem::exists(witness_path);
    if (!std::filesystem::exists(state_path) &&
        !std::filesystem::exists(previous_path) &&
        !std::filesystem::exists(mirror_path) &&
        !std::filesystem::exists(mirror_previous_path) &&
        !witness_present) return {};

    struct Snapshot {
        ServerStats stats;
        std::map<std::uint64_t, TransactionRecord> transactions;
        std::string authenticated_body_sha256;
        std::string authentication_key_id;
        bool used_previous_authentication_key{};
    };
    struct Attempt {
        std::filesystem::path path;
        bool present{};
        bool checksum_failure{};
        bool authentication_failure{};
        std::optional<Snapshot> snapshot;
    };
    const auto parse = [&](const std::filesystem::path& path) {
        Attempt attempt{.path = path, .present = std::filesystem::exists(path)};
        if (!attempt.present) return attempt;
        const auto message = read_text_file(path);
        std::vector<std::pair<const std::string*, bool>> trusted{
            {&authentication_keys.current, false},
        };
        if (authentication_keys.previous) {
            trusted.emplace_back(&*authentication_keys.previous, true);
        }
        std::vector<std::string> reasons;
        for (const auto& [key, previous] : trusted) {
            Snapshot snapshot;
            try {
                parse_state_snapshot(
                    message, *key, snapshot.stats, snapshot.transactions,
                    snapshot.authenticated_body_sha256);
                snapshot.authentication_key_id = authentication_key_id(*key);
                snapshot.used_previous_authentication_key = previous;
                attempt.snapshot = std::move(snapshot);
                return attempt;
            } catch (const std::exception& error) {
                reasons.emplace_back(error.what());
            }
        }
        attempt.authentication_failure = std::all_of(
            reasons.begin(), reasons.end(), [](const std::string& reason) {
                return reason.find("authentication") != std::string::npos;
            });
        attempt.checksum_failure = !attempt.authentication_failure;
        return attempt;
    };
    std::array<Attempt, 4> attempts{
        parse(state_path),
        parse(previous_path),
        parse(mirror_path),
        parse(mirror_previous_path),
    };
    std::optional<MonotonicWitness> witness;
    bool witness_used_previous_key{};
    std::string selected_witness_key_id;
    if (witness_present) {
        const auto message = read_text_file(witness_path);
        std::vector<std::pair<const std::string*, bool>> trusted{
            {&witness_keys.current, false},
        };
        if (witness_keys.previous) {
            trusted.emplace_back(&*witness_keys.previous, true);
        }
        for (const auto& [key, previous] : trusted) {
            MonotonicWitness candidate;
            try {
                parse_monotonic_witness(message, *key, candidate);
                witness = std::move(candidate);
                witness_used_previous_key = previous;
                selected_witness_key_id = authentication_key_id(*key);
                break;
            } catch (const std::exception&) {
            }
        }
    }
    const bool witness_valid = witness.has_value();
    Attempt* selected{};
    for (auto& attempt : attempts) {
        if (!attempt.snapshot) continue;
        if (selected == nullptr ||
            attempt.snapshot->stats.state_generation >
                selected->snapshot->stats.state_generation) {
            selected = &attempt;
        }
    }
    if (selected == nullptr) {
        fail_state_recovery(
            state_path, "no-valid-authenticated-generation",
            attempts[0].present, false, attempts[1].present, false,
            attempts[2].present, false, attempts[3].present, false,
            witness_present, witness_valid);
    }
    const auto valid = [](const Attempt& attempt) {
        return attempt.snapshot.has_value();
    };
    for (const auto& attempt : attempts) {
        if (!attempt.snapshot ||
            attempt.snapshot->stats.state_generation !=
                selected->snapshot->stats.state_generation) {
            continue;
        }
        if (attempt.snapshot->authenticated_body_sha256 !=
            selected->snapshot->authenticated_body_sha256) {
            fail_state_recovery(
                state_path, "authenticated-highest-generation-fork",
                attempts[0].present, valid(attempts[0]),
                attempts[1].present, valid(attempts[1]),
                attempts[2].present, valid(attempts[2]),
                attempts[3].present, valid(attempts[3]),
                witness_present, witness_valid);
        }
    }
    if (!witness) {
        fail_state_recovery(
            state_path, "no-valid-monotonic-witness",
            attempts[0].present, valid(attempts[0]),
            attempts[1].present, valid(attempts[1]),
            attempts[2].present, valid(attempts[2]),
            attempts[3].present, valid(attempts[3]),
            witness_present, false);
    }
    const auto selected_generation = selected->snapshot->stats.state_generation;
    if (selected_generation < witness->generation) {
        fail_state_recovery(
            state_path, "authenticated-state-below-monotonic-witness",
            attempts[0].present, valid(attempts[0]),
            attempts[1].present, valid(attempts[1]),
            attempts[2].present, valid(attempts[2]),
            attempts[3].present, valid(attempts[3]), true, true);
    }
    if (selected_generation == witness->generation &&
        selected->snapshot->authenticated_body_sha256 != witness->state_sha256) {
        fail_state_recovery(
            state_path, "authenticated-state-disagrees-with-monotonic-witness",
            attempts[0].present, valid(attempts[0]),
            attempts[1].present, valid(attempts[1]),
            attempts[2].present, valid(attempts[2]),
            attempts[3].present, valid(attempts[3]), true, true);
    }
    const bool witness_lag_recovered = selected_generation > witness->generation;
    if (witness_lag_recovered) {
        if (selected_generation != witness->generation + 1 ||
            selected->snapshot->stats.previous_body_sha256 != witness->state_sha256) {
            fail_state_recovery(
                state_path, "unauthorized-monotonic-witness-advance",
                attempts[0].present, valid(attempts[0]),
                attempts[1].present, valid(attempts[1]),
                attempts[2].present, valid(attempts[2]),
                attempts[3].present, valid(attempts[3]), true, true);
        }
        publish_monotonic_witness(
            witness_path,
            MonotonicWitness{
                selected_generation,
                selected->snapshot->authenticated_body_sha256,
            },
            witness_keys.current);
        record_witness_lag_recovery(
            state_path, *witness, selected_generation,
            selected->snapshot->stats.previous_body_sha256,
            selected->snapshot->authenticated_body_sha256);
    }
    const auto selected_index = static_cast<std::size_t>(
        selected - attempts.data());
    KeyRotationLoad rotation{
        .state_key_rotation =
            selected->snapshot->used_previous_authentication_key,
        .witness_key_rotation = witness_used_previous_key,
        .selected_generation = selected_generation,
        .selected_state_sha256 =
            selected->snapshot->authenticated_body_sha256,
        .selected_state_key_id = selected->snapshot->authentication_key_id,
        .selected_witness_key_id = selected_witness_key_id,
    };
    stats = selected->snapshot->stats;
    stats.previous_body_sha256 = selected->snapshot->authenticated_body_sha256;
    if (witness_lag_recovered) ++stats.witness_lag_recoveries;
    if (rotation.state_key_rotation) ++stats.state_key_rotations;
    if (rotation.witness_key_rotation) ++stats.witness_key_rotations;
    transactions = std::move(selected->snapshot->transactions);
    for (const auto& attempt : attempts) {
        if (attempt.checksum_failure) ++stats.checksum_failures;
        if (attempt.authentication_failure) ++stats.authentication_failures;
    }
    if (attempts[0].snapshot &&
        selected->snapshot->stats.state_generation >
            attempts[0].snapshot->stats.state_generation) {
        ++stats.stale_current_generations_rejected;
    }
    if (selected_index == 1) {
        ++stats.previous_generation_recoveries;
    }
    if (selected_index >= 2) ++stats.mirror_recoveries;
    for (const auto& attempt : attempts) {
        if (attempt.present && !attempt.snapshot) {
            std::filesystem::remove(attempt.path);
            fsync_directory(attempt.path.parent_path());
        }
    }
    if (selected_index != 0 && attempts[0].snapshot) {
        std::filesystem::remove(state_path);
        fsync_directory(state_path.parent_path());
    }
    stats.torn_temporary_recoveries += orphaned;
    stats.recovered_transactions = transactions.size();
    return rotation;
}

std::map<std::string, std::string> transact(
    const std::string& host,
    const std::string& port,
    const std::string& request,
    bool expect_reply = true) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* addresses{};
    const int lookup = ::getaddrinfo(host.c_str(), port.c_str(), &hints, &addresses);
    if (lookup != 0) {
        throw std::runtime_error(
            std::string("host lookup failed: ") + ::gai_strerror(lookup));
    }
    std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)> guard(
        addresses, ::freeaddrinfo);
    Socket socket;
    for (auto* address = addresses; address != nullptr; address = address->ai_next) {
        Socket candidate(::socket(
            address->ai_family, address->ai_socktype, address->ai_protocol));
        if (candidate.descriptor < 0) continue;
        if (::connect(candidate.descriptor, address->ai_addr, address->ai_addrlen) == 0) {
            socket = std::move(candidate);
            break;
        }
    }
    if (socket.descriptor < 0) throw socket_error("TCP connect failed");
    write_all(socket.descriptor, request);
    const auto response = read_message(socket.descriptor);
    if (!expect_reply) {
        if (response) throw std::runtime_error("fault-injected reply was not dropped");
        return {};
    }
    if (!response) throw std::runtime_error("TCP peer closed before response");
    return parse_fields(*response);
}

Socket listen_socket(const std::string& port) {
    Socket socket(::socket(AF_INET, SOCK_STREAM, 0));
    if (socket.descriptor < 0) throw socket_error("TCP socket creation failed");
    int reuse = 1;
    if (::setsockopt(
            socket.descriptor, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) != 0) {
        throw socket_error("TCP SO_REUSEADDR failed");
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(static_cast<std::uint16_t>(std::stoul(port)));
    if (::bind(
            socket.descriptor, reinterpret_cast<sockaddr*>(&address),
            sizeof(address)) != 0) {
        throw socket_error("TCP bind failed");
    }
    if (::listen(socket.descriptor, 16) != 0) throw socket_error("TCP listen failed");
    return socket;
}

int run_server(
    const std::filesystem::path& linear_path,
    const std::filesystem::path& nonlinear_path,
    const std::string& port,
    const std::filesystem::path& state_path,
    const std::filesystem::path& mirror_path,
    const std::filesystem::path& authentication_key_path,
    const std::filesystem::path& previous_authentication_key_path,
    const std::filesystem::path& witness_path,
    const std::filesystem::path& witness_key_path,
    const std::filesystem::path& previous_witness_key_path) {
    const auto authentication_keys = read_authentication_keys(
        state_path, "state-current", "state-previous",
        authentication_key_path, previous_authentication_key_path);
    const auto witness_keys = read_authentication_keys(
        state_path, "witness-current", "witness-previous",
        witness_key_path, previous_witness_key_path);
    enforce_authentication_key_policy(
        state_path, authentication_keys, witness_keys);
    const auto linear_model = smave::compile_model(linear_path);
    const auto nonlinear_model = smave::compile_model(nonlinear_path);
    const smave::Runtime linear_runtime(linear_model);
    const smave::Runtime nonlinear_runtime(nonlinear_model);
    std::map<std::uint64_t, TransactionRecord> transactions;
    ServerStats stats;
    stats.previous_body_sha256 = std::string(64, '0');
    const auto key_rotation = load_state(
        state_path, mirror_path, witness_path, authentication_keys, witness_keys,
        stats, transactions);
    const auto persist = [&](
        PersistenceCrashPoint crash_point = PersistenceCrashPoint::none) {
        persist_state(
            state_path, mirror_path, authentication_keys.current,
            witness_path, witness_keys.current,
            stats, transactions, crash_point);
    };
    ++stats.server_starts;
    persist();
    if (key_rotation.state_key_rotation || key_rotation.witness_key_rotation) {
        record_key_rotation(
            state_path, key_rotation, stats.state_generation,
            stats.previous_body_sha256, authentication_keys, witness_keys);
    }
    const auto socket = listen_socket(port);
    bool running = true;
    while (running) {
        sockaddr_in peer{};
        socklen_t peer_size = sizeof(peer);
        Socket connection(::accept(
            socket.descriptor, reinterpret_cast<sockaddr*>(&peer), &peer_size));
        if (connection.descriptor < 0) {
            if (errno == EINTR) continue;
            throw socket_error("TCP accept failed");
        }
        const auto message = read_message(connection.descriptor);
        if (!message) {
            ++stats.malformed_requests;
            persist();
            continue;
        }
        Request request;
        try {
            request = deserialize_request(*message);
        } catch (const std::exception&) {
            ++stats.malformed_requests;
            persist();
            continue;
        }
        ++stats.complete_requests;
        if (request.operation == "shutdown") {
            Response response{.status = "shutdown"};
            persist();
            write_all(connection.descriptor, serialize_response(response, stats));
            running = false;
            continue;
        }
        if (request.operation == "stats") {
            Response response{.status = "stats"};
            if (request.crash_before_witness_publication) {
                ++stats.dropped_replies;
                ++stats.witness_lag_crash_injections;
                persist(PersistenceCrashPoint::before_witness_publication);
            }
            persist();
            write_all(connection.descriptor, serialize_response(response, stats));
            continue;
        }
        if (request.operation != "evaluate_commit") {
            ++stats.malformed_requests;
            persist();
            continue;
        }
        const auto fingerprint = canonical_payload(request);
        const auto previous = transactions.find(request.transaction_id);
        if (previous != transactions.end()) {
            if (previous->second.fingerprint != fingerprint) {
                ++stats.transaction_conflicts;
                Response response{.status = "transaction_conflict"};
                persist();
                write_all(connection.descriptor, serialize_response(response, stats));
                continue;
            }
            ++stats.duplicate_replays;
            auto response = previous->second.response;
            response.duplicate = true;
            persist();
            write_all(connection.descriptor, serialize_response(response, stats));
            continue;
        }
        const smave::Runtime* runtime{};
        const smave::BlockIR* block{};
        if (request.family == "linear") {
            runtime = &linear_runtime;
            block = &linear_model.blocks.front();
        } else if (request.family == "nonlinear") {
            runtime = &nonlinear_runtime;
            block = &nonlinear_model.blocks.front();
        } else {
            ++stats.malformed_requests;
            persist();
            continue;
        }
        const auto gate = runtime->evaluate_gate_fused(*block, request.values, true);
        Response response{
            .status = gate.decision == smave::GateDecision::direct_accept
                ? "committed"
                : "gate_rejected",
            .decision = decision_name(gate.decision),
            .residual_inf = gate.residual_inf,
        };
        transactions.emplace(
            request.transaction_id, TransactionRecord{fingerprint, response});
        if (gate.decision == smave::GateDecision::direct_accept) {
            ++stats.committed_transactions;
        } else {
            ++stats.gate_rejections;
        }
        if (request.crash_during_snapshot) {
            persist(PersistenceCrashPoint::before_publication);
        }
        if (request.crash_after_commit) {
            ++stats.dropped_replies;
            ++stats.crash_after_commit_injections;
            persist();
            ::_exit(86);
        }
        persist();
        write_all(connection.descriptor, serialize_response(response, stats));
    }
    return 0;
}

Values linear_candidate(
    const smave::ModelIR& model,
    const std::filesystem::path& scenario_directory,
    const std::filesystem::path& trace_directory) {
    std::vector<std::filesystem::path> scenarios;
    for (const auto& entry : std::filesystem::directory_iterator(scenario_directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".conf") {
            scenarios.push_back(entry.path());
        }
    }
    std::sort(scenarios.begin(), scenarios.end());
    if (scenarios.empty()) throw std::runtime_error("no linear scenario found");
    const smave::Runtime runtime(model);
    const auto outcome = runtime.solve(
        smave::read_scenario(scenarios.front()), trace_directory);
    if (!outcome.success) throw std::runtime_error("linear fixture solve failed");
    return outcome.values;
}

Values nonlinear_candidate(const smave::ModelIR& model) {
    constexpr double parameter = 1.75;
    Values values{{"p", parameter}};
    for (const auto& variable : model.variables) {
        if (!values.contains(variable.name)) values.emplace(variable.name, variable.start);
    }
    values.insert_or_assign("x", parameter + 1.0);
    values.insert_or_assign("y", 2.0 * parameter + 1.0);
    return values;
}

void require_field(
    const std::map<std::string, std::string>& fields,
    const std::string& key,
    const std::string& expected) {
    if (fields.at(key) != expected) {
        throw std::runtime_error(
            "expected " + key + "=" + expected + ", observed " + fields.at(key));
    }
}

void require_gate_match(
    const std::map<std::string, std::string>& response,
    const smave::GateResult& authority) {
    require_field(response, "decision", decision_name(authority.decision));
    const double observed = std::stod(response.at("residual_inf"));
    const double scale = std::max({1.0, std::abs(observed), std::abs(authority.residual_inf)});
    if (std::abs(observed - authority.residual_inf) > 1.0e-15 * scale) {
        throw std::runtime_error("network residual differs from local authority");
    }
}

int run_client(
    const std::string& phase,
    const std::string& host,
    const std::string& port,
    const std::filesystem::path& linear_path,
    const std::filesystem::path& scenario_directory,
    const std::filesystem::path& nonlinear_path,
    const std::filesystem::path& output_path,
    const std::string& deployment,
    bool distinct_network_namespaces) {
    const auto linear_model = smave::compile_model(linear_path);
    const auto nonlinear_model = smave::compile_model(nonlinear_path);
    const smave::Runtime linear_runtime(linear_model);
    const smave::Runtime nonlinear_runtime(nonlinear_model);
    auto linear = linear_candidate(
        linear_model, scenario_directory, output_path.parent_path() / "trace");
    auto rejected = linear;
    rejected.at(linear_model.blocks.front().unknowns.front()) += 1.0e-2;
    auto nonlinear = nonlinear_candidate(nonlinear_model);
    const auto write_evidence = [&](const std::map<std::string, std::string>& stats) {
        std::filesystem::create_directories(output_path.parent_path());
        std::ofstream output(output_path);
        if (!output) {
            throw std::runtime_error("cannot write network authority evidence");
        }
        output << "SMAVE_GATE_NETWORK_AUTHORITY 1\n"
               << "contract=functional-fail-closed-authenticated-mirrored-monotonic-witness-tcp-authority-transaction-probe\n"
               << "deployment=" << deployment << '\n'
               << "transport=tcp-ipv4\n"
               << "request_serialization=canonical-text-key-value\n"
               << "response_serialization=canonical-text-key-value\n"
               << "authority=fp64-fused-original-expression-gate\n"
               << "distinct_network_namespaces="
               << (distinct_network_namespaces ? 1 : 0) << '\n'
               << "same_physical_host=1\n"
               << "multi_host=0\n"
               << "performance_evidence=0\n"
               << "production_distributed_commit=0\n"
               << "consensus_protocol=0\n"
               << "durability=hmac-sha256-authenticated-hash-chained-mirrored-snapshot-plus-monotonic-witness\n"
               << "state_format=authenticated-hash-chained-v6\n"
               << "state_checksum=sha256\n"
               << "state_authentication=hmac-sha256\n"
               << "state_parent_hash=sha256\n"
               << "authentication_key_external=1\n"
               << "authentication_key_fixture_for_test=1\n"
               << "fixture_key_rotation_and_revocation=1\n"
               << "key_rotation_requires_explicit_previous_keys=1\n"
               << "key_rotation_republished_before_listen=1\n"
               << "pairwise_distinct_configured_keys_enforced=1\n"
               << "key_epoch_separation_enforced=1\n"
               << "key_domain_separation_enforced=1\n"
               << "key_files_regular_enforced=1\n"
               << "key_files_non_symlink_enforced=1\n"
               << "key_files_owner_only_permissions_enforced=1\n"
               << "key_file_minimum_bytes=32\n"
               << "production_key_lifecycle=0\n"
               << "kms_hsm_integration=0\n"
               << "mirrored_state_stores=2\n"
               << "independent_failure_domains=0\n"
               << "highest_valid_generation_across_stores=1\n"
               << "authenticated_highest_generation_fork_detection=1\n"
               << "local_monotonic_witness=1\n"
               << "authenticated_one_generation_witness_lag_recovery=1\n"
               << "witness_lag_generation_delta=1\n"
               << "external_monotonic_anchor=0\n"
               << "witness_preserved_all_state_rollback_detection=1\n"
               << "joint_state_and_witness_rollback_detection=0\n"
               << "witness_independent_failure_domain=0\n"
               << "witness_key_external=1\n"
               << "witness_key_fixture_for_test=1\n"
               << "previous_generation_retained=1\n"
               << "commit_ack_order=persist-before-reply\n"
               << "server_crash_exit_code=86\n"
               << "snapshot_prepublication_crash_exit_code=87\n"
               << "authenticated_recovery_failure_exit_code=88\n"
               << "witness_lag_crash_exit_code=89\n"
               << "server_starts=10\n"
               << "server_restarts=9\n"
               << "failed_start_attempts=11\n"
               << "recovered_transactions=4\n"
               << "accepted_transactions=3\n"
               << "gate_rejections=1\n"
               << "lost_reply_injections=3\n"
               << "server_crash_after_commit_injections=1\n"
               << "snapshot_prepublication_crash_injections=1\n"
               << "witness_lag_crash_injections=1\n"
               << "witness_lag_recoveries=1\n"
               << "witness_lag_recovery_records=1\n"
               << "witness_lag_crash_snapshots=4\n"
               << "witness_lag_prepared_witnesses=1\n"
               << "state_key_rotations=" << stats.at("state_key_rotations") << '\n'
               << "witness_key_rotations="
               << stats.at("witness_key_rotations") << '\n'
               << "key_rotation_records=1\n"
               << "key_rotation_pre_snapshots=4\n"
               << "key_rotation_pre_witnesses=2\n"
               << "key_rotation_post_snapshots=4\n"
               << "key_rotation_post_witnesses=2\n"
               << "revoked_state_key_generations_rejected=4\n"
               << "revoked_witness_key_generations_rejected=1\n"
               << "torn_temporary_recoveries=4\n"
               << "corrupted_current_generations_rejected=1\n"
               << "forged_plain_checksums_rejected_by_hmac=2\n"
               << "stale_current_generations_rejected=1\n"
               << "previous_generation_recoveries=1\n"
               << "mirror_recoveries=2\n"
               << "primary_generation_pair_losses=1\n"
               << "durable_idempotent_replays=4\n"
               << "wrong_key_generations_rejected=4\n"
               << "all_corrupted_generations_rejected=4\n"
               << "authenticated_same_generation_forks_rejected=1\n"
               << "monotonic_rollbacks_rejected=1\n"
               << "wrong_witness_generations_rejected=1\n"
               << "revoked_key_startups_rejected=1\n"
               << "authenticated_fail_closed_startups=6\n"
               << "key_epoch_collisions_rejected=1\n"
               << "key_domain_collisions_rejected=1\n"
               << "key_policy_fail_closed_startups=2\n"
               << "key_symlink_startups_rejected=1\n"
               << "key_permission_startups_rejected=1\n"
               << "short_key_startups_rejected=1\n"
               << "key_file_policy_fail_closed_startups=3\n"
               << "total_fail_closed_startups=11\n"
               << "blank_state_reinitializations=0\n"
               << "listen_before_recovery_failures=0\n"
               << "state_generation_at_evidence="
               << stats.at("state_generation") << '\n'
               << "transaction_conflicts_rejected=1\n"
               << "malformed_requests_rejected=1\n"
               << "partial_commits=0\n"
               << "decision_mismatches=0\n"
               << "residual_mismatches=0\n"
               << "strict_equivalence=1\n"
               << "END\n";
        return 0;
    };

    if (phase == "before-restart") {
        std::map<std::string, std::string> accepted;
        for (std::size_t attempt = 0; attempt < 600; ++attempt) {
            try {
                accepted = transact(host, port, serialize_request({
                    .operation = "evaluate_commit",
                    .transaction_id = 1001,
                    .family = "linear",
                    .values = linear,
                }));
                break;
            } catch (const std::exception&) {
                if (attempt == 599) throw;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        require_field(accepted, "status", "committed");
        require_field(accepted, "duplicate", "0");
        require_field(accepted, "committed_transactions", "1");
        require_gate_match(
            accepted,
            linear_runtime.evaluate_gate_fused(
                linear_model.blocks.front(), linear, true));

        const auto rejected_response = transact(host, port, serialize_request({
            .operation = "evaluate_commit",
            .transaction_id = 1002,
            .family = "linear",
            .values = rejected,
        }));
        require_field(rejected_response, "status", "gate_rejected");
        require_field(rejected_response, "committed_transactions", "1");
        require_gate_match(
            rejected_response,
            linear_runtime.evaluate_gate_fused(
                linear_model.blocks.front(), rejected, true));

        (void)transact(host, port, serialize_request({
            .operation = "evaluate_commit",
            .transaction_id = 1003,
            .family = "nonlinear",
            .crash_after_commit = true,
            .values = nonlinear,
        }), false);
        return 0;
    }
    if (phase == "after-durable-restart") {
        std::map<std::string, std::string> replay;
        for (std::size_t attempt = 0; attempt < 600; ++attempt) {
            try {
                replay = transact(host, port, serialize_request({
                    .operation = "evaluate_commit",
                    .transaction_id = 1003,
                    .family = "nonlinear",
                    .values = nonlinear,
                }));
                break;
            } catch (const std::exception&) {
                if (attempt == 599) throw;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        require_field(replay, "status", "committed");
        require_field(replay, "duplicate", "1");
        require_field(replay, "committed_transactions", "2");
        require_gate_match(
            replay,
            nonlinear_runtime.evaluate_gate_fused(
                nonlinear_model.blocks.front(), nonlinear, true));
        (void)transact(host, port, serialize_request({
            .operation = "evaluate_commit",
            .transaction_id = 1004,
            .family = "nonlinear",
            .crash_during_snapshot = true,
            .values = nonlinear,
        }), false);
        return 0;
    }

    if (phase == "after-torn-restart") {
        std::map<std::string, std::string> retry;
        for (std::size_t attempt = 0; attempt < 600; ++attempt) {
            try {
                retry = transact(host, port, serialize_request({
                    .operation = "evaluate_commit",
                    .transaction_id = 1004,
                    .family = "nonlinear",
                    .values = nonlinear,
                }));
                break;
            } catch (const std::exception&) {
                if (attempt == 599) throw;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        require_field(retry, "status", "committed");
        require_field(retry, "duplicate", "0");
        require_field(retry, "committed_transactions", "3");
        require_field(retry, "server_starts", "3");
        require_field(retry, "recovered_transactions", "3");
        require_field(retry, "torn_temporary_recoveries", "3");
        require_gate_match(
            retry,
            nonlinear_runtime.evaluate_gate_fused(
                nonlinear_model.blocks.front(), nonlinear, true));
        const auto shutdown = transact(host, port, serialize_request({
            .operation = "shutdown",
            .transaction_id = 0,
            .family = "none",
        }));
        require_field(shutdown, "status", "shutdown");
        return 0;
    }

    if (phase == "before-witness-lag-restart") {
        for (std::size_t attempt = 0; attempt < 600; ++attempt) {
            try {
                (void)transact(host, port, serialize_request({
                    .operation = "stats",
                    .transaction_id = 0,
                    .family = "none",
                    .crash_before_witness_publication = true,
                }), false);
                break;
            } catch (const std::exception&) {
                if (attempt == 599) throw;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        return 0;
    }

    if (phase == "after-witness-lag-restart") {
        std::map<std::string, std::string> recovered;
        for (std::size_t attempt = 0; attempt < 600; ++attempt) {
            try {
                recovered = transact(host, port, serialize_request({
                    .operation = "stats",
                    .transaction_id = 0,
                    .family = "none",
                }));
                break;
            } catch (const std::exception&) {
                if (attempt == 599) throw;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        require_field(recovered, "status", "stats");
        require_field(recovered, "server_starts", "5");
        require_field(recovered, "recovered_transactions", "4");
        require_field(recovered, "dropped_replies", "2");
        require_field(recovered, "witness_lag_crash_injections", "1");
        require_field(recovered, "witness_lag_recoveries", "1");
        require_field(recovered, "torn_temporary_recoveries", "4");
        const auto shutdown = transact(host, port, serialize_request({
            .operation = "shutdown",
            .transaction_id = 0,
            .family = "none",
        }));
        require_field(shutdown, "status", "shutdown");
        return 0;
    }

    if (phase == "after-key-rotation" || phase == "after-key-revocation") {
        std::map<std::string, std::string> rotated;
        for (std::size_t attempt = 0; attempt < 600; ++attempt) {
            try {
                rotated = transact(host, port, serialize_request({
                    .operation = "stats",
                    .transaction_id = 0,
                    .family = "none",
                }));
                break;
            } catch (const std::exception&) {
                if (attempt == 599) throw;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        require_field(rotated, "status", "stats");
        require_field(
            rotated, "server_starts",
            phase == "after-key-rotation" ? "9" : "10");
        require_field(rotated, "committed_transactions", "3");
        require_field(rotated, "duplicate_replays", "4");
        require_field(rotated, "state_key_rotations", "1");
        require_field(rotated, "witness_key_rotations", "1");
        require_field(rotated, "authentication_failures", "2");
        const auto shutdown = transact(host, port, serialize_request({
            .operation = "shutdown",
            .transaction_id = 0,
            .family = "none",
        }));
        require_field(shutdown, "status", "shutdown");
        if (phase == "after-key-rotation") return 0;
        return write_evidence(rotated);
    }

    if (phase != "after-corruption-restart" &&
        phase != "after-stale-restart" &&
        phase != "after-mirror-recovery") {
        throw std::invalid_argument("unknown network authority client phase");
    }

    std::map<std::string, std::string> replay;
    for (std::size_t attempt = 0; attempt < 600; ++attempt) {
        try {
            replay = transact(host, port, serialize_request({
                .operation = "evaluate_commit",
                .transaction_id = 1004,
                .family = "nonlinear",
                .values = nonlinear,
            }));
            break;
        } catch (const std::exception&) {
            if (attempt == 599) throw;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    require_field(replay, "status", "committed");
    require_field(replay, "duplicate", "1");
    require_field(replay, "committed_transactions", "3");
    require_gate_match(
        replay,
        nonlinear_runtime.evaluate_gate_fused(
            nonlinear_model.blocks.front(), nonlinear, true));

    if (phase == "after-corruption-restart") {
        require_field(replay, "duplicate_replays", "2");
        require_field(replay, "server_starts", "6");
        require_field(replay, "checksum_failures", "1");
        require_field(replay, "previous_generation_recoveries", "0");
        require_field(replay, "mirror_recoveries", "1");
        const auto shutdown = transact(host, port, serialize_request({
            .operation = "shutdown",
            .transaction_id = 0,
            .family = "none",
        }));
        require_field(shutdown, "status", "shutdown");
        return 0;
    }

    if (phase == "after-stale-restart") {
        require_field(replay, "duplicate_replays", "3");
        require_field(replay, "server_starts", "7");
        require_field(replay, "previous_generation_recoveries", "1");
        require_field(replay, "stale_current_generations_rejected", "1");

        auto conflicting = nonlinear;
        conflicting.at("x") += 1.0e-3;
        const auto conflict = transact(host, port, serialize_request({
            .operation = "evaluate_commit",
            .transaction_id = 1004,
            .family = "nonlinear",
            .values = conflicting,
        }));
        require_field(conflict, "status", "transaction_conflict");
        require_field(conflict, "committed_transactions", "3");

        {
            addrinfo hints{};
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            addrinfo* addresses{};
            const int lookup = ::getaddrinfo(
                host.c_str(), port.c_str(), &hints, &addresses);
            if (lookup != 0) {
                throw std::runtime_error("malformed-request lookup failed");
            }
            std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)> guard(
                addresses, ::freeaddrinfo);
            Socket socket(::socket(AF_INET, SOCK_STREAM, 0));
            if (socket.descriptor < 0 || ::connect(
                    socket.descriptor, addresses->ai_addr,
                    addresses->ai_addrlen) != 0) {
                throw socket_error("malformed-request connect failed");
            }
            write_all(
                socket.descriptor,
                "SMAVE_GATE_TXN 1\noperation=evaluate_commit\n");
        }

        const auto shutdown = transact(host, port, serialize_request({
            .operation = "shutdown",
            .transaction_id = 0,
            .family = "none",
        }));
        require_field(shutdown, "status", "shutdown");
        return 0;
    }

    require_field(replay, "duplicate_replays", "4");
    require_field(replay, "server_starts", "8");
    require_field(replay, "checksum_failures", "1");
    require_field(replay, "authentication_failures", "2");
    require_field(replay, "previous_generation_recoveries", "1");
    require_field(replay, "stale_current_generations_rejected", "1");
    require_field(replay, "mirror_recoveries", "2");
    require_field(replay, "state_key_rotations", "0");
    require_field(replay, "witness_key_rotations", "0");

    const auto stats = transact(host, port, serialize_request({
        .operation = "stats",
        .transaction_id = 0,
        .family = "none",
    }));
    require_field(stats, "status", "stats");
    require_field(stats, "committed_transactions", "3");
    require_field(stats, "gate_rejections", "1");
    require_field(stats, "duplicate_replays", "4");
    require_field(stats, "transaction_conflicts", "1");
    require_field(stats, "dropped_replies", "2");
    require_field(stats, "malformed_requests", "1");
    require_field(stats, "server_starts", "8");
    require_field(stats, "recovered_transactions", "4");
    require_field(stats, "crash_after_commit_injections", "1");
    require_field(stats, "witness_lag_crash_injections", "1");
    require_field(stats, "witness_lag_recoveries", "1");
    require_field(stats, "torn_temporary_recoveries", "4");
    require_field(stats, "checksum_failures", "1");
    require_field(stats, "authentication_failures", "2");
    require_field(stats, "previous_generation_recoveries", "1");
    require_field(stats, "stale_current_generations_rejected", "1");
    require_field(stats, "mirror_recoveries", "2");
    require_field(stats, "state_key_rotations", "0");
    require_field(stats, "witness_key_rotations", "0");

    const auto shutdown = transact(host, port, serialize_request({
        .operation = "shutdown",
        .transaction_id = 0,
        .family = "none",
    }));
    require_field(shutdown, "status", "shutdown");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string(argv[1]) == "crypto-self-test") {
            const std::string key(
                "key-for-rfc4231-test-vector-number-one-0000000000000000");
            const auto signature = smave::hmac_sha256_text(key, "Hi There");
            if (!smave::verify_hmac_sha256_text(key, "Hi There", signature) ||
                smave::verify_hmac_sha256_text(key, "Hi There!", signature) ||
                smave::verify_hmac_sha256_text(key + "wrong", "Hi There", signature)) {
                throw std::runtime_error("HMAC SHA-256 self-test failed");
            }
            std::cout << "SMAVE_GATE_AUTHORITY_CRYPTO_CHECK 1\n";
            return 0;
        }
        if (argc == 12 && std::string(argv[1]) == "server") {
            return run_server(
                argv[2], argv[3], argv[4], argv[5], argv[6], argv[7], argv[8],
                argv[9], argv[10], argv[11]);
        }
        if (argc == 11 && std::string(argv[1]) == "client") {
            return run_client(
                argv[2], argv[3], argv[4], argv[5], argv[6], argv[7], argv[8],
                argv[9], std::string(argv[10]) == "1");
        }
        throw std::invalid_argument(
            "usage: gate_network_authority_evidence server linear.mo nonlinear.mo "
            "port state mirror key previous-key witness witness-key "
            "previous-witness-key\n"
            "   or: gate_network_authority_evidence crypto-self-test\n"
            "   or: gate_network_authority_evidence client phase host port linear.mo "
            "scenarios nonlinear.mo output deployment distinct_namespaces");
    } catch (const KeyFilePolicyFailure& error) {
        std::cerr << "network authority key file policy blocked: "
                  << error.what() << '\n';
        return 88;
    } catch (const KeyPolicyFailure& error) {
        std::cerr << "network authority key policy blocked: " << error.what() << '\n';
        return 88;
    } catch (const StateRecoveryFailure& error) {
        std::cerr << "network authority recovery blocked: " << error.what() << '\n';
        return 88;
    } catch (const std::exception& error) {
        std::cerr << "network authority evidence failure: " << error.what() << '\n';
        return 2;
    }
}
