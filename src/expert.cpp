#include "smave/expert.hpp"

#include "smave/device.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace smave {
namespace {

std::string digest(std::string_view input) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (const unsigned char character : input) {
        hash ^= character;
        hash *= 1099511628211ULL;
    }
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << hash;
    return output.str();
}

int rank(EvidenceLevel level) { return static_cast<int>(level); }
int rank(Permission permission) { return static_cast<int>(permission); }

EvidenceLevel required_evidence(Permission permission) {
    switch (permission) {
        case Permission::shadow: return EvidenceLevel::e0;
        case Permission::warm_start: return EvidenceLevel::e2;
        case Permission::corrected: return EvidenceLevel::e2;
        case Permission::direct: return EvidenceLevel::e3;
    }
    return EvidenceLevel::e4;
}

std::string bundle_contract(const RuntimeBundle& bundle) {
    std::ostringstream contract;
    contract << bundle.bundle_id << '|' << bundle.model_source_hash << '|'
             << bundle.ir_schema_version << '|' << bundle.domain_version << '|'
             << bundle.tolerance_profile << '|' << bundle.hardware_profile << '|'
             << bundle.terminal_fallback << '|';
    for (const auto& version : bundle.expert_versions) contract << version << ';';
    contract << '|';
    for (const auto& hash : bundle.expert_artifact_hashes) contract << hash << ';';
    contract << '|';
    for (const auto& hash : bundle.expert_evidence_hashes) contract << hash << ';';
    return contract.str();
}

std::int64_t now_seconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

}  // namespace

bool Expert::apply_preconditioner(
    const BlockIR&,
    const BlockContext&,
    const std::vector<double>&,
    std::vector<double>&) const {
    return false;
}

bool Expert::apply_preconditioner_batch(
    const BlockIR& block,
    const std::vector<BlockContext>& contexts,
    const std::vector<std::vector<double>>& residuals,
    std::vector<std::vector<double>>& results) const {
    if (contexts.size() != residuals.size()) return false;
    results.resize(residuals.size());
    for (std::size_t index = 0; index < residuals.size(); ++index) {
        if (!apply_preconditioner(
                block, contexts[index], residuals[index], results[index])) return false;
    }
    return true;
}

bool Expert::apply_preconditioner_batch_on_device(
    const std::string& device,
    const BlockIR& block,
    const std::vector<BlockContext>& contexts,
    const std::vector<std::vector<double>>& residuals,
    std::vector<std::vector<double>>& results,
    DeviceExecutionResult* execution) const {
    if (execution != nullptr) {
        *execution = DeviceExecutionResult{};
        execution->backend = device == "cpu" ? "cpu-expert-batch" : "unsupported-device";
        execution->available = device == "cpu";
        execution->reason = device == "cpu"
            ? "CPU expert batch selected"
            : "expert does not implement requested device batch";
    }
    return device == "cpu" &&
        apply_preconditioner_batch(block, contexts, residuals, results);
}

bool Expert::device_batch_is_resident(
    const std::string&, std::size_t, std::size_t) const {
    return false;
}

void RuntimeBundle::add_expert(
    std::string version,
    std::string artifact_hash,
    std::string evidence_hash) {
    if (version.empty() || artifact_hash.empty() || evidence_hash.empty()) {
        throw std::invalid_argument("bundle expert binding cannot be empty");
    }
    expert_versions.push_back(std::move(version));
    expert_artifact_hashes.push_back(std::move(artifact_hash));
    expert_evidence_hashes.push_back(std::move(evidence_hash));
    seal();
}

void RuntimeBundle::seal() { bundle_hash = digest(bundle_contract(*this)); }

void RuntimeBundle::write(const std::filesystem::path& path) const {
    if (expert_versions.size() != expert_artifact_hashes.size() ||
        expert_versions.size() != expert_evidence_hashes.size()) {
        throw std::invalid_argument("bundle expert, artifact and evidence counts differ");
    }
    if (bundle_hash != digest(bundle_contract(*this))) {
        throw std::invalid_argument("RuntimeBundle hash is invalid");
    }
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write bundle: " + path.string());
    output << "SMAVE_BUNDLE 1\n"
           << "ID " << std::quoted(bundle_id) << '\n'
           << "MODEL " << std::quoted(model_source_hash) << '\n'
           << "IR " << std::quoted(ir_schema_version) << '\n'
           << "DOMAIN " << std::quoted(domain_version) << '\n'
           << "TOLERANCE " << std::quoted(tolerance_profile) << '\n'
           << "HARDWARE " << std::quoted(hardware_profile) << '\n'
           << "FALLBACK " << std::quoted(terminal_fallback) << '\n'
           << "EXPERTS " << expert_versions.size() << '\n';
    for (std::size_t index = 0; index < expert_versions.size(); ++index) {
        output << "EXPERT " << std::quoted(expert_versions[index]) << ' '
               << std::quoted(expert_artifact_hashes[index]) << ' '
               << std::quoted(expert_evidence_hashes[index]) << '\n';
    }
    output << "\nHASH " << std::quoted(bundle_hash) << "\nEND\n";
}

RuntimeBundle RuntimeBundle::read(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read bundle: " + path.string());
    auto tag = [&](std::string_view expected) {
        std::string actual;
        input >> actual;
        if (!input || actual != expected) {
            throw std::runtime_error("invalid bundle: expected " + std::string(expected));
        }
    };
    RuntimeBundle bundle;
    tag("SMAVE_BUNDLE");
    int version{}; input >> version;
    if (version != 1) throw std::runtime_error("unsupported bundle schema");
    tag("ID"); input >> std::quoted(bundle.bundle_id);
    tag("MODEL"); input >> std::quoted(bundle.model_source_hash);
    tag("IR"); input >> std::quoted(bundle.ir_schema_version);
    tag("DOMAIN"); input >> std::quoted(bundle.domain_version);
    tag("TOLERANCE"); input >> std::quoted(bundle.tolerance_profile);
    tag("HARDWARE"); input >> std::quoted(bundle.hardware_profile);
    tag("FALLBACK"); input >> std::quoted(bundle.terminal_fallback);
    tag("EXPERTS"); std::size_t count{}; input >> count;
    bundle.expert_versions.resize(count);
    bundle.expert_artifact_hashes.resize(count);
    bundle.expert_evidence_hashes.resize(count);
    for (std::size_t index = 0; index < count; ++index) {
        tag("EXPERT");
        input >> std::quoted(bundle.expert_versions[index])
              >> std::quoted(bundle.expert_artifact_hashes[index])
              >> std::quoted(bundle.expert_evidence_hashes[index]);
    }
    tag("HASH"); input >> std::quoted(bundle.bundle_hash);
    tag("END");
    input >> std::ws;
    if (!input.eof()) throw std::runtime_error("trailing RuntimeBundle content");
    if (bundle.bundle_hash != digest(bundle_contract(bundle))) {
        throw std::runtime_error("RuntimeBundle integrity check failed");
    }
    return bundle;
}

void Registry::register_expert(
    std::shared_ptr<const Expert> expert, ExpertGrant grant) {
    if (!expert) throw std::invalid_argument("cannot register a null expert");
    if (expert->version() != grant.expert_version) {
        throw std::invalid_argument("expert version and grant differ");
    }
    if (grant.evidence_bundle.empty() || grant.artifact_hash.empty()) {
        throw std::invalid_argument("expert grant lacks immutable evidence or artifact hash");
    }
    if (grant.resident_bytes == 0) {
        throw std::invalid_argument("expert grant resident size must be positive");
    }
    const Capability capability = expert->match(BlockIR{});
    if (rank(grant.permission) > rank(capability.maximum_permission) ||
        rank(grant.evidence_level) > rank(capability.evidence_level)) {
        throw std::invalid_argument("grant exceeds expert capability");
    }
    if (rank(grant.evidence_level) < rank(required_evidence(grant.permission))) {
        throw std::invalid_argument("grant evidence is insufficient for permission");
    }
    const std::string version = grant.expert_version;
    if (!entries_.emplace(version, Entry{std::move(expert), std::move(grant)}).second) {
        throw std::invalid_argument("expert version is already registered");
    }
}

const Expert& Registry::expert(const std::string& version) const {
    const auto iterator = entries_.find(version);
    if (iterator == entries_.end()) throw std::out_of_range("expert is not registered: " + version);
    return *iterator->second.expert;
}

const ExpertGrant& Registry::grant(const std::string& version) const {
    const auto iterator = entries_.find(version);
    if (iterator == entries_.end()) throw std::out_of_range("grant is not registered: " + version);
    return iterator->second.grant;
}

bool Registry::compatible(
    const std::string& version,
    const BlockIR& block,
    const RuntimeBundle& bundle,
    Permission requested) const {
    const auto iterator = entries_.find(version);
    if (iterator == entries_.end()) return false;
    if (std::find(bundle.expert_versions.begin(), bundle.expert_versions.end(), version) ==
        bundle.expert_versions.end()) return false;
    const auto& entry = iterator->second;
    const auto& grant = entry.grant;
    if (grant.block_family != "*" && grant.block_family != block.fingerprint) return false;
    if (grant.domain_version != bundle.domain_version ||
        grant.tolerance_profile != bundle.tolerance_profile ||
        grant.hardware_profile != bundle.hardware_profile) return false;
    if (grant.expires_unix_seconds != 0 && grant.expires_unix_seconds <= now_seconds()) return false;
    if (rank(requested) > rank(grant.permission) ||
        rank(grant.evidence_level) < rank(required_evidence(requested))) return false;
    const Capability capability = entry.expert->match(block);
    if (rank(requested) > rank(capability.maximum_permission)) return false;
    if (block.event_related && !capability.event_related) return false;
    if (block.linear && !capability.linear) return false;
    if (!block.linear && !capability.nonlinear) return false;
    return true;
}

void Registry::validate_bundle(const RuntimeBundle& bundle, const ModelIR& model) const {
    if (bundle.model_source_hash != model.source_hash) {
        throw std::invalid_argument("RuntimeBundle targets a different source model");
    }
    if (bundle.ir_schema_version != model.schema_version) {
        throw std::invalid_argument("RuntimeBundle targets a different IR schema");
    }
    if (bundle.terminal_fallback != "original-damped-newton") {
        throw std::invalid_argument("RuntimeBundle removed the required original fallback");
    }
    if (bundle.bundle_hash != digest(bundle_contract(bundle))) {
        throw std::invalid_argument("RuntimeBundle hash is invalid");
    }
    if (bundle.expert_versions.size() != bundle.expert_artifact_hashes.size() ||
        bundle.expert_versions.size() != bundle.expert_evidence_hashes.size()) {
        throw std::invalid_argument("bundle expert, artifact and evidence counts differ");
    }
    for (std::size_t index = 0; index < bundle.expert_versions.size(); ++index) {
        const auto& version = bundle.expert_versions[index];
        if (!entries_.contains(version)) {
            throw std::invalid_argument("bundle references unregistered expert: " + version);
        }
        if (entries_.at(version).grant.artifact_hash !=
            bundle.expert_artifact_hashes[index]) {
            throw std::invalid_argument("bundle expert artifact hash mismatch: " + version);
        }
        if (entries_.at(version).grant.evidence_bundle !=
            bundle.expert_evidence_hashes[index]) {
            throw std::invalid_argument("bundle expert evidence hash mismatch: " + version);
        }
    }
}

ContinuationWarmStartExpert::ContinuationWarmStartExpert(const ModelIR& model) {
    for (const auto& variable : model.variables) starts_[variable.name] = variable.start;
}

std::string ContinuationWarmStartExpert::version() const {
    return "continuation-warm-start-v1";
}

Capability ContinuationWarmStartExpert::match(const BlockIR&) const {
    return Capability{
        .linear = false,
        .nonlinear = true,
        .event_related = false,
        .preconditioner = false,
        .backend_roles = {BackendRole::initializer},
        .devices = {"cpu"},
        .evidence_level = EvidenceLevel::e2,
        .maximum_permission = Permission::warm_start,
    };
}

Estimate ContinuationWarmStartExpert::estimate(
    const BlockIR& block, const BlockContext& context) const {
    const bool has_history = std::all_of(
        block.unknowns.begin(), block.unknowns.end(),
        [&](const std::string& name) { return context.previous_solution.contains(name); });
    return Estimate{
        .pass_probability = has_history ? 0.90 : 0.55,
        .expected_setup_time_us = 0.2,
        .expected_solve_time_us = 0.5,
        .expected_correction_time_us = has_history ? 8.0 : 25.0,
        .failure_cost_us = 50.0,
        .risk_score = context.ood_score + (has_history ? 0.02 : 0.10),
        .ood_score = context.ood_score,
    };
}

ExpertResult ContinuationWarmStartExpert::solve(
    const BlockIR& block,
    const BlockContext& context,
    const SolveBudget&) const {
    ExpertResult result;
    for (const auto& unknown : block.unknowns) {
        const auto previous = context.previous_solution.find(unknown);
        result.candidate[unknown] = previous != context.previous_solution.end()
            ? previous->second
            : starts_.at(unknown);
    }
    result.status = "candidate";
    result.uncertainty = context.previous_solution.empty() ? 0.5 : 0.1;
    result.telemetry["history_variables"] =
        static_cast<double>(context.previous_solution.size());
    return result;
}

RuntimeBundle make_default_bundle(const ModelIR& model) {
    RuntimeBundle bundle;
    bundle.bundle_id = "development-" + model.source_hash;
    bundle.model_source_hash = model.source_hash;
    bundle.expert_versions = {"continuation-warm-start-v1"};
    bundle.expert_artifact_hashes = {"builtin-continuation-v1"};
    bundle.expert_evidence_hashes = {"builtin-phase1-golden-tests"};
    bundle.seal();
    return bundle;
}

Registry make_default_registry(const ModelIR& model) {
    Registry registry;
    auto expert = std::make_shared<ContinuationWarmStartExpert>(model);
    registry.register_expert(
        expert,
        ExpertGrant{
            .expert_version = expert->version(),
            .block_family = "*",
            .domain_version = "domain-v1",
            .tolerance_profile = "default",
            .hardware_profile = "cpu",
            .permission = Permission::warm_start,
            .evidence_level = EvidenceLevel::e2,
            .evidence_bundle = "builtin-phase1-golden-tests",
            .artifact_hash = "builtin-continuation-v1",
            .expires_unix_seconds = 0,
        });
    return registry;
}

std::string to_string(EvidenceLevel level) {
    return "E" + std::to_string(rank(level));
}

std::string to_string(Permission permission) {
    switch (permission) {
        case Permission::shadow: return "shadow";
        case Permission::warm_start: return "warm_start";
        case Permission::corrected: return "corrected";
        case Permission::direct: return "direct";
    }
    return "unknown";
}

}  // namespace smave
