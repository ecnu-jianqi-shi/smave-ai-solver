#pragma once

#include "smave/expression.hpp"
#include "smave/ir.hpp"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace smave {

struct LinearSystem {
    std::vector<std::string> unknowns;
    std::vector<std::vector<double>> matrix;
    SparsityPattern sparsity;
    std::vector<double> sparse_values;
    std::vector<double> right_hand_side;
    bool symmetric{false};
    bool positive_definite{false};
    double diagonal_condition_estimate{0.0};
    double coefficient_dynamic_range{1.0};
    double row_nonzero_coefficient_of_variation{0.0};
    double row_l1_condition_estimate{1.0};
    double diagonal_dominance_fraction{0.0};
    double mean_diagonal_row_l1_fraction{0.0};
    double normalized_mean_bandwidth{0.0};

    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] bool has_dense_matrix() const;
    [[nodiscard]] bool has_sparse_matrix() const;
    [[nodiscard]] std::size_t nonzeros() const;
    [[nodiscard]] std::size_t dense_storage_bytes() const;
    [[nodiscard]] std::size_t sparse_storage_bytes() const;
    [[nodiscard]] double coefficient(std::size_t row, std::size_t column) const;
    [[nodiscard]] std::vector<double> multiply(
        const std::vector<double>& vector) const;
    [[nodiscard]] std::vector<double> multiply_transpose(
        const std::vector<double>& vector) const;
};

void classify_linear_system(
    LinearSystem& system,
    double symmetry_relative_tolerance = 1.0e-12);

struct KrylovResult {
    bool converged{false};
    bool breakdown{false};
    bool stagnated{false};
    std::vector<double> solution;
    std::vector<double> residual_history;
    int iterations{};
    std::string reason;
};

struct BatchedKrylovResult {
    bool valid{false};
    std::vector<double> solution;
    std::vector<int> iterations;
    std::vector<double> residual_inf;
    std::vector<bool> converged;
    std::string reason;
};

struct SparseDirectResult {
    bool solved{false};
    std::vector<double> solution;
    std::vector<std::size_t> column_order;
    std::size_t row_swaps{0};
    std::size_t initial_nonzeros{0};
    std::size_t upper_nonzeros{0};
    std::size_t ordering_fill_edges{0};
    std::size_t natural_fill_edges{0};
    double minimum_scaled_pivot{0.0};
};

struct IndustrialSparseDirectResult {
    bool available{false};
    bool solved{false};
    std::string backend;
    std::string reason;
    std::vector<double> solution;
    std::size_t matrix_nonzeros{0};
    double residual_inf{0.0};
};

struct SparseSpdDirectResult {
    bool available{false};
    bool solved{false};
    std::string backend;
    std::string reason;
    std::vector<double> solution;
    std::size_t matrix_nonzeros{};
    double factor_seconds{};
    double solve_seconds{};
    double gate_seconds{};
    double residual_inf{};
};

struct StructuredDirectResult {
    bool eligible{false};
    bool solved{false};
    bool periodic{false};
    std::string backend;
    std::string reason;
    std::vector<double> solution;
    double residual_inf{};
};

class PeriodicTridiagonalFactorization {
public:
    PeriodicTridiagonalFactorization(
        std::size_t size,
        double diagonal,
        double off_diagonal);

    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] bool valid() const;
    [[nodiscard]] bool solve(
        const std::vector<double>& right_hand_side,
        std::vector<double>& solution) const;
    [[nodiscard]] bool solve_batch(
        const std::vector<double>& right_hand_sides,
        std::size_t batch,
        std::vector<double>& solutions) const;

private:
    [[nodiscard]] bool solve_nonperiodic(
        const double* right_hand_side,
        double* solution,
        double* workspace) const;

    std::size_t size_{};
    double lower_{};
    double upper_{};
    double corner_upper_{};
    double corner_lower_{};
    double gamma_{};
    std::vector<double> inverse_pivots_;
    std::vector<double> upper_factors_;
    std::vector<double> correction_;
    double correction_denominator_{};
    bool valid_{};
};

class PeriodicLowerBidiagonalFactorization {
public:
    PeriodicLowerBidiagonalFactorization(
        std::size_t size,
        double diagonal,
        double lower);

    [[nodiscard]] bool valid() const;
    [[nodiscard]] bool solve(
        const std::vector<double>& right_hand_side,
        std::vector<double>& solution) const;
    [[nodiscard]] bool solve_interleaved(
        const std::vector<double>& right_hand_sides,
        std::size_t batch,
        std::vector<double>& solutions) const;

private:
    std::size_t size_{};
    double inverse_diagonal_{};
    double lower_{};
    std::vector<double> wrap_coefficients_;
    double wrap_denominator_{};
    bool valid_{};
};

[[nodiscard]] bool periodic_lower_bidiagonal_relative_residual_interleaved(
    const std::vector<double>& right_hand_sides,
    const std::vector<double>& solutions,
    std::size_t batch,
    double diagonal,
    double lower,
    std::vector<double>& relative_residuals);

[[nodiscard]] bool frozen_burgers_relative_residual_interleaved(
    const std::vector<double>& states,
    const std::vector<double>& solutions,
    std::size_t batch,
    double diffusion_number,
    double convection_scale,
    std::vector<double>& relative_residuals);

class VariableTridiagonalWorkspace {
public:
    explicit VariableTridiagonalWorkspace(std::size_t size);

    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] bool solve(
        const std::vector<double>& lower,
        const std::vector<double>& diagonal,
        const std::vector<double>& upper,
        const std::vector<double>& right_hand_side,
        std::vector<double>& solution);
    [[nodiscard]] bool solve_strictly_diagonally_dominant_m_matrix(
        const std::vector<double>& lower,
        const std::vector<double>& diagonal,
        const std::vector<double>& upper,
        const std::vector<double>& right_hand_side,
        std::vector<double>& solution);
    [[nodiscard]] bool solve_strictly_diagonally_dominant_m_matrix_constant_off_diagonal(
        double lower,
        const std::vector<double>& diagonal,
        double upper,
        const std::vector<double>& right_hand_side,
        std::vector<double>& solution);
    [[nodiscard]] bool solve_cyclic(
        const std::vector<double>& lower,
        const std::vector<double>& diagonal,
        const std::vector<double>& upper,
        double top_right,
        double bottom_left,
        const std::vector<double>& right_hand_side,
        std::vector<double>& solution);
    [[nodiscard]] bool solve_cyclic_constant_diagonal(
        const std::vector<double>& lower,
        double diagonal,
        const std::vector<double>& upper,
        double top_right,
        double bottom_left,
        const std::vector<double>& right_hand_side,
        std::vector<double>& solution);
    [[nodiscard]] bool solve_cyclic_constant_diagonal_affine_off_diagonal(
        double diagonal,
        double lower_offset,
        double lower_state_scale,
        double upper_offset,
        double upper_state_scale,
        const std::vector<double>& state,
        const std::vector<double>& right_hand_side,
        std::vector<double>& solution);

private:
    [[nodiscard]] bool solve_impl(
        const std::vector<double>& lower,
        const std::vector<double>& diagonal,
        const std::vector<double>& upper,
        const std::vector<double>& right_hand_side,
        std::vector<double>& solution);
    [[nodiscard]] bool solve_constant_diagonal_impl(
        const std::vector<double>& lower,
        double diagonal,
        double first_diagonal,
        double last_diagonal,
        const std::vector<double>& upper,
        const std::vector<double>& right_hand_side,
        std::vector<double>& solution);

    std::size_t size_{};
    std::vector<double> inverse_pivots_;
    std::vector<double> modified_upper_;
    std::vector<double> modified_right_hand_side_;
    std::vector<double> modified_diagonal_;
    std::vector<double> primary_;
    std::vector<double> update_;
    std::vector<double> correction_;
};

class BatchedVariableTridiagonalWorkspace {
public:
    BatchedVariableTridiagonalWorkspace(std::size_t size, std::size_t batch);

    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] std::size_t batch() const;
    [[nodiscard]] bool solve_strictly_diagonally_dominant_m_matrix_interleaved(
        const std::vector<double>& lower,
        const std::vector<double>& diagonal,
        const std::vector<double>& upper,
        const std::vector<double>& right_hand_side,
        std::vector<double>& solution);
    [[nodiscard]] bool
    solve_strictly_diagonally_dominant_m_matrix_constant_off_diagonal_interleaved(
        double lower,
        const std::vector<double>& diagonal,
        double upper,
        const std::vector<double>& right_hand_side,
        std::vector<double>& solution);
    [[nodiscard]] bool
    solve_cyclic_constant_diagonal_affine_off_diagonal_interleaved(
        double diagonal,
        double lower_offset,
        double lower_state_scale,
        double upper_offset,
        double upper_state_scale,
        const std::vector<double>& state,
        const std::vector<double>& right_hand_side,
        std::vector<double>& solution);

private:
    std::size_t size_{};
    std::size_t batch_{};
    std::vector<double> inverse_pivots_;
    std::vector<double> modified_upper_;
    std::vector<double> modified_right_hand_side_;
    std::vector<double> correction_;
    std::vector<double> factors_;
};

class BatchedFivePointSsorPcgWorkspace {
public:
    BatchedFivePointSsorPcgWorkspace(
        std::size_t grid_width,
        std::size_t batch);

    [[nodiscard]] std::size_t grid_width() const;
    [[nodiscard]] std::size_t batch() const;
    [[nodiscard]] BatchedKrylovResult solve_interleaved(
        const std::vector<double>& west,
        const std::vector<double>& east,
        const std::vector<double>& south,
        const std::vector<double>& north,
        const std::vector<double>& diagonal,
        const std::vector<double>& right_hand_side,
        double relaxation,
        double absolute_tolerance,
        double relative_tolerance,
        int maximum_iterations);

private:
    std::size_t grid_width_{};
    std::size_t batch_{};
    std::vector<double> residual_;
    std::vector<double> preconditioned_;
    std::vector<double> direction_;
    std::vector<double> matrix_direction_;
    std::vector<double> forward_;
};

class AggregationMultigrid2D {
public:
    AggregationMultigrid2D(
        const LinearSystem& system,
        std::size_t grid_width,
        std::size_t maximum_coarse_unknowns = 16,
        std::size_t pre_smoothing_steps = 1,
        std::size_t post_smoothing_steps = 1,
        double smoothing_weight = 2.0 / 3.0);
    ~AggregationMultigrid2D();
    AggregationMultigrid2D(AggregationMultigrid2D&&) noexcept;
    AggregationMultigrid2D& operator=(AggregationMultigrid2D&&) noexcept;
    AggregationMultigrid2D(const AggregationMultigrid2D&) = delete;
    AggregationMultigrid2D& operator=(const AggregationMultigrid2D&) = delete;

    [[nodiscard]] bool valid() const;
    [[nodiscard]] const std::string& reason() const;
    [[nodiscard]] std::size_t levels() const;
    [[nodiscard]] std::size_t storage_bytes() const;
    [[nodiscard]] bool apply(
        const std::vector<double>& residual,
        std::vector<double>& correction) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

class GeometricFivePointMultigrid2D {
public:
    GeometricFivePointMultigrid2D(
        std::size_t grid_width,
        const std::vector<double>& west,
        const std::vector<double>& east,
        const std::vector<double>& south,
        const std::vector<double>& north,
        const std::vector<double>& diagonal,
        std::size_t maximum_coarse_unknowns = 16,
        std::size_t pre_smoothing_steps = 1,
        std::size_t post_smoothing_steps = 1,
        double smoothing_weight = 2.0 / 3.0);
    ~GeometricFivePointMultigrid2D();
    GeometricFivePointMultigrid2D(
        GeometricFivePointMultigrid2D&&) noexcept;
    GeometricFivePointMultigrid2D& operator=(
        GeometricFivePointMultigrid2D&&) noexcept;
    GeometricFivePointMultigrid2D(
        const GeometricFivePointMultigrid2D&) = delete;
    GeometricFivePointMultigrid2D& operator=(
        const GeometricFivePointMultigrid2D&) = delete;

    [[nodiscard]] bool valid() const;
    [[nodiscard]] const std::string& reason() const;
    [[nodiscard]] std::size_t levels() const;
    [[nodiscard]] std::size_t storage_bytes() const;
    [[nodiscard]] bool apply(
        const std::vector<double>& residual,
        std::vector<double>& correction) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

struct AggregationAmgPcgResult {
    bool eligible{false};
    bool solved{false};
    std::vector<double> solution;
    std::string backend{"pcg-aggregation-amg-cpu-v1"};
    std::string reason;
    std::size_t grid_width{};
    std::size_t levels{};
    std::size_t storage_bytes{};
    int iterations{};
    double residual_inf{};
};

[[nodiscard]] bool aggregation_amg_five_point_eligible(
    const LinearSystem& system,
    std::size_t* grid_width = nullptr,
    std::string* reason = nullptr);

[[nodiscard]] AggregationAmgPcgResult aggregation_amg_pcg_solve(
    const LinearSystem& system,
    double absolute_tolerance = 1.0e-12,
    double relative_tolerance = 1.0e-10,
    int maximum_iterations = 2000);

[[nodiscard]] bool tridiagonal_direct_solve(
    const std::vector<double>& lower,
    const std::vector<double>& diagonal,
    const std::vector<double>& upper,
    const std::vector<double>& right_hand_side,
    std::vector<double>& solution);

[[nodiscard]] bool cyclic_tridiagonal_direct_solve(
    const std::vector<double>& lower,
    const std::vector<double>& diagonal,
    const std::vector<double>& upper,
    double top_right,
    double bottom_left,
    const std::vector<double>& right_hand_side,
    std::vector<double>& solution);

[[nodiscard]] StructuredDirectResult structured_tridiagonal_direct_solve(
    const LinearSystem& system,
    double structural_zero_tolerance = 0.0);

using Preconditioner = std::function<bool(
    const std::vector<double>& residual,
    std::vector<double>& result)>;
using LinearOperator = std::function<bool(
    const std::vector<double>& input,
    std::vector<double>& output)>;

[[nodiscard]] LinearSystem assemble_linear_system(
    const ModelIR& model,
    const BlockIR& block,
    const std::unordered_map<std::string, Expression>& residuals,
    const std::unordered_map<std::string, double>& context);

void update_linear_right_hand_side(
    LinearSystem& system,
    const ModelIR& model,
    const BlockIR& block,
    const std::unordered_map<std::string, Expression>& residuals,
    const std::unordered_map<std::string, double>& context);

[[nodiscard]] bool dense_direct_solve(
    const LinearSystem& system,
    std::vector<double>& solution);

[[nodiscard]] SparseDirectResult sparse_ordered_threshold_pivot_solve(
    const LinearSystem& system,
    double pivot_threshold = 0.1);

[[nodiscard]] bool industrial_sparse_direct_available();
[[nodiscard]] std::string industrial_sparse_direct_backend();
[[nodiscard]] IndustrialSparseDirectResult industrial_sparse_direct_solve(
    const LinearSystem& system);

[[nodiscard]] SparseSpdDirectResult accelerate_sparse_spd_direct_solve(
    const LinearSystem& system);
[[nodiscard]] SparseSpdDirectResult accelerate_spd_band_direct_solve(
    const LinearSystem& system,
    std::size_t half_bandwidth);
[[nodiscard]] SparseSpdDirectResult accelerate_five_point_spd_direct_solve(
    std::size_t width,
    const std::vector<double>& west,
    const std::vector<double>& east,
    const std::vector<double>& south,
    const std::vector<double>& north,
    const std::vector<double>& diagonal,
    const std::vector<double>& right_hand_side);

[[nodiscard]] bool superlu_sparse_direct_available();
[[nodiscard]] std::string superlu_sparse_direct_backend();
[[nodiscard]] IndustrialSparseDirectResult superlu_sparse_direct_solve(
    const LinearSystem& system);

[[nodiscard]] Preconditioner jacobi_preconditioner(const LinearSystem& system);

[[nodiscard]] Preconditioner symmetric_gauss_seidel_preconditioner(
    const LinearSystem& system,
    double relaxation = 1.0);

[[nodiscard]] Preconditioner incomplete_cholesky_zero_preconditioner(
    const LinearSystem& system,
    const SparsityPattern& sparsity);

[[nodiscard]] Preconditioner incomplete_lu_zero_preconditioner(
    const LinearSystem& system,
    const SparsityPattern& sparsity);

[[nodiscard]] Preconditioner incomplete_lu_threshold_preconditioner(
    const LinearSystem& system,
    double drop_tolerance,
    std::size_t maximum_entries_per_triangle);

[[nodiscard]] KrylovResult preconditioned_conjugate_gradient(
    const LinearSystem& system,
    const std::vector<double>& initial,
    const Preconditioner& preconditioner,
    double absolute_tolerance,
    double relative_tolerance,
    int maximum_iterations);

[[nodiscard]] KrylovResult preconditioned_conjugate_gradient(
    std::size_t size,
    const LinearOperator& linear_operator,
    const std::vector<double>& right_hand_side,
    const std::vector<double>& initial,
    const Preconditioner& preconditioner,
    double absolute_tolerance,
    double relative_tolerance,
    int maximum_iterations);

[[nodiscard]] KrylovResult restarted_gmres(
    const LinearSystem& system,
    const std::vector<double>& initial,
    const Preconditioner& preconditioner,
    double absolute_tolerance,
    double relative_tolerance,
    int maximum_iterations,
    int restart_dimension = 20);

[[nodiscard]] KrylovResult restarted_gmres(
    std::size_t size,
    const LinearOperator& linear_operator,
    const std::vector<double>& right_hand_side,
    const std::vector<double>& initial,
    const Preconditioner& preconditioner,
    double absolute_tolerance,
    double relative_tolerance,
    int maximum_iterations,
    int restart_dimension = 20);

[[nodiscard]] KrylovResult least_squares_qr(
    const LinearSystem& system,
    const std::vector<double>& initial,
    double absolute_tolerance,
    double relative_tolerance,
    int maximum_iterations);

[[nodiscard]] std::unordered_map<std::string, double> linear_solution_values(
    const LinearSystem& system,
    const std::vector<double>& solution,
    const std::unordered_map<std::string, double>& context);

}  // namespace smave
