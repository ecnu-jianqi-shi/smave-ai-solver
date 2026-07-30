#include "smave/dae_learning.hpp"

#include "smave/runtime.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace smave {
namespace {

std::string digest(std::string_view input) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (const unsigned char value : input) {
        hash ^= value;
        hash *= 1099511628211ULL;
    }
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << hash;
    return output.str();
}

std::string contract(const DaeMultigridArtifact& artifact) {
    std::ostringstream output;
    output << artifact.schema_version << '|' << artifact.model_source_hash << '|'
           << artifact.unknown_count << '|' << std::setprecision(17);
    if (artifact.schema_version == "smave.dae-multigrid.v3") {
        output << artifact.residual_family << '|';
    }
    output
           << artifact.minimum_step << '|' << artifact.maximum_step << '|'
           << artifact.training_samples << '|';
    if (artifact.schema_version == "smave.dae-multigrid.v2" ||
        artifact.schema_version == "smave.dae-multigrid.v3") {
        output << artifact.training_dataset_id << '|'
               << artifact.training_dataset_version << '|'
               << artifact.training_dataset_manifest_hash << '|';
    }
    output << artifact.hierarchy.artifact_hash;
    return output.str();
}

void require_tag(std::istream& input, const std::string& expected) {
    std::string actual;
    input >> actual;
    if (!input || actual != expected) {
        throw std::runtime_error("invalid DAE multigrid artifact: expected " + expected);
    }
}

std::filesystem::path hierarchy_path(const std::filesystem::path& path) {
    return path.string() + ".multigrid";
}

std::vector<std::filesystem::path> scenario_files(const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> result;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".conf") {
            result.push_back(entry.path());
        }
    }
    std::sort(result.begin(), result.end());
    if (result.empty()) throw std::invalid_argument("DAE multigrid scenario suite is empty");
    return result;
}

double required_value(
    const std::unordered_map<std::string, double>& scenario,
    const std::string& key) {
    const auto found = scenario.find(key);
    if (found == scenario.end() || !std::isfinite(found->second)) {
        throw std::invalid_argument("DAE multigrid scenario lacks finite value: " + key);
    }
    return found->second;
}

}  // namespace

void DaeMultigridArtifact::seal() {
    hierarchy.validate();
    training_samples = hierarchy.training_samples;
    artifact_hash = digest(contract(*this));
}

void DaeMultigridArtifact::validate() const {
    if ((schema_version != "smave.dae-multigrid.v1" &&
         schema_version != "smave.dae-multigrid.v2" &&
         schema_version != "smave.dae-multigrid.v3") || model_source_hash.empty() ||
        unknown_count < 4 || !std::isfinite(minimum_step) ||
        !std::isfinite(maximum_step) || !(minimum_step > 0.0) ||
        maximum_step < minimum_step || training_samples == 0) {
        throw std::invalid_argument("invalid DAE multigrid artifact contract");
    }
    const bool has_lineage = !training_dataset_id.empty() ||
        !training_dataset_version.empty() || !training_dataset_manifest_hash.empty();
    if (schema_version == "smave.dae-multigrid.v1" && has_lineage) {
        throw std::invalid_argument("DAE multigrid v1 cannot contain training lineage");
    }
    if (schema_version == "smave.dae-multigrid.v2" &&
        (training_dataset_id.empty() || training_dataset_version.empty() ||
         training_dataset_manifest_hash.empty())) {
        throw std::invalid_argument("DAE multigrid v2 requires complete training lineage");
    }
    if (schema_version == "smave.dae-multigrid.v3" &&
        ((has_lineage && (training_dataset_id.empty() || training_dataset_version.empty() ||
                          training_dataset_manifest_hash.empty())) ||
         (residual_family != "semi-explicit-index1-step" &&
          residual_family != "fully-implicit-first-order-step"))) {
        throw std::invalid_argument(
            "DAE multigrid v3 requires complete optional lineage and residual family");
    }
    if (schema_version != "smave.dae-multigrid.v3" &&
        residual_family != "semi-explicit-index1-step") {
        throw std::invalid_argument("legacy DAE multigrid artifact has invalid residual family");
    }
    hierarchy.validate();
    if (hierarchy.model_source_hash != model_source_hash ||
        hierarchy.fine_operator.size() != unknown_count ||
        hierarchy.training_samples != training_samples ||
        hierarchy.features != std::vector<std::string>{"step"} ||
        hierarchy.feature_minimum != std::vector<double>{minimum_step} ||
        hierarchy.feature_maximum != std::vector<double>{maximum_step} ||
        !hierarchy.jacobian_mode) {
        throw std::invalid_argument("DAE multigrid hierarchy binding is inconsistent");
    }
    if (hierarchy.training_dataset_id != training_dataset_id ||
        hierarchy.training_dataset_version != training_dataset_version ||
        hierarchy.training_dataset_manifest_hash != training_dataset_manifest_hash) {
        throw std::invalid_argument("DAE wrapper and hierarchy training lineage differ");
    }
    if (artifact_hash != digest(contract(*this))) {
        throw std::invalid_argument("DAE multigrid artifact hash mismatch");
    }
}

void DaeMultigridArtifact::write(const std::filesystem::path& path) const {
    validate();
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    hierarchy.write(hierarchy_path(path));
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write DAE multigrid artifact");
    output << "SMAVE_DAE_MULTIGRID "
           << (schema_version == "smave.dae-multigrid.v3"
                   ? 3
                   : (schema_version == "smave.dae-multigrid.v2" ? 2 : 1)) << '\n'
           << "MODEL_SOURCE_HASH " << std::quoted(model_source_hash) << '\n';
    if (schema_version == "smave.dae-multigrid.v3") {
        output << "RESIDUAL_FAMILY " << std::quoted(residual_family) << '\n';
    }
    if (schema_version == "smave.dae-multigrid.v2" ||
        (schema_version == "smave.dae-multigrid.v3" &&
         !training_dataset_id.empty())) {
        output << "TRAINING_DATASET_ID " << std::quoted(training_dataset_id) << '\n'
               << "TRAINING_DATASET_VERSION " << std::quoted(training_dataset_version) << '\n'
               << "TRAINING_DATASET_MANIFEST_HASH "
               << std::quoted(training_dataset_manifest_hash) << '\n';
    }
    output << "UNKNOWN_COUNT " << unknown_count << '\n'
           << "MINIMUM_STEP " << std::setprecision(17) << minimum_step << '\n'
           << "MAXIMUM_STEP " << maximum_step << '\n'
           << "TRAINING_SAMPLES " << training_samples << '\n'
           << "HIERARCHY_HASH " << std::quoted(hierarchy.artifact_hash) << '\n'
           << "HASH " << std::quoted(artifact_hash) << "\nEND\n";
}

DaeMultigridArtifact DaeMultigridArtifact::read(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read DAE multigrid artifact");
    DaeMultigridArtifact artifact;
    require_tag(input, "SMAVE_DAE_MULTIGRID");
    int version{};
    input >> version;
    if (version < 1 || version > 3) {
        throw std::runtime_error("unsupported DAE multigrid artifact version");
    }
    artifact.schema_version = version == 3
        ? "smave.dae-multigrid.v3"
        : (version == 2 ? "smave.dae-multigrid.v2" : "smave.dae-multigrid.v1");
    require_tag(input, "MODEL_SOURCE_HASH"); input >> std::quoted(artifact.model_source_hash);
    if (version == 3) {
        require_tag(input, "RESIDUAL_FAMILY");
        input >> std::quoted(artifact.residual_family);
    }
    bool has_serialized_lineage = version == 2;
    if (version == 3) {
        const auto position = input.tellg();
        std::string tag;
        input >> tag;
        input.seekg(position);
        has_serialized_lineage = tag == "TRAINING_DATASET_ID";
    }
    if (has_serialized_lineage) {
        require_tag(input, "TRAINING_DATASET_ID");
        input >> std::quoted(artifact.training_dataset_id);
        require_tag(input, "TRAINING_DATASET_VERSION");
        input >> std::quoted(artifact.training_dataset_version);
        require_tag(input, "TRAINING_DATASET_MANIFEST_HASH");
        input >> std::quoted(artifact.training_dataset_manifest_hash);
    }
    require_tag(input, "UNKNOWN_COUNT"); input >> artifact.unknown_count;
    require_tag(input, "MINIMUM_STEP"); input >> artifact.minimum_step;
    require_tag(input, "MAXIMUM_STEP"); input >> artifact.maximum_step;
    require_tag(input, "TRAINING_SAMPLES"); input >> artifact.training_samples;
    require_tag(input, "HIERARCHY_HASH");
    std::string hierarchy_hash;
    input >> std::quoted(hierarchy_hash);
    require_tag(input, "HASH"); input >> std::quoted(artifact.artifact_hash);
    require_tag(input, "END");
    input >> std::ws;
    if (!input.eof()) throw std::runtime_error("trailing DAE multigrid content");
    artifact.hierarchy = LearnedMultigridArtifact::read(hierarchy_path(path));
    if (artifact.hierarchy.artifact_hash != hierarchy_hash) {
        throw std::runtime_error("DAE multigrid hierarchy payload hash mismatch");
    }
    artifact.validate();
    return artifact;
}

DaeMultigridArtifact train_dae_multigrid(
    const IndexOneDaeIR& model,
    const std::filesystem::path& scenario_directory) {
    model.validate();
    const std::size_t unknown_count = model.states.size() + model.algebraics.size();
    if (unknown_count < 4) {
        throw std::invalid_argument("DAE multigrid training requires at least four joint unknowns");
    }
    double minimum_step = std::numeric_limits<double>::infinity();
    double maximum_step{};
    std::vector<std::vector<std::vector<double>>> matrices;
    for (const auto& path : scenario_files(scenario_directory)) {
        const auto scenario = read_scenario(path);
        const double step = required_value(scenario, "step");
        const double target_time = required_value(scenario, "time");
        if (!(step > 0.0)) throw std::invalid_argument("DAE multigrid step must be positive");
        std::vector<double> previous_state;
        std::vector<double> candidate_state;
        std::vector<double> candidate_algebraic;
        for (const auto& variable : model.states) {
            previous_state.push_back(required_value(scenario, "previous." + variable.name));
            candidate_state.push_back(required_value(scenario, "state." + variable.name));
        }
        for (const auto& variable : model.algebraics) {
            candidate_algebraic.push_back(required_value(scenario, "algebraic." + variable.name));
        }
        const auto residual = evaluate_dae_step_residual(
            model, previous_state, candidate_state, candidate_algebraic, target_time, step);
        double residual_inf{};
        for (const double value : residual) residual_inf = std::max(residual_inf, std::abs(value));
        if (!std::isfinite(residual_inf) || residual_inf > 1.0e-7) {
            throw std::invalid_argument(
                "DAE multigrid training candidate fails the joint residual gate");
        }
        auto matrix = assemble_dae_step_jacobian(
            model, previous_state, candidate_state, candidate_algebraic, target_time, step);
        for (std::size_t row = 0; row < matrix.size(); ++row) {
            for (std::size_t column = row + 1; column < matrix.size(); ++column) {
                const double scale = std::max({1.0, std::abs(matrix[row][column]),
                                               std::abs(matrix[column][row])});
                if (std::abs(matrix[row][column] - matrix[column][row]) > 1.0e-7 * scale) {
                    throw std::invalid_argument(
                        "DAE multigrid training Jacobian is not near-symmetric");
                }
                const double value = 0.5 * (matrix[row][column] + matrix[column][row]);
                matrix[row][column] = value;
                matrix[column][row] = value;
            }
        }
        matrices.push_back(std::move(matrix));
        minimum_step = std::min(minimum_step, step);
        maximum_step = std::max(maximum_step, step);
    }
    DaeMultigridArtifact artifact;
    artifact.model_source_hash = model.source_hash;
    artifact.unknown_count = unknown_count;
    artifact.minimum_step = minimum_step;
    artifact.maximum_step = maximum_step;
    artifact.hierarchy = build_learned_multigrid_artifact(
        model.source_hash,
        "dae-joint-backward-euler-" + std::to_string(unknown_count),
        {"step"}, {minimum_step}, {maximum_step}, matrices, true);
    artifact.seal();
    artifact.validate();
    return artifact;
}

DaeMultigridArtifact train_dae_multigrid(
    const FullyImplicitDaeIR& model,
    const std::filesystem::path& scenario_directory) {
    model.validate();
    const std::size_t unknown_count = model.states.size() + model.algebraics.size();
    if (unknown_count < 4) {
        throw std::invalid_argument(
            "fully implicit DAE multigrid training requires at least four joint unknowns");
    }
    double minimum_step = std::numeric_limits<double>::infinity();
    double maximum_step{};
    std::vector<std::vector<std::vector<double>>> matrices;
    for (const auto& path : scenario_files(scenario_directory)) {
        const auto scenario = read_scenario(path);
        const double step = required_value(scenario, "step");
        const double target_time = required_value(scenario, "time");
        if (!(step > 0.0)) {
            throw std::invalid_argument(
                "fully implicit DAE multigrid step must be positive");
        }
        std::vector<double> previous_state;
        std::vector<double> candidate_state;
        std::vector<double> candidate_algebraic;
        for (const auto& variable : model.states) {
            previous_state.push_back(
                required_value(scenario, "previous." + variable.name));
            candidate_state.push_back(
                required_value(scenario, "state." + variable.name));
        }
        for (const auto& variable : model.algebraics) {
            candidate_algebraic.push_back(
                required_value(scenario, "algebraic." + variable.name));
        }
        const auto baseline = evaluate_fully_implicit_dae_step_residual(
            model, previous_state, candidate_state, candidate_algebraic,
            target_time, step);
        double residual_inf{};
        for (const double value : baseline) {
            residual_inf = std::max(residual_inf, std::abs(value));
        }
        if (!std::isfinite(residual_inf) || residual_inf > 1.0e-7) {
            throw std::invalid_argument(
                "fully implicit DAE multigrid candidate fails original residual gate");
        }
        const auto append_matrix = [&](
            const std::vector<double>& state,
            const std::vector<double>& algebraic) {
            const auto values = evaluate_fully_implicit_dae_step_residual(
                model, previous_state, state, algebraic, target_time, step);
            std::vector<std::vector<double>> matrix(
                unknown_count, std::vector<double>(unknown_count));
            for (std::size_t column = 0; column < unknown_count; ++column) {
                auto shifted_state = state;
                auto shifted_algebraic = algebraic;
                double reference{};
                if (column < model.states.size()) {
                    reference = shifted_state[column];
                } else {
                    reference = shifted_algebraic[column - model.states.size()];
                }
                const double perturbation =
                    1.0e-7 * std::max(1.0, std::abs(reference));
                if (column < model.states.size()) {
                    shifted_state[column] += perturbation;
                } else {
                    shifted_algebraic[column - model.states.size()] += perturbation;
                }
                const auto shifted = evaluate_fully_implicit_dae_step_residual(
                    model, previous_state, shifted_state, shifted_algebraic,
                    target_time, step);
                for (std::size_t row = 0; row < unknown_count; ++row) {
                    matrix[row][column] =
                        (shifted[row] - values[row]) / perturbation;
                }
            }
            for (std::size_t row = 0; row < matrix.size(); ++row) {
                for (std::size_t column = row + 1; column < matrix.size(); ++column) {
                    const double scale = std::max({
                        1.0, std::abs(matrix[row][column]),
                        std::abs(matrix[column][row])});
                    if (std::abs(matrix[row][column] - matrix[column][row]) >
                        1.0e-7 * scale) {
                        throw std::invalid_argument(
                            "fully implicit DAE multigrid Jacobian is not near-symmetric");
                    }
                    const double value =
                        0.5 * (matrix[row][column] + matrix[column][row]);
                    matrix[row][column] = value;
                    matrix[column][row] = value;
                }
            }
            matrices.push_back(std::move(matrix));
        };
        append_matrix(previous_state, candidate_algebraic);
        append_matrix(candidate_state, candidate_algebraic);
        minimum_step = std::min(minimum_step, step);
        maximum_step = std::max(maximum_step, step);
    }
    DaeMultigridArtifact artifact;
    artifact.schema_version = "smave.dae-multigrid.v3";
    artifact.residual_family = "fully-implicit-first-order-step";
    artifact.model_source_hash = model.source_hash;
    artifact.unknown_count = unknown_count;
    artifact.minimum_step = minimum_step;
    artifact.maximum_step = maximum_step;
    artifact.hierarchy = build_learned_multigrid_artifact(
        model.source_hash,
        "fully-implicit-joint-backward-euler-" + std::to_string(unknown_count),
        {"step"}, {minimum_step}, {maximum_step}, matrices, true);
    artifact.seal();
    artifact.validate();
    return artifact;
}

}  // namespace smave
