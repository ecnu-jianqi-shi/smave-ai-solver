#include "smave/pdebench_training.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

namespace smave {
namespace {

constexpr std::uint64_t kFnvOffset = 1469598103934665603ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

void require_tag(std::istream& input, const char* expected) {
    std::string tag;
    if (!(input >> tag) || tag != expected) {
        throw std::invalid_argument(
            std::string("invalid PDEBench training manifest field: expected ") +
            expected);
    }
}

bool read_boolean(std::istream& input, const char* field) {
    int value{};
    require_tag(input, field);
    if (!(input >> value) || (value != 0 && value != 1)) {
        throw std::invalid_argument(
            std::string("invalid PDEBench training boolean: ") + field);
    }
    return value == 1;
}

std::size_t read_size(std::istream& input, const char* field) {
    std::size_t value{};
    require_tag(input, field);
    if (!(input >> value) || value == 0) {
        throw std::invalid_argument(
            std::string("invalid PDEBench training size: ") + field);
    }
    return value;
}

std::string read_quoted(std::istream& input, const char* field) {
    std::string value;
    require_tag(input, field);
    if (!(input >> std::quoted(value))) {
        throw std::invalid_argument(
            std::string("invalid PDEBench training string: ") + field);
    }
    return value;
}

void validate_identifier(const std::string& value, const char* field) {
    if (value.empty() || !std::all_of(
            value.begin(), value.end(), [](unsigned char character) {
                return std::isalnum(character) || character == '-' ||
                    character == '_' || character == '.';
            })) {
        throw std::invalid_argument(
            std::string("invalid PDEBench training identifier: ") + field);
    }
}

std::uint64_t hash_file(
    const std::filesystem::path& path, std::uint64_t hash) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::invalid_argument(
            "cannot open PDEBench training tensor: " + path.string());
    }
    char buffer[64 * 1024];
    while (input) {
        input.read(buffer, sizeof(buffer));
        const auto count = input.gcount();
        for (std::streamsize index = 0; index < count; ++index) {
            hash ^= static_cast<unsigned char>(buffer[index]);
            hash *= kFnvPrime;
        }
    }
    if (!input.eof()) {
        throw std::invalid_argument(
            "cannot read PDEBench training tensor: " + path.string());
    }
    return hash;
}

std::string compute_payload_checksum(
    const std::filesystem::path& inputs,
    const std::filesystem::path& targets) {
    auto hash = hash_file(inputs, kFnvOffset);
    hash = hash_file(targets, hash);
    std::ostringstream output;
    output << std::hex << std::setw(16) << std::setfill('0') << hash;
    return output.str();
}

struct SourceRange {
    std::filesystem::path path;
    std::size_t sample_begin{};
    std::size_t sample_end{};
};

std::optional<SourceRange> parse_source_range(
    const std::filesystem::path& source) {
    const auto text = source.string();
    const auto marker = text.find("#samples=");
    if (marker == std::string::npos) return std::nullopt;
    const auto colon = text.find(':', marker + 9);
    const auto semicolon = text.find(';', colon + 1);
    if (colon == std::string::npos || semicolon == std::string::npos) {
        throw std::invalid_argument("invalid PDEBench SOURCE sample range");
    }
    SourceRange range;
    range.path = text.substr(0, marker);
    try {
        range.sample_begin = std::stoull(
            text.substr(marker + 9, colon - (marker + 9)));
        range.sample_end = std::stoull(
            text.substr(colon + 1, semicolon - colon - 1));
    } catch (const std::exception&) {
        throw std::invalid_argument("invalid PDEBench SOURCE sample range");
    }
    if (range.path.empty() || range.sample_begin >= range.sample_end) {
        throw std::invalid_argument("empty PDEBench SOURCE sample range");
    }
    return range;
}

std::vector<double> read_f64_tensor(
    const std::filesystem::path& path, std::size_t values) {
    std::vector<double> output(values);
    std::ifstream input(path, std::ios::binary);
    input.read(reinterpret_cast<char*>(output.data()),
               static_cast<std::streamsize>(output.size() * sizeof(double)));
    if (!input || input.peek() != std::char_traits<char>::eof()) {
        throw std::invalid_argument("cannot read exact PDEBench fp64 tensor");
    }
    if (!std::all_of(output.begin(), output.end(), [](double value) {
            return std::isfinite(value);
        })) {
        throw std::invalid_argument("PDEBench fp64 tensor contains non-finite values");
    }
    return output;
}

double recurrence_relative_residual(
    const std::vector<double>& inputs,
    const std::vector<double>& targets,
    std::size_t samples,
    std::size_t width,
    double inverse_diagonal,
    double feedback) {
    double maximum{};
    for (std::size_t sample = 0; sample < samples; ++sample) {
        const auto offset = sample * width;
        double residual_squared{};
        double input_squared{};
        for (std::size_t index = 0; index < width; ++index) {
            const auto previous = index == 0 ? width - 1 : index - 1;
            const auto residual = targets[offset + index] -
                inverse_diagonal * inputs[offset + index] -
                feedback * targets[offset + previous];
            residual_squared += residual * residual;
            input_squared += inputs[offset + index] * inputs[offset + index];
        }
        maximum = std::max(maximum,
            std::sqrt(residual_squared) /
                std::max(1.0, std::sqrt(input_squared)));
    }
    return maximum;
}

double burgers_relative_residual(
    const std::vector<double>& inputs,
    const std::vector<double>& targets,
    std::size_t samples,
    std::size_t width,
    double diffusion_number,
    double convection_scale) {
    double maximum{};
    for (std::size_t sample = 0; sample < samples; ++sample) {
        const auto offset = sample * width;
        double residual_squared{};
        double input_squared{};
        for (std::size_t index = 0; index < width; ++index) {
            const auto previous = index == 0 ? width - 1 : index - 1;
            const auto next = index + 1 == width ? 0 : index + 1;
            const auto state = inputs[offset + index];
            const auto convection = convection_scale * state;
            const auto product =
                (1.0 + 2.0 * diffusion_number) * targets[offset + index] +
                (-diffusion_number - convection) * targets[offset + previous] +
                (-diffusion_number + convection) * targets[offset + next];
            const auto residual = state - product;
            residual_squared += residual * residual;
            input_squared += state * state;
        }
        maximum = std::max(maximum,
            std::sqrt(residual_squared) /
                std::max(1.0, std::sqrt(input_squared)));
    }
    return maximum;
}

double retardation_relative_residual(
    const std::vector<double>& inputs,
    const std::vector<double>& targets,
    std::size_t samples,
    std::size_t width,
    double constant_ratio,
    double power_ratio,
    double concentration_exponent) {
    double maximum{};
    for (std::size_t sample = 0; sample < samples; ++sample) {
        const auto offset = sample * width;
        double residual_squared{};
        double right_squared{};
        for (std::size_t index = 1; index < width; ++index) {
            const auto ratio = constant_ratio + power_ratio * std::pow(
                std::max(inputs[offset + index], 1.0e-8),
                concentration_exponent);
            auto product = (ratio + 2.0) * targets[offset + index] -
                targets[offset + index - 1];
            if (index + 1 < width) {
                product -= targets[offset + index + 1];
            } else {
                product -= targets[offset + index];
            }
            const auto right = ratio * inputs[offset + index];
            const auto residual = right - product;
            residual_squared += residual * residual;
            right_squared += right * right;
        }
        maximum = std::max(maximum,
            std::sqrt(residual_squared) /
                std::max(1.0, std::sqrt(right_squared)));
    }
    return maximum;
}

std::vector<double> darcy_features(
    const double* coefficient,
    std::size_t width,
    std::size_t feature_width) {
    const auto block = width / feature_width;
    std::vector<double> features(feature_width * feature_width);
    for (std::size_t feature_row = 0; feature_row < feature_width; ++feature_row) {
        for (std::size_t feature_column = 0;
             feature_column < feature_width; ++feature_column) {
            double sum{};
            for (std::size_t row = 0; row < block; ++row) {
                for (std::size_t column = 0; column < block; ++column) {
                    sum += coefficient[
                        (feature_row * block + row) * width +
                        feature_column * block + column];
                }
            }
            features[feature_row * feature_width + feature_column] =
                sum / static_cast<double>(block * block);
        }
    }
    return features;
}

double darcy_relative_residual(
    const double* coefficient,
    const double* full_solution,
    std::size_t width) {
    const auto spacing = 1.0 / static_cast<double>(width - 1);
    const auto scale = 1.0 / (spacing * spacing);
    double residual_squared{};
    double right_squared{};
    for (std::size_t row = 1; row + 1 < width; ++row) {
        for (std::size_t column = 1; column + 1 < width; ++column) {
            const auto index = row * width + column;
            const auto center = coefficient[index];
            const auto harmonic = [](double left, double right) {
                return 2.0 * left * right / (left + right);
            };
            const auto west = harmonic(center, coefficient[index - 1]) * scale;
            const auto east = harmonic(center, coefficient[index + 1]) * scale;
            const auto south = harmonic(center, coefficient[index - width]) * scale;
            const auto north = harmonic(center, coefficient[index + width]) * scale;
            const auto product =
                (west + east + south + north) * full_solution[index] -
                west * full_solution[index - 1] -
                east * full_solution[index + 1] -
                south * full_solution[index - width] -
                north * full_solution[index + width];
            const auto residual = 1.0 - product;
            residual_squared += residual * residual;
            right_squared += 1.0;
        }
    }
    return std::sqrt(residual_squared / right_squared);
}

double relative_inf_error(
    const double* left, const double* right, std::size_t values) {
    double numerator{};
    double denominator{1.0};
    for (std::size_t index = 0; index < values; ++index) {
        numerator = std::max(numerator, std::abs(left[index] - right[index]));
        denominator = std::max(denominator, std::abs(right[index]));
    }
    return numerator / denominator;
}

double periodic_helmholtz_relative_residual(
    const std::vector<double>& inputs,
    const std::vector<double>& targets,
    std::size_t samples,
    std::size_t width,
    double stencil_number) {
    double maximum{};
    for (std::size_t sample = 0; sample < samples; ++sample) {
        const auto offset = sample * width * width;
        double residual_squared{};
        double right_squared{};
        for (std::size_t row = 0; row < width; ++row) {
            const auto south = row == 0 ? width - 1 : row - 1;
            const auto north = row + 1 == width ? 0 : row + 1;
            for (std::size_t column = 0; column < width; ++column) {
                const auto west = column == 0 ? width - 1 : column - 1;
                const auto east = column + 1 == width ? 0 : column + 1;
                const auto index = row * width + column;
                const auto value = targets[offset + index];
                const auto product = (1.0 + 4.0 * stencil_number) * value -
                    stencil_number * (
                        targets[offset + row * width + west] +
                        targets[offset + row * width + east] +
                        targets[offset + south * width + column] +
                        targets[offset + north * width + column]);
                const auto residual = inputs[offset + index] - product;
                residual_squared += residual * residual;
                right_squared += inputs[offset + index] * inputs[offset + index];
            }
        }
        maximum = std::max(maximum,
            std::sqrt(residual_squared) /
                std::max(1.0, std::sqrt(right_squared)));
    }
    return maximum;
}

void write_f64_tensor(
    const std::filesystem::path& path,
    const std::vector<double>& values) {
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(values.data()),
                 static_cast<std::streamsize>(values.size() * sizeof(double)));
    if (!output) throw std::runtime_error("cannot write learned tensor payload");
}

}  // namespace

std::size_t PdebenchTrainingManifest::tensor_bytes() const {
    const auto bytes_per_value = dtype == "fp32" ? std::size_t{4} :
        dtype == "fp64" ? std::size_t{8} : std::size_t{};
    if (bytes_per_value == 0 ||
        samples > std::numeric_limits<std::size_t>::max() / values_per_sample ||
        samples * values_per_sample >
            std::numeric_limits<std::size_t>::max() / bytes_per_value) {
        throw std::invalid_argument("PDEBench training tensor size overflows");
    }
    return samples * values_per_sample * bytes_per_value;
}

void PdebenchTrainingManifest::validate() const {
    validate_identifier(family, "FAMILY");
    if (source.empty()) {
        throw std::invalid_argument("PDEBench training SOURCE must not be empty");
    }
    if (samples == 0 || values_per_sample == 0) {
        throw std::invalid_argument("PDEBench training shape must be nonzero");
    }
    if (target_kind != "authoritative-next-state-pretraining" &&
        target_kind != "same-discrete-operator-solver-label") {
        throw std::invalid_argument("unsupported PDEBench TARGET_KIND");
    }
    if (solver_label !=
        (target_kind == "same-discrete-operator-solver-label")) {
        throw std::invalid_argument(
            "PDEBench SOLVER_LABEL conflicts with TARGET_KIND");
    }
    if (solver_label) {
        validate_identifier(discrete_operator_id, "DISCRETE_OPERATOR_ID");
        if (discrete_operator_id == "none") {
            throw std::invalid_argument(
                "solver labels require a concrete discrete operator id");
        }
    } else if (discrete_operator_id != "none" ||
               original_residual_certified) {
        throw std::invalid_argument(
            "pretraining data cannot claim an operator or residual certificate");
    }
    if (dtype != "fp32" && dtype != "fp64") {
        throw std::invalid_argument("PDEBench training DTYPE must be fp32 or fp64");
    }
    if (layout != "sample-major-contiguous") {
        throw std::invalid_argument("unsupported PDEBench training LAYOUT");
    }
    if (checksum.size() != 16 || !std::all_of(
            checksum.begin(), checksum.end(), [](unsigned char character) {
                return std::isdigit(character) ||
                    (character >= 'a' && character <= 'f');
            })) {
        throw std::invalid_argument("invalid PDEBench training CHECKSUM");
    }
    static_cast<void>(tensor_bytes());
}

void PdebenchTrainingManifest::validate_for(
    PdebenchTrainingUse use,
    const std::string& expected_operator_id) const {
    validate();
    if (use == PdebenchTrainingUse::Pretraining) {
        if (solver_label) {
            throw std::invalid_argument(
                "solver labels cannot enter the next-state pretrainer");
        }
        return;
    }
    if (!solver_label) {
        throw std::invalid_argument(
            "next-state pretraining data cannot enter the solver-label trainer");
    }
    if (expected_operator_id.empty()) {
        throw std::invalid_argument(
            "solver-label training requires an expected discrete operator id");
    }
    if (discrete_operator_id != expected_operator_id) {
        throw std::invalid_argument(
            "PDEBench solver-label discrete operator id mismatch");
    }
    if (use == PdebenchTrainingUse::DirectDeployment &&
        !original_residual_certified) {
        throw std::invalid_argument(
            "uncertified solver labels cannot grant Direct deployment authority");
    }
}

PdebenchTrainingManifest PdebenchTrainingManifest::read(
    const std::filesystem::path& manifest_path) {
    std::ifstream input(manifest_path);
    if (!input) {
        throw std::invalid_argument(
            "cannot open PDEBench training manifest: " + manifest_path.string());
    }
    std::string schema;
    std::getline(input, schema);
    if (schema != kPdebenchTrainingSetSchemaVersion) {
        throw std::invalid_argument("unsupported PDEBench training manifest schema");
    }
    PdebenchTrainingManifest manifest;
    manifest.family = read_quoted(input, "FAMILY");
    manifest.source = read_quoted(input, "SOURCE");
    manifest.samples = read_size(input, "SAMPLES");
    manifest.values_per_sample = read_size(input, "VALUES_PER_SAMPLE");
    manifest.target_kind = read_quoted(input, "TARGET_KIND");
    manifest.solver_label = read_boolean(input, "SOLVER_LABEL");
    manifest.discrete_operator_id = read_quoted(input, "DISCRETE_OPERATOR_ID");
    manifest.original_residual_certified =
        read_boolean(input, "ORIGINAL_RESIDUAL_CERTIFIED");
    manifest.dtype = read_quoted(input, "DTYPE");
    manifest.layout = read_quoted(input, "LAYOUT");
    manifest.checksum = read_quoted(input, "CHECKSUM");
    require_tag(input, "END");
    std::string trailing;
    if (input >> trailing) {
        throw std::invalid_argument(
            "PDEBench training manifest contains trailing content");
    }
    manifest.validate();
    return manifest;
}

PdebenchTrainingManifest PdebenchTrainingManifest::read_and_verify(
    const std::filesystem::path& prefix,
    PdebenchTrainingUse use,
    const std::string& expected_operator_id) {
    const auto manifest_path = prefix.string() + ".manifest.txt";
    const auto manifest = read(manifest_path);
    manifest.validate_for(use, expected_operator_id);
    const auto suffix = manifest.dtype == "fp32" ? ".f32" : ".f64";
    const auto inputs = std::filesystem::path(prefix.string() + ".inputs" + suffix);
    const auto targets = std::filesystem::path(prefix.string() + ".targets" + suffix);
    const auto expected_bytes = manifest.tensor_bytes();
    if (!std::filesystem::is_regular_file(inputs) ||
        !std::filesystem::is_regular_file(targets) ||
        std::filesystem::file_size(inputs) != expected_bytes ||
        std::filesystem::file_size(targets) != expected_bytes) {
        throw std::invalid_argument(
            "PDEBench training tensor shape does not match its manifest");
    }
    if (compute_payload_checksum(inputs, targets) != manifest.checksum) {
        throw std::invalid_argument(
            "PDEBench training tensor checksum mismatch");
    }
    return manifest;
}

void LearnedPeriodicRecurrenceArtifact::validate() const {
    validate_identifier(family, "FAMILY");
    validate_identifier(discrete_operator_id, "DISCRETE_OPERATOR_ID");
    if (width < 2 || !std::isfinite(inverse_diagonal) ||
        !std::isfinite(feedback) || inverse_diagonal <= 0.0 ||
        feedback < 0.0 || feedback >= 1.0 ||
        !std::isfinite(training_maximum_relative_residual) ||
        !std::isfinite(heldout_maximum_relative_residual) ||
        training_maximum_relative_residual > 1.0e-12 ||
        heldout_maximum_relative_residual > 1.0e-12 ||
        training_checksum.size() != 16 || heldout_checksum.size() != 16 ||
        training_checksum == heldout_checksum) {
        throw std::invalid_argument("invalid learned periodic recurrence artifact");
    }
}

void LearnedPeriodicRecurrenceArtifact::write(
    const std::filesystem::path& path) const {
    validate();
    std::ofstream output(path);
    output << std::setprecision(17)
           << "SMAVE_LEARNED_PERIODIC_RECURRENCE 1\n"
           << "FAMILY " << std::quoted(family) << '\n'
           << "DISCRETE_OPERATOR_ID " << std::quoted(discrete_operator_id) << '\n'
           << "WIDTH " << width << '\n'
           << "INVERSE_DIAGONAL " << inverse_diagonal << '\n'
           << "FEEDBACK " << feedback << '\n'
           << "TRAINING_MAXIMUM_RELATIVE_RESIDUAL "
           << training_maximum_relative_residual << '\n'
           << "HELDOUT_MAXIMUM_RELATIVE_RESIDUAL "
           << heldout_maximum_relative_residual << '\n'
           << "TRAINING_CHECKSUM " << std::quoted(training_checksum) << '\n'
           << "HELDOUT_CHECKSUM " << std::quoted(heldout_checksum) << '\n'
           << "END\n";
    if (!output) throw std::runtime_error("cannot write learned recurrence artifact");
}

LearnedPeriodicRecurrenceArtifact LearnedPeriodicRecurrenceArtifact::read(
    const std::filesystem::path& path) {
    std::ifstream input(path);
    std::string schema;
    std::getline(input, schema);
    if (schema != "SMAVE_LEARNED_PERIODIC_RECURRENCE 1") {
        throw std::invalid_argument("unsupported learned recurrence artifact schema");
    }
    LearnedPeriodicRecurrenceArtifact artifact;
    artifact.family = read_quoted(input, "FAMILY");
    artifact.discrete_operator_id = read_quoted(input, "DISCRETE_OPERATOR_ID");
    artifact.width = read_size(input, "WIDTH");
    require_tag(input, "INVERSE_DIAGONAL");
    input >> artifact.inverse_diagonal;
    require_tag(input, "FEEDBACK");
    input >> artifact.feedback;
    require_tag(input, "TRAINING_MAXIMUM_RELATIVE_RESIDUAL");
    input >> artifact.training_maximum_relative_residual;
    require_tag(input, "HELDOUT_MAXIMUM_RELATIVE_RESIDUAL");
    input >> artifact.heldout_maximum_relative_residual;
    artifact.training_checksum = read_quoted(input, "TRAINING_CHECKSUM");
    artifact.heldout_checksum = read_quoted(input, "HELDOUT_CHECKSUM");
    require_tag(input, "END");
    std::string trailing;
    if (!input || input >> trailing) {
        throw std::invalid_argument("invalid learned recurrence artifact tail");
    }
    artifact.validate();
    return artifact;
}

LearnedPeriodicRecurrenceArtifact fit_pdebench_periodic_recurrence(
    const std::filesystem::path& training_prefix,
    const std::filesystem::path& heldout_prefix,
    const std::string& expected_operator_id) {
    const auto training = PdebenchTrainingManifest::read_and_verify(
        training_prefix, PdebenchTrainingUse::DirectDeployment,
        expected_operator_id);
    const auto heldout = PdebenchTrainingManifest::read_and_verify(
        heldout_prefix, PdebenchTrainingUse::DirectDeployment,
        expected_operator_id);
    if (training.family != heldout.family ||
        training.values_per_sample != heldout.values_per_sample ||
        training.dtype != "fp64" || heldout.dtype != "fp64") {
        throw std::invalid_argument("incompatible periodic recurrence datasets");
    }
    const auto training_range = parse_source_range(training.source);
    const auto heldout_range = parse_source_range(heldout.source);
    if (!training_range || !heldout_range ||
        training_range->path != heldout_range->path ||
        !(training_range->sample_end <= heldout_range->sample_begin ||
          heldout_range->sample_end <= training_range->sample_begin)) {
        throw std::invalid_argument(
            "periodic recurrence training and heldout source ranges overlap");
    }
    const auto training_values =
        training.samples * training.values_per_sample;
    const auto heldout_values = heldout.samples * heldout.values_per_sample;
    const auto training_inputs = read_f64_tensor(
        training_prefix.string() + ".inputs.f64", training_values);
    const auto training_targets = read_f64_tensor(
        training_prefix.string() + ".targets.f64", training_values);
    const auto heldout_inputs = read_f64_tensor(
        heldout_prefix.string() + ".inputs.f64", heldout_values);
    const auto heldout_targets = read_f64_tensor(
        heldout_prefix.string() + ".targets.f64", heldout_values);

    long double centered_input_squared{};
    long double centered_input_target{};
    const auto width = training.values_per_sample;
    for (std::size_t sample = 0; sample < training.samples; ++sample) {
        const auto offset = sample * width;
        for (std::size_t index = 0; index < width; ++index) {
            const auto previous = index == 0 ? width - 1 : index - 1;
            const long double x = training_inputs[offset + index];
            const long double prior = training_targets[offset + previous];
            const long double y = training_targets[offset + index];
            const auto centered_input = x - prior;
            centered_input_squared += centered_input * centered_input;
            centered_input_target += centered_input * (y - prior);
        }
    }
    if (!(centered_input_squared > 0.0L)) {
        throw std::invalid_argument("singular periodic recurrence fit");
    }
    LearnedPeriodicRecurrenceArtifact artifact;
    artifact.family = training.family;
    artifact.discrete_operator_id = expected_operator_id;
    artifact.width = width;
    artifact.inverse_diagonal = static_cast<double>(
        centered_input_target / centered_input_squared);
    artifact.feedback = 1.0 - artifact.inverse_diagonal;
    artifact.training_maximum_relative_residual = recurrence_relative_residual(
        training_inputs, training_targets, training.samples, width,
        artifact.inverse_diagonal, artifact.feedback);
    artifact.heldout_maximum_relative_residual = recurrence_relative_residual(
        heldout_inputs, heldout_targets, heldout.samples, width,
        artifact.inverse_diagonal, artifact.feedback);
    artifact.training_checksum = training.checksum;
    artifact.heldout_checksum = heldout.checksum;
    artifact.validate();
    return artifact;
}

void LearnedFrozenBurgersArtifact::validate() const {
    if (width < 3 || !std::isfinite(diffusion_number) ||
        !std::isfinite(convection_scale) || diffusion_number <= 0.0 ||
        convection_scale <= 0.0 ||
        !std::isfinite(training_maximum_relative_residual) ||
        !std::isfinite(heldout_maximum_relative_residual) ||
        training_maximum_relative_residual > 1.0e-12 ||
        heldout_maximum_relative_residual > 1.0e-12 ||
        training_checksum.size() != 16 || heldout_checksum.size() != 16 ||
        training_checksum == heldout_checksum) {
        throw std::invalid_argument("invalid learned frozen Burgers artifact");
    }
}

void LearnedFrozenBurgersArtifact::write(
    const std::filesystem::path& path) const {
    validate();
    std::ofstream output(path);
    output << std::setprecision(17)
           << "SMAVE_LEARNED_FROZEN_BURGERS 1\n"
           << "DISCRETE_OPERATOR_ID \"frozen-burgers-cyclic-tridiagonal-v1\"\n"
           << "WIDTH " << width << '\n'
           << "DIFFUSION_NUMBER " << diffusion_number << '\n'
           << "CONVECTION_SCALE " << convection_scale << '\n'
           << "TRAINING_MAXIMUM_RELATIVE_RESIDUAL "
           << training_maximum_relative_residual << '\n'
           << "HELDOUT_MAXIMUM_RELATIVE_RESIDUAL "
           << heldout_maximum_relative_residual << '\n'
           << "TRAINING_CHECKSUM " << std::quoted(training_checksum) << '\n'
           << "HELDOUT_CHECKSUM " << std::quoted(heldout_checksum) << '\n'
           << "END\n";
    if (!output) throw std::runtime_error("cannot write learned Burgers artifact");
}

LearnedFrozenBurgersArtifact LearnedFrozenBurgersArtifact::read(
    const std::filesystem::path& path) {
    std::ifstream input(path);
    std::string schema;
    std::getline(input, schema);
    if (schema != "SMAVE_LEARNED_FROZEN_BURGERS 1") {
        throw std::invalid_argument("unsupported learned Burgers artifact schema");
    }
    const auto operator_id = read_quoted(input, "DISCRETE_OPERATOR_ID");
    if (operator_id != "frozen-burgers-cyclic-tridiagonal-v1") {
        throw std::invalid_argument("learned Burgers operator id mismatch");
    }
    LearnedFrozenBurgersArtifact artifact;
    artifact.width = read_size(input, "WIDTH");
    require_tag(input, "DIFFUSION_NUMBER"); input >> artifact.diffusion_number;
    require_tag(input, "CONVECTION_SCALE"); input >> artifact.convection_scale;
    require_tag(input, "TRAINING_MAXIMUM_RELATIVE_RESIDUAL");
    input >> artifact.training_maximum_relative_residual;
    require_tag(input, "HELDOUT_MAXIMUM_RELATIVE_RESIDUAL");
    input >> artifact.heldout_maximum_relative_residual;
    artifact.training_checksum = read_quoted(input, "TRAINING_CHECKSUM");
    artifact.heldout_checksum = read_quoted(input, "HELDOUT_CHECKSUM");
    require_tag(input, "END");
    std::string trailing;
    if (!input || input >> trailing) {
        throw std::invalid_argument("invalid learned Burgers artifact tail");
    }
    artifact.validate();
    return artifact;
}

LearnedFrozenBurgersArtifact fit_pdebench_frozen_burgers(
    const std::filesystem::path& training_prefix,
    const std::filesystem::path& heldout_prefix) {
    constexpr const char* operator_id =
        "frozen-burgers-cyclic-tridiagonal-v1";
    const auto training = PdebenchTrainingManifest::read_and_verify(
        training_prefix, PdebenchTrainingUse::DirectDeployment, operator_id);
    const auto heldout = PdebenchTrainingManifest::read_and_verify(
        heldout_prefix, PdebenchTrainingUse::DirectDeployment, operator_id);
    if (training.family != "burgers" || heldout.family != "burgers" ||
        training.values_per_sample != heldout.values_per_sample ||
        training.dtype != "fp64" || heldout.dtype != "fp64") {
        throw std::invalid_argument("incompatible frozen Burgers datasets");
    }
    const auto training_range = parse_source_range(training.source);
    const auto heldout_range = parse_source_range(heldout.source);
    if (!training_range || !heldout_range ||
        training_range->path != heldout_range->path ||
        !(training_range->sample_end <= heldout_range->sample_begin ||
          heldout_range->sample_end <= training_range->sample_begin)) {
        throw std::invalid_argument(
            "Burgers training and heldout source ranges overlap");
    }
    const auto width = training.values_per_sample;
    const auto training_values = training.samples * width;
    const auto heldout_values = heldout.samples * width;
    const auto training_inputs = read_f64_tensor(
        training_prefix.string() + ".inputs.f64", training_values);
    const auto training_targets = read_f64_tensor(
        training_prefix.string() + ".targets.f64", training_values);
    const auto heldout_inputs = read_f64_tensor(
        heldout_prefix.string() + ".inputs.f64", heldout_values);
    const auto heldout_targets = read_f64_tensor(
        heldout_prefix.string() + ".targets.f64", heldout_values);
    long double aa{};
    long double ab{};
    long double bb{};
    long double ar{};
    long double br{};
    for (std::size_t sample = 0; sample < training.samples; ++sample) {
        const auto offset = sample * width;
        for (std::size_t index = 0; index < width; ++index) {
            const auto previous = index == 0 ? width - 1 : index - 1;
            const auto next = index + 1 == width ? 0 : index + 1;
            const long double center = training_targets[offset + index];
            const long double left = training_targets[offset + previous];
            const long double right = training_targets[offset + next];
            const long double state = training_inputs[offset + index];
            const long double diffusion_basis = 2.0L * center - left - right;
            const long double convection_basis = state * (right - left);
            const long double response = state - center;
            aa += diffusion_basis * diffusion_basis;
            ab += diffusion_basis * convection_basis;
            bb += convection_basis * convection_basis;
            ar += diffusion_basis * response;
            br += convection_basis * response;
        }
    }
    const auto determinant = aa * bb - ab * ab;
    if (!(std::abs(determinant) > 0.0L)) {
        throw std::invalid_argument("singular frozen Burgers fit");
    }
    LearnedFrozenBurgersArtifact artifact;
    artifact.width = width;
    artifact.diffusion_number = static_cast<double>((ar * bb - br * ab) / determinant);
    artifact.convection_scale = static_cast<double>((br * aa - ar * ab) / determinant);
    artifact.training_maximum_relative_residual = burgers_relative_residual(
        training_inputs, training_targets, training.samples, width,
        artifact.diffusion_number, artifact.convection_scale);
    artifact.heldout_maximum_relative_residual = burgers_relative_residual(
        heldout_inputs, heldout_targets, heldout.samples, width,
        artifact.diffusion_number, artifact.convection_scale);
    artifact.training_checksum = training.checksum;
    artifact.heldout_checksum = heldout.checksum;
    artifact.validate();
    return artifact;
}

void LearnedFrozenRetardationArtifact::validate() const {
    if (width < 3 || !(constant_ratio > 0.0) || !(power_ratio > 0.0) ||
        !std::isfinite(constant_ratio) || !std::isfinite(power_ratio) ||
        !std::isfinite(concentration_exponent) ||
        concentration_exponent <= -1.0 || concentration_exponent >= 1.0 ||
        !std::isfinite(training_maximum_relative_residual) ||
        !std::isfinite(heldout_maximum_relative_residual) ||
        training_maximum_relative_residual > 1.0e-10 ||
        heldout_maximum_relative_residual > 1.0e-10 ||
        training_checksum.size() != 16 || heldout_checksum.size() != 16 ||
        training_checksum == heldout_checksum) {
        throw std::invalid_argument("invalid learned frozen retardation artifact");
    }
}

void LearnedFrozenRetardationArtifact::write(
    const std::filesystem::path& path) const {
    validate();
    std::ofstream output(path);
    output << std::setprecision(17)
           << "SMAVE_LEARNED_FROZEN_RETARDATION 1\n"
           << "DISCRETE_OPERATOR_ID \"frozen-retardation-tridiagonal-v1\"\n"
           << "WIDTH " << width << '\n'
           << "CONSTANT_RATIO " << constant_ratio << '\n'
           << "POWER_RATIO " << power_ratio << '\n'
           << "CONCENTRATION_EXPONENT " << concentration_exponent << '\n'
           << "TRAINING_MAXIMUM_RELATIVE_RESIDUAL "
           << training_maximum_relative_residual << '\n'
           << "HELDOUT_MAXIMUM_RELATIVE_RESIDUAL "
           << heldout_maximum_relative_residual << '\n'
           << "TRAINING_CHECKSUM " << std::quoted(training_checksum) << '\n'
           << "HELDOUT_CHECKSUM " << std::quoted(heldout_checksum) << '\n'
           << "END\n";
    if (!output) throw std::runtime_error("cannot write retardation artifact");
}

LearnedFrozenRetardationArtifact LearnedFrozenRetardationArtifact::read(
    const std::filesystem::path& path) {
    std::ifstream input(path);
    std::string schema;
    std::getline(input, schema);
    if (schema != "SMAVE_LEARNED_FROZEN_RETARDATION 1") {
        throw std::invalid_argument("unsupported retardation artifact schema");
    }
    if (read_quoted(input, "DISCRETE_OPERATOR_ID") !=
        "frozen-retardation-tridiagonal-v1") {
        throw std::invalid_argument("retardation artifact operator id mismatch");
    }
    LearnedFrozenRetardationArtifact artifact;
    artifact.width = read_size(input, "WIDTH");
    require_tag(input, "CONSTANT_RATIO"); input >> artifact.constant_ratio;
    require_tag(input, "POWER_RATIO"); input >> artifact.power_ratio;
    require_tag(input, "CONCENTRATION_EXPONENT");
    input >> artifact.concentration_exponent;
    require_tag(input, "TRAINING_MAXIMUM_RELATIVE_RESIDUAL");
    input >> artifact.training_maximum_relative_residual;
    require_tag(input, "HELDOUT_MAXIMUM_RELATIVE_RESIDUAL");
    input >> artifact.heldout_maximum_relative_residual;
    artifact.training_checksum = read_quoted(input, "TRAINING_CHECKSUM");
    artifact.heldout_checksum = read_quoted(input, "HELDOUT_CHECKSUM");
    require_tag(input, "END");
    std::string trailing;
    if (!input || input >> trailing) {
        throw std::invalid_argument("invalid retardation artifact tail");
    }
    artifact.validate();
    return artifact;
}

LearnedFrozenRetardationArtifact fit_pdebench_frozen_retardation(
    const std::filesystem::path& training_prefix,
    const std::filesystem::path& heldout_prefix) {
    constexpr const char* operator_id = "frozen-retardation-tridiagonal-v1";
    const auto training = PdebenchTrainingManifest::read_and_verify(
        training_prefix, PdebenchTrainingUse::DirectDeployment, operator_id);
    const auto heldout = PdebenchTrainingManifest::read_and_verify(
        heldout_prefix, PdebenchTrainingUse::DirectDeployment, operator_id);
    if (training.family != "diffusion-sorption" ||
        heldout.family != training.family ||
        training.values_per_sample != heldout.values_per_sample ||
        training.dtype != "fp64" || heldout.dtype != "fp64") {
        throw std::invalid_argument("incompatible frozen retardation datasets");
    }
    const auto training_range = parse_source_range(training.source);
    const auto heldout_range = parse_source_range(heldout.source);
    if (!training_range || !heldout_range ||
        training_range->path != heldout_range->path ||
        !(training_range->sample_end <= heldout_range->sample_begin ||
          heldout_range->sample_end <= training_range->sample_begin)) {
        throw std::invalid_argument(
            "retardation training and heldout source ranges overlap");
    }
    const auto width = training.values_per_sample;
    const auto training_values = training.samples * width;
    const auto heldout_values = heldout.samples * width;
    const auto training_inputs = read_f64_tensor(
        training_prefix.string() + ".inputs.f64", training_values);
    const auto training_targets = read_f64_tensor(
        training_prefix.string() + ".targets.f64", training_values);
    const auto heldout_inputs = read_f64_tensor(
        heldout_prefix.string() + ".inputs.f64", heldout_values);
    const auto heldout_targets = read_f64_tensor(
        heldout_prefix.string() + ".targets.f64", heldout_values);

    auto fit_at_exponent = [&](double exponent) {
        long double aa{};
        long double ab{};
        long double bb{};
        long double ar{};
        long double br{};
        for (std::size_t sample = 0; sample < training.samples; ++sample) {
            const auto offset = sample * width;
            for (std::size_t index = 1; index < width; ++index) {
                const long double delta =
                    training_targets[offset + index] -
                    training_inputs[offset + index];
                const long double power = std::pow(std::max(
                    training_inputs[offset + index], 1.0e-8), exponent);
                long double laplacian = training_targets[offset + index] -
                    training_targets[offset + index - 1];
                if (index + 1 < width) {
                    laplacian += training_targets[offset + index] -
                        training_targets[offset + index + 1];
                }
                const long double response = -laplacian;
                const long double first = delta;
                const long double second = power * delta;
                aa += first * first;
                ab += first * second;
                bb += second * second;
                ar += first * response;
                br += second * response;
            }
        }
        const auto determinant = aa * bb - ab * ab;
        if (!(std::abs(determinant) > 0.0L)) {
            return std::array<double, 3>{INFINITY, INFINITY, INFINITY};
        }
        const auto constant = static_cast<double>((ar * bb - br * ab) / determinant);
        const auto power = static_cast<double>((br * aa - ar * ab) / determinant);
        const auto residual = retardation_relative_residual(
            training_inputs, training_targets, training.samples, width,
            constant, power, exponent);
        return std::array<double, 3>{residual, constant, power};
    };
    double best_exponent{};
    std::array<double, 3> best{INFINITY, 0.0, 0.0};
    double center = -0.125;
    double radius = 0.25;
    for (int refinement = 0; refinement < 6; ++refinement) {
        for (int candidate = 0; candidate <= 100; ++candidate) {
            const auto exponent = center - radius +
                2.0 * radius * static_cast<double>(candidate) / 100.0;
            const auto fit = fit_at_exponent(exponent);
            if (fit[0] < best[0] && fit[1] > 0.0 && fit[2] > 0.0) {
                best = fit;
                best_exponent = exponent;
            }
        }
        center = best_exponent;
        radius *= 0.05;
    }
    LearnedFrozenRetardationArtifact artifact;
    artifact.width = width;
    artifact.constant_ratio = best[1];
    artifact.power_ratio = best[2];
    artifact.concentration_exponent = best_exponent;
    artifact.training_maximum_relative_residual = best[0];
    artifact.heldout_maximum_relative_residual = retardation_relative_residual(
        heldout_inputs, heldout_targets, heldout.samples, width,
        artifact.constant_ratio, artifact.power_ratio,
        artifact.concentration_exponent);
    artifact.training_checksum = training.checksum;
    artifact.heldout_checksum = heldout.checksum;
    artifact.validate();
    return artifact;
}

std::vector<double> LearnedDarcyNearestArtifact::predict(
    const std::vector<double>& coefficient) const {
    if (coefficient.size() != width * width) {
        throw std::invalid_argument("Darcy nearest coefficient shape mismatch");
    }
    const auto features = darcy_features(
        coefficient.data(), width, feature_width);
    const auto feature_values = feature_width * feature_width;
    std::size_t best_index{};
    double best_distance = std::numeric_limits<double>::infinity();
    for (std::size_t prototype = 0; prototype < prototypes; ++prototype) {
        double distance{};
        const auto offset = prototype * feature_values;
        for (std::size_t feature = 0; feature < feature_values; ++feature) {
            const auto difference = features[feature] -
                prototype_features[offset + feature];
            distance += difference * difference;
        }
        if (distance < best_distance) {
            best_distance = distance;
            best_index = prototype;
        }
    }
    const auto offset = best_index * width * width;
    return std::vector<double>(
        prototype_solutions.begin() + static_cast<std::ptrdiff_t>(offset),
        prototype_solutions.begin() +
            static_cast<std::ptrdiff_t>(offset + width * width));
}

void LearnedDarcyNearestArtifact::validate() const {
    const auto feature_values = feature_width * feature_width;
    const auto solution_values = width * width;
    if (width < 4 || feature_width == 0 || width % feature_width != 0 ||
        prototypes == 0 || prototype_features.size() != prototypes * feature_values ||
        prototype_solutions.size() != prototypes * solution_values ||
        !std::all_of(prototype_features.begin(), prototype_features.end(),
                     [](double value) { return std::isfinite(value); }) ||
        !std::all_of(prototype_solutions.begin(), prototype_solutions.end(),
                     [](double value) { return std::isfinite(value); }) ||
        !std::isfinite(heldout_mean_relative_inf_error) ||
        !std::isfinite(heldout_maximum_relative_inf_error) ||
        !std::isfinite(heldout_maximum_relative_residual) ||
        training_checksum.size() != 16 || heldout_checksum.size() != 16 ||
        payload_checksum.size() != 16 || training_checksum == heldout_checksum) {
        throw std::invalid_argument("invalid learned Darcy nearest artifact");
    }
}

void LearnedDarcyNearestArtifact::write(
    const std::filesystem::path& prefix) const {
    validate();
    write_f64_tensor(prefix.string() + ".features.f64", prototype_features);
    write_f64_tensor(prefix.string() + ".solutions.f64", prototype_solutions);
    std::ofstream output(prefix.string() + ".manifest.txt");
    output << std::setprecision(17)
           << "SMAVE_LEARNED_DARCY_NEAREST 1\n"
           << "DISCRETE_OPERATOR_ID \"variable-darcy-five-point-v1\"\n"
           << "AUTHORITY \"warm-start-only\"\n"
           << "WIDTH " << width << '\n'
           << "FEATURE_WIDTH " << feature_width << '\n'
           << "PROTOTYPES " << prototypes << '\n'
           << "HELDOUT_MEAN_RELATIVE_INF_ERROR "
           << heldout_mean_relative_inf_error << '\n'
           << "HELDOUT_MAXIMUM_RELATIVE_INF_ERROR "
           << heldout_maximum_relative_inf_error << '\n'
           << "HELDOUT_MAXIMUM_RELATIVE_RESIDUAL "
           << heldout_maximum_relative_residual << '\n'
           << "TRAINING_CHECKSUM " << std::quoted(training_checksum) << '\n'
           << "HELDOUT_CHECKSUM " << std::quoted(heldout_checksum) << '\n'
           << "PAYLOAD_CHECKSUM " << std::quoted(payload_checksum) << '\n'
           << "END\n";
    if (!output) throw std::runtime_error("cannot write Darcy nearest manifest");
}

LearnedDarcyNearestArtifact LearnedDarcyNearestArtifact::read(
    const std::filesystem::path& prefix) {
    std::ifstream input(prefix.string() + ".manifest.txt");
    std::string schema;
    std::getline(input, schema);
    if (schema != "SMAVE_LEARNED_DARCY_NEAREST 1" ||
        read_quoted(input, "DISCRETE_OPERATOR_ID") !=
            "variable-darcy-five-point-v1" ||
        read_quoted(input, "AUTHORITY") != "warm-start-only") {
        throw std::invalid_argument("unsupported Darcy nearest artifact schema");
    }
    LearnedDarcyNearestArtifact artifact;
    artifact.width = read_size(input, "WIDTH");
    artifact.feature_width = read_size(input, "FEATURE_WIDTH");
    artifact.prototypes = read_size(input, "PROTOTYPES");
    require_tag(input, "HELDOUT_MEAN_RELATIVE_INF_ERROR");
    input >> artifact.heldout_mean_relative_inf_error;
    require_tag(input, "HELDOUT_MAXIMUM_RELATIVE_INF_ERROR");
    input >> artifact.heldout_maximum_relative_inf_error;
    require_tag(input, "HELDOUT_MAXIMUM_RELATIVE_RESIDUAL");
    input >> artifact.heldout_maximum_relative_residual;
    artifact.training_checksum = read_quoted(input, "TRAINING_CHECKSUM");
    artifact.heldout_checksum = read_quoted(input, "HELDOUT_CHECKSUM");
    artifact.payload_checksum = read_quoted(input, "PAYLOAD_CHECKSUM");
    require_tag(input, "END");
    std::string trailing;
    if (!input || input >> trailing) {
        throw std::invalid_argument("invalid Darcy nearest artifact tail");
    }
    const auto feature_values = artifact.prototypes *
        artifact.feature_width * artifact.feature_width;
    const auto solution_values = artifact.prototypes *
        artifact.width * artifact.width;
    const auto features_path =
        std::filesystem::path(prefix.string() + ".features.f64");
    const auto solutions_path =
        std::filesystem::path(prefix.string() + ".solutions.f64");
    artifact.prototype_features = read_f64_tensor(features_path, feature_values);
    artifact.prototype_solutions = read_f64_tensor(solutions_path, solution_values);
    if (compute_payload_checksum(features_path, solutions_path) !=
        artifact.payload_checksum) {
        throw std::invalid_argument("Darcy nearest payload checksum mismatch");
    }
    artifact.validate();
    return artifact;
}

LearnedDarcyNearestArtifact fit_pdebench_darcy_nearest(
    const std::filesystem::path& training_prefix,
    const std::filesystem::path& heldout_prefix,
    std::size_t feature_width) {
    constexpr const char* operator_id = "variable-darcy-five-point-v1";
    const auto training = PdebenchTrainingManifest::read_and_verify(
        training_prefix, PdebenchTrainingUse::DirectDeployment, operator_id);
    const auto heldout = PdebenchTrainingManifest::read_and_verify(
        heldout_prefix, PdebenchTrainingUse::DirectDeployment, operator_id);
    const auto width = static_cast<std::size_t>(std::llround(
        std::sqrt(static_cast<double>(training.values_per_sample))));
    if (training.family != "darcy" || heldout.family != "darcy" ||
        width * width != training.values_per_sample ||
        heldout.values_per_sample != training.values_per_sample ||
        feature_width == 0 || width % feature_width != 0 ||
        training.dtype != "fp64" || heldout.dtype != "fp64") {
        throw std::invalid_argument("incompatible Darcy nearest datasets");
    }
    const auto training_range = parse_source_range(training.source);
    const auto heldout_range = parse_source_range(heldout.source);
    if (!training_range || !heldout_range ||
        training_range->path != heldout_range->path ||
        !(training_range->sample_end <= heldout_range->sample_begin ||
          heldout_range->sample_end <= training_range->sample_begin)) {
        throw std::invalid_argument("Darcy training and heldout ranges overlap");
    }
    const auto training_values = training.samples * width * width;
    const auto heldout_values = heldout.samples * width * width;
    const auto training_inputs = read_f64_tensor(
        training_prefix.string() + ".inputs.f64", training_values);
    const auto training_targets = read_f64_tensor(
        training_prefix.string() + ".targets.f64", training_values);
    const auto heldout_inputs = read_f64_tensor(
        heldout_prefix.string() + ".inputs.f64", heldout_values);
    const auto heldout_targets = read_f64_tensor(
        heldout_prefix.string() + ".targets.f64", heldout_values);
    LearnedDarcyNearestArtifact artifact;
    artifact.width = width;
    artifact.feature_width = feature_width;
    artifact.prototypes = training.samples;
    artifact.prototype_solutions = training_targets;
    for (std::size_t sample = 0; sample < training.samples; ++sample) {
        const auto features = darcy_features(
            training_inputs.data() + sample * width * width,
            width, feature_width);
        artifact.prototype_features.insert(
            artifact.prototype_features.end(), features.begin(), features.end());
    }
    artifact.training_checksum = training.checksum;
    artifact.heldout_checksum = heldout.checksum;
    const auto features_path = std::filesystem::path(
        training_prefix.string() + ".darcy-nearest-features.tmp");
    const auto solutions_path = std::filesystem::path(
        training_prefix.string() + ".darcy-nearest-solutions.tmp");
    write_f64_tensor(features_path, artifact.prototype_features);
    write_f64_tensor(solutions_path, artifact.prototype_solutions);
    artifact.payload_checksum = compute_payload_checksum(features_path, solutions_path);
    std::filesystem::remove(features_path);
    std::filesystem::remove(solutions_path);
    double error_sum{};
    for (std::size_t sample = 0; sample < heldout.samples; ++sample) {
        const std::vector<double> coefficient(
            heldout_inputs.begin() + static_cast<std::ptrdiff_t>(sample * width * width),
            heldout_inputs.begin() + static_cast<std::ptrdiff_t>((sample + 1) * width * width));
        const auto prediction = artifact.predict(coefficient);
        const auto* target = heldout_targets.data() + sample * width * width;
        const auto error = relative_inf_error(
            prediction.data(), target, width * width);
        error_sum += error;
        artifact.heldout_maximum_relative_inf_error = std::max(
            artifact.heldout_maximum_relative_inf_error, error);
        artifact.heldout_maximum_relative_residual = std::max(
            artifact.heldout_maximum_relative_residual,
            darcy_relative_residual(
                coefficient.data(), prediction.data(), width));
    }
    artifact.heldout_mean_relative_inf_error =
        error_sum / static_cast<double>(heldout.samples);
    artifact.validate();
    return artifact;
}

void LearnedPeriodicHelmholtzArtifact::validate() const {
    validate_identifier(family, "FAMILY");
    validate_identifier(discrete_operator_id, "DISCRETE_OPERATOR_ID");
    if (width < 2 || (width & (width - 1)) != 0 ||
        !(stencil_number >= 0.0) || !std::isfinite(stencil_number) ||
        !std::isfinite(training_maximum_relative_residual) ||
        !std::isfinite(heldout_maximum_relative_residual) ||
        training_maximum_relative_residual > 1.0e-12 ||
        heldout_maximum_relative_residual > 1.0e-12 ||
        training_checksum.size() != 16 || heldout_checksum.size() != 16 ||
        training_checksum == heldout_checksum) {
        throw std::invalid_argument("invalid learned periodic Helmholtz artifact");
    }
}

void LearnedPeriodicHelmholtzArtifact::write(
    const std::filesystem::path& path) const {
    validate();
    std::ofstream output(path);
    output << std::setprecision(17)
           << "SMAVE_LEARNED_PERIODIC_HELMHOLTZ 1\n"
           << "FAMILY " << std::quoted(family) << '\n'
           << "DISCRETE_OPERATOR_ID " << std::quoted(discrete_operator_id) << '\n'
           << "WIDTH " << width << '\n'
           << "STENCIL_NUMBER " << stencil_number << '\n'
           << "TRAINING_MAXIMUM_RELATIVE_RESIDUAL "
           << training_maximum_relative_residual << '\n'
           << "HELDOUT_MAXIMUM_RELATIVE_RESIDUAL "
           << heldout_maximum_relative_residual << '\n'
           << "TRAINING_CHECKSUM " << std::quoted(training_checksum) << '\n'
           << "HELDOUT_CHECKSUM " << std::quoted(heldout_checksum) << '\n'
           << "END\n";
    if (!output) throw std::runtime_error("cannot write Helmholtz artifact");
}

LearnedPeriodicHelmholtzArtifact LearnedPeriodicHelmholtzArtifact::read(
    const std::filesystem::path& path) {
    std::ifstream input(path);
    std::string schema;
    std::getline(input, schema);
    if (schema != "SMAVE_LEARNED_PERIODIC_HELMHOLTZ 1") {
        throw std::invalid_argument("unsupported periodic Helmholtz artifact schema");
    }
    LearnedPeriodicHelmholtzArtifact artifact;
    artifact.family = read_quoted(input, "FAMILY");
    artifact.discrete_operator_id = read_quoted(input, "DISCRETE_OPERATOR_ID");
    artifact.width = read_size(input, "WIDTH");
    require_tag(input, "STENCIL_NUMBER"); input >> artifact.stencil_number;
    require_tag(input, "TRAINING_MAXIMUM_RELATIVE_RESIDUAL");
    input >> artifact.training_maximum_relative_residual;
    require_tag(input, "HELDOUT_MAXIMUM_RELATIVE_RESIDUAL");
    input >> artifact.heldout_maximum_relative_residual;
    artifact.training_checksum = read_quoted(input, "TRAINING_CHECKSUM");
    artifact.heldout_checksum = read_quoted(input, "HELDOUT_CHECKSUM");
    require_tag(input, "END");
    std::string trailing;
    if (!input || input >> trailing) {
        throw std::invalid_argument("invalid periodic Helmholtz artifact tail");
    }
    artifact.validate();
    return artifact;
}

LearnedPeriodicHelmholtzArtifact fit_pdebench_periodic_helmholtz(
    const std::filesystem::path& training_prefix,
    const std::filesystem::path& heldout_prefix,
    const std::string& expected_family,
    const std::string& expected_operator_id) {
    const auto training = PdebenchTrainingManifest::read_and_verify(
        training_prefix, PdebenchTrainingUse::DirectDeployment,
        expected_operator_id);
    const auto heldout = PdebenchTrainingManifest::read_and_verify(
        heldout_prefix, PdebenchTrainingUse::DirectDeployment,
        expected_operator_id);
    const auto width = static_cast<std::size_t>(std::llround(
        std::sqrt(static_cast<double>(training.values_per_sample))));
    if (training.family != expected_family || heldout.family != expected_family ||
        width * width != training.values_per_sample ||
        heldout.values_per_sample != training.values_per_sample ||
        training.dtype != "fp64" || heldout.dtype != "fp64") {
        throw std::invalid_argument("incompatible periodic Helmholtz datasets");
    }
    const auto training_range = parse_source_range(training.source);
    const auto heldout_range = parse_source_range(heldout.source);
    if (!training_range || !heldout_range ||
        training_range->path != heldout_range->path ||
        !(training_range->sample_end <= heldout_range->sample_begin ||
          heldout_range->sample_end <= training_range->sample_begin)) {
        throw std::invalid_argument("Helmholtz training and heldout ranges overlap");
    }
    const auto training_values = training.samples * width * width;
    const auto heldout_values = heldout.samples * width * width;
    const auto training_inputs = read_f64_tensor(
        training_prefix.string() + ".inputs.f64", training_values);
    const auto training_targets = read_f64_tensor(
        training_prefix.string() + ".targets.f64", training_values);
    const auto heldout_inputs = read_f64_tensor(
        heldout_prefix.string() + ".inputs.f64", heldout_values);
    const auto heldout_targets = read_f64_tensor(
        heldout_prefix.string() + ".targets.f64", heldout_values);
    long double basis_squared{};
    long double basis_response{};
    for (std::size_t sample = 0; sample < training.samples; ++sample) {
        const auto offset = sample * width * width;
        for (std::size_t row = 0; row < width; ++row) {
            const auto south = row == 0 ? width - 1 : row - 1;
            const auto north = row + 1 == width ? 0 : row + 1;
            for (std::size_t column = 0; column < width; ++column) {
                const auto west = column == 0 ? width - 1 : column - 1;
                const auto east = column + 1 == width ? 0 : column + 1;
                const auto index = row * width + column;
                const long double center = training_targets[offset + index];
                const long double basis = 4.0L * center -
                    training_targets[offset + row * width + west] -
                    training_targets[offset + row * width + east] -
                    training_targets[offset + south * width + column] -
                    training_targets[offset + north * width + column];
                const long double response =
                    training_inputs[offset + index] - center;
                basis_squared += basis * basis;
                basis_response += basis * response;
            }
        }
    }
    if (!(basis_squared > 0.0L)) {
        throw std::invalid_argument("singular periodic Helmholtz fit");
    }
    LearnedPeriodicHelmholtzArtifact artifact;
    artifact.family = expected_family;
    artifact.discrete_operator_id = expected_operator_id;
    artifact.width = width;
    artifact.stencil_number = static_cast<double>(basis_response / basis_squared);
    artifact.training_maximum_relative_residual =
        periodic_helmholtz_relative_residual(
            training_inputs, training_targets, training.samples,
            width, artifact.stencil_number);
    artifact.heldout_maximum_relative_residual =
        periodic_helmholtz_relative_residual(
            heldout_inputs, heldout_targets, heldout.samples,
            width, artifact.stencil_number);
    artifact.training_checksum = training.checksum;
    artifact.heldout_checksum = heldout.checksum;
    artifact.validate();
    return artifact;
}

}  // namespace smave
