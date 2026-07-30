#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace smave {

struct ResidencyConfig {
    std::string device{"cpu"};
    std::size_t capacity_bytes{static_cast<std::size_t>(-1)};
    std::uint64_t minimum_invocations{1};
};

struct ResidencyDecision {
    bool admitted{false};
    bool cache_hit{false};
    std::string expert_version;
    std::string device{"cpu"};
    std::size_t expert_bytes{};
    std::size_t resident_bytes{};
    std::uint64_t invocation_heat{};
    std::vector<std::string> evicted_experts;
    std::string reason;
};

struct ResidencySnapshot {
    std::string device{"cpu"};
    std::size_t capacity_bytes{};
    std::size_t resident_bytes{};
    std::vector<std::string> resident_experts;
};

class ExpertResidencyManager {
public:
    explicit ExpertResidencyManager(ResidencyConfig config = {});

    [[nodiscard]] ResidencyDecision request(
        const std::string& expert_version,
        std::size_t expert_bytes);
    [[nodiscard]] ResidencySnapshot snapshot() const;

private:
    struct Entry {
        std::size_t bytes{};
        std::uint64_t heat{};
        std::uint64_t last_used{};
    };

    ResidencyConfig config_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::uint64_t> heat_;
    std::unordered_map<std::string, Entry> resident_;
    std::size_t resident_bytes_{};
    std::uint64_t sequence_{};
};

}  // namespace smave
