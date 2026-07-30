#include "smave/device.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2) {
            std::cerr << "usage: smave_cuda_device_evidence OUTPUT_DIRECTORY\n";
            return 2;
        }
        const std::filesystem::path directory(argv[1]);
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory);

        require(smave::cuda_gpu_available(), "CUDA GPU is unavailable");
        require(!smave::cuda_gpu_device_name().empty(), "CUDA GPU has no device identity");

        // Affine batch test
        const std::vector<float> probe_inputs{1, 2, 3, 4, 5, 6, 7, 8};
        const std::vector<float> probe_weights{1, 0, 0, 1, 0.5F, -1, 2, 0};
        const std::vector<float> probe_bias{0.25F, -0.5F};
        const auto probe = smave::cuda_gpu_affine_batch(
            probe_inputs, 2, 4, probe_weights, 2, probe_bias, 1.0e-5, 1.0e-5);
        require(probe.available && probe.executed && probe.verified &&
                    probe.backend == "cuda-affine-batch-gpu-v1" &&
                    probe.output == std::vector<float>({5.25F, 4.0F, 13.25F, 10.0F}),
                "CUDA affine kernel failed execution or reference gate");

        const auto invalid = smave::cuda_gpu_affine_batch(
            {1.0F}, 2, 2, {1.0F}, 1, {0.0F}, 1.0e-5, 1.0e-5);
        require(!invalid.executed && !invalid.verified &&
                    invalid.reason.find("invalid CUDA affine batch") != std::string::npos,
                "invalid CUDA batch was not rejected before execution");

        // Weighted Jacobi stencil test - mirrors Metal fused Jacobi contract
        constexpr std::size_t stencil_width = 8;
        constexpr std::size_t stencil_batch = 2;
        const auto stencil_items = stencil_batch * stencil_width * stencil_width;
        const std::vector<double> stencil_neighbors(stencil_items, 1.0);
        const std::vector<double> stencil_inv_diag(stencil_items, 0.25);
        const std::vector<double> stencil_rhs(stencil_items, 1.0);
        const auto stencil = smave::cuda_weighted_jacobi_2d_batch(
            stencil_neighbors, stencil_neighbors, stencil_neighbors,
            stencil_neighbors, stencil_inv_diag, stencil_rhs,
            stencil_batch, stencil_width, 2000, 2.0 / 3.0, 1.0e-4);
        require(stencil.available && stencil.executed && stencil.verified &&
                    stencil.backend == "cuda-fused-weighted-jacobi-2d-fp32-v1" &&
                    stencil.output.size() == stencil_items &&
                    stencil.maximum_relative_residual <= 1.0e-4,
                std::string("CUDA weighted Jacobi stencil failed execution or FP64 residual gate: ") + stencil.reason +
                    " residual=" + std::to_string(stencil.maximum_relative_residual));

        const auto invalid_stencil = smave::cuda_weighted_jacobi_2d_batch(
            {1.0}, {1.0}, {1.0}, {1.0}, {1.0}, {1.0}, 2, 2, 10, 0.8);
        require(!invalid_stencil.executed && !invalid_stencil.verified &&
                    invalid_stencil.reason.find("invalid CUDA stencil") != std::string::npos,
                "invalid CUDA stencil was not rejected before execution");

        const auto evidence_file = directory / "evidence.txt";
        std::ofstream evidence(evidence_file);
        evidence << "NATIVE_CUDA_DEVICE_EVIDENCE_V1\n"
                 << "cuda_device_available=1\n"
                 << "cuda_device_name=" << smave::cuda_gpu_device_name() << "\n"
                 << "cuda_affine_backend=" << probe.backend << "\n"
                 << "cuda_affine_executed=" << probe.executed << "\n"
                 << "cuda_affine_verified=" << probe.verified << "\n"
                 << "cuda_affine_upload_us=" << probe.upload_us << "\n"
                 << "cuda_affine_kernel_us=" << probe.kernel_us << "\n"
                 << "cuda_affine_download_us=" << probe.download_us << "\n"
                 << "cuda_affine_max_abs_error=" << probe.maximum_absolute_error << "\n"
                 << "cuda_affine_max_rel_error=" << probe.maximum_relative_error << "\n"
                 << "cuda_stencil_backend=" << stencil.backend << "\n"
                 << "cuda_stencil_executed=" << stencil.executed << "\n"
                 << "cuda_stencil_verified=" << stencil.verified << "\n"
                 << "cuda_stencil_batch=" << stencil.batch << "\n"
                 << "cuda_stencil_width=" << stencil.width << "\n"
                 << "cuda_stencil_iterations=" << stencil.iterations << "\n"
                 << "cuda_stencil_setup_us=" << stencil.setup_us << "\n"
                 << "cuda_stencil_kernel_us=" << stencil.kernel_us << "\n"
                 << "cuda_stencil_download_us=" << stencil.download_us << "\n"
                 << "cuda_stencil_max_rel_residual=" << stencil.maximum_relative_residual << "\n"
                 << "cuda_invalid_shape_rejected=1\n"
                 << "END\n";
        evidence.close();
        std::cout << "CUDA device execution evidence passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "CUDA device execution evidence failure: " << error.what() << '\n';
        return 1;
    }
}
