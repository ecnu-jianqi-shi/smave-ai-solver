#include "smave/compiler.hpp"
#include "smave/device.hpp"
#include "smave/operator.hpp"
#include "smave/learning.hpp"
#include "smave/runtime.hpp"
#include "smave/tensor.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2) {
            std::cerr << "usage: smave_device_execution_evidence OUTPUT_DIRECTORY\n";
            return 2;
        }
        const std::filesystem::path directory(argv[1]);
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory);

        require(smave::metal_gpu_available(), "Metal GPU is unavailable");
        require(!smave::metal_gpu_device_name().empty(), "Metal GPU has no device identity");
        const std::vector<float> probe_inputs{
            1, 2, 3, 4,
            5, 6, 7, 8,
        };
        const std::vector<float> probe_weights{
            1, 0, 0, 1,
            0.5F, -1, 2, 0,
        };
        const std::vector<float> probe_bias{0.25F, -0.5F};
        const auto probe = smave::metal_gpu_affine_batch(
            probe_inputs, 2, 4, probe_weights, 2, probe_bias);
        require(probe.available && probe.executed && probe.verified &&
                    probe.backend == "metal-affine-batch-gpu-v1" &&
                    probe.output == std::vector<float>({5.25F, 4.0F, 13.25F, 10.0F}),
                "raw Metal affine kernel failed execution or reference gate");
        const auto invalid = smave::metal_gpu_affine_batch(
            {1.0F}, 2, 2, {1.0F}, 1, {0.0F});
        require(!invalid.executed && !invalid.verified &&
                    invalid.reason.find("invalid Metal affine batch") != std::string::npos,
                "invalid Metal batch was not rejected before execution");

        constexpr std::size_t stencil_width = 8;
        constexpr std::size_t stencil_batch = 2;
        const auto stencil_items =
            stencil_batch * stencil_width * stencil_width;
        const std::vector<double> stencil_neighbors(stencil_items, 1.0);
        const std::vector<double> stencil_inverse_diagonal(stencil_items, 0.25);
        const std::vector<double> stencil_right(stencil_items, 1.0);
        const auto stencil = smave::metal_weighted_jacobi_2d_batch(
            stencil_neighbors, stencil_neighbors, stencil_neighbors,
            stencil_neighbors, stencil_inverse_diagonal, stencil_right,
            stencil_batch, stencil_width, 2000, 2.0 / 3.0, 1.0e-4);
        require(stencil.available && stencil.executed && stencil.verified &&
                    stencil.maximum_relative_residual <= 1.0e-4,
                "Metal fused stencil failed the FP64 original residual gate");

        constexpr std::size_t spectral_width = 8;
        constexpr std::size_t spectral_batch = 2;
        constexpr double spectral_diffusion = 0.125;
        constexpr double pi = 3.141592653589793238462643383279502884;
        const auto spectral_plane = spectral_width * spectral_width;
        std::vector<double> spectral_expected(spectral_batch * spectral_plane);
        std::vector<double> spectral_right(spectral_batch * spectral_plane);
        for (std::size_t item = 0; item < spectral_batch; ++item) {
            const auto offset = item * spectral_plane;
            for (std::size_t row = 0; row < spectral_width; ++row) {
                for (std::size_t column = 0; column < spectral_width; ++column) {
                    const auto index = offset + row * spectral_width + column;
                    spectral_expected[index] = item == 0
                        ? 1.0 + 0.25 * (column % 2 == 0 ? 1.0 : -1.0) +
                            0.1 * std::cos(2.0 * pi * row / spectral_width)
                        : (row % 2 == 0 ? 1.0 : -1.0) +
                            0.2 * ((row + column) % 2 == 0 ? 1.0 : -1.0) +
                            0.05 * std::sin(2.0 * pi * column / spectral_width);
                }
            }
            for (std::size_t row = 0; row < spectral_width; ++row) {
                const auto south = row == 0 ? spectral_width - 1 : row - 1;
                const auto north = row + 1 == spectral_width ? 0 : row + 1;
                for (std::size_t column = 0; column < spectral_width; ++column) {
                    const auto west = column == 0 ? spectral_width - 1 : column - 1;
                    const auto east = column + 1 == spectral_width ? 0 : column + 1;
                    const auto index = offset + row * spectral_width + column;
                    spectral_right[index] =
                        (1.0 + 4.0 * spectral_diffusion) *
                            spectral_expected[index] -
                        spectral_diffusion * (
                            spectral_expected[offset + row * spectral_width + west] +
                            spectral_expected[offset + row * spectral_width + east] +
                            spectral_expected[offset + south * spectral_width + column] +
                            spectral_expected[offset + north * spectral_width + column]);
                }
            }
        }
        const auto spectral = smave::accelerate_periodic_helmholtz_2d_batch(
            spectral_right, spectral_batch, spectral_width, spectral_diffusion);
        double spectral_error{};
        for (std::size_t index = 0; index < spectral_expected.size(); ++index) {
            spectral_error = std::max(
                spectral_error,
                std::abs(spectral.output[index] - spectral_expected[index]));
        }
        require(spectral.available && spectral.executed && spectral.verified &&
                    spectral.backend ==
                        "accelerate-vdsp-real-gcd-periodic-helmholtz-2d-fp64-v3" &&
                    spectral.output.size() == spectral_expected.size() &&
                    spectral_error <= 1.0e-12,
                "Accelerate real FFT Helmholtz failed DC/Nyquist manufactured gate");
        smave::AcceleratePeriodicHelmholtz2DPlan spectral_plan(
            spectral_width, spectral_diffusion);
        std::vector<double> spectral_plan_output;
        double spectral_plan_kernel_us{};
        require(spectral_plan.available() && spectral_plan.solve(
                    std::vector<double>(
                        spectral_right.begin(), spectral_right.begin() + spectral_plane),
                    spectral_plan_output, &spectral_plan_kernel_us),
                "Accelerate persistent real FFT Helmholtz plan failed execution");
        double spectral_plan_error{};
        for (std::size_t index = 0; index < spectral_plane; ++index) {
            spectral_plan_error = std::max(
                spectral_plan_error,
                std::abs(spectral_plan_output[index] - spectral_expected[index]));
        }
        require(spectral_plan_error <= 1.0e-12,
                "Accelerate persistent real FFT plan failed manufactured gate");

        require(smave::coreml_neural_engine_available(),
                "CoreML exposes no Neural Engine compute device");
        constexpr std::size_t neural_width = 256;
        std::vector<float> neural_input(neural_width);
        std::vector<float> neural_weights(neural_width * neural_width);
        std::vector<float> neural_bias(neural_width);
        for (std::size_t row = 0; row < neural_width; ++row) {
            neural_input[row] =
                static_cast<float>(static_cast<int>(row % 13) - 6) / 8.0F;
            neural_bias[row] = static_cast<float>(row % 7) / 32.0F;
            for (std::size_t column = 0; column < neural_width; ++column) {
                neural_weights[row * neural_width + column] = row == column
                    ? 1.25F
                    : ((row + column) % 31 == 0 ? 0.015625F : 0.0F);
            }
        }
        const auto neural_probe = smave::coreml_neural_engine_affine(
            neural_input, neural_weights, neural_width, neural_bias,
            directory / "coreml", 2.0e-4, 2.0e-4);
        require(neural_probe.available && neural_probe.executed &&
                    neural_probe.verified &&
                    neural_probe.backend == "coreml-affine-neural-engine-v1" &&
                    neural_probe.device_name == "Apple Neural Engine" &&
                    neural_probe.output.size() == neural_width,
                "CoreML affine operator did not execute on the Neural Engine");
        constexpr std::size_t neural_throughput_batch = 64;
        std::vector<float> neural_throughput_weights(
            neural_width * neural_width);
        std::vector<float> neural_throughput_bias(neural_width);
        for (std::size_t index = 0; index < neural_width; ++index) {
            neural_throughput_weights[index * neural_width + index] = 1.0F;
        }
        std::vector<float> neural_throughput_inputs(
            neural_throughput_batch * neural_width);
        for (std::size_t item = 0; item < neural_throughput_batch; ++item) {
            for (std::size_t index = 0; index < neural_width; ++index) {
                neural_throughput_inputs[item * neural_width + index] =
                    neural_input[index] +
                    static_cast<float>(item % 7) / 64.0F;
            }
        }
        const auto neural_throughput_started = std::chrono::steady_clock::now();
        const auto neural_throughput = smave::coreml_neural_engine_affine_batch(
            neural_throughput_inputs, neural_throughput_batch, neural_width,
            neural_throughput_weights, neural_width, neural_throughput_bias,
            directory / "coreml-throughput", 2.0e-5, 2.0e-5);
        const auto neural_throughput_total_us =
            std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - neural_throughput_started).count();
        require(neural_throughput.available && neural_throughput.executed &&
                    neural_throughput.verified &&
                    neural_throughput.output.size() ==
                        neural_throughput_batch * neural_width,
                "CoreML affine throughput batch failed Neural Engine gates: " +
                    neural_throughput.reason + ", abs=" +
                    std::to_string(neural_throughput.maximum_absolute_error) +
                    ", rel=" +
                    std::to_string(neural_throughput.maximum_relative_error));
        const auto neural_throughput_warm_started = std::chrono::steady_clock::now();
        const auto neural_throughput_warm = smave::coreml_neural_engine_affine_batch(
            neural_throughput_inputs, neural_throughput_batch, neural_width,
            neural_throughput_weights, neural_width, neural_throughput_bias,
            directory / "coreml-throughput", 2.0e-5, 2.0e-5);
        const auto neural_throughput_warm_total_us =
            std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() -
                neural_throughput_warm_started).count();
        require(neural_throughput_warm.executed &&
                    neural_throughput_warm.verified,
                "CoreML warm throughput batch failed reference gate");
        std::vector<double> neural_cpu_reference(
            neural_throughput_batch * neural_width);
        const auto neural_cpu_started = std::chrono::steady_clock::now();
        for (std::size_t item = 0; item < neural_throughput_batch; ++item) {
            for (std::size_t row = 0; row < neural_width; ++row) {
                double value = neural_throughput_bias[row];
                for (std::size_t column = 0; column < neural_width; ++column) {
                    value += static_cast<double>(
                        neural_throughput_weights[row * neural_width + column]) *
                        neural_throughput_inputs[item * neural_width + column];
                }
                neural_cpu_reference[item * neural_width + row] = value;
            }
        }
        const auto neural_cpu_total_us =
            std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - neural_cpu_started).count();
        volatile double neural_cpu_sink = neural_cpu_reference.back();
        (void)neural_cpu_sink;
        const auto neural_tensor_batch_started = std::chrono::steady_clock::now();
        const auto neural_tensor_batch =
            smave::coreml_neural_engine_affine_tensor_batch(
                neural_throughput_inputs, neural_throughput_batch, neural_width,
                neural_throughput_weights, neural_width, neural_throughput_bias,
                directory / "coreml-tensor-throughput", 2.0e-5, 2.0e-5);
        const auto neural_tensor_batch_total_us =
            std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() -
                neural_tensor_batch_started).count();
        require(neural_tensor_batch.available && neural_tensor_batch.executed &&
                    neural_tensor_batch.verified,
                "CoreML tensor-native throughput batch failed: " +
                    neural_tensor_batch.reason);
        const auto neural_tensor_batch_warm_started =
            std::chrono::steady_clock::now();
        const auto neural_tensor_batch_warm =
            smave::coreml_neural_engine_affine_tensor_batch(
                neural_throughput_inputs, neural_throughput_batch, neural_width,
                neural_throughput_weights, neural_width, neural_throughput_bias,
                directory / "coreml-tensor-throughput", 2.0e-5, 2.0e-5);
        const auto neural_tensor_batch_warm_total_us =
            std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() -
                neural_tensor_batch_warm_started).count();
        require(neural_tensor_batch_warm.executed &&
                    neural_tensor_batch_warm.verified,
                "CoreML warm tensor-native throughput batch failed");
        const auto invalid_neural = smave::coreml_neural_engine_affine(
            {1.0F, 2.0F}, {1.0F}, 2, {0.0F, 0.0F},
            directory / "invalid-coreml");
        require(!invalid_neural.executed && !invalid_neural.verified &&
                    invalid_neural.reason.find("invalid CoreML affine shape") !=
                        std::string::npos,
                "invalid CoreML affine operator was not rejected before execution");
        const auto invalid_neural_batch = smave::coreml_neural_engine_affine_batch(
            {1.0F, 2.0F}, 2, 2, {1.0F}, 2, {0.0F, 0.0F},
            directory / "invalid-coreml-batch");
        require(!invalid_neural_batch.executed &&
                    !invalid_neural_batch.verified &&
                    invalid_neural_batch.reason.find(
                        "invalid CoreML affine batch shape") != std::string::npos,
                "invalid CoreML affine batch was not rejected before execution");

        const auto neural_source = directory / "NeuralEngineIdentity.mo";
        std::ofstream neural_model_file(neural_source);
        neural_model_file << "model NeuralEngineIdentity\n";
        for (std::size_t index = 1; index <= neural_width; ++index) {
            neural_model_file << "parameter Real b" << index << "=1; Real x"
                              << index << ";\n";
        }
        neural_model_file << "equation\n";
        for (std::size_t index = 1; index <= neural_width; ++index) {
            const std::size_t next = index == neural_width ? 1 : index + 1;
            neural_model_file << "x" << index << "+0*x" << next
                              << "=b" << index << ";\n";
        }
        neural_model_file << "end NeuralEngineIdentity;\n";
        neural_model_file.close();
        const auto neural_model = smave::compile_model(neural_source);
        smave::LinearPreconditionerArtifact neural_artifact;
        neural_artifact.model_source_hash = neural_model.source_hash;
        neural_artifact.block_fingerprint =
            neural_model.blocks.front().fingerprint;
        neural_artifact.training_samples = 1;
        neural_artifact.maximum_matrix_drift = 0.0;
        neural_artifact.inverse_operator.assign(
            neural_width, std::vector<double>(neural_width));
        for (std::size_t index = 0; index < neural_width; ++index) {
            neural_artifact.features.push_back("b" + std::to_string(index + 1));
            neural_artifact.feature_minimum.push_back(0.0);
            neural_artifact.feature_maximum.push_back(4.0);
            neural_artifact.inverse_operator[index][index] = 1.0;
        }
        neural_artifact.seal();
        const smave::LearnedLinearPreconditionerExpert neural_expert(
            neural_artifact);
        std::vector<std::unordered_map<std::string, double>> neural_scenarios(2);
        for (std::size_t item = 0; item < neural_scenarios.size(); ++item) {
            for (std::size_t index = 1; index <= neural_width; ++index) {
                neural_scenarios[item]["b" + std::to_string(index)] =
                    1.0 + static_cast<double>((item + index) % 4) * 0.5;
            }
        }
        const smave::Runtime neural_fallback(neural_model);
        const auto neural_batch = smave::TensorBucketScheduler(
            2, "coreml-neural-engine", false).solve_linear_batch(
                neural_model, neural_model.blocks.front(), neural_expert,
                neural_scenarios, neural_fallback,
                directory / "npu-traces");
        const auto neural_warm_batch = smave::TensorBucketScheduler(
            2, "coreml-neural-engine", false).solve_linear_batch(
                neural_model, neural_model.blocks.front(), neural_expert,
                neural_scenarios, neural_fallback,
                directory / "npu-traces-warm");
        require(neural_batch.metrics.device == "coreml-neural-engine" &&
                    neural_batch.metrics.device_backend ==
                        "coreml-affine-neural-engine-tensor-batch-v3" &&
                    neural_batch.metrics.device_name == "Apple Neural Engine" &&
                    neural_batch.metrics.device_batches == 1 &&
                    neural_batch.metrics.device_rejections == 0 &&
                    neural_batch.metrics.accepted == neural_scenarios.size() &&
                    neural_batch.metrics.fallback_count == 0,
                "Neural Engine preconditioner batch failed original equation gates");
        require(neural_warm_batch.metrics.device_batches == 1 &&
                    neural_warm_batch.metrics.device_rejections == 0 &&
                    neural_warm_batch.metrics.accepted == neural_scenarios.size() &&
                    neural_warm_batch.metrics.fallback_count == 0 &&
                    neural_warm_batch.metrics.device_upload_us <
                        neural_batch.metrics.device_upload_us,
                "Neural Engine cached batch did not reuse the verified model");
        const auto neural_auto_small = smave::TensorBucketScheduler(
            2, "auto", false).solve_linear_batch(
                neural_model, neural_model.blocks.front(), neural_expert,
                neural_scenarios, neural_fallback,
                directory / "npu-auto-small-traces");
        require(neural_auto_small.metrics.device == "auto" &&
                    neural_auto_small.metrics.device_batches == 0 &&
                    neural_auto_small.metrics.accepted == neural_scenarios.size() &&
                    neural_auto_small.metrics.fallback_count == 0,
                "automatic device routing did not keep an undersized batch on CPU");
        std::vector<std::unordered_map<std::string, double>>
            neural_auto_large_scenarios(64);
        for (std::size_t item = 0; item < neural_auto_large_scenarios.size(); ++item) {
            for (std::size_t index = 1; index <= neural_width; ++index) {
                neural_auto_large_scenarios[item]["b" + std::to_string(index)] =
                    1.0 + static_cast<double>((item + index) % 4) * 0.5;
            }
        }
        const auto neural_auto_large = smave::TensorBucketScheduler(
            64, "auto", false).solve_linear_batch(
                neural_model, neural_model.blocks.front(), neural_expert,
                neural_auto_large_scenarios, neural_fallback,
                directory / "npu-auto-large-traces");
        require(neural_auto_large.metrics.device == "auto" &&
                    neural_auto_large.metrics.device_backend.empty() &&
                    neural_auto_large.metrics.device_batches == 0 &&
                    neural_auto_large.metrics.device_rejections == 0 &&
                    neural_auto_large.metrics.accepted ==
                        neural_auto_large_scenarios.size() &&
                    neural_auto_large.metrics.fallback_count == 0,
                "automatic device routing used a cold ANE model");
        const auto neural_forced_ane_large = smave::TensorBucketScheduler(
            64, "coreml-neural-engine", false).solve_linear_batch(
                neural_model, neural_model.blocks.front(), neural_expert,
                neural_auto_large_scenarios, neural_fallback,
                directory / "npu-forced-ane-large-traces");
        require(neural_forced_ane_large.metrics.device_backend ==
                        "coreml-affine-neural-engine-tensor-batch-v3" &&
                    neural_forced_ane_large.metrics.device_batches == 1 &&
                    neural_forced_ane_large.metrics.device_rejections == 0 &&
                    neural_forced_ane_large.metrics.accepted ==
                        neural_auto_large_scenarios.size() &&
                    neural_forced_ane_large.metrics.fallback_count == 0,
                "forced ANE crossover bucket failed original equation gates");
        const auto neural_forced_ane_large_warm = smave::TensorBucketScheduler(
            64, "coreml-neural-engine", false).solve_linear_batch(
                neural_model, neural_model.blocks.front(), neural_expert,
                neural_auto_large_scenarios, neural_fallback,
                directory / "npu-forced-ane-large-warm-traces");
        require(neural_forced_ane_large_warm.metrics.device_backend ==
                        "coreml-affine-neural-engine-tensor-batch-v3" &&
                    neural_forced_ane_large_warm.metrics.device_batches == 1 &&
                    neural_forced_ane_large_warm.metrics.device_rejections == 0 &&
                    neural_forced_ane_large_warm.metrics.accepted ==
                        neural_auto_large_scenarios.size() &&
                    neural_forced_ane_large_warm.metrics.fallback_count == 0 &&
                    neural_forced_ane_large_warm.metrics.device_upload_us <
                        neural_forced_ane_large.metrics.device_upload_us,
                "resident ANE crossover bucket failed reuse or original gates");
        const auto neural_auto_resident_large = smave::TensorBucketScheduler(
            64, "auto", false).solve_linear_batch(
                neural_model, neural_model.blocks.front(), neural_expert,
                neural_auto_large_scenarios, neural_fallback,
                directory / "npu-auto-resident-large-traces");
        require(neural_auto_resident_large.metrics.device_backend.empty() &&
                    neural_auto_resident_large.metrics.device_batches == 0 &&
                    neural_auto_resident_large.metrics.device_rejections == 0 &&
                    neural_auto_resident_large.metrics.accepted ==
                        neural_auto_large_scenarios.size() &&
                    neural_auto_resident_large.metrics.fallback_count == 0,
                "automatic device routing ignored the Accelerate crossover threshold");
        const auto neural_cpu_large = smave::TensorBucketScheduler(
            64, "cpu", false).solve_linear_batch(
                neural_model, neural_model.blocks.front(), neural_expert,
                neural_auto_large_scenarios, neural_fallback,
                directory / "npu-cpu-large-traces");
        require(neural_cpu_large.metrics.device_batches == 0 &&
                    neural_cpu_large.metrics.accepted ==
                        neural_auto_large_scenarios.size() &&
                    neural_cpu_large.metrics.fallback_count == 0,
                "CPU reference bucket failed original equation gates");

        const auto source = directory / "GpuBatchSpd.mo";
        std::ofstream model_file(source);
        model_file << "model GpuBatchSpd\n";
        for (int index = 1; index <= 8; ++index) {
            model_file << "parameter Real b" << index
                       << "=1; Real x" << index << ";\n";
        }
        model_file << "equation\n4*x1-x2=b1;\n";
        for (int index = 2; index <= 7; ++index) {
            model_file << "-x" << index - 1 << "+4*x" << index
                       << "-x" << index + 1 << "=b" << index << ";\n";
        }
        model_file << "-x7+4*x8=b8;\nend GpuBatchSpd;\n";
        model_file.close();
        const auto model = smave::compile_model(source);
        const auto training = directory / "training";
        std::filesystem::create_directories(training);
        for (int sample = 1; sample <= 3; ++sample) {
            std::ofstream scenario(training / ("sample-" + std::to_string(sample) + ".conf"));
            for (int index = 1; index <= 8; ++index) {
                scenario << "b" << index << '=' << sample + index * 0.01 << '\n';
            }
        }
        const auto artifact = smave::train_linear_preconditioner(
            model, "block-1", training);
        const smave::LearnedLinearPreconditionerExpert expert(artifact);
        std::vector<std::unordered_map<std::string, double>> scenarios(64);
        for (std::size_t item = 0; item < scenarios.size(); ++item) {
            for (int index = 1; index <= 8; ++index) {
                scenarios[item]["b" + std::to_string(index)] =
                    1.0 + static_cast<double>(item % 3) + index * 0.01;
            }
        }
        const smave::Runtime fallback(model);
        const auto cpu = smave::TensorBucketScheduler(32, "cpu").solve_linear_batch(
            model, model.blocks.front(), expert, scenarios, fallback,
            directory / "cpu-traces");
        const auto gpu = smave::TensorBucketScheduler(
            32, "metal-gpu", false).solve_linear_batch(
                model, model.blocks.front(), expert, scenarios, fallback,
                directory / "gpu-traces");
        require(cpu.metrics.accepted == 64 && cpu.metrics.fallback_count == 0 &&
                    gpu.metrics.accepted == 64 && gpu.metrics.fallback_count == 0 &&
                    gpu.metrics.device == "metal-gpu" &&
                    gpu.metrics.device_backend == "metal-affine-batch-gpu-v1" &&
                    gpu.metrics.device_name == smave::metal_gpu_device_name() &&
                    gpu.metrics.device_batches == 2 &&
                    gpu.metrics.device_rejections == 0 &&
                    gpu.metrics.device_upload_us > 0.0 &&
                    gpu.metrics.kernel_us > 0.0 &&
                    gpu.metrics.device_download_us > 0.0 &&
                    gpu.metrics.device_maximum_relative_error <= 2.0e-5,
                "learned preconditioner did not execute verified Metal batches");
        double maximum_solution_difference{};
        for (std::size_t item = 0; item < scenarios.size(); ++item) {
            require(cpu.outcomes[item].success && gpu.outcomes[item].success &&
                        !cpu.outcomes[item].blocks.empty() &&
                        !gpu.outcomes[item].blocks.empty() &&
                        cpu.outcomes[item].blocks.front().gate.decision ==
                            smave::GateDecision::direct_accept &&
                        gpu.outcomes[item].blocks.front().gate.decision ==
                            smave::GateDecision::direct_accept,
                    "CPU or GPU learned candidate bypassed the original equation gate");
            for (const auto& unknown : model.blocks.front().unknowns) {
                maximum_solution_difference = std::max(
                    maximum_solution_difference,
                    std::abs(cpu.outcomes[item].values.at(unknown) -
                             gpu.outcomes[item].values.at(unknown)));
            }
        }
        require(maximum_solution_difference <= 2.0e-5,
                "Metal and CPU accepted solutions diverged");

        std::ofstream evidence(directory / "evidence.txt");
        evidence << std::setprecision(17)
                 << "SMAVE_DEVICE_EXECUTION_EVIDENCE 1\n"
                 << "SUCCESS 1\n"
                 << "GPU_AVAILABLE 1\n"
                 << "GPU_DEVICE \"" << gpu.metrics.device_name << "\"\n"
                 << "GPU_BACKEND \"" << gpu.metrics.device_backend << "\"\n"
                 << "RAW_COMMAND_EXECUTED " << probe.executed << "\n"
                 << "RAW_REFERENCE_GATE " << probe.verified << "\n"
                 << "RAW_KERNEL_US " << probe.kernel_us << "\n"
                 << "STENCIL_BACKEND \"" << stencil.backend << "\"\n"
                 << "STENCIL_ITERATIONS " << stencil.iterations << "\n"
                 << "STENCIL_SETUP_US " << stencil.setup_us << "\n"
                 << "STENCIL_KERNEL_US " << stencil.kernel_us << "\n"
                 << "STENCIL_FP64_RELATIVE_RESIDUAL "
                 << stencil.maximum_relative_residual << "\n"
                 << "SPECTRAL_BACKEND \"" << spectral.backend << "\"\n"
                 << "SPECTRAL_KERNEL_US " << spectral.kernel_us << "\n"
                 << "SPECTRAL_MAXIMUM_ABSOLUTE_ERROR " << spectral_error << "\n"
                 << "SPECTRAL_PLAN_KERNEL_US " << spectral_plan_kernel_us << "\n"
                 << "SPECTRAL_PLAN_MAXIMUM_ABSOLUTE_ERROR "
                 << spectral_plan_error << "\n"
                 << "BATCH_REQUESTS " << gpu.metrics.requests << "\n"
                 << "GPU_BATCHES " << gpu.metrics.device_batches << "\n"
                 << "GPU_REJECTIONS " << gpu.metrics.device_rejections << "\n"
                 << "ORIGINAL_EQUATION_ACCEPTED " << gpu.metrics.accepted << "\n"
                 << "RUNTIME_FALLBACKS " << gpu.metrics.fallback_count << "\n"
                 << "UPLOAD_US " << gpu.metrics.device_upload_us << "\n"
                 << "KERNEL_US " << gpu.metrics.kernel_us << "\n"
                 << "DOWNLOAD_US " << gpu.metrics.device_download_us << "\n"
                 << "MAXIMUM_DEVICE_REFERENCE_ERROR "
                 << gpu.metrics.device_maximum_absolute_error << "\n"
                 << "MAXIMUM_CPU_GPU_SOLUTION_DIFFERENCE "
                 << maximum_solution_difference << "\n"
                 << "INVALID_SHAPE_REJECTED 1\n"
                 << "NPU_AVAILABLE " << neural_probe.available << "\n"
                 << "NPU_DEVICE \"" << neural_probe.device_name << "\"\n"
                 << "NPU_BACKEND \"" << neural_probe.backend << "\"\n"
                 << "NPU_COMPUTE_PLAN_PREFERRED 1\n"
                 << "NPU_EXECUTED " << neural_probe.executed << "\n"
                 << "NPU_REFERENCE_GATE " << neural_probe.verified << "\n"
                 << "NPU_KERNEL_US " << neural_probe.kernel_us << "\n"
                 << "NPU_MAXIMUM_REFERENCE_ERROR "
                 << neural_probe.maximum_absolute_error << "\n"
                 << "NPU_THROUGHPUT_BATCH " << neural_throughput_batch << "\n"
                 << "NPU_THROUGHPUT_UPLOAD_US "
                 << neural_throughput.upload_us << "\n"
                 << "NPU_THROUGHPUT_DEVICE_KERNEL_US "
                 << neural_throughput.kernel_us << "\n"
                 << "NPU_THROUGHPUT_DOWNLOAD_US "
                 << neural_throughput.download_us << "\n"
                 << "NPU_THROUGHPUT_TOTAL_US "
                 << neural_throughput_total_us << "\n"
                 << "NPU_THROUGHPUT_WARM_UPLOAD_US "
                 << neural_throughput_warm.upload_us << "\n"
                 << "NPU_THROUGHPUT_WARM_DEVICE_KERNEL_US "
                 << neural_throughput_warm.kernel_us << "\n"
                 << "NPU_THROUGHPUT_WARM_DOWNLOAD_US "
                 << neural_throughput_warm.download_us << "\n"
                 << "NPU_THROUGHPUT_WARM_TOTAL_US "
                 << neural_throughput_warm_total_us << "\n"
                 << "NPU_THROUGHPUT_CPU_FP64_US "
                 << neural_cpu_total_us << "\n"
                 << "NPU_THROUGHPUT_WARM_VS_CPU "
                 << neural_cpu_total_us / neural_throughput_warm_total_us << "\n"
                 << "NPU_TENSOR_BATCH_UPLOAD_US "
                 << neural_tensor_batch.upload_us << "\n"
                 << "NPU_TENSOR_BATCH_DEVICE_KERNEL_US "
                 << neural_tensor_batch.kernel_us << "\n"
                 << "NPU_TENSOR_BATCH_TOTAL_US "
                 << neural_tensor_batch_total_us << "\n"
                 << "NPU_TENSOR_BATCH_WARM_UPLOAD_US "
                 << neural_tensor_batch_warm.upload_us << "\n"
                 << "NPU_TENSOR_BATCH_WARM_DEVICE_KERNEL_US "
                 << neural_tensor_batch_warm.kernel_us << "\n"
                 << "NPU_TENSOR_BATCH_WARM_TOTAL_US "
                 << neural_tensor_batch_warm_total_us << "\n"
                 << "NPU_TENSOR_BATCH_WARM_VS_CPU "
                 << neural_cpu_total_us / neural_tensor_batch_warm_total_us << "\n"
                 << "NPU_SOLVER_BATCHES "
                 << neural_batch.metrics.device_batches << "\n"
                 << "NPU_COLD_UPLOAD_US "
                 << neural_batch.metrics.device_upload_us << "\n"
                 << "NPU_COLD_DEVICE_KERNEL_US "
                 << neural_batch.metrics.device_kernel_us << "\n"
                 << "NPU_COLD_APPLY_US "
                 << neural_batch.metrics.kernel_us << "\n"
                 << "NPU_COLD_TOTAL_US "
                 << neural_batch.metrics.total_us << "\n"
                 << "NPU_WARM_UPLOAD_US "
                 << neural_warm_batch.metrics.device_upload_us << "\n"
                 << "NPU_WARM_DEVICE_KERNEL_US "
                 << neural_warm_batch.metrics.device_kernel_us << "\n"
                 << "NPU_WARM_APPLY_US "
                 << neural_warm_batch.metrics.kernel_us << "\n"
                 << "NPU_WARM_TOTAL_US "
                 << neural_warm_batch.metrics.total_us << "\n"
                 << "NPU_ORIGINAL_EQUATION_ACCEPTED "
                 << neural_batch.metrics.accepted << "\n"
                 << "NPU_RUNTIME_FALLBACKS "
                 << neural_batch.metrics.fallback_count << "\n"
                 << "NPU_AUTO_SMALL_DEVICE_BATCHES "
                 << neural_auto_small.metrics.device_batches << "\n"
                 << "NPU_AUTO_SMALL_TOTAL_US "
                 << neural_auto_small.metrics.total_us << "\n"
                 << "NPU_AUTO_LARGE_DEVICE_BATCHES "
                 << neural_auto_large.metrics.device_batches << "\n"
                 << "NPU_AUTO_LARGE_ACCEPTED "
                 << neural_auto_large.metrics.accepted << "\n"
                 << "NPU_AUTO_LARGE_TOTAL_US "
                 << neural_auto_large.metrics.total_us << "\n"
                 << "NPU_CPU_LARGE_TOTAL_US "
                 << neural_cpu_large.metrics.total_us << "\n"
                 << "NPU_AUTO_LARGE_VS_CPU_COMPLETE "
                 << neural_cpu_large.metrics.total_us /
                        neural_auto_large.metrics.total_us << "\n"
                 << "NPU_FORCED_ANE_LARGE_TOTAL_US "
                 << neural_forced_ane_large.metrics.total_us << "\n"
                 << "NPU_FORCED_ANE_LARGE_VS_CPU_COMPLETE "
                 << neural_cpu_large.metrics.total_us /
                        neural_forced_ane_large.metrics.total_us << "\n"
                 << "NPU_FORCED_ANE_LARGE_WARM_TOTAL_US "
                 << neural_forced_ane_large_warm.metrics.total_us << "\n"
                 << "NPU_FORCED_ANE_LARGE_WARM_VS_CPU_COMPLETE "
                 << neural_cpu_large.metrics.total_us /
                        neural_forced_ane_large_warm.metrics.total_us << "\n"
                 << "NPU_AUTO_RESIDENT_LARGE_TOTAL_US "
                 << neural_auto_resident_large.metrics.total_us << "\n"
                 << "NPU_AUTO_RESIDENT_LARGE_VS_CPU_COMPLETE "
                 << neural_cpu_large.metrics.total_us /
                        neural_auto_resident_large.metrics.total_us << "\n"
                 << "NPU_INVALID_SHAPE_REJECTED 1\n";
        std::cout << "device execution evidence passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "device execution evidence failure: " << error.what() << '\n';
        return 1;
    }
}
