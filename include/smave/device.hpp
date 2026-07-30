#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <memory>

namespace smave {

struct DeviceExecutionResult {
    bool available{false};
    bool executed{false};
    bool verified{false};
    std::string backend;
    std::string device_name;
    std::string reason;
    std::vector<float> output;
    std::size_t batch{};
    std::size_t input_width{};
    std::size_t output_width{};
    double upload_us{};
    double kernel_us{};
    double download_us{};
    double maximum_absolute_error{};
    double maximum_relative_error{};
};

struct StructuredDeviceSolveResult {
    bool available{false};
    bool executed{false};
    bool verified{false};
    std::string backend;
    std::string device_name;
    std::string reason;
    std::vector<double> output;
    std::size_t batch{};
    std::size_t width{};
    double setup_us{};
    double kernel_us{};
};

struct MetalStencilSolveResult {
    bool available{false};
    bool executed{false};
    bool verified{false};
    std::string backend;
    std::string device_name;
    std::string reason;
    std::vector<double> output;
    std::size_t batch{};
    std::size_t width{};
    std::size_t iterations{};
    double setup_us{};
    double kernel_us{};
    double download_us{};
    double maximum_relative_residual{};
};

class AcceleratePeriodicHelmholtz2DPlan {
public:
    AcceleratePeriodicHelmholtz2DPlan(
        std::size_t width,
        double diffusion_number);
    ~AcceleratePeriodicHelmholtz2DPlan();
    AcceleratePeriodicHelmholtz2DPlan(
        AcceleratePeriodicHelmholtz2DPlan&&) noexcept;
    AcceleratePeriodicHelmholtz2DPlan& operator=(
        AcceleratePeriodicHelmholtz2DPlan&&) noexcept;
    AcceleratePeriodicHelmholtz2DPlan(
        const AcceleratePeriodicHelmholtz2DPlan&) = delete;
    AcceleratePeriodicHelmholtz2DPlan& operator=(
        const AcceleratePeriodicHelmholtz2DPlan&) = delete;

    [[nodiscard]] bool available() const;
    [[nodiscard]] const std::string& reason() const;
    [[nodiscard]] double setup_us() const;
    [[nodiscard]] bool solve(
        const std::vector<double>& right_hand_side,
        std::vector<double>& solution,
        double* kernel_us = nullptr);
    [[nodiscard]] bool solve_batch(
        const std::vector<double>& right_hand_sides,
        std::size_t batch,
        std::vector<double>& solutions,
        double* kernel_us = nullptr);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

[[nodiscard]] bool metal_gpu_available();
[[nodiscard]] std::string metal_gpu_device_name();

[[nodiscard]] DeviceExecutionResult metal_gpu_affine_batch(
    const std::vector<float>& inputs,
    std::size_t batch,
    std::size_t input_width,
    const std::vector<float>& weights,
    std::size_t output_width,
    const std::vector<float>& bias,
    double absolute_tolerance = 1.0e-5,
    double relative_tolerance = 1.0e-5);

[[nodiscard]] MetalStencilSolveResult metal_weighted_jacobi_2d_batch(
    const std::vector<double>& west,
    const std::vector<double>& east,
    const std::vector<double>& south,
    const std::vector<double>& north,
    const std::vector<double>& inverse_diagonal,
    const std::vector<double>& right_hand_sides,
    std::size_t batch,
    std::size_t width,
    std::size_t iterations,
    double relaxation,
    double residual_tolerance = 1.0e-6);

[[nodiscard]] MetalStencilSolveResult metal_frozen_burgers_1d_batch(
    const std::vector<double>& states,
    std::size_t batch,
    std::size_t width,
    double diffusion_number,
    double convection_scale,
    std::size_t iterations,
    double relaxation,
    double residual_tolerance = 1.0e-10);

[[nodiscard]] MetalStencilSolveResult metal_frozen_retardation_1d_batch(
    const std::vector<double>& states,
    std::size_t batch,
    std::size_t width,
    double constant_ratio,
    double power_ratio,
    double concentration_exponent,
    std::size_t iterations,
    double relaxation,
    double residual_tolerance = 1.0e-10);

[[nodiscard]] bool coreml_neural_engine_available();

[[nodiscard]] DeviceExecutionResult coreml_neural_engine_affine(
    const std::vector<float>& input,
    const std::vector<float>& weights,
    std::size_t output_width,
    const std::vector<float>& bias,
    const std::filesystem::path& working_directory,
    double absolute_tolerance = 1.0e-5,
    double relative_tolerance = 1.0e-5);

[[nodiscard]] DeviceExecutionResult coreml_neural_engine_affine_batch(
    const std::vector<float>& inputs,
    std::size_t batch,
    std::size_t input_width,
    const std::vector<float>& weights,
    std::size_t output_width,
    const std::vector<float>& bias,
    const std::filesystem::path& working_directory,
    double absolute_tolerance = 1.0e-5,
    double relative_tolerance = 1.0e-5);

[[nodiscard]] DeviceExecutionResult coreml_neural_engine_affine_tensor_batch(
    const std::vector<float>& inputs,
    std::size_t batch,
    std::size_t input_width,
    const std::vector<float>& weights,
    std::size_t output_width,
    const std::vector<float>& bias,
    const std::filesystem::path& working_directory,
    double absolute_tolerance = 1.0e-5,
    double relative_tolerance = 1.0e-5);

[[nodiscard]] bool coreml_neural_engine_affine_tensor_batch_is_resident(
    std::size_t batch,
    std::size_t input_width,
    const std::vector<float>& weights,
    std::size_t output_width,
    const std::vector<float>& bias,
    const std::filesystem::path& working_directory);

[[nodiscard]] StructuredDeviceSolveResult
accelerate_periodic_helmholtz_2d_batch(
    const std::vector<double>& right_hand_sides,
    std::size_t batch,
    std::size_t width,
    double diffusion_number);

[[nodiscard]] bool cuda_gpu_available();

[[nodiscard]] std::string cuda_gpu_device_name();

[[nodiscard]] DeviceExecutionResult cuda_gpu_affine_batch(
    const std::vector<float>& inputs,
    std::size_t batch,
    std::size_t input_width,
    const std::vector<float>& weights,
    std::size_t output_width,
    const std::vector<float>& bias,
    double absolute_tolerance = 1.0e-5,
    double relative_tolerance = 1.0e-5);

[[nodiscard]] MetalStencilSolveResult cuda_weighted_jacobi_2d_batch(
    const std::vector<double>& west,
    const std::vector<double>& east,
    const std::vector<double>& south,
    const std::vector<double>& north,
    const std::vector<double>& inverse_diagonal,
    const std::vector<double>& right_hand_sides,
    std::size_t batch,
    std::size_t width,
    std::size_t iterations,
    double relaxation,
    double residual_tolerance = 1.0e-6);

}  // namespace smave
