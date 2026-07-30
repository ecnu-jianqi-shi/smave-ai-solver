#include "smave/residency.hpp"

#include <algorithm>
#include <stdexcept>
#include <tuple>

namespace smave {

ExpertResidencyManager::ExpertResidencyManager(ResidencyConfig config)
    : config_(std::move(config)) {
    if (config_.device != "cpu") {
        throw std::invalid_argument("expert residency currently supports only the cpu device");
    }
    if (config_.capacity_bytes == 0) {
        throw std::invalid_argument("expert residency capacity must be positive");
    }
    if (config_.minimum_invocations == 0) {
        throw std::invalid_argument("expert residency minimum invocations must be positive");
    }
}

ResidencyDecision ExpertResidencyManager::request(
    const std::string& expert_version,
    std::size_t expert_bytes) {
    if (expert_version.empty()) {
        throw std::invalid_argument("resident expert version must not be empty");
    }
    if (expert_bytes == 0) {
        throw std::invalid_argument("resident expert size must be positive");
    }

    std::lock_guard lock(mutex_);
    ++sequence_;
    const std::uint64_t invocation_heat = ++heat_[expert_version];
    ResidencyDecision decision;
    decision.expert_version = expert_version;
    decision.device = config_.device;
    decision.expert_bytes = expert_bytes;
    decision.invocation_heat = invocation_heat;

    const auto existing = resident_.find(expert_version);
    if (existing != resident_.end()) {
        if (existing->second.bytes != expert_bytes) {
            decision.reason = "resident artifact size changed for the same expert version";
            decision.resident_bytes = resident_bytes_;
            return decision;
        }
        existing->second.heat = invocation_heat;
        existing->second.last_used = sequence_;
        decision.admitted = true;
        decision.cache_hit = true;
        decision.resident_bytes = resident_bytes_;
        decision.reason = "expert is already resident on cpu";
        return decision;
    }
    if (invocation_heat < config_.minimum_invocations) {
        decision.reason = "expert invocation heat is below the admission threshold";
        decision.resident_bytes = resident_bytes_;
        return decision;
    }
    if (expert_bytes > config_.capacity_bytes) {
        decision.reason = "expert artifact exceeds the cpu residency budget";
        decision.resident_bytes = resident_bytes_;
        return decision;
    }

    std::vector<std::pair<std::string, Entry>> victims;
    victims.reserve(resident_.size());
    for (const auto& entry : resident_) victims.push_back(entry);
    std::sort(victims.begin(), victims.end(), [](const auto& left, const auto& right) {
        return std::tie(left.second.heat, left.second.last_used, left.first) <
            std::tie(right.second.heat, right.second.last_used, right.first);
    });

    std::size_t available = config_.capacity_bytes - resident_bytes_;
    std::size_t reclaimable = 0;
    std::size_t victim_count = 0;
    while (available + reclaimable < expert_bytes && victim_count < victims.size()) {
        if (victims[victim_count].second.heat >= invocation_heat) break;
        reclaimable += victims[victim_count].second.bytes;
        ++victim_count;
    }
    if (available + reclaimable < expert_bytes) {
        decision.reason = "cpu residency budget is occupied by equally hot or hotter experts";
        decision.resident_bytes = resident_bytes_;
        return decision;
    }

    for (std::size_t index = 0; index < victim_count; ++index) {
        const auto& [version, entry] = victims[index];
        resident_bytes_ -= entry.bytes;
        resident_.erase(version);
        decision.evicted_experts.push_back(version);
    }
    resident_.emplace(expert_version, Entry{expert_bytes, invocation_heat, sequence_});
    resident_bytes_ += expert_bytes;
    decision.admitted = true;
    decision.resident_bytes = resident_bytes_;
    decision.reason = decision.evicted_experts.empty()
        ? "expert loaded into cpu residency"
        : "expert loaded after deterministic heat-based eviction";
    return decision;
}

ResidencySnapshot ExpertResidencyManager::snapshot() const {
    std::lock_guard lock(mutex_);
    ResidencySnapshot snapshot;
    snapshot.device = config_.device;
    snapshot.capacity_bytes = config_.capacity_bytes;
    snapshot.resident_bytes = resident_bytes_;
    snapshot.resident_experts.reserve(resident_.size());
    for (const auto& [version, _] : resident_) snapshot.resident_experts.push_back(version);
    std::sort(snapshot.resident_experts.begin(), snapshot.resident_experts.end());
    return snapshot;
}

}  // namespace smave
