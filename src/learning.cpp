#include "smave/learning.hpp"

#include "smave/device.hpp"

#include "smave/runtime.hpp"
#include "smave/linear.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

#if defined(SMAVE_HAVE_ACCELERATE_SPARSE)
#include <Accelerate/Accelerate.h>
#endif

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

std::string contract(const AffineWarmStartArtifact& artifact) {
    std::ostringstream output;
    output << std::setprecision(17) << artifact.schema_version << '|'
           << artifact.expert_version << '|' << artifact.model_source_hash << '|'
           << artifact.block_fingerprint << '|' << artifact.training_samples << '|'
           << artifact.training_rmse << '|';
    if (artifact.schema_version == "smave.affine-warm-start.v2") {
        output << artifact.training_dataset_id << '|'
               << artifact.training_dataset_version << '|'
               << artifact.training_dataset_manifest_hash << '|';
    }
    for (const auto& name : artifact.features) output << name << ';';
    output << '|';
    for (const auto& name : artifact.outputs) output << name << ';';
    output << '|';
    for (const double value : artifact.feature_minimum) output << value << ';';
    output << '|';
    for (const double value : artifact.feature_maximum) output << value << ';';
    output << '|';
    for (const auto& row : artifact.coefficients) {
        for (const double value : row) output << value << ';';
        output << '|';
    }
    return output.str();
}

std::string contract(const LinearPreconditionerArtifact& artifact) {
    std::ostringstream output;
    output << std::setprecision(17) << artifact.schema_version << '|'
           << artifact.expert_version << '|' << artifact.model_source_hash << '|'
           << artifact.block_fingerprint << '|' << artifact.training_samples << '|'
           << artifact.maximum_matrix_drift << '|';
    if (artifact.schema_version == "smave.linear-preconditioner.v2") {
        output << artifact.training_dataset_id << '|'
               << artifact.training_dataset_version << '|'
               << artifact.training_dataset_manifest_hash << '|';
    }
    for (const auto& name : artifact.features) output << name << ';';
    output << '|';
    for (const double value : artifact.feature_minimum) output << value << ';';
    output << '|';
    for (const double value : artifact.feature_maximum) output << value << ';';
    output << '|';
    for (const auto& row : artifact.inverse_operator) {
        for (const double value : row) output << value << ';';
        output << '|';
    }
    return output.str();
}

std::string contract(const LearnedMultigridArtifact& artifact) {
    std::ostringstream output;
    output << std::setprecision(17) << artifact.schema_version << '|'
           << artifact.expert_version << '|' << artifact.model_source_hash << '|'
           << artifact.block_fingerprint << '|' << artifact.training_samples << '|'
           << artifact.jacobian_mode << '|'
           << artifact.pre_smoothing_steps << '|' << artifact.post_smoothing_steps << '|'
           << artifact.smoothing_weight << '|' << artifact.maximum_matrix_drift << '|'
           << artifact.maximum_probe_contraction << '|';
    if (artifact.schema_version == "smave.learned-multigrid.v3") {
        output << artifact.training_dataset_id << '|'
               << artifact.training_dataset_version << '|'
               << artifact.training_dataset_manifest_hash << '|';
    }
    for (const auto& name : artifact.features) output << name << ';';
    output << '|';
    for (const double value : artifact.feature_minimum) output << value << ';';
    output << '|';
    for (const double value : artifact.feature_maximum) output << value << ';';
    output << '|';
    for (const auto& matrix : artifact.level_operators) {
        for (const auto& row : matrix) {
            for (const double value : row) output << value << ';';
            output << '|';
        }
        output << '#';
    }
    for (const auto& matrix : artifact.level_prolongations) {
        for (const auto& row : matrix) {
            for (const double value : row) output << value << ';';
            output << '|';
        }
        output << '#';
    }
    for (const auto& row : artifact.coarse_inverse) {
        for (const double value : row) output << value << ';';
        output << '|';
    }
    return output.str();
}

std::string legacy_multigrid_contract(const LearnedMultigridArtifact& artifact) {
    std::ostringstream output;
    output << std::setprecision(17) << "smave.learned-multigrid.v1" << '|'
           << artifact.expert_version << '|' << artifact.model_source_hash << '|'
           << artifact.block_fingerprint << '|' << artifact.training_samples << '|'
           << artifact.jacobian_mode << '|'
           << artifact.pre_smoothing_steps << '|' << artifact.post_smoothing_steps << '|'
           << artifact.smoothing_weight << '|' << artifact.maximum_matrix_drift << '|'
           << artifact.maximum_probe_contraction << '|';
    for (const auto& name : artifact.features) output << name << ';';
    output << '|';
    for (const double value : artifact.feature_minimum) output << value << ';';
    output << '|';
    for (const double value : artifact.feature_maximum) output << value << ';';
    output << '|';
    for (const auto* matrix : {&artifact.fine_operator, &artifact.prolongation,
                              &artifact.coarse_inverse}) {
        for (const auto& row : *matrix) {
            for (const double value : row) output << value << ';';
            output << '|';
        }
        output << '#';
    }
    return output.str();
}

template <typename Artifact>
void validate_training_lineage(
    const Artifact& artifact,
    std::string_view legacy_schema,
    std::string_view lineage_schema) {
    if (artifact.schema_version != legacy_schema && artifact.schema_version != lineage_schema) {
        throw std::invalid_argument("unsupported expert artifact schema");
    }
    const bool has_lineage = !artifact.training_dataset_id.empty() ||
        !artifact.training_dataset_version.empty() ||
        !artifact.training_dataset_manifest_hash.empty();
    if (artifact.schema_version == legacy_schema && has_lineage) {
        throw std::invalid_argument("legacy expert artifact cannot contain training lineage");
    }
    if (artifact.schema_version == lineage_schema &&
        (artifact.training_dataset_id.empty() || artifact.training_dataset_version.empty() ||
         artifact.training_dataset_manifest_hash.empty())) {
        throw std::invalid_argument("expert artifact requires complete training lineage");
    }
}

template <typename Artifact>
bool certificate_matches_training_lineage(
    const Artifact& artifact,
    const VerificationCertificate& certificate) {
    return certificate.training_dataset_id == artifact.training_dataset_id &&
        certificate.training_dataset_version == artifact.training_dataset_version &&
        certificate.training_dataset_manifest_hash == artifact.training_dataset_manifest_hash;
}

bool finite_matrix(const std::vector<std::vector<double>>& matrix) {
    return !matrix.empty() && std::all_of(
        matrix.begin(), matrix.end(), [](const auto& row) {
            return !row.empty() && std::all_of(
                row.begin(), row.end(), [](double value) { return std::isfinite(value); });
        });
}

std::vector<double> matrix_vector(
    const std::vector<std::vector<double>>& matrix,
    const std::vector<double>& vector) {
    std::vector<double> result(matrix.size());
    for (std::size_t row = 0; row < matrix.size(); ++row) {
        for (std::size_t column = 0; column < vector.size(); ++column) {
            result[row] += matrix[row][column] * vector[column];
        }
    }
    return result;
}

std::vector<double> multigrid_cycle(
    const LearnedMultigridArtifact& artifact,
    const std::vector<double>& right_hand_side) {
    const auto cycle = [&](std::size_t level, const std::vector<double>& right,
                           auto&& cycle_ref) -> std::vector<double> {
        const auto& matrix = artifact.level_operators[level];
        if (level + 1U == artifact.level_operators.size()) {
            return matrix_vector(artifact.coarse_inverse, right);
        }
        const auto& prolongation = artifact.level_prolongations[level];
        const std::size_t fine_size = matrix.size();
        const std::size_t coarse_size = prolongation.front().size();
        std::vector<double> solution(fine_size);
        const auto smooth = [&](std::size_t steps) {
            for (std::size_t step = 0; step < steps; ++step) {
                const auto action = matrix_vector(matrix, solution);
                for (std::size_t index = 0; index < fine_size; ++index) {
                    solution[index] += artifact.smoothing_weight *
                        (right[index] - action[index]) / matrix[index][index];
                }
            }
        };
        smooth(artifact.pre_smoothing_steps);
        const auto action = matrix_vector(matrix, solution);
        std::vector<double> coarse_residual(coarse_size);
        for (std::size_t coarse = 0; coarse < coarse_size; ++coarse) {
            for (std::size_t fine = 0; fine < fine_size; ++fine) {
                coarse_residual[coarse] += prolongation[fine][coarse] *
                    (right[fine] - action[fine]);
            }
        }
        const auto coarse_correction = cycle_ref(
            level + 1U, coarse_residual, cycle_ref);
        for (std::size_t fine = 0; fine < fine_size; ++fine) {
            for (std::size_t coarse = 0; coarse < coarse_size; ++coarse) {
                solution[fine] += prolongation[fine][coarse] *
                    coarse_correction[coarse];
            }
        }
        smooth(artifact.post_smoothing_steps);
        return solution;
    };
    return cycle(0, right_hand_side, cycle);
}

double basis_residual_contraction(
    const LearnedMultigridArtifact& artifact,
    const std::vector<std::vector<double>>& matrix) {
    double worst{};
    for (std::size_t basis = 0; basis < matrix.size(); ++basis) {
        std::vector<double> error(matrix.size());
        error[basis] = 1.0;
        const auto right = matrix_vector(matrix, error);
        const auto correction = multigrid_cycle(artifact, right);
        const auto action = matrix_vector(matrix, correction);
        double residual_norm{};
        double initial_norm{};
        for (std::size_t index = 0; index < matrix.size(); ++index) {
            residual_norm += std::pow(right[index] - action[index], 2.0);
            initial_norm += right[index] * right[index];
        }
        worst = std::max(worst, std::sqrt(residual_norm / initial_norm));
    }
    return worst;
}

bool solve_system(
    std::vector<std::vector<double>> matrix,
    std::vector<double> right,
    std::vector<double>& result) {
    const std::size_t size = right.size();
    result.assign(size, 0.0);
    for (std::size_t column = 0; column < size; ++column) {
        std::size_t pivot = column;
        for (std::size_t row = column + 1; row < size; ++row) {
            if (std::abs(matrix[row][column]) > std::abs(matrix[pivot][column])) pivot = row;
        }
        if (std::abs(matrix[pivot][column]) < 1.0e-14) return false;
        std::swap(matrix[pivot], matrix[column]);
        std::swap(right[pivot], right[column]);
        for (std::size_t row = column + 1; row < size; ++row) {
            const double factor = matrix[row][column] / matrix[column][column];
            for (std::size_t item = column; item < size; ++item) {
                matrix[row][item] -= factor * matrix[column][item];
            }
            right[row] -= factor * right[column];
        }
    }
    for (std::size_t reverse = 0; reverse < size; ++reverse) {
        const std::size_t row = size - reverse - 1;
        double value = right[row];
        for (std::size_t column = row + 1; column < size; ++column) {
            value -= matrix[row][column] * result[column];
        }
        result[row] = value / matrix[row][row];
    }
    return true;
}

const BlockIR& block_by_id(const ModelIR& model, const std::string& block_id) {
    const auto iterator = std::find_if(
        model.blocks.begin(), model.blocks.end(),
        [&](const BlockIR& block) { return block.id == block_id; });
    if (iterator == model.blocks.end()) throw std::invalid_argument("unknown block: " + block_id);
    return *iterator;
}

std::vector<std::filesystem::path> scenario_files(const std::filesystem::path& directory) {
    if (!std::filesystem::is_directory(directory)) {
        throw std::invalid_argument("training scenario path is not a directory");
    }
    std::vector<std::filesystem::path> paths;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".conf") {
            paths.push_back(entry.path());
        }
    }
    std::sort(paths.begin(), paths.end());
    if (paths.empty()) throw std::invalid_argument("training suite contains no .conf files");
    return paths;
}

bool symmetric_positive_definite(
    const std::vector<std::vector<double>>& matrix) {
    const std::size_t size = matrix.size();
    std::vector<std::vector<double>> lower(size, std::vector<double>(size));
    for (std::size_t row = 0; row < size; ++row) {
        for (std::size_t column = 0; column < size; ++column) {
            if (std::abs(matrix[row][column] - matrix[column][row]) >
                1.0e-8 * (1.0 + std::max(
                    std::abs(matrix[row][column]),
                    std::abs(matrix[column][row])))) return false;
        }
        for (std::size_t column = 0; column <= row; ++column) {
            double value = matrix[row][column];
            for (std::size_t inner = 0; inner < column; ++inner) {
                value -= lower[row][inner] * lower[column][inner];
            }
            if (row == column) {
                if (!(value > 1.0e-14) || !std::isfinite(value)) return false;
                lower[row][column] = std::sqrt(value);
            } else {
                lower[row][column] = value / lower[column][column];
            }
        }
    }
    return true;
}

std::vector<std::vector<double>> finite_difference_jacobian(
    const std::unordered_map<std::string, Expression>& residuals,
    const BlockIR& block,
    std::unordered_map<std::string, double> values) {
    std::vector<std::vector<double>> jacobian(
        block.equation_ids.size(), std::vector<double>(block.unknowns.size()));
    for (std::size_t column = 0; column < block.unknowns.size(); ++column) {
        const auto& unknown = block.unknowns[column];
        const double original = values.at(unknown);
        const double step = std::cbrt(std::numeric_limits<double>::epsilon()) *
            std::max(1.0, std::abs(original));
        values[unknown] = original + step;
        std::vector<double> plus;
        plus.reserve(block.equation_ids.size());
        for (const auto& equation : block.equation_ids) {
            plus.push_back(residuals.at(equation).evaluate(values));
        }
        values[unknown] = original - step;
        for (std::size_t row = 0; row < block.equation_ids.size(); ++row) {
            const double minus = residuals.at(block.equation_ids[row]).evaluate(values);
            jacobian[row][column] = (plus[row] - minus) / (2.0 * step);
        }
        values[unknown] = original;
    }
    return jacobian;
}

}  // namespace

void LinearPreconditionerArtifact::seal() {
    artifact_hash = digest(contract(*this));
    if (expert_version.empty()) {
        expert_version = "learned-linear-pc-" + artifact_hash;
    }
    artifact_hash = digest(contract(*this));
}

void LinearPreconditionerArtifact::validate() const {
    validate_training_lineage(
        *this, "smave.linear-preconditioner.v1", "smave.linear-preconditioner.v2");
    if (expert_version.empty() || model_source_hash.empty() ||
        block_fingerprint.empty() || inverse_operator.empty()) {
        throw std::invalid_argument("linear preconditioner identity is incomplete");
    }
    if (training_samples == 0 || feature_minimum.size() != features.size() ||
        feature_maximum.size() != features.size()) {
        throw std::invalid_argument("linear preconditioner training evidence is incomplete");
    }
    for (std::size_t index = 0; index < features.size(); ++index) {
        if (feature_minimum[index] > feature_maximum[index]) {
            throw std::invalid_argument("linear preconditioner domain is invalid");
        }
    }
    const std::size_t size = inverse_operator.size();
    for (const auto& row : inverse_operator) {
        if (row.size() != size || !std::all_of(row.begin(), row.end(), [](double value) {
                return std::isfinite(value);
            })) {
            throw std::invalid_argument("linear preconditioner operator shape is invalid");
        }
    }
    if (!symmetric_positive_definite(inverse_operator)) {
        throw std::invalid_argument("linear preconditioner operator is not SPD");
    }
    if (artifact_hash != digest(contract(*this))) {
        throw std::invalid_argument("linear preconditioner integrity check failed");
    }
}

void LinearPreconditionerArtifact::write(
    const std::filesystem::path& path) const {
    validate();
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write linear preconditioner artifact");
    output << std::setprecision(17)
           << "SMAVE_LINEAR_PC "
           << (schema_version == "smave.linear-preconditioner.v2" ? 2 : 1) << '\n'
           << "VERSION " << std::quoted(expert_version) << '\n'
           << "MODEL " << std::quoted(model_source_hash) << '\n'
           << "BLOCK " << std::quoted(block_fingerprint) << '\n';
    if (schema_version == "smave.linear-preconditioner.v2") {
        output << "TRAINING_DATASET_ID " << std::quoted(training_dataset_id) << '\n'
               << "TRAINING_DATASET_VERSION " << std::quoted(training_dataset_version) << '\n'
               << "TRAINING_DATASET_MANIFEST_HASH "
               << std::quoted(training_dataset_manifest_hash) << '\n';
    }
    output << "TRAINING " << training_samples << ' ' << maximum_matrix_drift << '\n'
           << "FEATURES " << features.size();
    for (const auto& name : features) output << ' ' << std::quoted(name);
    output << "\nDOMAIN";
    for (std::size_t index = 0; index < features.size(); ++index) {
        output << ' ' << feature_minimum[index] << ' ' << feature_maximum[index];
    }
    output << "\nINVERSE_OPERATOR " << inverse_operator.size() << ' '
           << inverse_operator.size() << '\n';
    for (const auto& row : inverse_operator) {
        for (const double value : row) output << value << ' ';
        output << '\n';
    }
    output << "\nHASH " << std::quoted(artifact_hash) << "\nEND\n";
}

LinearPreconditionerArtifact LinearPreconditionerArtifact::read(
    const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read linear preconditioner artifact");
    auto tag = [&](std::string_view expected) {
        std::string actual; input >> actual;
        if (!input || actual != expected) {
            throw std::runtime_error(
                "invalid linear preconditioner artifact: expected " +
                std::string(expected));
        }
    };
    LinearPreconditionerArtifact artifact;
    tag("SMAVE_LINEAR_PC"); int schema{}; input >> schema;
    if (schema != 1 && schema != 2) {
        throw std::runtime_error("unsupported linear preconditioner schema");
    }
    artifact.schema_version = schema == 2
        ? "smave.linear-preconditioner.v2"
        : "smave.linear-preconditioner.v1";
    tag("VERSION"); input >> std::quoted(artifact.expert_version);
    tag("MODEL"); input >> std::quoted(artifact.model_source_hash);
    tag("BLOCK"); input >> std::quoted(artifact.block_fingerprint);
    if (schema == 2) {
        tag("TRAINING_DATASET_ID"); input >> std::quoted(artifact.training_dataset_id);
        tag("TRAINING_DATASET_VERSION"); input >> std::quoted(artifact.training_dataset_version);
        tag("TRAINING_DATASET_MANIFEST_HASH");
        input >> std::quoted(artifact.training_dataset_manifest_hash);
    }
    tag("TRAINING"); input >> artifact.training_samples >> artifact.maximum_matrix_drift;
    tag("FEATURES"); std::size_t feature_count{}; input >> feature_count;
    artifact.features.resize(feature_count);
    for (auto& name : artifact.features) input >> std::quoted(name);
    tag("DOMAIN");
    artifact.feature_minimum.resize(feature_count);
    artifact.feature_maximum.resize(feature_count);
    for (std::size_t index = 0; index < feature_count; ++index) {
        input >> artifact.feature_minimum[index] >> artifact.feature_maximum[index];
    }
    tag("INVERSE_OPERATOR"); std::size_t rows{}, columns{}; input >> rows >> columns;
    artifact.inverse_operator.assign(rows, std::vector<double>(columns));
    for (auto& row : artifact.inverse_operator) for (auto& value : row) input >> value;
    tag("HASH"); input >> std::quoted(artifact.artifact_hash);
    tag("END");
    input >> std::ws;
    if (!input.eof()) throw std::runtime_error("trailing linear preconditioner content");
    artifact.validate();
    return artifact;
}

void LearnedMultigridArtifact::seal() {
    artifact_hash = digest(contract(*this));
    if (expert_version.empty()) expert_version = "learned-multigrid-" + artifact_hash;
    artifact_hash = digest(contract(*this));
}

void LearnedMultigridArtifact::validate() const {
    validate_training_lineage(
        *this, "smave.learned-multigrid.v2", "smave.learned-multigrid.v3");
    if (expert_version.empty() || model_source_hash.empty() || block_fingerprint.empty() ||
        training_samples == 0 || feature_minimum.size() != features.size() ||
        feature_maximum.size() != features.size()) {
        throw std::invalid_argument("learned multigrid identity or training evidence is incomplete");
    }
    for (std::size_t index = 0; index < features.size(); ++index) {
        if (feature_minimum[index] > feature_maximum[index]) {
            throw std::invalid_argument("learned multigrid domain is invalid");
        }
    }
    const std::size_t fine_size = fine_operator.size();
    if (fine_size < 4 || level_operators.size() < 2 ||
        level_prolongations.size() + 1U != level_operators.size() ||
        level_operators.front() != fine_operator || !finite_matrix(coarse_inverse)) {
        throw std::invalid_argument("learned multigrid operator contract is invalid");
    }
    for (std::size_t level = 0; level < level_operators.size(); ++level) {
        const auto& matrix = level_operators[level];
        const std::size_t size = matrix.size();
        if (!finite_matrix(matrix) || !symmetric_positive_definite(matrix)) {
            throw std::invalid_argument("learned multigrid level operator is not SPD");
        }
        for (const auto& row : matrix) if (row.size() != size) {
            throw std::invalid_argument("learned multigrid level operator shape is invalid");
        }
        if (level + 1U == level_operators.size()) continue;
        const auto& transfer = level_prolongations[level];
        const std::size_t coarse_size = level_operators[level + 1U].size();
        if (transfer.size() != size || !finite_matrix(transfer) ||
            transfer.front().size() != coarse_size || coarse_size >= size) {
            throw std::invalid_argument("learned multigrid transfer shape is invalid");
        }
        for (const auto& row : transfer) if (row.size() != coarse_size) {
            throw std::invalid_argument("learned multigrid transfer rows are inconsistent");
        }
        const auto& coarse_operator = level_operators[level + 1U];
        for (std::size_t left = 0; left < coarse_size; ++left) {
            for (std::size_t right = 0; right < coarse_size; ++right) {
                double projected{};
                for (std::size_t row = 0; row < size; ++row) {
                    for (std::size_t column = 0; column < size; ++column) {
                        projected += transfer[row][left] * matrix[row][column] *
                            transfer[column][right];
                    }
                }
                if (std::abs(projected - coarse_operator[left][right]) >
                    1.0e-10 * (1.0 + std::abs(coarse_operator[left][right]))) {
                    throw std::invalid_argument(
                        "learned multigrid Galerkin hierarchy is inconsistent");
                }
            }
        }
    }
    const std::size_t coarse_size = level_operators.back().size();
    if (coarse_inverse.size() != coarse_size ||
        !symmetric_positive_definite(coarse_inverse)) {
        throw std::invalid_argument("learned multigrid coarse inverse is invalid");
    }
    for (const auto& row : coarse_inverse) if (row.size() != coarse_size) {
        throw std::invalid_argument("learned multigrid coarse inverse shape is invalid");
    }
    const auto& coarsest = level_operators.back();
    for (std::size_t row = 0; row < coarse_size; ++row) {
        for (std::size_t column = 0; column < coarse_size; ++column) {
            double product{};
            for (std::size_t inner = 0; inner < coarse_size; ++inner) {
                product += coarsest[row][inner] * coarse_inverse[inner][column];
            }
            const double expected = row == column ? 1.0 : 0.0;
            if (std::abs(product - expected) > 1.0e-9) {
                throw std::invalid_argument(
                    "learned multigrid coarse inverse replay failed");
            }
        }
    }
    if (pre_smoothing_steps == 0 || post_smoothing_steps == 0 ||
        !(smoothing_weight > 0.0 && smoothing_weight < 2.0) ||
        !(maximum_probe_contraction >= 0.0 && maximum_probe_contraction < 1.0) ||
        !std::isfinite(maximum_matrix_drift)) {
        throw std::invalid_argument("learned multigrid stability evidence is invalid");
    }
    const double replayed_contraction = basis_residual_contraction(*this, fine_operator);
    if (!std::isfinite(replayed_contraction) || replayed_contraction >= 1.0 ||
        replayed_contraction > maximum_probe_contraction + 1.0e-12) {
        throw std::invalid_argument(
            "learned multigrid contraction evidence is inconsistent with the artifact");
    }
    std::vector<std::vector<double>> cycle_operator(
        fine_size, std::vector<double>(fine_size));
    for (std::size_t column = 0; column < fine_size; ++column) {
        std::vector<double> unit(fine_size);
        unit[column] = 1.0;
        const auto action = multigrid_cycle(*this, unit);
        for (std::size_t row = 0; row < fine_size; ++row) {
            cycle_operator[row][column] = action[row];
        }
    }
    if (!symmetric_positive_definite(cycle_operator)) {
        throw std::invalid_argument(
            "learned multigrid V-cycle is not an SPD PCG preconditioner");
    }
    if (artifact_hash != digest(contract(*this))) {
        throw std::invalid_argument("learned multigrid integrity check failed");
    }
}

void LearnedMultigridArtifact::write(const std::filesystem::path& path) const {
    validate();
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write learned multigrid artifact");
    const auto write_matrix = [&](std::string_view name, const auto& matrix) {
        output << name << ' ' << matrix.size() << ' ' << matrix.front().size() << '\n';
        for (const auto& row : matrix) {
            for (const double value : row) output << value << ' ';
            output << '\n';
        }
    };
    output << std::setprecision(17)
           << "SMAVE_LEARNED_MULTIGRID "
           << (schema_version == "smave.learned-multigrid.v3" ? 3 : 2) << '\n'
           << "VERSION " << std::quoted(expert_version) << '\n'
           << "MODEL " << std::quoted(model_source_hash) << '\n'
           << "BLOCK " << std::quoted(block_fingerprint) << '\n';
    if (schema_version == "smave.learned-multigrid.v3") {
        output << "TRAINING_DATASET_ID " << std::quoted(training_dataset_id) << '\n'
               << "TRAINING_DATASET_VERSION " << std::quoted(training_dataset_version) << '\n'
               << "TRAINING_DATASET_MANIFEST_HASH "
               << std::quoted(training_dataset_manifest_hash) << '\n';
    }
    output << "TRAINING " << training_samples << ' ' << maximum_matrix_drift << '\n'
           << "MODE " << jacobian_mode << '\n'
           << "SMOOTHER " << pre_smoothing_steps << ' ' << post_smoothing_steps << ' '
           << smoothing_weight << ' ' << maximum_probe_contraction << '\n'
           << "FEATURES " << features.size();
    for (const auto& name : features) output << ' ' << std::quoted(name);
    output << "\nDOMAIN";
    for (std::size_t index = 0; index < features.size(); ++index) {
        output << ' ' << feature_minimum[index] << ' ' << feature_maximum[index];
    }
    output << '\n';
    output << "LEVELS " << level_operators.size() << '\n';
    for (std::size_t level = 0; level < level_operators.size(); ++level) {
        write_matrix("LEVEL_OPERATOR", level_operators[level]);
        if (level < level_prolongations.size()) {
            write_matrix("LEVEL_PROLONGATION", level_prolongations[level]);
        }
    }
    write_matrix("COARSE_INVERSE", coarse_inverse);
    output << "HASH " << std::quoted(artifact_hash) << "\nEND\n";
}

LearnedMultigridArtifact LearnedMultigridArtifact::read(
    const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read learned multigrid artifact");
    const auto tag = [&](std::string_view expected) {
        std::string actual; input >> actual;
        if (!input || actual != expected) {
            throw std::runtime_error("invalid learned multigrid artifact: expected " +
                                     std::string(expected));
        }
    };
    const auto read_matrix = [&](std::string_view name) {
        tag(name); std::size_t rows{}, columns{}; input >> rows >> columns;
        std::vector<std::vector<double>> matrix(rows, std::vector<double>(columns));
        for (auto& row : matrix) for (double& value : row) input >> value;
        return matrix;
    };
    LearnedMultigridArtifact artifact;
    tag("SMAVE_LEARNED_MULTIGRID"); int schema{}; input >> schema;
    if (schema != 1 && schema != 2 && schema != 3) {
        throw std::runtime_error("unsupported learned multigrid schema");
    }
    tag("VERSION"); input >> std::quoted(artifact.expert_version);
    tag("MODEL"); input >> std::quoted(artifact.model_source_hash);
    tag("BLOCK"); input >> std::quoted(artifact.block_fingerprint);
    if (schema == 3) {
        artifact.schema_version = "smave.learned-multigrid.v3";
        tag("TRAINING_DATASET_ID"); input >> std::quoted(artifact.training_dataset_id);
        tag("TRAINING_DATASET_VERSION"); input >> std::quoted(artifact.training_dataset_version);
        tag("TRAINING_DATASET_MANIFEST_HASH");
        input >> std::quoted(artifact.training_dataset_manifest_hash);
    }
    tag("TRAINING"); input >> artifact.training_samples >> artifact.maximum_matrix_drift;
    tag("MODE"); input >> artifact.jacobian_mode;
    tag("SMOOTHER"); input >> artifact.pre_smoothing_steps >> artifact.post_smoothing_steps
                            >> artifact.smoothing_weight >> artifact.maximum_probe_contraction;
    tag("FEATURES"); std::size_t count{}; input >> count; artifact.features.resize(count);
    for (auto& name : artifact.features) input >> std::quoted(name);
    tag("DOMAIN"); artifact.feature_minimum.resize(count); artifact.feature_maximum.resize(count);
    for (std::size_t index = 0; index < count; ++index) {
        input >> artifact.feature_minimum[index] >> artifact.feature_maximum[index];
    }
    if (schema == 1) {
        artifact.fine_operator = read_matrix("FINE_OPERATOR");
        artifact.prolongation = read_matrix("PROLONGATION");
        artifact.coarse_inverse = read_matrix("COARSE_INVERSE");
        tag("HASH"); input >> std::quoted(artifact.artifact_hash); tag("END");
        if (artifact.artifact_hash != digest(legacy_multigrid_contract(artifact))) {
            throw std::runtime_error("legacy learned multigrid integrity check failed");
        }
        artifact.schema_version = "smave.learned-multigrid.v2";
        artifact.level_operators = {artifact.fine_operator};
        artifact.level_prolongations = {artifact.prolongation};
        std::vector<std::vector<double>> coarse_operator(
            artifact.coarse_inverse.size(),
            std::vector<double>(artifact.coarse_inverse.size()));
        for (std::size_t column = 0; column < coarse_operator.size(); ++column) {
            std::vector<double> unit(coarse_operator.size());
            unit[column] = 1.0;
            std::vector<double> inverse_column;
            if (!solve_system(artifact.coarse_inverse, unit, inverse_column)) {
                throw std::runtime_error("legacy multigrid coarse inverse is singular");
            }
            for (std::size_t row = 0; row < coarse_operator.size(); ++row) {
                coarse_operator[row][column] = inverse_column[row];
            }
        }
        artifact.level_operators.push_back(std::move(coarse_operator));
        artifact.seal();
    } else {
        tag("LEVELS"); std::size_t levels{}; input >> levels;
        artifact.level_operators.reserve(levels);
        artifact.level_prolongations.reserve(levels - 1U);
        for (std::size_t level = 0; level < levels; ++level) {
            artifact.level_operators.push_back(read_matrix("LEVEL_OPERATOR"));
            if (level + 1U < levels) {
                artifact.level_prolongations.push_back(
                    read_matrix("LEVEL_PROLONGATION"));
            }
        }
        artifact.fine_operator = artifact.level_operators.front();
        artifact.prolongation = artifact.level_prolongations.front();
        artifact.coarse_inverse = read_matrix("COARSE_INVERSE");
        tag("HASH"); input >> std::quoted(artifact.artifact_hash); tag("END");
    }
    input >> std::ws;
    if (!input.eof()) throw std::runtime_error("trailing learned multigrid content");
    artifact.validate();
    return artifact;
}

LearnedLinearPreconditionerExpert::LearnedLinearPreconditionerExpert(
    LinearPreconditionerArtifact artifact,
    std::optional<VerificationCertificate> certificate)
    : artifact_(std::move(artifact)), certificate_(std::move(certificate)) {
    artifact_.validate();
    const auto width = artifact_.inverse_operator.size();
    contiguous_inverse_operator_.reserve(width * width);
    for (const auto& row : artifact_.inverse_operator) {
        contiguous_inverse_operator_.insert(
            contiguous_inverse_operator_.end(), row.begin(), row.end());
    }
    if (certificate_) {
        certificate_->validate();
        if (certificate_->expert_version != artifact_.expert_version ||
            certificate_->artifact_hash != artifact_.artifact_hash ||
            certificate_->block_fingerprint != artifact_.block_fingerprint ||
            !certificate_matches_training_lineage(artifact_, *certificate_)) {
            throw std::invalid_argument("verification certificate does not match preconditioner");
        }
    }
}

std::string LearnedLinearPreconditionerExpert::version() const {
    return artifact_.expert_version;
}

Capability LearnedLinearPreconditionerExpert::match(const BlockIR& block) const {
    const bool compatible = block.linear &&
        block.fingerprint == artifact_.block_fingerprint &&
        block.unknowns.size() == artifact_.inverse_operator.size();
    return Capability{
        .linear = compatible,
        .nonlinear = false,
        .event_related = false,
        .preconditioner = true,
        .backend_roles = {BackendRole::preconditioner},
        .devices = {"cpu", "metal-gpu", "coreml-neural-engine"},
        .evidence_level = EvidenceLevel::e2,
        .maximum_permission = Permission::corrected,
    };
}

double LearnedLinearPreconditionerExpert::ood_score(
    const BlockContext& context) const {
    if (certificate_ && !certificate_->contains(context.values)) {
        return std::numeric_limits<double>::infinity();
    }
    double score = 0.0;
    for (std::size_t index = 0; index < artifact_.features.size(); ++index) {
        const auto value = context.values.find(artifact_.features[index]);
        if (value == context.values.end()) return std::numeric_limits<double>::infinity();
        const double width = std::max(
            artifact_.feature_maximum[index] - artifact_.feature_minimum[index], 1.0e-12);
        if (value->second < artifact_.feature_minimum[index]) {
            score = std::max(
                score,
                (artifact_.feature_minimum[index] - value->second) / width);
        } else if (value->second > artifact_.feature_maximum[index]) {
            score = std::max(
                score,
                (value->second - artifact_.feature_maximum[index]) / width);
        }
    }
    return score;
}

Estimate LearnedLinearPreconditionerExpert::estimate(
    const BlockIR&, const BlockContext& context) const {
    const double ood = ood_score(context);
    const std::size_t dimension = artifact_.inverse_operator.size();
    const double flops = static_cast<double>(dimension * dimension);
    return Estimate{
        .pass_probability = ood == 0.0 ? 0.98 : 0.0,
        .expected_setup_time_us = 0.2,
        .expected_solve_time_us = 0.5,
        .expected_correction_time_us = 0.002 * flops,
        .failure_cost_us = 50.0,
        .risk_score = 0.0002 + ood,
        .ood_score = ood,
    };
}

ExpertResult LearnedLinearPreconditionerExpert::solve(
    const BlockIR&, const BlockContext&, const SolveBudget&) const {
    ExpertResult result;
    result.status = "preconditioner";
    result.uncertainty = artifact_.maximum_matrix_drift;
    return result;
}

bool LearnedLinearPreconditionerExpert::apply_preconditioner(
    const BlockIR& block,
    const BlockContext& context,
    const std::vector<double>& residual,
    std::vector<double>& result) const {
    if (!match(block).linear || ood_score(context) != 0.0 ||
        residual.size() != artifact_.inverse_operator.size()) return false;
    const std::size_t size = residual.size();
    result.resize(size);
#if defined(SMAVE_HAVE_ACCELERATE_SPARSE)
    if (size <= static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        cblas_dgemv(
            CblasRowMajor, CblasNoTrans,
            static_cast<int>(size), static_cast<int>(size), 1.0,
            contiguous_inverse_operator_.data(), static_cast<int>(size),
            residual.data(), 1, 0.0, result.data(), 1);
    } else
#endif
    {
        const double* op = contiguous_inverse_operator_.data();
        for (std::size_t index = 0; index < size; ++index) {
            double sum = 0.0;
            const double* row = op + index * size;
            for (std::size_t column = 0; column < size; ++column) {
                sum += row[column] * residual[column];
            }
            result[index] = sum;
        }
    }
    return std::all_of(result.begin(), result.end(), [](double value) {
        return std::isfinite(value);
    });
}

bool LearnedLinearPreconditionerExpert::apply_preconditioner_batch(
    const BlockIR& block,
    const std::vector<BlockContext>& contexts,
    const std::vector<std::vector<double>>& residuals,
    std::vector<std::vector<double>>& results) const {
    const std::size_t batch = residuals.size();
    const std::size_t size = artifact_.inverse_operator.size();
    if (contexts.size() != batch || !match(block).linear) return false;
    for (std::size_t item = 0; item < batch; ++item) {
        if (ood_score(contexts[item]) != 0.0 || residuals[item].size() != size) return false;
    }
    std::vector<double> packed_residuals(batch * size);
    for (std::size_t item = 0; item < batch; ++item) {
        std::copy(
            residuals[item].begin(), residuals[item].end(),
            packed_residuals.begin() + item * size);
    }
    std::vector<double> packed_results(batch * size);
#if defined(SMAVE_HAVE_ACCELERATE_SPARSE)
    if (batch <= static_cast<std::size_t>(std::numeric_limits<int>::max()) &&
        size <= static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    cblas_dgemm(
        CblasRowMajor, CblasNoTrans, CblasTrans,
        static_cast<int>(batch), static_cast<int>(size),
        static_cast<int>(size), 1.0,
        packed_residuals.data(), static_cast<int>(size),
        contiguous_inverse_operator_.data(), static_cast<int>(size),
        0.0, packed_results.data(), static_cast<int>(size));
    } else
#endif
    {
    for (std::size_t row = 0; row < size; ++row) {
        for (std::size_t column = 0; column < size; ++column) {
            const double weight = artifact_.inverse_operator[row][column];
            for (std::size_t item = 0; item < batch; ++item) {
                packed_results[item * size + row] +=
                    weight * residuals[item][column];
            }
        }
    }
    }
    results.assign(batch, std::vector<double>(size));
    for (std::size_t item = 0; item < batch; ++item) {
        std::copy(
            packed_results.begin() + item * size,
            packed_results.begin() + (item + 1) * size,
            results[item].begin());
    }
    for (const auto& result : results) {
        if (!std::all_of(result.begin(), result.end(), [](double value) {
                return std::isfinite(value);
            })) return false;
    }
    return true;
}

bool LearnedLinearPreconditionerExpert::apply_preconditioner_batch_on_device(
    const std::string& device,
    const BlockIR& block,
    const std::vector<BlockContext>& contexts,
    const std::vector<std::vector<double>>& residuals,
    std::vector<std::vector<double>>& results,
    DeviceExecutionResult* execution) const {
    if (device == "cpu") {
        return Expert::apply_preconditioner_batch_on_device(
            device, block, contexts, residuals, results, execution);
    }
    const std::size_t batch = residuals.size();
    const std::size_t size = artifact_.inverse_operator.size();
    const bool metal = device == "metal-gpu";
    const bool neural_engine = device == "coreml-neural-engine";
    if ((!metal && !neural_engine) || contexts.size() != batch ||
        !match(block).linear || batch == 0) {
        if (execution != nullptr) {
            *execution = DeviceExecutionResult{};
            execution->backend = neural_engine
                ? "coreml-affine-neural-engine-tensor-batch-v3"
                : "metal-affine-batch-gpu-v1";
            execution->reason = "learned preconditioner device or batch contract mismatch";
        }
        return false;
    }
    std::vector<float> inputs;
    inputs.reserve(batch * size);
    for (std::size_t item = 0; item < batch; ++item) {
        if (ood_score(contexts[item]) != 0.0 || residuals[item].size() != size) {
            if (execution != nullptr) {
                *execution = DeviceExecutionResult{};
                execution->backend = neural_engine
                    ? "coreml-affine-neural-engine-tensor-batch-v3"
                    : "metal-affine-batch-gpu-v1";
                execution->reason =
                    "learned preconditioner device batch is OOD or malformed";
            }
            return false;
        }
        for (const auto value : residuals[item]) inputs.push_back(static_cast<float>(value));
    }
    std::vector<float> weights;
    weights.reserve(size * size);
    for (const auto& row : artifact_.inverse_operator) {
        for (const auto value : row) weights.push_back(static_cast<float>(value));
    }
    const std::vector<float> bias(size);
    DeviceExecutionResult device_result;
    if (metal) {
        device_result = metal_gpu_affine_batch(
            inputs, batch, size, weights, size, bias, 2.0e-5, 2.0e-5);
    } else {
        const auto working_directory = std::filesystem::temp_directory_path() /
            ("smave-coreml-preconditioner-" + artifact_.artifact_hash);
        device_result = coreml_neural_engine_affine_tensor_batch(
            inputs, batch, size, weights, size, bias,
            working_directory, 2.0e-5, 2.0e-5);
    }
    if (execution != nullptr) *execution = device_result;
    if (!device_result.executed || !device_result.verified ||
        device_result.output.size() != batch * size) return false;
    results.assign(batch, std::vector<double>(size));
    for (std::size_t item = 0; item < batch; ++item) {
        for (std::size_t index = 0; index < size; ++index) {
            results[item][index] = device_result.output[item * size + index];
        }
    }
    return true;
}

bool LearnedLinearPreconditionerExpert::device_batch_is_resident(
    const std::string& device,
    std::size_t batch,
    std::size_t width) const {
    if (device != "coreml-neural-engine" || batch == 0 ||
        width != artifact_.inverse_operator.size()) return false;
    std::vector<float> weights;
    weights.reserve(width * width);
    for (const auto& row : artifact_.inverse_operator) {
        if (row.size() != width) return false;
        for (const auto value : row) weights.push_back(static_cast<float>(value));
    }
    const std::vector<float> bias(width);
    const auto working_directory = std::filesystem::temp_directory_path() /
        ("smave-coreml-preconditioner-" + artifact_.artifact_hash);
    return coreml_neural_engine_affine_tensor_batch_is_resident(
        batch, width, weights, width, bias, working_directory);
}

LearnedMultigridExpert::LearnedMultigridExpert(
    LearnedMultigridArtifact artifact,
    std::optional<VerificationCertificate> certificate)
    : artifact_(std::move(artifact)), certificate_(std::move(certificate)) {
    artifact_.validate();
    if (certificate_) {
        certificate_->validate();
        if (certificate_->expert_version != artifact_.expert_version ||
            certificate_->artifact_hash != artifact_.artifact_hash ||
            certificate_->block_fingerprint != artifact_.block_fingerprint ||
            !certificate_matches_training_lineage(artifact_, *certificate_)) {
            throw std::invalid_argument("verification certificate does not match multigrid");
        }
    }
}

std::string LearnedMultigridExpert::version() const { return artifact_.expert_version; }

Capability LearnedMultigridExpert::match(const BlockIR& block) const {
    const bool compatible = !block.event_related &&
        (artifact_.jacobian_mode ? (!block.linear && block.smooth) : block.linear) &&
        block.fingerprint == artifact_.block_fingerprint &&
        block.unknowns.size() == artifact_.fine_operator.size();
    return Capability{
        .linear = compatible && !artifact_.jacobian_mode,
        .nonlinear = compatible && artifact_.jacobian_mode,
        .event_related = false,
        .preconditioner = true,
        .backend_roles = {BackendRole::preconditioner},
        .devices = {"cpu"},
        .evidence_level = EvidenceLevel::e2,
        .maximum_permission = Permission::corrected,
    };
}

double LearnedMultigridExpert::ood_score(const BlockContext& context) const {
    if (certificate_ && !certificate_->contains(context.values)) {
        return std::numeric_limits<double>::infinity();
    }
    double score{};
    for (std::size_t index = 0; index < artifact_.features.size(); ++index) {
        const auto value = context.values.find(artifact_.features[index]);
        if (value == context.values.end()) return std::numeric_limits<double>::infinity();
        const double width = std::max(
            artifact_.feature_maximum[index] - artifact_.feature_minimum[index], 1.0e-12);
        if (value->second < artifact_.feature_minimum[index]) {
            score = std::max(score,
                (artifact_.feature_minimum[index] - value->second) / width);
        } else if (value->second > artifact_.feature_maximum[index]) {
            score = std::max(score,
                (value->second - artifact_.feature_maximum[index]) / width);
        }
    }
    return score;
}

Estimate LearnedMultigridExpert::estimate(
    const BlockIR&, const BlockContext& context) const {
    const double ood = ood_score(context);
    const std::size_t dimension = artifact_.fine_operator.size();
    const double flops = static_cast<double>(dimension * dimension);
    return Estimate{
        .pass_probability = ood == 0.0 ? 0.96 : 0.0,
        .expected_setup_time_us = 0.5,
        .expected_solve_time_us = 0.002 * flops,
        .expected_correction_time_us = 0.002 * flops,
        .failure_cost_us = 50.0,
        .risk_score = artifact_.maximum_probe_contraction * 1.0e-3 + ood,
        .ood_score = ood,
    };
}

ExpertResult LearnedMultigridExpert::solve(
    const BlockIR&, const BlockContext&, const SolveBudget&) const {
    ExpertResult result;
    result.status = "preconditioner";
    result.uncertainty = std::max(
        artifact_.maximum_matrix_drift, artifact_.maximum_probe_contraction);
    result.telemetry["probe_contraction"] = artifact_.maximum_probe_contraction;
    return result;
}

bool LearnedMultigridExpert::apply_preconditioner(
    const BlockIR& block,
    const BlockContext& context,
    const std::vector<double>& residual,
    std::vector<double>& result) const {
    const auto capability = match(block);
    if ((!capability.linear && !capability.nonlinear) || ood_score(context) != 0.0 ||
        residual.size() != artifact_.fine_operator.size()) return false;
    return apply_learned_multigrid(artifact_, residual, result);
}

void AffineWarmStartArtifact::seal() {
    artifact_hash = digest(contract(*this));
    if (expert_version.empty()) expert_version = "affine-warm-start-" + artifact_hash;
    artifact_hash = digest(contract(*this));
}

void AffineWarmStartArtifact::validate() const {
    validate_training_lineage(
        *this, "smave.affine-warm-start.v1", "smave.affine-warm-start.v2");
    if (expert_version.empty() || model_source_hash.empty() || block_fingerprint.empty()) {
        throw std::invalid_argument("affine artifact identity is incomplete");
    }
    if (features.empty() || outputs.empty() || training_samples < features.size() + 1) {
        throw std::invalid_argument("affine artifact has insufficient training coverage");
    }
    if (feature_minimum.size() != features.size() ||
        feature_maximum.size() != features.size() ||
        coefficients.size() != outputs.size()) {
        throw std::invalid_argument("affine artifact shape mismatch");
    }
    for (std::size_t index = 0; index < features.size(); ++index) {
        if (feature_minimum[index] > feature_maximum[index]) {
            throw std::invalid_argument("affine artifact domain is invalid");
        }
    }
    for (const auto& row : coefficients) {
        if (row.size() != features.size() + 1) {
            throw std::invalid_argument("affine artifact coefficient shape mismatch");
        }
    }
    if (artifact_hash != digest(contract(*this))) {
        throw std::invalid_argument("affine artifact integrity check failed");
    }
}

void AffineWarmStartArtifact::write(const std::filesystem::path& path) const {
    validate();
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write affine artifact");
    output << std::setprecision(17)
           << "SMAVE_AFFINE "
           << (schema_version == "smave.affine-warm-start.v2" ? 2 : 1) << '\n'
           << "VERSION " << std::quoted(expert_version) << '\n'
           << "MODEL " << std::quoted(model_source_hash) << '\n'
           << "BLOCK " << std::quoted(block_fingerprint) << '\n';
    if (schema_version == "smave.affine-warm-start.v2") {
        output << "TRAINING_DATASET_ID " << std::quoted(training_dataset_id) << '\n'
               << "TRAINING_DATASET_VERSION " << std::quoted(training_dataset_version) << '\n'
               << "TRAINING_DATASET_MANIFEST_HASH "
               << std::quoted(training_dataset_manifest_hash) << '\n';
    }
    output << "TRAINING " << training_samples << ' ' << training_rmse << '\n'
           << "FEATURES " << features.size();
    for (const auto& name : features) output << ' ' << std::quoted(name);
    output << "\nOUTPUTS " << outputs.size();
    for (const auto& name : outputs) output << ' ' << std::quoted(name);
    output << "\nDOMAIN";
    for (std::size_t index = 0; index < features.size(); ++index) {
        output << ' ' << feature_minimum[index] << ' ' << feature_maximum[index];
    }
    output << "\nCOEFFICIENTS " << coefficients.size() << ' '
           << (features.size() + 1) << '\n';
    for (const auto& row : coefficients) {
        for (const double value : row) output << value << ' ';
        output << '\n';
    }
    output << "HASH " << std::quoted(artifact_hash) << "\nEND\n";
}

AffineWarmStartArtifact AffineWarmStartArtifact::read(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read affine artifact");
    auto tag = [&](std::string_view expected) {
        std::string actual; input >> actual;
        if (!input || actual != expected) {
            throw std::runtime_error("invalid affine artifact: expected " + std::string(expected));
        }
    };
    AffineWarmStartArtifact artifact;
    tag("SMAVE_AFFINE"); int schema{}; input >> schema;
    if (schema != 1 && schema != 2) {
        throw std::runtime_error("unsupported affine artifact schema");
    }
    artifact.schema_version = schema == 2
        ? "smave.affine-warm-start.v2"
        : "smave.affine-warm-start.v1";
    tag("VERSION"); input >> std::quoted(artifact.expert_version);
    tag("MODEL"); input >> std::quoted(artifact.model_source_hash);
    tag("BLOCK"); input >> std::quoted(artifact.block_fingerprint);
    if (schema == 2) {
        tag("TRAINING_DATASET_ID"); input >> std::quoted(artifact.training_dataset_id);
        tag("TRAINING_DATASET_VERSION"); input >> std::quoted(artifact.training_dataset_version);
        tag("TRAINING_DATASET_MANIFEST_HASH");
        input >> std::quoted(artifact.training_dataset_manifest_hash);
    }
    tag("TRAINING"); input >> artifact.training_samples >> artifact.training_rmse;
    tag("FEATURES"); std::size_t feature_count{}; input >> feature_count;
    artifact.features.resize(feature_count);
    for (auto& name : artifact.features) input >> std::quoted(name);
    tag("OUTPUTS"); std::size_t output_count{}; input >> output_count;
    artifact.outputs.resize(output_count);
    for (auto& name : artifact.outputs) input >> std::quoted(name);
    tag("DOMAIN");
    artifact.feature_minimum.resize(feature_count);
    artifact.feature_maximum.resize(feature_count);
    for (std::size_t index = 0; index < feature_count; ++index) {
        input >> artifact.feature_minimum[index] >> artifact.feature_maximum[index];
    }
    tag("COEFFICIENTS"); std::size_t rows{}, columns{}; input >> rows >> columns;
    artifact.coefficients.assign(rows, std::vector<double>(columns));
    for (auto& row : artifact.coefficients) for (auto& value : row) input >> value;
    tag("HASH"); input >> std::quoted(artifact.artifact_hash);
    tag("END");
    input >> std::ws;
    if (!input.eof()) throw std::runtime_error("trailing affine artifact content");
    artifact.validate();
    return artifact;
}

AffineWarmStartExpert::AffineWarmStartExpert(
    AffineWarmStartArtifact artifact,
    std::optional<VerificationCertificate> certificate)
    : artifact_(std::move(artifact)), certificate_(std::move(certificate)) {
    artifact_.validate();
    if (certificate_) {
        certificate_->validate();
        if (certificate_->expert_version != artifact_.expert_version ||
            certificate_->artifact_hash != artifact_.artifact_hash ||
            certificate_->block_fingerprint != artifact_.block_fingerprint ||
            !certificate_matches_training_lineage(artifact_, *certificate_)) {
            throw std::invalid_argument("verification certificate does not match affine expert");
        }
    }
}

std::string AffineWarmStartExpert::version() const { return artifact_.expert_version; }

Capability AffineWarmStartExpert::match(const BlockIR& block) const {
    const bool compatible = block.fingerprint == artifact_.block_fingerprint && !block.linear;
    return Capability{
        .linear = false,
        .nonlinear = compatible,
        .event_related = false,
        .preconditioner = false,
        .backend_roles = {BackendRole::initializer},
        .devices = {"cpu"},
        .evidence_level = EvidenceLevel::e2,
        .maximum_permission = Permission::warm_start,
    };
}

Estimate AffineWarmStartExpert::estimate(
    const BlockIR&, const BlockContext& context) const {
    if (certificate_ && !certificate_->contains(context.values)) return Estimate{};
    double ood = 0.0;
    for (std::size_t index = 0; index < artifact_.features.size(); ++index) {
        const auto value = context.values.find(artifact_.features[index]);
        if (value == context.values.end()) return Estimate{};
        const double width = std::max(
            artifact_.feature_maximum[index] - artifact_.feature_minimum[index], 1.0e-12);
        if (value->second < artifact_.feature_minimum[index]) {
            ood = std::max(ood, (artifact_.feature_minimum[index] - value->second) / width);
        } else if (value->second > artifact_.feature_maximum[index]) {
            ood = std::max(ood, (value->second - artifact_.feature_maximum[index]) / width);
        }
    }
    return Estimate{
        .pass_probability = ood == 0.0 ? 0.95 : 0.0,
        .expected_setup_time_us = 0.2,
        .expected_solve_time_us = 1.0,
        .expected_correction_time_us = 6.0,
        .failure_cost_us = 50.0,
        .risk_score = 0.01 + ood,
        .ood_score = ood,
    };
}

ExpertResult AffineWarmStartExpert::solve(
    const BlockIR&, const BlockContext& context, const SolveBudget&) const {
    ExpertResult result;
    const auto estimate_value = estimate(BlockIR{}, context);
    if (estimate_value.ood_score > 0.0 || estimate_value.pass_probability == 0.0) {
        result.status = "ood";
        return result;
    }
    for (std::size_t output = 0; output < artifact_.outputs.size(); ++output) {
        double value = artifact_.coefficients[output][0];
        for (std::size_t feature = 0; feature < artifact_.features.size(); ++feature) {
            value += artifact_.coefficients[output][feature + 1] *
                context.values.at(artifact_.features[feature]);
        }
        result.candidate[artifact_.outputs[output]] = value;
    }
    result.status = "candidate";
    result.uncertainty = artifact_.training_rmse;
    result.telemetry["training_rmse"] = artifact_.training_rmse;
    return result;
}

AffineWarmStartArtifact train_affine_warm_start(
    const ModelIR& model,
    const std::string& block_id,
    const std::filesystem::path& scenario_directory,
    const std::filesystem::path& trace_directory) {
    const auto& block = block_by_id(model, block_id);
    if (block.linear || block.event_related) {
        throw std::invalid_argument("affine warm-start training requires a nonlinear non-event block");
    }
    AffineWarmStartArtifact artifact;
    artifact.model_source_hash = model.source_hash;
    artifact.block_fingerprint = block.fingerprint;
    artifact.outputs = block.unknowns;
    for (const auto& context : block.contexts) {
        const auto variable = std::find_if(
            model.variables.begin(), model.variables.end(),
            [&](const VariableIR& item) { return item.name == context; });
        if (variable != model.variables.end() && variable->kind == "parameter") {
            artifact.features.push_back(context);
        }
    }
    if (artifact.features.empty()) {
        throw std::invalid_argument("block has no parameter features for affine training");
    }
    Registry empty_registry;
    RuntimeBundle fallback_bundle;
    fallback_bundle.bundle_id = "training-labels-" + model.source_hash;
    fallback_bundle.model_source_hash = model.source_hash;
    fallback_bundle.expert_versions.clear();
    fallback_bundle.seal();
    const Runtime label_runtime(model, std::move(empty_registry), fallback_bundle);
    std::vector<std::vector<double>> design;
    std::vector<std::vector<double>> labels(artifact.outputs.size());
    artifact.feature_minimum.assign(artifact.features.size(), std::numeric_limits<double>::infinity());
    artifact.feature_maximum.assign(artifact.features.size(), -std::numeric_limits<double>::infinity());
    for (const auto& path : scenario_files(scenario_directory)) {
        auto scenario = read_scenario(path);
        for (auto iterator = scenario.begin(); iterator != scenario.end();) {
            if (iterator->first.ends_with("_previous")) iterator = scenario.erase(iterator);
            else ++iterator;
        }
        const auto outcome = label_runtime.solve(scenario, trace_directory / path.stem());
        if (!outcome.success) continue;
        std::vector<double> row{1.0};
        bool complete = true;
        for (std::size_t index = 0; index < artifact.features.size(); ++index) {
            const auto value = scenario.find(artifact.features[index]);
            if (value == scenario.end()) { complete = false; break; }
            row.push_back(value->second);
            artifact.feature_minimum[index] = std::min(artifact.feature_minimum[index], value->second);
            artifact.feature_maximum[index] = std::max(artifact.feature_maximum[index], value->second);
        }
        if (!complete) continue;
        design.push_back(std::move(row));
        for (std::size_t output = 0; output < artifact.outputs.size(); ++output) {
            labels[output].push_back(outcome.values.at(artifact.outputs[output]));
        }
    }
    artifact.training_samples = design.size();
    const std::size_t width = artifact.features.size() + 1;
    if (design.size() < width) throw std::invalid_argument("insufficient converged training scenarios");
    std::vector<std::vector<double>> gram(width, std::vector<double>(width));
    for (const auto& row : design) {
        for (std::size_t left = 0; left < width; ++left) {
            for (std::size_t right = 0; right < width; ++right) {
                gram[left][right] += row[left] * row[right];
            }
        }
    }
    for (std::size_t index = 1; index < width; ++index) gram[index][index] += 1.0e-10;
    double squared_error = 0.0;
    for (std::size_t output = 0; output < artifact.outputs.size(); ++output) {
        std::vector<double> right(width);
        for (std::size_t sample = 0; sample < design.size(); ++sample) {
            for (std::size_t feature = 0; feature < width; ++feature) {
                right[feature] += design[sample][feature] * labels[output][sample];
            }
        }
        std::vector<double> coefficients;
        if (!solve_system(gram, right, coefficients)) {
            throw std::runtime_error("affine training normal equation is singular");
        }
        artifact.coefficients.push_back(coefficients);
        for (std::size_t sample = 0; sample < design.size(); ++sample) {
            double prediction = 0.0;
            for (std::size_t feature = 0; feature < width; ++feature) {
                prediction += coefficients[feature] * design[sample][feature];
            }
            squared_error += std::pow(prediction - labels[output][sample], 2);
        }
    }
    artifact.training_rmse = std::sqrt(
        squared_error / static_cast<double>(design.size() * artifact.outputs.size()));
    artifact.seal();
    artifact.validate();
    return artifact;
}

void register_affine_expert(
    Registry& registry,
    const AffineWarmStartArtifact& artifact,
    const std::string& domain_version,
    const std::string& tolerance_profile,
    const std::string& hardware_profile,
    std::optional<VerificationCertificate> certificate) {
    const std::string evidence = certificate
        ? certificate->certificate_hash
        : "original-solver-labels-" + std::to_string(artifact.training_samples);
    auto expert = std::make_shared<AffineWarmStartExpert>(
        artifact, std::move(certificate));
    registry.register_expert(
        expert,
        ExpertGrant{
            .expert_version = expert->version(),
            .block_family = artifact.block_fingerprint,
            .domain_version = domain_version,
            .tolerance_profile = tolerance_profile,
            .hardware_profile = hardware_profile,
            .permission = Permission::warm_start,
            .evidence_level = EvidenceLevel::e2,
            .evidence_bundle = evidence,
            .artifact_hash = artifact.artifact_hash,
            .expires_unix_seconds = 0,
        });
}

LinearPreconditionerArtifact train_linear_preconditioner(
    const ModelIR& model,
    const std::string& block_id,
    const std::filesystem::path& scenario_directory) {
    const auto& block = block_by_id(model, block_id);
    if (!block.linear || block.event_related || block.unknowns.size() < 2) {
        throw std::invalid_argument(
            "linear preconditioner training requires a multi-variable linear non-event block");
    }
    std::unordered_map<std::string, Expression> residuals;
    for (const auto& equation : model.equations) {
        residuals.emplace(equation.id, Expression(equation.residual));
    }
    LinearPreconditionerArtifact artifact;
    artifact.model_source_hash = model.source_hash;
    artifact.block_fingerprint = block.fingerprint;
    for (const auto& context : block.contexts) {
        const auto variable = std::find_if(
            model.variables.begin(), model.variables.end(),
            [&](const VariableIR& item) { return item.name == context; });
        if (variable != model.variables.end() && variable->kind == "parameter") {
            artifact.features.push_back(context);
        }
    }
    artifact.feature_minimum.assign(
        artifact.features.size(), std::numeric_limits<double>::infinity());
    artifact.feature_maximum.assign(
        artifact.features.size(), -std::numeric_limits<double>::infinity());
    std::vector<std::vector<std::vector<double>>> matrices;
    for (const auto& path : scenario_files(scenario_directory)) {
        const auto scenario = read_scenario(path);
        bool complete = true;
        for (std::size_t index = 0; index < artifact.features.size(); ++index) {
            const auto value = scenario.find(artifact.features[index]);
            if (value == scenario.end()) { complete = false; break; }
            artifact.feature_minimum[index] = std::min(
                artifact.feature_minimum[index], value->second);
            artifact.feature_maximum[index] = std::max(
                artifact.feature_maximum[index], value->second);
        }
        if (!complete) continue;
        const auto system = assemble_linear_system(
            model, block, residuals, scenario);
        if (!system.positive_definite) {
            throw std::invalid_argument(
                "training encountered a non-SPD matrix; PCG permission denied");
        }
        for (std::size_t index = 0; index < system.matrix.size(); ++index) {
            if (!(system.matrix[index][index] > 1.0e-14) ||
                !std::isfinite(system.matrix[index][index])) {
                throw std::invalid_argument("training encountered an invalid matrix diagonal");
            }
        }
        matrices.push_back(system.matrix);
    }
    artifact.training_samples = matrices.size();
    if (matrices.empty()) {
        throw std::invalid_argument("no valid preconditioner training scenarios");
    }
    const std::size_t size = block.unknowns.size();
    std::vector<std::vector<double>> mean_matrix(size, std::vector<double>(size));
    for (const auto& matrix : matrices) {
        for (std::size_t row = 0; row < size; ++row) {
            for (std::size_t column = 0; column < size; ++column) {
                mean_matrix[row][column] += matrix[row][column];
            }
        }
    }
    for (auto& row : mean_matrix) {
        for (double& value : row) value /= static_cast<double>(matrices.size());
    }
    artifact.inverse_operator.assign(size, std::vector<double>(size));
    for (std::size_t column = 0; column < size; ++column) {
        std::vector<double> unit(size);
        unit[column] = 1.0;
        std::vector<double> inverse_column;
        if (!solve_system(mean_matrix, unit, inverse_column)) {
            throw std::runtime_error("mean training matrix is singular");
        }
        for (std::size_t row = 0; row < size; ++row) {
            artifact.inverse_operator[row][column] = inverse_column[row];
        }
    }
    for (std::size_t row = 0; row < size; ++row) {
        for (std::size_t column = row + 1; column < size; ++column) {
            const double symmetric = 0.5 * (
                artifact.inverse_operator[row][column] +
                artifact.inverse_operator[column][row]);
            artifact.inverse_operator[row][column] = symmetric;
            artifact.inverse_operator[column][row] = symmetric;
        }
    }
    for (const auto& matrix : matrices) {
        for (std::size_t row = 0; row < size; ++row) {
            for (std::size_t column = 0; column < size; ++column) {
                artifact.maximum_matrix_drift = std::max(
                    artifact.maximum_matrix_drift,
                    std::abs(matrix[row][column] - mean_matrix[row][column]) /
                        std::max(std::abs(mean_matrix[row][column]), 1.0));
            }
        }
    }
    artifact.seal();
    artifact.validate();
    return artifact;
}

LearnedMultigridArtifact build_learned_multigrid_artifact(
    const std::string& model_source_hash,
    const std::string& block_fingerprint,
    const std::vector<std::string>& features,
    const std::vector<double>& feature_minimum,
    const std::vector<double>& feature_maximum,
    const std::vector<std::vector<std::vector<double>>>& matrices,
    bool jacobian_mode) {
    if (matrices.empty()) throw std::invalid_argument("no valid multigrid training matrices");
    const std::size_t fine_size = matrices.front().size();
    if (fine_size < 4 || feature_minimum.size() != features.size() ||
        feature_maximum.size() != features.size()) {
        throw std::invalid_argument("learned multigrid builder dimensions are invalid");
    }
    for (const auto& matrix : matrices) {
        if (matrix.size() != fine_size || !finite_matrix(matrix) ||
            !symmetric_positive_definite(matrix)) {
            throw std::invalid_argument("learned multigrid builder requires SPD matrices");
        }
        for (const auto& row : matrix) if (row.size() != fine_size) {
            throw std::invalid_argument("learned multigrid builder matrix is not square");
        }
    }
    LearnedMultigridArtifact artifact;
    artifact.model_source_hash = model_source_hash;
    artifact.block_fingerprint = block_fingerprint;
    artifact.features = features;
    artifact.feature_minimum = feature_minimum;
    artifact.feature_maximum = feature_maximum;
    artifact.training_samples = matrices.size();
    artifact.jacobian_mode = jacobian_mode;
    artifact.fine_operator.assign(fine_size, std::vector<double>(fine_size));
    for (const auto& matrix : matrices) {
        for (std::size_t row = 0; row < fine_size; ++row) {
            for (std::size_t column = 0; column < fine_size; ++column) {
                artifact.fine_operator[row][column] += matrix[row][column];
            }
        }
    }
    for (auto& row : artifact.fine_operator) for (double& value : row) {
        value /= static_cast<double>(matrices.size());
    }
    const auto make_prolongation = [](std::size_t fine, std::size_t coarse) {
        std::vector<std::vector<double>> result(fine, std::vector<double>(coarse));
        for (std::size_t index = 0; index < fine; ++index) {
            if (index % 2U == 0U) result[index][index / 2U] = 1.0;
            else {
                const auto left = index / 2U;
                result[index][left] = 0.5;
                if (left + 1U < coarse) result[index][left + 1U] = 0.5;
                else result[index][left] = 1.0;
            }
        }
        return result;
    };
    const auto galerkin = [](const auto& fine, const auto& transfer) {
        const auto fine_size = fine.size();
        const auto coarse_size = transfer.front().size();
        std::vector<std::vector<double>> result(
            coarse_size, std::vector<double>(coarse_size));
        for (std::size_t left = 0; left < coarse_size; ++left) {
            for (std::size_t right = 0; right < coarse_size; ++right) {
                for (std::size_t row = 0; row < fine_size; ++row) {
                    for (std::size_t column = 0; column < fine_size; ++column) {
                        result[left][right] += transfer[row][left] * fine[row][column] *
                            transfer[column][right];
                    }
                }
            }
        }
        return result;
    };
    artifact.level_operators = {artifact.fine_operator};
    while (artifact.level_operators.size() == 1U ||
           artifact.level_operators.back().size() > 4U) {
        const auto& fine = artifact.level_operators.back();
        const auto transfer = make_prolongation(
            fine.size(), (fine.size() + 1U) / 2U);
        const auto coarse = galerkin(fine, transfer);
        if (!symmetric_positive_definite(coarse)) {
            throw std::invalid_argument("learned multigrid Galerkin level is not SPD");
        }
        artifact.level_prolongations.push_back(transfer);
        artifact.level_operators.push_back(coarse);
    }
    artifact.prolongation = artifact.level_prolongations.front();
    const auto& coarsest = artifact.level_operators.back();
    artifact.coarse_inverse.assign(
        coarsest.size(), std::vector<double>(coarsest.size()));
    for (std::size_t column = 0; column < coarsest.size(); ++column) {
        std::vector<double> unit(coarsest.size()); unit[column] = 1.0;
        std::vector<double> inverse_column;
        if (!solve_system(coarsest, unit, inverse_column)) {
            throw std::runtime_error("learned multigrid coarsest solve is singular");
        }
        for (std::size_t row = 0; row < coarsest.size(); ++row) {
            artifact.coarse_inverse[row][column] = inverse_column[row];
        }
    }
    for (const auto& matrix : matrices) {
        for (std::size_t row = 0; row < fine_size; ++row) {
            for (std::size_t column = 0; column < fine_size; ++column) {
                artifact.maximum_matrix_drift = std::max(
                    artifact.maximum_matrix_drift,
                    std::abs(matrix[row][column] - artifact.fine_operator[row][column]) /
                        std::max(std::abs(artifact.fine_operator[row][column]), 1.0));
            }
        }
    }
    double best_contraction = std::numeric_limits<double>::infinity();
    double best_weight{};
    for (const double weight : {0.4, 0.5, 2.0 / 3.0, 0.8, 1.0}) {
        artifact.smoothing_weight = weight;
        double worst{};
        for (const auto& matrix : matrices) {
            worst = std::max(worst, basis_residual_contraction(artifact, matrix));
        }
        if (worst < best_contraction) {
            best_contraction = worst;
            artifact.maximum_probe_contraction = worst;
            best_weight = weight;
        }
    }
    if (!(best_contraction < 1.0)) {
        throw std::invalid_argument("learned multigrid builder found no contracting smoother");
    }
    artifact.smoothing_weight = best_weight;
    artifact.seal();
    artifact.validate();
    return artifact;
}

bool apply_learned_multigrid(
    const LearnedMultigridArtifact& artifact,
    const std::vector<double>& residual,
    std::vector<double>& result) {
    if (residual.size() != artifact.fine_operator.size()) return false;
    result = multigrid_cycle(artifact, residual);
    return result.size() == residual.size() && std::all_of(
        result.begin(), result.end(), [](double value) { return std::isfinite(value); });
}

LearnedMultigridArtifact train_learned_multigrid(
    const ModelIR& model,
    const std::string& block_id,
    const std::filesystem::path& scenario_directory) {
    const auto& block = block_by_id(model, block_id);
    if (block.event_related || !block.smooth || block.unknowns.size() < 4) {
        throw std::invalid_argument(
            "learned multigrid training requires a smooth non-event block with at least four unknowns");
    }
    std::unordered_map<std::string, Expression> residuals;
    for (const auto& equation : model.equations) {
        residuals.emplace(equation.id, Expression(equation.residual));
    }
    LearnedMultigridArtifact artifact;
    artifact.jacobian_mode = !block.linear;
    artifact.model_source_hash = model.source_hash;
    artifact.block_fingerprint = block.fingerprint;
    for (const auto& context : block.contexts) {
        const auto variable = std::find_if(
            model.variables.begin(), model.variables.end(),
            [&](const VariableIR& item) { return item.name == context; });
        if (variable != model.variables.end() && variable->kind == "parameter") {
            artifact.features.push_back(context);
        }
    }
    artifact.feature_minimum.assign(
        artifact.features.size(), std::numeric_limits<double>::infinity());
    artifact.feature_maximum.assign(
        artifact.features.size(), -std::numeric_limits<double>::infinity());
    std::vector<std::vector<std::vector<double>>> matrices;
    for (const auto& path : scenario_files(scenario_directory)) {
        const auto scenario = read_scenario(path);
        bool complete = true;
        for (std::size_t index = 0; index < artifact.features.size(); ++index) {
            const auto value = scenario.find(artifact.features[index]);
            if (value == scenario.end()) { complete = false; break; }
            artifact.feature_minimum[index] = std::min(
                artifact.feature_minimum[index], value->second);
            artifact.feature_maximum[index] = std::max(
                artifact.feature_maximum[index], value->second);
        }
        if (!complete) continue;
        std::vector<std::vector<double>> matrix;
        if (block.linear) {
            matrix = assemble_linear_system(model, block, residuals, scenario).matrix;
        } else {
            const auto solved = Runtime(model).solve(
                scenario, ".smave/multigrid-training");
            if (!solved.success) {
                throw std::invalid_argument(
                    "nonlinear multigrid training scenario did not pass the original solver gate");
            }
            matrix = finite_difference_jacobian(residuals, block, solved.values);
        }
        if (!symmetric_positive_definite(matrix)) {
            throw std::invalid_argument(
                "multigrid training encountered a non-SPD matrix; PCG permission denied");
        }
        for (std::size_t row = 0; row < matrix.size(); ++row) {
            for (std::size_t column = row + 1; column < matrix.size(); ++column) {
                const double value = 0.5 * (matrix[row][column] + matrix[column][row]);
                matrix[row][column] = value;
                matrix[column][row] = value;
            }
        }
        matrices.push_back(std::move(matrix));
    }
    return build_learned_multigrid_artifact(
        model.source_hash,
        block.fingerprint,
        artifact.features,
        artifact.feature_minimum,
        artifact.feature_maximum,
        matrices,
        !block.linear);
}

void register_linear_preconditioner(
    Registry& registry,
    const LinearPreconditionerArtifact& artifact,
    const std::string& domain_version,
    const std::string& tolerance_profile,
    const std::string& hardware_profile,
    std::optional<VerificationCertificate> certificate) {
    const std::string evidence = certificate
        ? certificate->certificate_hash
        : "matrix-residual-traces-" + std::to_string(artifact.training_samples);
    auto expert = std::make_shared<LearnedLinearPreconditionerExpert>(
        artifact, std::move(certificate));
    registry.register_expert(
        expert,
        ExpertGrant{
            .expert_version = expert->version(),
            .block_family = artifact.block_fingerprint,
            .domain_version = domain_version,
            .tolerance_profile = tolerance_profile,
            .hardware_profile = hardware_profile,
            .permission = Permission::corrected,
            .evidence_level = EvidenceLevel::e2,
            .evidence_bundle = evidence,
            .artifact_hash = artifact.artifact_hash,
            .expires_unix_seconds = 0,
        });
}

void register_learned_multigrid(
    Registry& registry,
    const LearnedMultigridArtifact& artifact,
    const std::string& domain_version,
    const std::string& tolerance_profile,
    const std::string& hardware_profile,
    std::optional<VerificationCertificate> certificate) {
    const std::string evidence = certificate
        ? certificate->certificate_hash
        : "multigrid-contraction-probes-" + std::to_string(artifact.training_samples);
    auto expert = std::make_shared<LearnedMultigridExpert>(
        artifact, std::move(certificate));
    registry.register_expert(
        expert,
        ExpertGrant{
            .expert_version = expert->version(),
            .block_family = artifact.block_fingerprint,
            .domain_version = domain_version,
            .tolerance_profile = tolerance_profile,
            .hardware_profile = hardware_profile,
            .permission = Permission::corrected,
            .evidence_level = EvidenceLevel::e2,
            .evidence_bundle = evidence,
            .artifact_hash = artifact.artifact_hash,
            .expires_unix_seconds = 0,
        });
}

VerificationCertificate verify_affine_warm_start(
    const ModelIR& model,
    const AffineWarmStartArtifact& artifact,
    std::size_t maximum_depth,
    const std::filesystem::path& trace_directory) {
    artifact.validate();
    const auto block = std::find_if(
        model.blocks.begin(), model.blocks.end(),
        [&](const BlockIR& item) { return item.fingerprint == artifact.block_fingerprint; });
    if (block == model.blocks.end()) {
        throw std::invalid_argument("affine block fingerprint is absent from model");
    }
    auto registry = make_default_registry(model);
    register_affine_expert(registry, artifact);
    auto bundle = make_default_bundle(model);
    bundle.add_expert(
        artifact.expert_version,
        artifact.artifact_hash,
        registry.grant(artifact.expert_version).evidence_bundle);
    const Runtime runtime(model, std::move(registry), bundle);
    auto certificate = verify_cells(
        artifact.expert_version,
        artifact.artifact_hash,
        artifact.block_fingerprint,
        artifact.features,
        artifact.feature_minimum,
        artifact.feature_maximum,
        [&](const std::unordered_map<std::string, double>& context) {
            const auto outcome = runtime.solve(context, trace_directory);
            ProbeResult probe;
            probe.accepted = outcome.success && !outcome.blocks.empty() &&
                outcome.blocks.front().path != SolvePath::full_fallback;
            if (!outcome.blocks.empty()) {
                probe.residual = outcome.blocks.front().gate.residual_inf;
                probe.risk = static_cast<double>(outcome.blocks.front().expert_iterations) /
                    8.0;
            }
            probe.reason = probe.accepted
                ? "corrected candidate passed runtime gate"
                : "candidate required full fallback or original solver failed";
            return probe;
        },
        maximum_depth);
    if (artifact.schema_version == "smave.affine-warm-start.v2") {
        certificate.schema_version = "smave.verified-cells.v2";
        certificate.training_dataset_id = artifact.training_dataset_id;
        certificate.training_dataset_version = artifact.training_dataset_version;
        certificate.training_dataset_manifest_hash = artifact.training_dataset_manifest_hash;
        certificate.seal();
        certificate.validate();
    }
    return certificate;
}

VerificationCertificate verify_linear_preconditioner(
    const ModelIR& model,
    const LinearPreconditionerArtifact& artifact,
    std::size_t maximum_depth,
    const std::filesystem::path& trace_directory) {
    artifact.validate();
    const auto block_iterator = std::find_if(
        model.blocks.begin(), model.blocks.end(),
        [&](const BlockIR& item) { return item.fingerprint == artifact.block_fingerprint; });
    if (block_iterator == model.blocks.end()) {
        throw std::invalid_argument("preconditioner block fingerprint is absent from model");
    }
    auto registry = make_default_registry(model);
    register_linear_preconditioner(registry, artifact);
    auto bundle = make_default_bundle(model);
    bundle.add_expert(
        artifact.expert_version,
        artifact.artifact_hash,
        registry.grant(artifact.expert_version).evidence_bundle);
    const Runtime runtime(model, std::move(registry), bundle);
    auto certificate = verify_cells(
        artifact.expert_version,
        artifact.artifact_hash,
        artifact.block_fingerprint,
        artifact.features,
        artifact.feature_minimum,
        artifact.feature_maximum,
        [&](const std::unordered_map<std::string, double>& context) {
            const auto outcome = runtime.solve(context, trace_directory);
            ProbeResult probe;
            probe.accepted = outcome.success && !outcome.blocks.empty() &&
                outcome.blocks.front().path == SolvePath::corrected_accept &&
                outcome.blocks.front().preconditioner_version == artifact.expert_version;
            if (!outcome.blocks.empty()) {
                probe.residual = outcome.blocks.front().gate.residual_inf;
                probe.risk = outcome.blocks.front().krylov_breakdown ||
                        outcome.blocks.front().krylov_stagnated
                    ? 1.0
                    : static_cast<double>(outcome.blocks.front().krylov_iterations) /
                        std::max<std::size_t>(1, block_iterator->unknowns.size());
            }
            probe.reason = probe.accepted
                ? "learned preconditioner PCG passed true residual gate"
                : "learned preconditioner rejected, stalled, or cascaded";
            return probe;
        },
        maximum_depth);
    if (artifact.schema_version == "smave.linear-preconditioner.v2") {
        certificate.schema_version = "smave.verified-cells.v2";
        certificate.training_dataset_id = artifact.training_dataset_id;
        certificate.training_dataset_version = artifact.training_dataset_version;
        certificate.training_dataset_manifest_hash = artifact.training_dataset_manifest_hash;
        certificate.seal();
        certificate.validate();
    }
    return certificate;
}

VerificationCertificate verify_learned_multigrid(
    const ModelIR& model,
    const LearnedMultigridArtifact& artifact,
    std::size_t maximum_depth,
    const std::filesystem::path& trace_directory) {
    artifact.validate();
    const auto block_iterator = std::find_if(
        model.blocks.begin(), model.blocks.end(),
        [&](const BlockIR& item) { return item.fingerprint == artifact.block_fingerprint; });
    if (block_iterator == model.blocks.end()) {
        throw std::invalid_argument("multigrid block fingerprint is absent from model");
    }
    auto registry = make_default_registry(model);
    register_learned_multigrid(registry, artifact);
    auto bundle = make_default_bundle(model);
    bundle.add_expert(
        artifact.expert_version,
        artifact.artifact_hash,
        registry.grant(artifact.expert_version).evidence_bundle);
    const Runtime runtime(model, std::move(registry), bundle);
    auto certificate = verify_cells(
        artifact.expert_version,
        artifact.artifact_hash,
        artifact.block_fingerprint,
        artifact.features,
        artifact.feature_minimum,
        artifact.feature_maximum,
        [&](const std::unordered_map<std::string, double>& context) {
            const auto outcome = runtime.solve(context, trace_directory);
            ProbeResult probe;
            probe.accepted = outcome.success && !outcome.blocks.empty() &&
                outcome.blocks.front().path == SolvePath::corrected_accept &&
                outcome.blocks.front().preconditioner_version == artifact.expert_version;
            if (!outcome.blocks.empty()) {
                probe.residual = outcome.blocks.front().gate.residual_inf;
                probe.risk = outcome.blocks.front().krylov_breakdown ||
                        outcome.blocks.front().krylov_stagnated
                    ? 1.0
                    : static_cast<double>(outcome.blocks.front().krylov_iterations) /
                        std::max<std::size_t>(1, block_iterator->unknowns.size());
            }
            probe.reason = probe.accepted
                ? "learned multigrid PCG passed true residual gate"
                : "learned multigrid rejected, stalled, or cascaded";
            return probe;
        },
        maximum_depth);
    if (artifact.schema_version == "smave.learned-multigrid.v3") {
        certificate.schema_version = "smave.verified-cells.v2";
        certificate.training_dataset_id = artifact.training_dataset_id;
        certificate.training_dataset_version = artifact.training_dataset_version;
        certificate.training_dataset_manifest_hash = artifact.training_dataset_manifest_hash;
        certificate.seal();
        certificate.validate();
    }
    return certificate;
}

}  // namespace smave
