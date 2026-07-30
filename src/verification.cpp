#include "smave/verification.hpp"

#include <algorithm>
#include <cmath>
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

std::string contract(const VerificationCertificate& certificate) {
    std::ostringstream output;
    output << std::setprecision(17) << certificate.schema_version << '|'
           << certificate.expert_version << '|' << certificate.artifact_hash << '|'
           << certificate.block_fingerprint << '|' << certificate.domain_version << '|';
    if (certificate.schema_version == "smave.verified-cells.v2") {
        output << certificate.training_dataset_id << '|'
               << certificate.training_dataset_version << '|'
               << certificate.training_dataset_manifest_hash << '|';
    }
    output << certificate.total_probes << '|';
    for (const auto& cell : certificate.cells) {
        for (const auto& feature : cell.features) output << feature << ';';
        for (const double value : cell.lower) output << value << ';';
        for (const double value : cell.upper) output << value << ';';
        output << cell.worst_residual << ';' << cell.worst_risk << ';' << cell.probes << '|';
    }
    for (const auto& item : certificate.counterexamples) {
        std::vector<std::pair<std::string, double>> ordered(item.context.begin(), item.context.end());
        std::sort(ordered.begin(), ordered.end());
        for (const auto& [name, value] : ordered) output << name << '=' << value << ';';
        output << item.residual << ';' << item.risk << ';' << item.reason << '|';
    }
    return output.str();
}

std::vector<std::unordered_map<std::string, double>> probes(const VerifiedCell& cell) {
    const std::size_t dimensions = cell.features.size();
    std::vector<std::unordered_map<std::string, double>> result;
    if (dimensions > 12) {
        std::unordered_map<std::string, double> center;
        for (std::size_t index = 0; index < dimensions; ++index) {
            center[cell.features[index]] = 0.5 * (cell.lower[index] + cell.upper[index]);
        }
        result.push_back(center);
        for (std::size_t axis = 0; axis < dimensions; ++axis) {
            auto lower = center;
            auto upper = center;
            lower[cell.features[axis]] = cell.lower[axis];
            upper[cell.features[axis]] = cell.upper[axis];
            result.push_back(std::move(lower));
            result.push_back(std::move(upper));
        }
        auto alternating_a = center;
        auto alternating_b = center;
        for (std::size_t index = 0; index < dimensions; ++index) {
            alternating_a[cell.features[index]] = index % 2 == 0
                ? cell.lower[index] : cell.upper[index];
            alternating_b[cell.features[index]] = index % 2 == 0
                ? cell.upper[index] : cell.lower[index];
        }
        result.push_back(std::move(alternating_a));
        result.push_back(std::move(alternating_b));
        return result;
    }
    const std::size_t corners = std::size_t{1} << dimensions;
    result.reserve(corners + 1);
    for (std::size_t mask = 0; mask < corners; ++mask) {
        std::unordered_map<std::string, double> context;
        for (std::size_t index = 0; index < dimensions; ++index) {
            context[cell.features[index]] = (mask & (std::size_t{1} << index))
                ? cell.upper[index] : cell.lower[index];
        }
        result.push_back(std::move(context));
    }
    std::unordered_map<std::string, double> center;
    for (std::size_t index = 0; index < dimensions; ++index) {
        center[cell.features[index]] = 0.5 * (cell.lower[index] + cell.upper[index]);
    }
    result.push_back(std::move(center));
    return result;
}

void verify_recursive(
    VerifiedCell cell,
    const VerificationProbe& probe,
    std::size_t depth,
    std::size_t maximum_depth,
    double minimum_width,
    VerificationCertificate& certificate) {
    bool accepted = true;
    Counterexample worst;
    for (const auto& context : probes(cell)) {
        const ProbeResult result = probe(context);
        ++cell.probes;
        ++certificate.total_probes;
        cell.worst_residual = std::max(cell.worst_residual, result.residual);
        cell.worst_risk = std::max(cell.worst_risk, result.risk);
        if (!result.accepted) {
            accepted = false;
            if (result.residual + result.risk >= worst.residual + worst.risk) {
                worst = Counterexample{context, result.residual, result.risk, result.reason};
            }
        }
    }
    if (accepted) {
        certificate.cells.push_back(std::move(cell));
        return;
    }
    std::size_t split = 0;
    double width = 0.0;
    for (std::size_t index = 0; index < cell.features.size(); ++index) {
        const double candidate = cell.upper[index] - cell.lower[index];
        if (candidate > width) { width = candidate; split = index; }
    }
    if (depth >= maximum_depth || width <= minimum_width) {
        const auto duplicate = std::find_if(
            certificate.counterexamples.begin(), certificate.counterexamples.end(),
            [&](const Counterexample& item) { return item.context == worst.context; });
        if (duplicate == certificate.counterexamples.end()) {
            certificate.counterexamples.push_back(std::move(worst));
        } else if (worst.residual + worst.risk > duplicate->residual + duplicate->risk) {
            *duplicate = std::move(worst);
        }
        return;
    }
    const double midpoint = 0.5 * (cell.lower[split] + cell.upper[split]);
    VerifiedCell left = cell;
    VerifiedCell right = cell;
    left.upper[split] = midpoint;
    right.lower[split] = midpoint;
    left.probes = right.probes = 0;
    left.worst_residual = right.worst_residual = 0.0;
    left.worst_risk = right.worst_risk = 0.0;
    verify_recursive(std::move(left), probe, depth + 1, maximum_depth, minimum_width, certificate);
    verify_recursive(std::move(right), probe, depth + 1, maximum_depth, minimum_width, certificate);
}

}  // namespace

void VerificationCertificate::seal() { certificate_hash = digest(contract(*this)); }

void VerificationCertificate::validate() const {
    if ((schema_version != "smave.verified-cells.v1" &&
         schema_version != "smave.verified-cells.v2") || expert_version.empty() ||
        artifact_hash.empty() || block_fingerprint.empty()) {
        throw std::invalid_argument("verification certificate identity is invalid");
    }
    const bool has_training_dataset = !training_dataset_id.empty() ||
        !training_dataset_version.empty() || !training_dataset_manifest_hash.empty();
    if (schema_version == "smave.verified-cells.v1" && has_training_dataset) {
        throw std::invalid_argument("verification certificate v1 cannot contain training lineage");
    }
    if (schema_version == "smave.verified-cells.v2" &&
        (training_dataset_id.empty() || training_dataset_version.empty() ||
         training_dataset_manifest_hash.empty())) {
        throw std::invalid_argument(
            "verification certificate v2 requires complete training lineage");
    }
    for (const auto& cell : cells) {
        if (cell.features.empty() || cell.features.size() != cell.lower.size() ||
            cell.lower.size() != cell.upper.size() || cell.probes == 0) {
            throw std::invalid_argument("verified cell shape is invalid");
        }
        for (std::size_t index = 0; index < cell.lower.size(); ++index) {
            if (cell.lower[index] > cell.upper[index]) {
                throw std::invalid_argument("verified cell bounds are invalid");
            }
        }
    }
    if (certificate_hash != digest(contract(*this))) {
        throw std::invalid_argument("verification certificate integrity check failed");
    }
}

bool VerificationCertificate::contains(
    const std::unordered_map<std::string, double>& context) const {
    return std::any_of(cells.begin(), cells.end(), [&](const VerifiedCell& cell) {
        for (std::size_t index = 0; index < cell.features.size(); ++index) {
            const auto value = context.find(cell.features[index]);
            if (value == context.end() || value->second < cell.lower[index] ||
                value->second > cell.upper[index]) return false;
        }
        return true;
    });
}

VerificationCertificate verify_cells(
    const std::string& expert_version,
    const std::string& artifact_hash,
    const std::string& block_fingerprint,
    const std::vector<std::string>& features,
    const std::vector<double>& lower,
    const std::vector<double>& upper,
    const VerificationProbe& probe,
    std::size_t maximum_depth,
    double minimum_width) {
    if (!probe || features.empty() || features.size() != lower.size() ||
        lower.size() != upper.size()) {
        throw std::invalid_argument("verified-cell request is invalid");
    }
    VerificationCertificate certificate;
    certificate.expert_version = expert_version;
    certificate.artifact_hash = artifact_hash;
    certificate.block_fingerprint = block_fingerprint;
    verify_recursive(
        VerifiedCell{features, lower, upper}, probe, 0, maximum_depth,
        minimum_width, certificate);
    certificate.seal();
    certificate.validate();
    return certificate;
}

void VerificationCertificate::write(const std::filesystem::path& path) const {
    validate();
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write verification certificate");
    output << std::setprecision(17)
           << "SMAVE_VERIFIED_CELLS "
           << (schema_version == "smave.verified-cells.v2" ? 2 : 1) << '\n'
           << "EXPERT " << std::quoted(expert_version) << '\n'
           << "ARTIFACT " << std::quoted(artifact_hash) << '\n'
           << "BLOCK " << std::quoted(block_fingerprint) << '\n'
           << "DOMAIN " << std::quoted(domain_version) << '\n';
    if (schema_version == "smave.verified-cells.v2") {
        output << "TRAINING_DATASET_ID " << std::quoted(training_dataset_id) << '\n'
               << "TRAINING_DATASET_VERSION " << std::quoted(training_dataset_version) << '\n'
               << "TRAINING_DATASET_MANIFEST_HASH "
               << std::quoted(training_dataset_manifest_hash) << '\n';
    }
    output << "PROBES " << total_probes << '\n'
           << "CELLS " << cells.size() << '\n';
    for (const auto& cell : cells) {
        output << "CELL " << cell.features.size() << ' ' << cell.worst_residual << ' '
               << cell.worst_risk << ' ' << cell.probes;
        for (std::size_t index = 0; index < cell.features.size(); ++index) {
            output << ' ' << std::quoted(cell.features[index]) << ' '
                   << cell.lower[index] << ' ' << cell.upper[index];
        }
        output << '\n';
    }
    output << "COUNTEREXAMPLES " << counterexamples.size() << '\n';
    for (const auto& item : counterexamples) {
        output << "COUNTEREXAMPLE " << item.context.size() << ' ' << item.residual << ' '
               << item.risk << ' ' << std::quoted(item.reason);
        std::vector<std::pair<std::string, double>> ordered(item.context.begin(), item.context.end());
        std::sort(ordered.begin(), ordered.end());
        for (const auto& [name, value] : ordered) output << ' ' << std::quoted(name) << ' ' << value;
        output << '\n';
    }
    output << "HASH " << std::quoted(certificate_hash) << "\nEND\n";
}

VerificationCertificate VerificationCertificate::read(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read verification certificate");
    auto tag = [&](std::string_view expected) {
        std::string actual; input >> actual;
        if (!input || actual != expected) throw std::runtime_error("invalid verification certificate");
    };
    VerificationCertificate certificate;
    tag("SMAVE_VERIFIED_CELLS"); int version{}; input >> version;
    if (version != 1 && version != 2) {
        throw std::runtime_error("unsupported verification certificate schema");
    }
    certificate.schema_version = version == 2
        ? "smave.verified-cells.v2"
        : "smave.verified-cells.v1";
    tag("EXPERT"); input >> std::quoted(certificate.expert_version);
    tag("ARTIFACT"); input >> std::quoted(certificate.artifact_hash);
    tag("BLOCK"); input >> std::quoted(certificate.block_fingerprint);
    tag("DOMAIN"); input >> std::quoted(certificate.domain_version);
    if (version >= 2) {
        tag("TRAINING_DATASET_ID"); input >> std::quoted(certificate.training_dataset_id);
        tag("TRAINING_DATASET_VERSION");
        input >> std::quoted(certificate.training_dataset_version);
        tag("TRAINING_DATASET_MANIFEST_HASH");
        input >> std::quoted(certificate.training_dataset_manifest_hash);
    }
    tag("PROBES"); input >> certificate.total_probes;
    tag("CELLS"); std::size_t cells{}; input >> cells;
    for (std::size_t item = 0; item < cells; ++item) {
        tag("CELL"); VerifiedCell cell; std::size_t dimensions{};
        input >> dimensions >> cell.worst_residual >> cell.worst_risk >> cell.probes;
        cell.features.resize(dimensions); cell.lower.resize(dimensions); cell.upper.resize(dimensions);
        for (std::size_t index = 0; index < dimensions; ++index) {
            input >> std::quoted(cell.features[index]) >> cell.lower[index] >> cell.upper[index];
        }
        certificate.cells.push_back(std::move(cell));
    }
    tag("COUNTEREXAMPLES"); std::size_t counterexamples{}; input >> counterexamples;
    for (std::size_t item = 0; item < counterexamples; ++item) {
        tag("COUNTEREXAMPLE"); Counterexample counterexample; std::size_t dimensions{};
        input >> dimensions >> counterexample.residual >> counterexample.risk
              >> std::quoted(counterexample.reason);
        for (std::size_t index = 0; index < dimensions; ++index) {
            std::string name; double value{}; input >> std::quoted(name) >> value;
            counterexample.context[name] = value;
        }
        certificate.counterexamples.push_back(std::move(counterexample));
    }
    tag("HASH"); input >> std::quoted(certificate.certificate_hash);
    tag("END");
    input >> std::ws;
    if (!input.eof()) throw std::runtime_error("trailing verification certificate content");
    certificate.validate();
    return certificate;
}

void VerificationCertificate::export_counterexamples(
    const std::filesystem::path& directory) const {
    validate();
    std::filesystem::create_directories(directory);
    for (std::size_t index = 0; index < counterexamples.size(); ++index) {
        std::ofstream output(
            directory / ("counterexample-" + std::to_string(index + 1) + ".conf"));
        if (!output) throw std::runtime_error("cannot export verification counterexample");
        output << std::setprecision(17)
               << "# reason=" << counterexamples[index].reason << '\n'
               << "# residual=" << counterexamples[index].residual << '\n'
               << "# risk=" << counterexamples[index].risk << '\n';
        std::vector<std::pair<std::string, double>> ordered(
            counterexamples[index].context.begin(),
            counterexamples[index].context.end());
        std::sort(ordered.begin(), ordered.end());
        for (const auto& [name, value] : ordered) output << name << '=' << value << '\n';
    }
}

}  // namespace smave
