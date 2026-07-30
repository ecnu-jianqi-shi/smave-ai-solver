// Native CUDA device kernels for the SMAVE solver.
// Compiled only when CMake configures SMAVE_HAVE_CUDA_RUNTIME.

#include <cuda_runtime.h>

namespace smave {

__global__ void smave_cuda_affine_kernel(
    const float* inputs,
    const float* weights,
    const float* bias,
    float* outputs,
    unsigned int total_threads,
    unsigned int input_width,
    unsigned int output_width) {
    const unsigned int thread = blockIdx.x * blockDim.x + threadIdx.x;
    if (thread >= total_threads) return;
    const unsigned int item = thread / output_width;
    const unsigned int row = thread % output_width;
    float value = bias[row];
    for (unsigned int column = 0; column < input_width; ++column) {
        value += weights[row * input_width + column] *
                 inputs[item * input_width + column];
    }
    outputs[thread] = value;
}

__global__ void smave_cuda_weighted_jacobi_2d_kernel(
    const float* west,
    const float* east,
    const float* south,
    const float* north,
    const float* inverse_diagonal,
    const float* right,
    float* current,
    float* next,
    float* output,
    unsigned int width,
    unsigned int iterations,
    float relaxation) {
    const unsigned int batch_index = blockIdx.x;
    const unsigned int size = width * width;
    const unsigned int base = batch_index * size;
    extern __shared__ float shared[];
    float* first = shared;
    float* second = shared + size;
    for (unsigned int index = threadIdx.x; index < size; index += blockDim.x) {
        first[index] = 0.0f;
    }
    __syncthreads();
    float* current_ptr = first;
    float* next_ptr = second;
    for (unsigned int iteration = 0; iteration < iterations; ++iteration) {
        for (unsigned int index = threadIdx.x; index < size; index += blockDim.x) {
            const unsigned int row = index / width;
            const unsigned int column = index - row * width;
            float value = current_ptr[index] / inverse_diagonal[base + index];
            if (column > 0) value -= west[base + index] * current_ptr[index - 1];
            if (column + 1 < width) value -= east[base + index] * current_ptr[index + 1];
            if (row > 0) value -= south[base + index] * current_ptr[index - width];
            if (row + 1 < width) value -= north[base + index] * current_ptr[index + width];
            next_ptr[index] = fmaf(
                relaxation * inverse_diagonal[base + index],
                right[base + index] - value, current_ptr[index]);
        }
        __syncthreads();
        float* swap = current_ptr;
        current_ptr = next_ptr;
        next_ptr = swap;
        __syncthreads();
    }
    for (unsigned int index = threadIdx.x; index < size; index += blockDim.x) {
        output[base + index] = current_ptr[index];
    }
}

void cuda_launch_affine(
    const float* inputs,
    const float* weights,
    const float* bias,
    float* outputs,
    unsigned int total_threads,
    unsigned int input_width,
    unsigned int output_width,
    unsigned int grid_size,
    unsigned int block_size) {
    smave_cuda_affine_kernel<<<grid_size, block_size>>>(
        inputs, weights, bias, outputs, total_threads, input_width, output_width);
}

void cuda_launch_weighted_jacobi_2d(
    const float* west,
    const float* east,
    const float* south,
    const float* north,
    const float* inverse_diagonal,
    const float* right,
    float* current,
    float* next,
    float* output,
    unsigned int width,
    unsigned int iterations,
    float relaxation,
    unsigned int batch_size,
    unsigned int block_size) {
    const unsigned int shared_bytes = 2u * width * width * sizeof(float);
    smave_cuda_weighted_jacobi_2d_kernel<<<batch_size, block_size, shared_bytes>>>(
        west, east, south, north, inverse_diagonal, right,
        current, next, output, width, iterations, relaxation);
}

}  // namespace smave
