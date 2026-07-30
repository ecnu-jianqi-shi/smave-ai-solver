#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace smave {

inline constexpr const char* kPdebenchTrainingSetSchemaVersion =
    "SMAVE_PDEBENCH_TRAINING_SET 1";

enum class PdebenchTrainingUse {
    Pretraining,
    SolverLabelTraining,
    DirectDeployment,
};

struct PdebenchTrainingManifest {
    std::string family;
    std::filesystem::path source;
    std::size_t samples{};
    std::size_t values_per_sample{};
    std::string target_kind;
    bool solver_label{};
    std::string discrete_operator_id;
    bool original_residual_certified{};
    std::string dtype;
    std::string layout;
    std::string checksum;

    [[nodiscard]] std::size_t tensor_bytes() const;
    void validate() const;
    void validate_for(
        PdebenchTrainingUse use,
        const std::string& expected_operator_id = {}) const;

    static PdebenchTrainingManifest read(
        const std::filesystem::path& manifest_path);
    static PdebenchTrainingManifest read_and_verify(
        const std::filesystem::path& prefix,
        PdebenchTrainingUse use,
        const std::string& expected_operator_id = {});
};

struct LearnedPeriodicRecurrenceArtifact {
    std::string family;
    std::string discrete_operator_id;
    std::size_t width{};
    double inverse_diagonal{};
    double feedback{};
    double training_maximum_relative_residual{};
    double heldout_maximum_relative_residual{};
    std::string training_checksum;
    std::string heldout_checksum;

    void validate() const;
    void write(const std::filesystem::path& path) const;
    static LearnedPeriodicRecurrenceArtifact read(
        const std::filesystem::path& path);
};

[[nodiscard]] LearnedPeriodicRecurrenceArtifact
fit_pdebench_periodic_recurrence(
    const std::filesystem::path& training_prefix,
    const std::filesystem::path& heldout_prefix,
    const std::string& expected_operator_id);

struct LearnedFrozenBurgersArtifact {
    std::size_t width{};
    double diffusion_number{};
    double convection_scale{};
    double training_maximum_relative_residual{};
    double heldout_maximum_relative_residual{};
    std::string training_checksum;
    std::string heldout_checksum;

    void validate() const;
    void write(const std::filesystem::path& path) const;
    static LearnedFrozenBurgersArtifact read(
        const std::filesystem::path& path);
};

[[nodiscard]] LearnedFrozenBurgersArtifact fit_pdebench_frozen_burgers(
    const std::filesystem::path& training_prefix,
    const std::filesystem::path& heldout_prefix);

struct LearnedFrozenRetardationArtifact {
    std::size_t width{};
    double constant_ratio{};
    double power_ratio{};
    double concentration_exponent{};
    double training_maximum_relative_residual{};
    double heldout_maximum_relative_residual{};
    std::string training_checksum;
    std::string heldout_checksum;

    void validate() const;
    void write(const std::filesystem::path& path) const;
    static LearnedFrozenRetardationArtifact read(
        const std::filesystem::path& path);
};

[[nodiscard]] LearnedFrozenRetardationArtifact
fit_pdebench_frozen_retardation(
    const std::filesystem::path& training_prefix,
    const std::filesystem::path& heldout_prefix);

struct LearnedDarcyNearestArtifact {
    std::size_t width{};
    std::size_t feature_width{};
    std::size_t prototypes{};
    std::vector<double> prototype_features;
    std::vector<double> prototype_solutions;
    double heldout_mean_relative_inf_error{};
    double heldout_maximum_relative_inf_error{};
    double heldout_maximum_relative_residual{};
    std::string training_checksum;
    std::string heldout_checksum;
    std::string payload_checksum;

    [[nodiscard]] std::vector<double> predict(
        const std::vector<double>& coefficient) const;
    void validate() const;
    void write(const std::filesystem::path& prefix) const;
    static LearnedDarcyNearestArtifact read(
        const std::filesystem::path& prefix);
};

[[nodiscard]] LearnedDarcyNearestArtifact fit_pdebench_darcy_nearest(
    const std::filesystem::path& training_prefix,
    const std::filesystem::path& heldout_prefix,
    std::size_t feature_width = 4);

struct LearnedPeriodicHelmholtzArtifact {
    std::string family;
    std::string discrete_operator_id;
    std::size_t width{};
    double stencil_number{};
    double training_maximum_relative_residual{};
    double heldout_maximum_relative_residual{};
    std::string training_checksum;
    std::string heldout_checksum;

    void validate() const;
    void write(const std::filesystem::path& path) const;
    static LearnedPeriodicHelmholtzArtifact read(
        const std::filesystem::path& path);
};

[[nodiscard]] LearnedPeriodicHelmholtzArtifact
fit_pdebench_periodic_helmholtz(
    const std::filesystem::path& training_prefix,
    const std::filesystem::path& heldout_prefix,
    const std::string& expected_family,
    const std::string& expected_operator_id);

}  // namespace smave
