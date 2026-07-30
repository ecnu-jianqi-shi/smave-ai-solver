#include "smave/device.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_map>

#if defined(SMAVE_HAVE_ACCELERATE_SPARSE)
#include <Accelerate/Accelerate.h>
#endif

#if defined(__APPLE__)
#include <dispatch/dispatch.h>
#endif

#if defined(SMAVE_HAVE_METAL_RUNTIME)
#include <dlfcn.h>
#include <objc/message.h>
#include <objc/runtime.h>
#endif

#if defined(SMAVE_HAVE_CUDA_RUNTIME)
#include <cuda_runtime.h>

namespace smave {
void cuda_launch_affine(
    const float* inputs,
    const float* weights,
    const float* bias,
    float* outputs,
    unsigned int total_threads,
    unsigned int input_width,
    unsigned int output_width,
    unsigned int grid_size,
    unsigned int block_size);

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
    unsigned int block_size);
}  // namespace smave
#endif

namespace smave {
namespace {

using Clock = std::chrono::steady_clock;

double elapsed_us(Clock::time_point started) {
    return std::chrono::duration<double, std::micro>(Clock::now() - started).count();
}

#if defined(SMAVE_HAVE_METAL_RUNTIME)

using Object = id;

template <class Result, class... Arguments>
Result send(Object object, const char* selector, Arguments... arguments) {
    return reinterpret_cast<Result (*)(Object, SEL, Arguments...)>(objc_msgSend)(
        object, sel_registerName(selector), arguments...);
}

struct Size3 {
    unsigned long width;
    unsigned long height;
    unsigned long depth;
};

class AutoreleasePool {
public:
    AutoreleasePool()
        : pool_(send<Object>(reinterpret_cast<Object>(
              objc_getClass("NSAutoreleasePool")), "new")) {}
    ~AutoreleasePool() {
        if (pool_ != nullptr) send<void>(pool_, "drain");
    }

private:
    Object pool_{};
};

struct MetalRuntime {
    void* handle{};
    Object device{};
    std::string device_name;

    MetalRuntime() {
        handle = dlopen(
            "/System/Library/Frameworks/Metal.framework/Metal", RTLD_NOW | RTLD_LOCAL);
        if (handle == nullptr) return;
        const auto create = reinterpret_cast<Object (*)()>(
            dlsym(handle, "MTLCreateSystemDefaultDevice"));
        if (create == nullptr) return;
        device = create();
        if (device == nullptr) return;
        const auto name = send<Object>(device, "name");
        const auto text = name != nullptr
            ? send<const char*>(name, "UTF8String")
            : nullptr;
        if (text != nullptr) device_name = text;
    }

    ~MetalRuntime() {
        if (handle != nullptr) dlclose(handle);
    }
};

MetalRuntime& metal_runtime() {
    static MetalRuntime runtime;
    return runtime;
}

void* coreml_runtime_handle() {
    static void* handle = dlopen(
        "/System/Library/Frameworks/CoreML.framework/CoreML",
        RTLD_NOW | RTLD_LOCAL);
    return handle;
}

Object make_string(const char* text) {
    const auto string_class = reinterpret_cast<Object>(objc_getClass("NSString"));
    return send<Object>(string_class, "stringWithUTF8String:", text);
}

void append_varint(std::vector<unsigned char>& output, std::uint64_t value) {
    while (value >= 0x80U) {
        output.push_back(static_cast<unsigned char>((value & 0x7fU) | 0x80U));
        value >>= 7U;
    }
    output.push_back(static_cast<unsigned char>(value));
}

void append_key(
    std::vector<unsigned char>& output, std::uint32_t field, std::uint32_t wire) {
    append_varint(output, (static_cast<std::uint64_t>(field) << 3U) | wire);
}

void append_message(
    std::vector<unsigned char>& output,
    std::uint32_t field,
    const std::vector<unsigned char>& message) {
    append_key(output, field, 2);
    append_varint(output, message.size());
    output.insert(output.end(), message.begin(), message.end());
}

void append_string(
    std::vector<unsigned char>& output,
    std::uint32_t field,
    const std::string& value) {
    append_key(output, field, 2);
    append_varint(output, value.size());
    output.insert(output.end(), value.begin(), value.end());
}

std::vector<unsigned char> array_feature(
    const std::vector<std::size_t>& shape_values) {
    std::vector<unsigned char> array;
    std::vector<unsigned char> packed_shape;
    for (const auto size : shape_values) append_varint(packed_shape, size);
    append_message(array, 1, packed_shape);
    append_key(array, 2, 0);
    append_varint(array, 65568);
    std::vector<unsigned char> feature_type;
    append_message(feature_type, 5, array);
    return feature_type;
}

std::vector<unsigned char> array_feature(std::size_t size) {
    return array_feature(std::vector<std::size_t>{size});
}

std::vector<unsigned char> feature_description(
    const std::string& name, std::size_t size) {
    std::vector<unsigned char> feature;
    append_string(feature, 1, name);
    append_message(feature, 3, array_feature(size));
    return feature;
}

std::vector<unsigned char> feature_description(
    const std::string& name, const std::vector<std::size_t>& shape) {
    std::vector<unsigned char> feature;
    append_string(feature, 1, name);
    append_message(feature, 3, array_feature(shape));
    return feature;
}

std::vector<unsigned char> weight_parameters(const std::vector<float>& values) {
    std::vector<unsigned char> packed(values.size() * sizeof(float));
    if (!packed.empty()) std::memcpy(packed.data(), values.data(), packed.size());
    std::vector<unsigned char> weights;
    append_message(weights, 1, packed);
    return weights;
}

std::vector<unsigned char> coreml_affine_model(
    std::size_t input_width,
    std::size_t output_width,
    const std::vector<float>& weights,
    const std::vector<float>& bias) {
    std::vector<unsigned char> description;
    append_message(description, 1, feature_description("input", input_width));
    append_message(description, 10, feature_description("output", output_width));

    std::vector<unsigned char> inner_product;
    append_key(inner_product, 1, 0); append_varint(inner_product, input_width);
    append_key(inner_product, 2, 0); append_varint(inner_product, output_width);
    append_key(inner_product, 10, 0); append_varint(inner_product, 1);
    append_message(inner_product, 20, weight_parameters(weights));
    append_message(inner_product, 21, weight_parameters(bias));

    std::vector<unsigned char> layer;
    append_string(layer, 1, "smave_affine");
    append_string(layer, 2, "input");
    append_string(layer, 3, "output");
    append_message(layer, 140, inner_product);

    std::vector<unsigned char> network;
    append_message(network, 1, layer);
    append_key(network, 5, 0); append_varint(network, 1);

    std::vector<unsigned char> model;
    append_key(model, 1, 0); append_varint(model, 4);
    append_message(model, 2, description);
    append_message(model, 500, network);
    return model;
}

std::vector<unsigned char> coreml_affine_tensor_batch_model(
    std::size_t batch,
    std::size_t input_width,
    std::size_t output_width,
    const std::vector<float>& weights,
    const std::vector<float>& bias) {
    std::vector<unsigned char> description;
    append_message(description, 1, feature_description(
        "input", std::vector<std::size_t>{batch, input_width}));
    append_message(description, 10, feature_description(
        "output", std::vector<std::size_t>{batch, output_width}));
    std::vector<unsigned char> inner_product;
    append_key(inner_product, 1, 0); append_varint(inner_product, input_width);
    append_key(inner_product, 2, 0); append_varint(inner_product, output_width);
    append_key(inner_product, 10, 0); append_varint(inner_product, 1);
    append_message(inner_product, 20, weight_parameters(weights));
    append_message(inner_product, 21, weight_parameters(bias));
    std::vector<unsigned char> layer;
    append_string(layer, 1, "smave_affine_tensor_batch");
    append_string(layer, 2, "input");
    append_string(layer, 3, "output");
    append_message(layer, 140, inner_product);
    std::vector<unsigned char> network;
    append_message(network, 1, layer);
    append_key(network, 5, 0); append_varint(network, 1);
    std::vector<unsigned char> model;
    append_key(model, 1, 0); append_varint(model, 4);
    append_message(model, 2, description);
    append_message(model, 500, network);
    return model;
}

Object file_url(const std::filesystem::path& path) {
    const auto url_class = reinterpret_cast<Object>(objc_getClass("NSURL"));
    return send<Object>(url_class, "fileURLWithPath:",
                        make_string(path.string().c_str()));
}

std::string error_description(Object error) {
    if (error == nullptr) return {};
    const auto description = send<Object>(error, "localizedDescription");
    const auto text = description != nullptr
        ? send<const char*>(description, "UTF8String")
        : nullptr;
    return text != nullptr ? std::string(text) : std::string("unknown Metal error");
}

std::string coreml_cache_key(
    const std::filesystem::path& working_directory,
    const std::vector<unsigned char>& model) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (const auto byte : model) {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    return working_directory.string() + '|' + std::to_string(model.size()) + '|' +
        std::to_string(hash);
}

struct CoreMLCachedModel {
    Object model{};

    ~CoreMLCachedModel() {
        if (model != nullptr) send<void>(model, "release");
    }
};

std::mutex& coreml_model_cache_mutex() {
    static std::mutex mutex;
    return mutex;
}

std::unordered_map<std::string, std::shared_ptr<CoreMLCachedModel>>&
coreml_model_cache() {
    static std::unordered_map<std::string, std::shared_ptr<CoreMLCachedModel>> cache;
    return cache;
}

std::shared_ptr<CoreMLCachedModel> load_coreml_cached_model(
    const std::vector<unsigned char>& bytes,
    const std::filesystem::path& working_directory,
    DeviceExecutionResult& result) {
    const auto cache_key = coreml_cache_key(working_directory, bytes);
    std::lock_guard lock(coreml_model_cache_mutex());
    const auto existing = coreml_model_cache().find(cache_key);
    if (existing != coreml_model_cache().end()) return existing->second;
    std::filesystem::create_directories(working_directory);
    const auto model_path = working_directory / "smave-affine-tensor-batch.mlmodel";
    {
        std::ofstream output(model_path, std::ios::binary);
        if (!output) {
            result.reason = "cannot write generated CoreML tensor batch model";
            return {};
        }
        output.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
    }
    const auto model_class = reinterpret_cast<Object>(objc_getClass("MLModel"));
    Object error{};
    const auto compiled_url = send<Object>(
        model_class, "compileModelAtURL:error:", file_url(model_path), &error);
    if (compiled_url == nullptr) {
        result.reason = "CoreML tensor batch model compilation failed: " +
            error_description(error);
        return {};
    }
    const auto configuration = send<Object>(
        reinterpret_cast<Object>(objc_getClass("MLModelConfiguration")), "new");
    send<Object>(configuration, "autorelease");
    constexpr long cpu_and_neural_engine = 3;
    send<void>(configuration, "setComputeUnits:", cpu_and_neural_engine);
    __block Object compute_plan{};
    __block Object compute_plan_error{};
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
    send<void>(
        reinterpret_cast<Object>(objc_getClass("MLComputePlan")),
        "loadContentsOfURL:configuration:completionHandler:",
        compiled_url, configuration,
        ^(Object plan, Object plan_error) {
            compute_plan = plan != nullptr ? send<Object>(plan, "retain") : nullptr;
            compute_plan_error = plan_error != nullptr
                ? send<Object>(plan_error, "retain")
                : nullptr;
            dispatch_semaphore_signal(semaphore);
        });
    dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
    if (compute_plan != nullptr) send<Object>(compute_plan, "autorelease");
    if (compute_plan_error != nullptr) send<Object>(compute_plan_error, "autorelease");
    if (compute_plan == nullptr) {
        result.reason = "CoreML tensor batch compute plan failed: " +
            error_description(compute_plan_error);
        return {};
    }
    const auto structure = send<Object>(compute_plan, "modelStructure");
    const auto network = structure != nullptr
        ? send<Object>(structure, "neuralNetwork")
        : nullptr;
    const auto layers = network != nullptr ? send<Object>(network, "layers") : nullptr;
    const auto layer_count = layers != nullptr
        ? send<unsigned long>(layers, "count")
        : 0UL;
    const auto neural_class = reinterpret_cast<Object>(
        objc_getClass("MLNeuralEngineComputeDevice"));
    bool neural_engine_preferred{};
    for (unsigned long index = 0; index < layer_count; ++index) {
        const auto layer = send<Object>(layers, "objectAtIndex:", index);
        const auto usage = send<Object>(
            compute_plan, "computeDeviceUsageForNeuralNetworkLayer:", layer);
        const auto preferred = usage != nullptr
            ? send<Object>(usage, "preferredComputeDevice")
            : nullptr;
        neural_engine_preferred = neural_engine_preferred ||
            (preferred != nullptr &&
             send<bool>(preferred, "isKindOfClass:", neural_class));
    }
    if (!neural_engine_preferred) {
        result.reason =
            "CoreML tensor batch compute plan does not prefer Neural Engine";
        return {};
    }
    const auto model = send<Object>(
        model_class, "modelWithContentsOfURL:configuration:error:",
        compiled_url, configuration, &error);
    if (model == nullptr) {
        result.reason = "CoreML tensor batch model load failed: " +
            error_description(error);
        return {};
    }
    auto cached = std::make_shared<CoreMLCachedModel>();
    cached->model = send<Object>(model, "retain");
    coreml_model_cache().emplace(cache_key, cached);
    return cached;
}

void verify_affine_batch_reference(
    const std::vector<float>& inputs,
    std::size_t batch,
    std::size_t input_width,
    const std::vector<float>& weights,
    std::size_t output_width,
    const std::vector<float>& bias,
    DeviceExecutionResult& result,
    double absolute_tolerance,
    double relative_tolerance) {
    std::vector<double> references(batch * output_width);
#if defined(SMAVE_HAVE_ACCELERATE_SPARSE)
    const std::vector<double> double_inputs(inputs.begin(), inputs.end());
    const std::vector<double> double_weights(weights.begin(), weights.end());
    cblas_dgemm(
        CblasRowMajor, CblasNoTrans, CblasTrans,
        static_cast<int>(batch), static_cast<int>(output_width),
        static_cast<int>(input_width), 1.0,
        double_inputs.data(), static_cast<int>(input_width),
        double_weights.data(), static_cast<int>(input_width),
        0.0, references.data(), static_cast<int>(output_width));
    for (std::size_t item = 0; item < batch; ++item) {
        for (std::size_t row = 0; row < output_width; ++row) {
            references[item * output_width + row] += bias[row];
        }
    }
#else
    for (std::size_t item = 0; item < batch; ++item) {
        for (std::size_t row = 0; row < output_width; ++row) {
            double reference = bias[row];
            for (std::size_t column = 0; column < input_width; ++column) {
                reference += static_cast<double>(
                    weights[row * input_width + column]) *
                    static_cast<double>(inputs[item * input_width + column]);
            }
            references[item * output_width + row] = reference;
        }
    }
#endif
    for (std::size_t index = 0; index < references.size(); ++index) {
        const auto absolute_error = std::abs(result.output[index] - references[index]);
        result.maximum_absolute_error = std::max(
            result.maximum_absolute_error, absolute_error);
        result.maximum_relative_error = std::max(
            result.maximum_relative_error,
            absolute_error / std::max(1.0, std::abs(references[index])));
    }
    result.verified = result.maximum_absolute_error <= absolute_tolerance ||
        result.maximum_relative_error <= relative_tolerance;
}

#endif

}  // namespace

bool metal_gpu_available() {
#if defined(SMAVE_HAVE_METAL_RUNTIME)
    AutoreleasePool pool;
    return metal_runtime().device != nullptr;
#else
    return false;
#endif
}

std::string metal_gpu_device_name() {
#if defined(SMAVE_HAVE_METAL_RUNTIME)
    AutoreleasePool pool;
    return metal_runtime().device_name;
#else
    return {};
#endif
}

DeviceExecutionResult metal_gpu_affine_batch(
    const std::vector<float>& inputs,
    std::size_t batch,
    std::size_t input_width,
    const std::vector<float>& weights,
    std::size_t output_width,
    const std::vector<float>& bias,
    double absolute_tolerance,
    double relative_tolerance) {
    DeviceExecutionResult result;
    result.backend = "metal-affine-batch-gpu-v1";
    result.batch = batch;
    result.input_width = input_width;
    result.output_width = output_width;
    if (batch == 0 || input_width == 0 || output_width == 0 ||
        inputs.size() != batch * input_width ||
        weights.size() != output_width * input_width ||
        bias.size() != output_width ||
        !(absolute_tolerance > 0.0) || !(relative_tolerance > 0.0) ||
        !std::all_of(inputs.begin(), inputs.end(), [](float value) {
            return std::isfinite(value);
        }) ||
        !std::all_of(weights.begin(), weights.end(), [](float value) {
            return std::isfinite(value);
        }) ||
        !std::all_of(bias.begin(), bias.end(), [](float value) {
            return std::isfinite(value);
        })) {
        result.reason = "invalid Metal affine batch shape, tolerance, or finite-value contract";
        return result;
    }
#if !defined(SMAVE_HAVE_METAL_RUNTIME)
    result.reason = "Metal runtime is unavailable in this build";
    return result;
#else
    AutoreleasePool pool;
    auto& runtime = metal_runtime();
    result.available = runtime.device != nullptr;
    result.device_name = runtime.device_name;
    if (!result.available) {
        result.reason = "Metal has no default GPU device";
        return result;
    }
    constexpr const char* source = R"METAL(
#include <metal_stdlib>
using namespace metal;
kernel void affine_batch(
    device const float* inputs [[buffer(0)]],
    device const float* weights [[buffer(1)]],
    device const float* bias [[buffer(2)]],
    device float* outputs [[buffer(3)]],
    constant uint& input_width [[buffer(4)]],
    constant uint& output_width [[buffer(5)]],
    uint index [[thread_position_in_grid]]) {
    uint batch_index = index / output_width;
    uint output_index = index - batch_index * output_width;
    float value = bias[output_index];
    for (uint input_index = 0; input_index < input_width; ++input_index) {
        value = fma(
            inputs[batch_index * input_width + input_index],
            weights[output_index * input_width + input_index], value);
    }
    outputs[index] = value;
}
)METAL";
    Object compilation_error{};
    const auto library = send<Object>(
        runtime.device, "newLibraryWithSource:options:error:",
        make_string(source), static_cast<Object>(nullptr), &compilation_error);
    if (library == nullptr) {
        result.reason = "Metal source compilation failed: " +
            error_description(compilation_error);
        return result;
    }
    const auto function = send<Object>(
        library, "newFunctionWithName:", make_string("affine_batch"));
    if (function == nullptr) {
        result.reason = "Metal library is missing affine_batch";
        return result;
    }
    Object pipeline_error{};
    const auto pipeline = send<Object>(
        runtime.device, "newComputePipelineStateWithFunction:error:",
        function, &pipeline_error);
    if (pipeline == nullptr) {
        result.reason = "Metal pipeline creation failed: " +
            error_description(pipeline_error);
        return result;
    }
    const auto queue = send<Object>(runtime.device, "newCommandQueue");
    if (queue == nullptr) {
        result.reason = "Metal command queue creation failed";
        return result;
    }
    const auto upload_started = Clock::now();
    constexpr unsigned long shared_storage = 0;
    const auto input_buffer = send<Object>(
        runtime.device, "newBufferWithBytes:length:options:", inputs.data(),
        inputs.size() * sizeof(float), shared_storage);
    const auto weight_buffer = send<Object>(
        runtime.device, "newBufferWithBytes:length:options:", weights.data(),
        weights.size() * sizeof(float), shared_storage);
    const auto bias_buffer = send<Object>(
        runtime.device, "newBufferWithBytes:length:options:", bias.data(),
        bias.size() * sizeof(float), shared_storage);
    const auto output_items = batch * output_width;
    const auto output_buffer = send<Object>(
        runtime.device, "newBufferWithLength:options:",
        output_items * sizeof(float), shared_storage);
    const auto input_width_value = static_cast<unsigned int>(input_width);
    const auto output_width_value = static_cast<unsigned int>(output_width);
    const auto input_width_buffer = send<Object>(
        runtime.device, "newBufferWithBytes:length:options:", &input_width_value,
        sizeof(input_width_value), shared_storage);
    const auto output_width_buffer = send<Object>(
        runtime.device, "newBufferWithBytes:length:options:", &output_width_value,
        sizeof(output_width_value), shared_storage);
    result.upload_us = elapsed_us(upload_started);
    if (input_buffer == nullptr || weight_buffer == nullptr || bias_buffer == nullptr ||
        output_buffer == nullptr || input_width_buffer == nullptr ||
        output_width_buffer == nullptr) {
        result.reason = "Metal shared buffer allocation failed";
        return result;
    }
    const auto command_buffer = send<Object>(queue, "commandBuffer");
    const auto encoder = command_buffer != nullptr
        ? send<Object>(command_buffer, "computeCommandEncoder")
        : nullptr;
    if (encoder == nullptr) {
        result.reason = "Metal compute encoder creation failed";
        return result;
    }
    send<void>(encoder, "setComputePipelineState:", pipeline);
    send<void>(encoder, "setBuffer:offset:atIndex:", input_buffer, 0UL, 0UL);
    send<void>(encoder, "setBuffer:offset:atIndex:", weight_buffer, 0UL, 1UL);
    send<void>(encoder, "setBuffer:offset:atIndex:", bias_buffer, 0UL, 2UL);
    send<void>(encoder, "setBuffer:offset:atIndex:", output_buffer, 0UL, 3UL);
    send<void>(encoder, "setBuffer:offset:atIndex:", input_width_buffer, 0UL, 4UL);
    send<void>(encoder, "setBuffer:offset:atIndex:", output_width_buffer, 0UL, 5UL);
    const auto maximum_threads = send<unsigned long>(
        pipeline, "maxTotalThreadsPerThreadgroup");
    const auto threads = std::max<unsigned long>(
        1, std::min<unsigned long>(maximum_threads, output_items));
    const Size3 grid{static_cast<unsigned long>(output_items), 1, 1};
    const Size3 group{threads, 1, 1};
    send<void>(encoder, "dispatchThreads:threadsPerThreadgroup:", grid, group);
    send<void>(encoder, "endEncoding");
    const auto kernel_started = Clock::now();
    send<void>(command_buffer, "commit");
    send<void>(command_buffer, "waitUntilCompleted");
    result.kernel_us = elapsed_us(kernel_started);
    const auto status = send<unsigned long>(command_buffer, "status");
    constexpr unsigned long completed_status = 4;
    if (status != completed_status) {
        const auto command_error = send<Object>(command_buffer, "error");
        result.reason = "Metal command buffer did not complete: " +
            error_description(command_error);
        return result;
    }
    result.executed = true;
    const auto download_started = Clock::now();
    const auto contents = send<void*>(output_buffer, "contents");
    if (contents == nullptr) {
        result.reason = "Metal output buffer has no CPU-visible contents";
        return result;
    }
    result.output.resize(output_items);
    std::memcpy(result.output.data(), contents, output_items * sizeof(float));
    result.download_us = elapsed_us(download_started);
    for (std::size_t batch_index = 0; batch_index < batch; ++batch_index) {
        for (std::size_t output_index = 0; output_index < output_width; ++output_index) {
            double reference = static_cast<double>(bias[output_index]);
            for (std::size_t input_index = 0; input_index < input_width; ++input_index) {
                reference += static_cast<double>(
                    inputs[batch_index * input_width + input_index]) *
                    static_cast<double>(
                        weights[output_index * input_width + input_index]);
            }
            const double actual = result.output[
                batch_index * output_width + output_index];
            const double absolute_error = std::abs(actual - reference);
            const double relative_error = absolute_error /
                std::max(1.0, std::abs(reference));
            result.maximum_absolute_error = std::max(
                result.maximum_absolute_error, absolute_error);
            result.maximum_relative_error = std::max(
                result.maximum_relative_error, relative_error);
        }
    }
    result.verified = result.maximum_absolute_error <= absolute_tolerance ||
        result.maximum_relative_error <= relative_tolerance;
    result.reason = result.verified
        ? "Metal command completed and CPU fp64 reference gate passed"
        : "Metal command completed but CPU fp64 reference gate failed";
    return result;
#endif
}

MetalStencilSolveResult metal_weighted_jacobi_2d_batch(
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
    double residual_tolerance) {
    MetalStencilSolveResult result;
    result.backend = "metal-fused-weighted-jacobi-2d-fp32-v1";
    result.batch = batch;
    result.width = width;
    result.iterations = iterations;
    const auto size = width * width;
    const auto items = batch * size;
    if (batch == 0 || width == 0 || iterations == 0 || items == 0 ||
        west.size() != items || east.size() != items || south.size() != items ||
        north.size() != items || inverse_diagonal.size() != items ||
        right_hand_sides.size() != items || !(relaxation > 0.0) ||
        !(relaxation < 1.0) || !std::isfinite(relaxation) ||
        !(residual_tolerance > 0.0) || !std::isfinite(residual_tolerance)) {
        result.reason = "invalid Metal stencil shape or numeric contract";
        return result;
    }
    const auto finite_input = [](const std::vector<double>& values) {
        return std::all_of(values.begin(), values.end(), [](double value) {
            return std::isfinite(value);
        });
    };
    if (!finite_input(west) || !finite_input(east) || !finite_input(south) ||
        !finite_input(north) || !finite_input(inverse_diagonal) ||
        !finite_input(right_hand_sides) ||
        std::any_of(inverse_diagonal.begin(), inverse_diagonal.end(),
                    [](double value) { return !(value > 0.0); })) {
        result.reason = "Metal stencil inputs must be finite with positive diagonal inverse";
        return result;
    }
#if !defined(SMAVE_HAVE_METAL_RUNTIME)
    result.reason = "Metal runtime is unavailable in this build";
    return result;
#else
    AutoreleasePool pool;
    auto& runtime = metal_runtime();
    result.available = runtime.device != nullptr;
    result.device_name = runtime.device_name;
    if (!result.available) {
        result.reason = "Metal has no default GPU device";
        return result;
    }
    const auto setup_started = Clock::now();
    constexpr const char* source = R"METAL(
#include <metal_stdlib>
using namespace metal;
kernel void weighted_jacobi_2d(
    device const float* west [[buffer(0)]],
    device const float* east [[buffer(1)]],
    device const float* south [[buffer(2)]],
    device const float* north [[buffer(3)]],
    device const float* inverse_diagonal [[buffer(4)]],
    device const float* right [[buffer(5)]],
    device float* output [[buffer(6)]],
    constant uint& width [[buffer(7)]],
    constant uint& iterations [[buffer(8)]],
    constant float& relaxation [[buffer(9)]],
    threadgroup float* first [[threadgroup(0)]],
    threadgroup float* second [[threadgroup(1)]],
    uint thread_index [[thread_index_in_threadgroup]],
    uint threads [[threads_per_threadgroup]],
    uint batch_index [[threadgroup_position_in_grid]]) {
    uint size = width * width;
    uint base = batch_index * size;
    for (uint index = thread_index; index < size; index += threads) {
        first[index] = 0.0f;
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    threadgroup float* current = first;
    threadgroup float* next = second;
    for (uint iteration = 0; iteration < iterations; ++iteration) {
        for (uint index = thread_index; index < size; index += threads) {
            uint row = index / width;
            uint column = index - row * width;
            float value = current[index] / inverse_diagonal[base + index];
            if (column > 0) value -= west[base + index] * current[index - 1];
            if (column + 1 < width) value -= east[base + index] * current[index + 1];
            if (row > 0) value -= south[base + index] * current[index - width];
            if (row + 1 < width) value -= north[base + index] * current[index + width];
            next[index] = fma(
                relaxation * inverse_diagonal[base + index],
                right[base + index] - value, current[index]);
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
        threadgroup float* swap = current;
        current = next;
        next = swap;
    }
    for (uint index = thread_index; index < size; index += threads) {
        output[base + index] = current[index];
    }
}
)METAL";
    Object compilation_error{};
    const auto library = send<Object>(
        runtime.device, "newLibraryWithSource:options:error:",
        make_string(source), static_cast<Object>(nullptr), &compilation_error);
    if (library == nullptr) {
        result.reason = "Metal stencil source compilation failed: " +
            error_description(compilation_error);
        return result;
    }
    const auto function = send<Object>(
        library, "newFunctionWithName:", make_string("weighted_jacobi_2d"));
    Object pipeline_error{};
    const auto pipeline = function != nullptr
        ? send<Object>(runtime.device,
              "newComputePipelineStateWithFunction:error:", function,
              &pipeline_error)
        : nullptr;
    const auto queue = send<Object>(runtime.device, "newCommandQueue");
    if (pipeline == nullptr || queue == nullptr) {
        result.reason = "Metal stencil pipeline creation failed: " +
            error_description(pipeline_error);
        return result;
    }
    std::vector<float> west_f(west.begin(), west.end());
    std::vector<float> east_f(east.begin(), east.end());
    std::vector<float> south_f(south.begin(), south.end());
    std::vector<float> north_f(north.begin(), north.end());
    std::vector<float> inverse_diagonal_f(
        inverse_diagonal.begin(), inverse_diagonal.end());
    std::vector<float> right_f(right_hand_sides.begin(), right_hand_sides.end());
    constexpr unsigned long shared_storage = 0;
    const auto make_buffer = [&](const std::vector<float>& values) {
        return send<Object>(runtime.device, "newBufferWithBytes:length:options:",
            values.data(), values.size() * sizeof(float), shared_storage);
    };
    const auto west_buffer = make_buffer(west_f);
    const auto east_buffer = make_buffer(east_f);
    const auto south_buffer = make_buffer(south_f);
    const auto north_buffer = make_buffer(north_f);
    const auto inverse_diagonal_buffer = make_buffer(inverse_diagonal_f);
    const auto right_buffer = make_buffer(right_f);
    const auto output_buffer = send<Object>(runtime.device,
        "newBufferWithLength:options:", items * sizeof(float), shared_storage);
    const auto width_value = static_cast<unsigned int>(width);
    const auto iteration_value = static_cast<unsigned int>(iterations);
    const auto relaxation_value = static_cast<float>(relaxation);
    const auto width_buffer = send<Object>(runtime.device,
        "newBufferWithBytes:length:options:", &width_value,
        sizeof(width_value), shared_storage);
    const auto iteration_buffer = send<Object>(runtime.device,
        "newBufferWithBytes:length:options:", &iteration_value,
        sizeof(iteration_value), shared_storage);
    const auto relaxation_buffer = send<Object>(runtime.device,
        "newBufferWithBytes:length:options:", &relaxation_value,
        sizeof(relaxation_value), shared_storage);
    if (west_buffer == nullptr || east_buffer == nullptr || south_buffer == nullptr ||
        north_buffer == nullptr || inverse_diagonal_buffer == nullptr ||
        right_buffer == nullptr || output_buffer == nullptr || width_buffer == nullptr ||
        iteration_buffer == nullptr || relaxation_buffer == nullptr) {
        result.reason = "Metal stencil shared buffer allocation failed";
        return result;
    }
    result.setup_us = elapsed_us(setup_started);
    const auto command_buffer = send<Object>(queue, "commandBuffer");
    const auto encoder = command_buffer != nullptr
        ? send<Object>(command_buffer, "computeCommandEncoder") : nullptr;
    if (encoder == nullptr) {
        result.reason = "Metal stencil encoder creation failed";
        return result;
    }
    send<void>(encoder, "setComputePipelineState:", pipeline);
    const Object buffers[] = {west_buffer, east_buffer, south_buffer, north_buffer,
        inverse_diagonal_buffer, right_buffer, output_buffer, width_buffer,
        iteration_buffer, relaxation_buffer};
    for (unsigned long index = 0; index < 10; ++index) {
        send<void>(encoder, "setBuffer:offset:atIndex:", buffers[index], 0UL, index);
    }
    send<void>(encoder, "setThreadgroupMemoryLength:atIndex:",
        size * sizeof(float), 0UL);
    send<void>(encoder, "setThreadgroupMemoryLength:atIndex:",
        size * sizeof(float), 1UL);
    const auto maximum_threads = send<unsigned long>(
        pipeline, "maxTotalThreadsPerThreadgroup");
    const auto thread_count = std::max<unsigned long>(
        1, std::min<unsigned long>(maximum_threads, 256));
    const Size3 groups{static_cast<unsigned long>(batch), 1, 1};
    const Size3 threads{thread_count, 1, 1};
    send<void>(encoder, "dispatchThreadgroups:threadsPerThreadgroup:", groups, threads);
    send<void>(encoder, "endEncoding");
    const auto kernel_started = Clock::now();
    send<void>(command_buffer, "commit");
    send<void>(command_buffer, "waitUntilCompleted");
    result.kernel_us = elapsed_us(kernel_started);
    constexpr unsigned long completed_status = 4;
    if (send<unsigned long>(command_buffer, "status") != completed_status) {
        result.reason = "Metal stencil command failed: " +
            error_description(send<Object>(command_buffer, "error"));
        return result;
    }
    result.executed = true;
    const auto download_started = Clock::now();
    const auto contents = send<void*>(output_buffer, "contents");
    if (contents == nullptr) {
        result.reason = "Metal stencil output is not CPU-visible";
        return result;
    }
    const auto* output_f = static_cast<const float*>(contents);
    result.output.assign(output_f, output_f + items);
    result.download_us = elapsed_us(download_started);
    for (std::size_t batch_index = 0; batch_index < batch; ++batch_index) {
        double residual_inf{};
        double right_inf{1.0};
        const auto base = batch_index * size;
        for (std::size_t row = 0; row < width; ++row) {
            for (std::size_t column = 0; column < width; ++column) {
                const auto index = row * width + column;
                auto value = result.output[base + index] /
                    inverse_diagonal[base + index];
                if (column > 0) value -= west[base + index] *
                    result.output[base + index - 1];
                if (column + 1 < width) value -= east[base + index] *
                    result.output[base + index + 1];
                if (row > 0) value -= south[base + index] *
                    result.output[base + index - width];
                if (row + 1 < width) value -= north[base + index] *
                    result.output[base + index + width];
                residual_inf = std::max(residual_inf,
                    std::abs(right_hand_sides[base + index] - value));
                right_inf = std::max(right_inf,
                    std::abs(right_hand_sides[base + index]));
            }
        }
        result.maximum_relative_residual = std::max(
            result.maximum_relative_residual, residual_inf / right_inf);
    }
    result.verified = result.maximum_relative_residual <= residual_tolerance;
    result.reason = result.verified
        ? "Metal fused stencil completed and FP64 original residual gate passed"
        : "Metal fused stencil completed but FP64 original residual gate failed";
    return result;
#endif
}

MetalStencilSolveResult metal_frozen_burgers_1d_batch(
    const std::vector<double>& states,
    std::size_t batch,
    std::size_t width,
    double diffusion_number,
    double convection_scale,
    std::size_t iterations,
    double relaxation,
    double residual_tolerance) {
    MetalStencilSolveResult result;
    result.backend = "metal-frozen-burgers-weighted-jacobi-fp32-v1";
    result.batch = batch;
    result.width = width;
    result.iterations = iterations;
    const auto items = batch * width;
    if (batch == 0 || width < 3 || items == 0 || states.size() != items ||
        iterations == 0 || !(diffusion_number > 0.0) ||
        !(convection_scale > 0.0) || !(relaxation > 0.0) ||
        !(relaxation < 1.0) || !(residual_tolerance > 0.0) ||
        !std::isfinite(diffusion_number) || !std::isfinite(convection_scale) ||
        !std::isfinite(relaxation) || !std::isfinite(residual_tolerance) ||
        !std::all_of(states.begin(), states.end(), [](double value) {
            return std::isfinite(value);
        })) {
        result.reason = "invalid Metal frozen Burgers shape or numeric contract";
        return result;
    }
#if !defined(SMAVE_HAVE_METAL_RUNTIME)
    result.reason = "Metal runtime is unavailable in this build";
    return result;
#else
    AutoreleasePool pool;
    auto& runtime = metal_runtime();
    result.available = runtime.device != nullptr;
    result.device_name = runtime.device_name;
    if (!result.available) {
        result.reason = "Metal has no default GPU device";
        return result;
    }
    const auto setup_started = Clock::now();
    constexpr const char* source = R"METAL(
#include <metal_stdlib>
using namespace metal;
kernel void frozen_burgers_jacobi(
    device const float* state [[buffer(0)]],
    device float* output [[buffer(1)]],
    constant uint& width [[buffer(2)]],
    constant uint& iterations [[buffer(3)]],
    constant float& diffusion [[buffer(4)]],
    constant float& convection_scale [[buffer(5)]],
    constant float& relaxation [[buffer(6)]],
    threadgroup float* first [[threadgroup(0)]],
    threadgroup float* second [[threadgroup(1)]],
    uint thread_index [[thread_index_in_threadgroup]],
    uint threads [[threads_per_threadgroup]],
    uint batch_index [[threadgroup_position_in_grid]]) {
    uint base = batch_index * width;
    for (uint index = thread_index; index < width; index += threads) {
        first[index] = state[base + index];
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    threadgroup float* current = first;
    threadgroup float* next = second;
    float inverse_diagonal = 1.0f / (1.0f + 2.0f * diffusion);
    for (uint iteration = 0; iteration < iterations; ++iteration) {
        for (uint index = thread_index; index < width; index += threads) {
            uint previous = index == 0 ? width - 1 : index - 1;
            uint following = index + 1 == width ? 0 : index + 1;
            float convection = convection_scale * state[base + index];
            float product = (1.0f + 2.0f * diffusion) * current[index] +
                (-diffusion - convection) * current[previous] +
                (-diffusion + convection) * current[following];
            next[index] = fma(
                relaxation * inverse_diagonal,
                state[base + index] - product, current[index]);
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
        threadgroup float* swap = current;
        current = next;
        next = swap;
    }
    for (uint index = thread_index; index < width; index += threads) {
        output[base + index] = current[index];
    }
}
)METAL";
    Object compilation_error{};
    const auto library = send<Object>(
        runtime.device, "newLibraryWithSource:options:error:",
        make_string(source), static_cast<Object>(nullptr), &compilation_error);
    if (library == nullptr) {
        result.reason = "Metal Burgers source compilation failed: " +
            error_description(compilation_error);
        return result;
    }
    const auto function = send<Object>(
        library, "newFunctionWithName:", make_string("frozen_burgers_jacobi"));
    Object pipeline_error{};
    const auto pipeline = function != nullptr
        ? send<Object>(runtime.device,
              "newComputePipelineStateWithFunction:error:", function,
              &pipeline_error)
        : nullptr;
    const auto queue = send<Object>(runtime.device, "newCommandQueue");
    if (pipeline == nullptr || queue == nullptr) {
        result.reason = "Metal Burgers pipeline creation failed: " +
            error_description(pipeline_error);
        return result;
    }
    std::vector<float> states_f(states.begin(), states.end());
    constexpr unsigned long shared_storage = 0;
    const auto state_buffer = send<Object>(runtime.device,
        "newBufferWithBytes:length:options:", states_f.data(),
        states_f.size() * sizeof(float), shared_storage);
    const auto output_buffer = send<Object>(runtime.device,
        "newBufferWithLength:options:", items * sizeof(float), shared_storage);
    const std::uint32_t width_u32 = static_cast<std::uint32_t>(width);
    const std::uint32_t iterations_u32 = static_cast<std::uint32_t>(iterations);
    const float diffusion_f = static_cast<float>(diffusion_number);
    const float convection_f = static_cast<float>(convection_scale);
    const float relaxation_f = static_cast<float>(relaxation);
    const auto make_constant = [&](const auto& value) {
        return send<Object>(runtime.device, "newBufferWithBytes:length:options:",
            &value, sizeof(value), shared_storage);
    };
    const std::array<Object, 7> buffers{
        state_buffer, output_buffer, make_constant(width_u32),
        make_constant(iterations_u32), make_constant(diffusion_f),
        make_constant(convection_f), make_constant(relaxation_f)};
    result.setup_us = elapsed_us(setup_started);
    if (std::any_of(buffers.begin(), buffers.end(), [](Object value) {
            return value == nullptr;
        })) {
        result.reason = "Metal Burgers buffer allocation failed";
        return result;
    }
    const auto command_buffer = send<Object>(queue, "commandBuffer");
    const auto encoder = command_buffer != nullptr
        ? send<Object>(command_buffer, "computeCommandEncoder") : nullptr;
    if (encoder == nullptr) {
        result.reason = "Metal Burgers compute encoder creation failed";
        return result;
    }
    send<void>(encoder, "setComputePipelineState:", pipeline);
    for (unsigned long index = 0; index < buffers.size(); ++index) {
        send<void>(encoder, "setBuffer:offset:atIndex:", buffers[index], 0UL, index);
    }
    send<void>(encoder, "setThreadgroupMemoryLength:atIndex:",
        width * sizeof(float), 0UL);
    send<void>(encoder, "setThreadgroupMemoryLength:atIndex:",
        width * sizeof(float), 1UL);
    const auto maximum_threads = send<unsigned long>(
        pipeline, "maxTotalThreadsPerThreadgroup");
    const Size3 groups{static_cast<unsigned long>(batch), 1, 1};
    const Size3 threads{std::max<unsigned long>(
        1, std::min<unsigned long>(maximum_threads, 256)), 1, 1};
    send<void>(encoder, "dispatchThreadgroups:threadsPerThreadgroup:", groups, threads);
    send<void>(encoder, "endEncoding");
    const auto kernel_started = Clock::now();
    send<void>(command_buffer, "commit");
    send<void>(command_buffer, "waitUntilCompleted");
    result.kernel_us = elapsed_us(kernel_started);
    constexpr unsigned long completed_status = 4;
    if (send<unsigned long>(command_buffer, "status") != completed_status) {
        result.reason = "Metal Burgers command failed: " +
            error_description(send<Object>(command_buffer, "error"));
        return result;
    }
    result.executed = true;
    const auto download_started = Clock::now();
    const auto* output = static_cast<const float*>(
        send<void*>(output_buffer, "contents"));
    if (output == nullptr) {
        result.reason = "Metal Burgers output is not CPU-visible";
        return result;
    }
    result.output.assign(output, output + items);
    result.download_us = elapsed_us(download_started);
    for (std::size_t sample = 0; sample < batch; ++sample) {
        const auto base = sample * width;
        double residual_squared{};
        double right_squared{};
        for (std::size_t index = 0; index < width; ++index) {
            const auto previous = index == 0 ? width - 1 : index - 1;
            const auto following = index + 1 == width ? 0 : index + 1;
            const auto convection = convection_scale * states[base + index];
            const auto product =
                (1.0 + 2.0 * diffusion_number) * result.output[base + index] +
                (-diffusion_number - convection) * result.output[base + previous] +
                (-diffusion_number + convection) * result.output[base + following];
            const auto residual = states[base + index] - product;
            residual_squared += residual * residual;
            right_squared += states[base + index] * states[base + index];
        }
        result.maximum_relative_residual = std::max(
            result.maximum_relative_residual,
            std::sqrt(residual_squared) /
                std::max(1.0, std::sqrt(right_squared)));
    }
    result.verified = result.maximum_relative_residual <= residual_tolerance;
    result.reason = result.verified
        ? "Metal Burgers completed and FP64 original residual gate passed"
        : "Metal Burgers completed but FP64 original residual gate failed";
    return result;
#endif
}

MetalStencilSolveResult metal_frozen_retardation_1d_batch(
    const std::vector<double>& states,
    std::size_t batch,
    std::size_t width,
    double constant_ratio,
    double power_ratio,
    double concentration_exponent,
    std::size_t iterations,
    double relaxation,
    double residual_tolerance) {
    MetalStencilSolveResult result;
    result.backend = "metal-frozen-retardation-weighted-jacobi-fp32-v1";
    result.batch = batch;
    result.width = width;
    result.iterations = iterations;
    const auto items = batch * width;
    if (batch == 0 || width < 3 || items == 0 || states.size() != items ||
        !(constant_ratio > 0.0) || !(power_ratio > 0.0) ||
        !std::isfinite(concentration_exponent) || iterations == 0 ||
        !(relaxation > 0.0) || !(relaxation < 1.0) ||
        !(residual_tolerance > 0.0) || !std::isfinite(constant_ratio) ||
        !std::isfinite(power_ratio) || !std::isfinite(relaxation) ||
        !std::isfinite(residual_tolerance) ||
        !std::all_of(states.begin(), states.end(), [](double value) {
            return std::isfinite(value);
        })) {
        result.reason = "invalid Metal frozen retardation shape or numeric contract";
        return result;
    }
#if !defined(SMAVE_HAVE_METAL_RUNTIME)
    result.reason = "Metal runtime is unavailable in this build";
    return result;
#else
    AutoreleasePool pool;
    auto& runtime = metal_runtime();
    result.available = runtime.device != nullptr;
    result.device_name = runtime.device_name;
    if (!result.available) {
        result.reason = "Metal has no default GPU device";
        return result;
    }
    const auto setup_started = Clock::now();
    constexpr const char* source = R"METAL(
#include <metal_stdlib>
using namespace metal;
kernel void frozen_retardation_jacobi(
    device const float* state [[buffer(0)]],
    device float* output [[buffer(1)]],
    constant uint& width [[buffer(2)]],
    constant uint& iterations [[buffer(3)]],
    constant float& constant_ratio [[buffer(4)]],
    constant float& power_ratio [[buffer(5)]],
    constant float& exponent [[buffer(6)]],
    constant float& relaxation [[buffer(7)]],
    threadgroup float* first [[threadgroup(0)]],
    threadgroup float* second [[threadgroup(1)]],
    uint thread_index [[thread_index_in_threadgroup]],
    uint threads [[threads_per_threadgroup]],
    uint batch_index [[threadgroup_position_in_grid]]) {
    uint unknowns = width - 1;
    uint base = batch_index * width;
    for (uint local = thread_index; local < unknowns; local += threads) {
        first[local] = state[base + local + 1];
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    threadgroup float* current = first;
    threadgroup float* next = second;
    for (uint iteration = 0; iteration < iterations; ++iteration) {
        for (uint local = thread_index; local < unknowns; local += threads) {
            uint index = local + 1;
            float concentration = max(state[base + index], 1.0e-8f);
            float ratio = constant_ratio + power_ratio * pow(concentration, exponent);
            float diagonal = ratio + (local + 1 == unknowns ? 1.0f : 2.0f);
            float product = diagonal * current[local];
            if (local > 0) product -= current[local - 1];
            if (local + 1 < unknowns) product -= current[local + 1];
            float right = ratio * state[base + index];
            if (local == 0) right += 1.0f;
            next[local] = fma(
                relaxation / diagonal, right - product, current[local]);
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
        threadgroup float* swap = current;
        current = next;
        next = swap;
    }
    if (thread_index == 0) output[base] = 1.0f;
    for (uint local = thread_index; local < unknowns; local += threads) {
        output[base + local + 1] = current[local];
    }
}
)METAL";
    Object compilation_error{};
    const auto library = send<Object>(
        runtime.device, "newLibraryWithSource:options:error:",
        make_string(source), static_cast<Object>(nullptr), &compilation_error);
    if (library == nullptr) {
        result.reason = "Metal retardation source compilation failed: " +
            error_description(compilation_error);
        return result;
    }
    const auto function = send<Object>(library, "newFunctionWithName:",
        make_string("frozen_retardation_jacobi"));
    Object pipeline_error{};
    const auto pipeline = function != nullptr
        ? send<Object>(runtime.device,
              "newComputePipelineStateWithFunction:error:", function,
              &pipeline_error)
        : nullptr;
    const auto queue = send<Object>(runtime.device, "newCommandQueue");
    if (pipeline == nullptr || queue == nullptr) {
        result.reason = "Metal retardation pipeline creation failed: " +
            error_description(pipeline_error);
        return result;
    }
    std::vector<float> states_f(states.begin(), states.end());
    constexpr unsigned long shared_storage = 0;
    const auto state_buffer = send<Object>(runtime.device,
        "newBufferWithBytes:length:options:", states_f.data(),
        states_f.size() * sizeof(float), shared_storage);
    const auto output_buffer = send<Object>(runtime.device,
        "newBufferWithLength:options:", items * sizeof(float), shared_storage);
    const std::uint32_t width_u32 = static_cast<std::uint32_t>(width);
    const std::uint32_t iterations_u32 = static_cast<std::uint32_t>(iterations);
    const float constant_f = static_cast<float>(constant_ratio);
    const float power_f = static_cast<float>(power_ratio);
    const float exponent_f = static_cast<float>(concentration_exponent);
    const float relaxation_f = static_cast<float>(relaxation);
    const auto make_constant = [&](const auto& value) {
        return send<Object>(runtime.device, "newBufferWithBytes:length:options:",
            &value, sizeof(value), shared_storage);
    };
    const std::array<Object, 8> buffers{
        state_buffer, output_buffer, make_constant(width_u32),
        make_constant(iterations_u32), make_constant(constant_f),
        make_constant(power_f), make_constant(exponent_f),
        make_constant(relaxation_f)};
    result.setup_us = elapsed_us(setup_started);
    if (std::any_of(buffers.begin(), buffers.end(), [](Object value) {
            return value == nullptr;
        })) {
        result.reason = "Metal retardation buffer allocation failed";
        return result;
    }
    const auto command_buffer = send<Object>(queue, "commandBuffer");
    const auto encoder = command_buffer != nullptr
        ? send<Object>(command_buffer, "computeCommandEncoder") : nullptr;
    if (encoder == nullptr) {
        result.reason = "Metal retardation compute encoder creation failed";
        return result;
    }
    send<void>(encoder, "setComputePipelineState:", pipeline);
    for (unsigned long index = 0; index < buffers.size(); ++index) {
        send<void>(encoder, "setBuffer:offset:atIndex:", buffers[index], 0UL, index);
    }
    const auto unknowns = width - 1;
    send<void>(encoder, "setThreadgroupMemoryLength:atIndex:",
        unknowns * sizeof(float), 0UL);
    send<void>(encoder, "setThreadgroupMemoryLength:atIndex:",
        unknowns * sizeof(float), 1UL);
    const auto maximum_threads = send<unsigned long>(
        pipeline, "maxTotalThreadsPerThreadgroup");
    const Size3 groups{static_cast<unsigned long>(batch), 1, 1};
    const Size3 threads{std::max<unsigned long>(
        1, std::min<unsigned long>(maximum_threads, 256)), 1, 1};
    send<void>(encoder, "dispatchThreadgroups:threadsPerThreadgroup:", groups, threads);
    send<void>(encoder, "endEncoding");
    const auto kernel_started = Clock::now();
    send<void>(command_buffer, "commit");
    send<void>(command_buffer, "waitUntilCompleted");
    result.kernel_us = elapsed_us(kernel_started);
    constexpr unsigned long completed_status = 4;
    if (send<unsigned long>(command_buffer, "status") != completed_status) {
        result.reason = "Metal retardation command failed: " +
            error_description(send<Object>(command_buffer, "error"));
        return result;
    }
    result.executed = true;
    const auto download_started = Clock::now();
    const auto* output = static_cast<const float*>(
        send<void*>(output_buffer, "contents"));
    if (output == nullptr) {
        result.reason = "Metal retardation output is not CPU-visible";
        return result;
    }
    result.output.assign(output, output + items);
    result.download_us = elapsed_us(download_started);
    for (std::size_t sample = 0; sample < batch; ++sample) {
        const auto base = sample * width;
        double residual_squared{};
        double right_squared{};
        for (std::size_t index = 1; index < width; ++index) {
            const auto ratio = constant_ratio + power_ratio * std::pow(
                std::max(states[base + index], 1.0e-8), concentration_exponent);
            auto product = (ratio + (index + 1 == width ? 1.0 : 2.0)) *
                result.output[base + index] - result.output[base + index - 1];
            if (index + 1 < width) product -= result.output[base + index + 1];
            auto right = ratio * states[base + index];
            if (index == 1) right += 1.0;
            const auto residual = right - product;
            residual_squared += residual * residual;
            right_squared += right * right;
        }
        result.maximum_relative_residual = std::max(
            result.maximum_relative_residual,
            std::sqrt(residual_squared) /
                std::max(1.0, std::sqrt(right_squared)));
    }
    result.verified = result.maximum_relative_residual <= residual_tolerance;
    result.reason = result.verified
        ? "Metal retardation completed and FP64 original residual gate passed"
        : "Metal retardation completed but FP64 original residual gate failed";
    return result;
#endif
}

bool coreml_neural_engine_available() {
#if !defined(SMAVE_HAVE_METAL_RUNTIME)
    return false;
#else
    AutoreleasePool pool;
    void* framework = coreml_runtime_handle();
    if (framework == nullptr) return false;
    const auto model_class = reinterpret_cast<Object>(objc_getClass("MLModel"));
    const auto neural_class = reinterpret_cast<Object>(
        objc_getClass("MLNeuralEngineComputeDevice"));
    if (model_class == nullptr || neural_class == nullptr) {
        return false;
    }
    const auto devices = send<Object>(model_class, "availableComputeDevices");
    const auto count = devices != nullptr
        ? send<unsigned long>(devices, "count")
        : 0UL;
    bool available{};
    for (unsigned long index = 0; index < count; ++index) {
        const auto device = send<Object>(devices, "objectAtIndex:", index);
        if (send<bool>(device, "isKindOfClass:", neural_class)) {
            available = true;
            break;
        }
    }
    return available;
#endif
}

DeviceExecutionResult coreml_neural_engine_affine(
    const std::vector<float>& input,
    const std::vector<float>& weights,
    std::size_t output_width,
    const std::vector<float>& bias,
    const std::filesystem::path& working_directory,
    double absolute_tolerance,
    double relative_tolerance) {
    DeviceExecutionResult result;
    result.backend = "coreml-affine-neural-engine-v1";
    result.batch = 1;
    result.input_width = input.size();
    result.output_width = output_width;
    if (input.empty() || output_width == 0 ||
        weights.size() != input.size() * output_width ||
        bias.size() != output_width || !(absolute_tolerance > 0.0) ||
        !(relative_tolerance > 0.0)) {
        result.reason = "invalid CoreML affine shape or tolerance contract";
        return result;
    }
#if !defined(SMAVE_HAVE_METAL_RUNTIME)
    result.reason = "CoreML runtime is unavailable in this build";
    return result;
#else
    AutoreleasePool pool;
    void* framework = coreml_runtime_handle();
    if (framework == nullptr) {
        result.reason = "CoreML framework could not be loaded";
        return result;
    }
    result.available = coreml_neural_engine_available();
    result.device_name = result.available ? "Apple Neural Engine" : std::string{};
    if (!result.available) {
        result.reason = "CoreML exposes no Neural Engine compute device";
        return result;
    }
    const auto upload_started = Clock::now();
    const auto bytes = coreml_affine_model(
        input.size(), output_width, weights, bias);
    const auto cache_key = coreml_cache_key(working_directory, bytes);
    const auto model_class = reinterpret_cast<Object>(objc_getClass("MLModel"));
    Object error{};
    std::shared_ptr<CoreMLCachedModel> cached_model;
    {
        std::lock_guard lock(coreml_model_cache_mutex());
        const auto existing = coreml_model_cache().find(cache_key);
        if (existing != coreml_model_cache().end()) {
            cached_model = existing->second;
        } else {
            std::filesystem::create_directories(working_directory);
            const auto model_path = working_directory / "smave-affine.mlmodel";
            {
                std::ofstream output(model_path, std::ios::binary);
                if (!output) {
                    result.reason = "cannot write generated CoreML model";
                    return result;
                }
                output.write(reinterpret_cast<const char*>(bytes.data()),
                             static_cast<std::streamsize>(bytes.size()));
            }
            const auto compiled_url = send<Object>(
                model_class, "compileModelAtURL:error:", file_url(model_path), &error);
            if (compiled_url == nullptr) {
                result.reason = "CoreML model compilation failed: " +
                    error_description(error);
                return result;
            }
            const auto configuration = send<Object>(
                reinterpret_cast<Object>(objc_getClass("MLModelConfiguration")), "new");
            send<Object>(configuration, "autorelease");
            constexpr long cpu_and_neural_engine = 3;
            send<void>(configuration, "setComputeUnits:", cpu_and_neural_engine);
            __block Object compute_plan{};
            __block Object compute_plan_error{};
            dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
            const auto compute_plan_class = reinterpret_cast<Object>(
                objc_getClass("MLComputePlan"));
            send<void>(
                compute_plan_class,
                "loadContentsOfURL:configuration:completionHandler:",
                compiled_url, configuration,
                ^(Object plan, Object plan_error) {
                    compute_plan = plan != nullptr
                        ? send<Object>(plan, "retain")
                        : nullptr;
                    compute_plan_error = plan_error != nullptr
                        ? send<Object>(plan_error, "retain")
                        : nullptr;
                    dispatch_semaphore_signal(semaphore);
                });
            dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
            if (compute_plan != nullptr) send<Object>(compute_plan, "autorelease");
            if (compute_plan_error != nullptr) {
                send<Object>(compute_plan_error, "autorelease");
            }
            if (compute_plan == nullptr) {
                result.reason = "CoreML compute plan failed: " +
                    error_description(compute_plan_error);
                return result;
            }
            const auto structure = send<Object>(compute_plan, "modelStructure");
            const auto network = structure != nullptr
                ? send<Object>(structure, "neuralNetwork")
                : nullptr;
            const auto layers = network != nullptr
                ? send<Object>(network, "layers")
                : nullptr;
            const auto layer_count = layers != nullptr
                ? send<unsigned long>(layers, "count")
                : 0UL;
            bool neural_engine_preferred{};
            const auto neural_class = reinterpret_cast<Object>(
                objc_getClass("MLNeuralEngineComputeDevice"));
            for (unsigned long index = 0; index < layer_count; ++index) {
                const auto layer = send<Object>(layers, "objectAtIndex:", index);
                const auto usage = send<Object>(
                    compute_plan,
                    "computeDeviceUsageForNeuralNetworkLayer:", layer);
                const auto preferred = usage != nullptr
                    ? send<Object>(usage, "preferredComputeDevice")
                    : nullptr;
                if (preferred != nullptr &&
                    send<bool>(preferred, "isKindOfClass:", neural_class)) {
                    neural_engine_preferred = true;
                }
            }
            if (!neural_engine_preferred) {
                result.reason =
                    "CoreML compute plan does not prefer Neural Engine for affine layer";
                return result;
            }
            const auto model = send<Object>(
                model_class, "modelWithContentsOfURL:configuration:error:",
                compiled_url, configuration, &error);
            if (model == nullptr) {
                result.reason = "CoreML model load failed: " +
                    error_description(error);
                return result;
            }
            cached_model = std::make_shared<CoreMLCachedModel>();
            cached_model->model = send<Object>(model, "retain");
            coreml_model_cache().emplace(cache_key, cached_model);
        }
    }
    const auto number_class = reinterpret_cast<Object>(objc_getClass("NSNumber"));
    const auto array_class = reinterpret_cast<Object>(objc_getClass("NSArray"));
    const auto shape_value = send<Object>(
        number_class, "numberWithUnsignedLong:",
        static_cast<unsigned long>(input.size()));
    const auto shape = send<Object>(array_class, "arrayWithObject:", shape_value);
    const auto multi_array = send<Object>(
        send<Object>(reinterpret_cast<Object>(objc_getClass("MLMultiArray")), "alloc"),
        "initWithShape:dataType:error:", shape, 65568L, &error);
    if (multi_array != nullptr) send<Object>(multi_array, "autorelease");
    if (multi_array == nullptr) {
        result.reason = "CoreML input allocation failed: " + error_description(error);
        return result;
    }
    auto* input_data = static_cast<float*>(send<void*>(multi_array, "dataPointer"));
    std::copy(input.begin(), input.end(), input_data);
    const auto feature_value = send<Object>(
        reinterpret_cast<Object>(objc_getClass("MLFeatureValue")),
        "featureValueWithMultiArray:", multi_array);
    const auto dictionary = send<Object>(
        reinterpret_cast<Object>(objc_getClass("NSDictionary")),
        "dictionaryWithObject:forKey:", feature_value, make_string("input"));
    const auto provider = send<Object>(
        send<Object>(reinterpret_cast<Object>(
            objc_getClass("MLDictionaryFeatureProvider")), "alloc"),
        "initWithDictionary:error:", dictionary, &error);
    if (provider != nullptr) send<Object>(provider, "autorelease");
    result.upload_us = elapsed_us(upload_started);
    const auto started = Clock::now();
    const auto prediction = provider != nullptr
        ? send<Object>(
              cached_model->model,
              "predictionFromFeatures:error:", provider, &error)
        : nullptr;
    result.kernel_us = elapsed_us(started);
    if (prediction == nullptr) {
        result.reason = "CoreML Neural Engine prediction failed: " +
            error_description(error);
        return result;
    }
    const auto output_feature = send<Object>(
        prediction, "featureValueForName:", make_string("output"));
    const auto output_array = output_feature != nullptr
        ? send<Object>(output_feature, "multiArrayValue")
        : nullptr;
    auto* output_data = output_array != nullptr
        ? static_cast<float*>(send<void*>(output_array, "dataPointer"))
        : nullptr;
    if (output_data == nullptr) {
        result.reason = "CoreML prediction returned no output multi-array";
        return result;
    }
    const auto download_started = Clock::now();
    result.output.assign(output_data, output_data + output_width);
    result.download_us = elapsed_us(download_started);
    result.executed = true;
    for (std::size_t row = 0; row < output_width; ++row) {
        double reference = bias[row];
        for (std::size_t column = 0; column < input.size(); ++column) {
            reference += static_cast<double>(weights[row * input.size() + column]) *
                static_cast<double>(input[column]);
        }
        const double absolute_error = std::abs(result.output[row] - reference);
        result.maximum_absolute_error = std::max(
            result.maximum_absolute_error, absolute_error);
        result.maximum_relative_error = std::max(
            result.maximum_relative_error,
            absolute_error / std::max(1.0, std::abs(reference)));
    }
    result.verified = result.maximum_absolute_error <= absolute_tolerance ||
        result.maximum_relative_error <= relative_tolerance;
    result.reason = result.verified
        ? "CoreML compute plan preferred Neural Engine and fp64 reference gate passed"
        : "CoreML Neural Engine prediction failed fp64 reference gate";
    return result;
#endif
}

DeviceExecutionResult coreml_neural_engine_affine_batch(
    const std::vector<float>& inputs,
    std::size_t batch,
    std::size_t input_width,
    const std::vector<float>& weights,
    std::size_t output_width,
    const std::vector<float>& bias,
    const std::filesystem::path& working_directory,
    double absolute_tolerance,
    double relative_tolerance) {
    DeviceExecutionResult result;
    result.backend = "coreml-affine-neural-engine-batch-v2";
    result.batch = batch;
    result.input_width = input_width;
    result.output_width = output_width;
    if (batch == 0 || input_width == 0 || output_width == 0 ||
        inputs.size() != batch * input_width ||
        weights.size() != input_width * output_width ||
        bias.size() != output_width || !(absolute_tolerance > 0.0) ||
        !(relative_tolerance > 0.0)) {
        result.reason = "invalid CoreML affine batch shape or tolerance contract";
        return result;
    }
#if !defined(SMAVE_HAVE_METAL_RUNTIME)
    result.reason = "CoreML runtime is unavailable in this build";
    return result;
#else
    AutoreleasePool pool;
    const auto bytes = coreml_affine_model(
        input_width, output_width, weights, bias);
    const auto cache_key = coreml_cache_key(working_directory, bytes);
    std::shared_ptr<CoreMLCachedModel> cached_model;
    {
        std::lock_guard lock(coreml_model_cache_mutex());
        const auto existing = coreml_model_cache().find(cache_key);
        if (existing != coreml_model_cache().end()) {
            cached_model = existing->second;
        }
    }
    std::size_t first_batch_item{};
    if (!cached_model) {
        const std::vector<float> first_input(
            inputs.begin(),
            inputs.begin() + static_cast<std::ptrdiff_t>(input_width));
        const auto first = coreml_neural_engine_affine(
            first_input, weights, output_width, bias, working_directory,
            absolute_tolerance, relative_tolerance);
        result.available = first.available;
        result.device_name = first.device_name;
        result.upload_us += first.upload_us;
        result.kernel_us += first.kernel_us;
        result.download_us += first.download_us;
        result.maximum_absolute_error = first.maximum_absolute_error;
        result.maximum_relative_error = first.maximum_relative_error;
        if (!first.executed || !first.verified) {
            result.reason = first.reason;
            return result;
        }
        result.output = first.output;
        first_batch_item = 1;
        {
            std::lock_guard lock(coreml_model_cache_mutex());
            const auto existing = coreml_model_cache().find(cache_key);
            if (existing != coreml_model_cache().end()) {
                cached_model = existing->second;
            }
        }
        if (!cached_model) {
            result.reason = "CoreML verified model was not retained in cache";
            return result;
        }
    } else {
        result.available = true;
        result.device_name = "Apple Neural Engine";
    }
    if (first_batch_item < batch) {
        Object error{};
        const auto upload_started = Clock::now();
        const auto mutable_array_class = reinterpret_cast<Object>(
            objc_getClass("NSMutableArray"));
        const auto providers = send<Object>(
            mutable_array_class, "arrayWithCapacity:",
            static_cast<unsigned long>(batch - first_batch_item));
        const auto number_class = reinterpret_cast<Object>(objc_getClass("NSNumber"));
        const auto array_class = reinterpret_cast<Object>(objc_getClass("NSArray"));
        const auto shape_value = send<Object>(
            number_class, "numberWithUnsignedLong:",
            static_cast<unsigned long>(input_width));
        const auto shape = send<Object>(array_class, "arrayWithObject:", shape_value);
        for (std::size_t item = first_batch_item; item < batch; ++item) {
            const auto multi_array = send<Object>(
                send<Object>(reinterpret_cast<Object>(
                    objc_getClass("MLMultiArray")), "alloc"),
                "initWithShape:dataType:error:", shape, 65568L, &error);
            if (multi_array != nullptr) send<Object>(multi_array, "autorelease");
            if (multi_array == nullptr) {
                result.reason = "CoreML batch input allocation failed: " +
                    error_description(error);
                return result;
            }
            auto* input_data = static_cast<float*>(
                send<void*>(multi_array, "dataPointer"));
            std::copy(
                inputs.begin() + static_cast<std::ptrdiff_t>(item * input_width),
                inputs.begin() + static_cast<std::ptrdiff_t>((item + 1) * input_width),
                input_data);
            const auto feature_value = send<Object>(
                reinterpret_cast<Object>(objc_getClass("MLFeatureValue")),
                "featureValueWithMultiArray:", multi_array);
            const auto dictionary = send<Object>(
                reinterpret_cast<Object>(objc_getClass("NSDictionary")),
                "dictionaryWithObject:forKey:", feature_value,
                make_string("input"));
            const auto provider = send<Object>(
                send<Object>(reinterpret_cast<Object>(
                    objc_getClass("MLDictionaryFeatureProvider")), "alloc"),
                "initWithDictionary:error:", dictionary, &error);
            if (provider != nullptr) send<Object>(provider, "autorelease");
            if (provider == nullptr) {
                result.reason = "CoreML batch provider creation failed: " +
                    error_description(error);
                return result;
            }
            send<void>(providers, "addObject:", provider);
        }
        const auto batch_provider = send<Object>(
            send<Object>(reinterpret_cast<Object>(
                objc_getClass("MLArrayBatchProvider")), "alloc"),
            "initWithFeatureProviderArray:", providers);
        if (batch_provider != nullptr) send<Object>(batch_provider, "autorelease");
        if (batch_provider == nullptr) {
            result.reason = "CoreML array batch provider creation failed";
            return result;
        }
        result.upload_us += elapsed_us(upload_started);
        const auto kernel_started = Clock::now();
        const auto predictions = send<Object>(
            cached_model->model, "predictionsFromBatch:error:",
            batch_provider, &error);
        result.kernel_us += elapsed_us(kernel_started);
        if (predictions == nullptr) {
            result.reason = "CoreML Neural Engine batch prediction failed: " +
                error_description(error);
            return result;
        }
        const auto prediction_count = send<long>(predictions, "count");
        if (prediction_count != static_cast<long>(batch - first_batch_item)) {
            result.reason = "CoreML batch prediction count mismatch";
            return result;
        }
        const auto download_started = Clock::now();
        result.output.reserve(batch * output_width);
        for (long item = 0; item < prediction_count; ++item) {
            const auto prediction = send<Object>(
                predictions, "featuresAtIndex:", item);
            const auto output_feature = prediction != nullptr
                ? send<Object>(prediction, "featureValueForName:", make_string("output"))
                : nullptr;
            const auto output_array = output_feature != nullptr
                ? send<Object>(output_feature, "multiArrayValue")
                : nullptr;
            auto* output_data = output_array != nullptr
                ? static_cast<float*>(send<void*>(output_array, "dataPointer"))
                : nullptr;
            if (output_data == nullptr) {
                result.reason = "CoreML batch prediction returned no output";
                return result;
            }
            result.output.insert(
                result.output.end(), output_data, output_data + output_width);
        }
        result.download_us += elapsed_us(download_started);
    }
    result.executed = result.output.size() == batch * output_width;
    if (!result.executed) {
        result.reason = "CoreML batch output size mismatch";
        return result;
    }
    for (std::size_t item = 0; item < batch; ++item) {
        for (std::size_t row = 0; row < output_width; ++row) {
            double reference = bias[row];
            for (std::size_t column = 0; column < input_width; ++column) {
                reference += static_cast<double>(
                    weights[row * input_width + column]) *
                    static_cast<double>(inputs[item * input_width + column]);
            }
            const auto value = result.output[item * output_width + row];
            const auto absolute_error = std::abs(value - reference);
            result.maximum_absolute_error = std::max(
                result.maximum_absolute_error, absolute_error);
            result.maximum_relative_error = std::max(
                result.maximum_relative_error,
                absolute_error / std::max(1.0, std::abs(reference)));
        }
    }
    result.verified = result.maximum_absolute_error <= absolute_tolerance ||
        result.maximum_relative_error <= relative_tolerance;
    result.reason = result.verified
        ? "CoreML batch reused an ANE-preferred model and passed fp64 reference gates"
        : "CoreML Neural Engine batch failed fp64 reference gates";
    return result;
#endif
}

DeviceExecutionResult coreml_neural_engine_affine_tensor_batch(
    const std::vector<float>& inputs,
    std::size_t batch,
    std::size_t input_width,
    const std::vector<float>& weights,
    std::size_t output_width,
    const std::vector<float>& bias,
    const std::filesystem::path& working_directory,
    double absolute_tolerance,
    double relative_tolerance) {
    DeviceExecutionResult result;
    result.backend = "coreml-affine-neural-engine-tensor-batch-v3";
    result.batch = batch;
    result.input_width = input_width;
    result.output_width = output_width;
    if (batch == 0 || input_width == 0 || output_width == 0 ||
        inputs.size() != batch * input_width ||
        weights.size() != input_width * output_width ||
        bias.size() != output_width || !(absolute_tolerance > 0.0) ||
        !(relative_tolerance > 0.0)) {
        result.reason =
            "invalid CoreML affine tensor batch shape or tolerance contract";
        return result;
    }
#if !defined(SMAVE_HAVE_METAL_RUNTIME)
    result.reason = "CoreML runtime is unavailable in this build";
    return result;
#else
    AutoreleasePool pool;
    result.available = coreml_neural_engine_available();
    result.device_name = result.available ? "Apple Neural Engine" : std::string{};
    if (!result.available) {
        result.reason = "CoreML exposes no Neural Engine compute device";
        return result;
    }
    const auto upload_started = Clock::now();
    const auto bytes = coreml_affine_tensor_batch_model(
        batch, input_width, output_width, weights, bias);
    const auto cached_model = load_coreml_cached_model(
        bytes, working_directory, result);
    if (!cached_model) return result;
    Object error{};
    const auto number_class = reinterpret_cast<Object>(objc_getClass("NSNumber"));
    const auto mutable_array = send<Object>(
        reinterpret_cast<Object>(objc_getClass("NSMutableArray")),
        "arrayWithCapacity:", 2UL);
    send<void>(mutable_array, "addObject:", send<Object>(
        number_class, "numberWithUnsignedLong:",
        static_cast<unsigned long>(batch)));
    send<void>(mutable_array, "addObject:", send<Object>(
        number_class, "numberWithUnsignedLong:",
        static_cast<unsigned long>(input_width)));
    const auto multi_array = send<Object>(
        send<Object>(reinterpret_cast<Object>(objc_getClass("MLMultiArray")), "alloc"),
        "initWithShape:dataType:error:", mutable_array, 65568L, &error);
    if (multi_array != nullptr) send<Object>(multi_array, "autorelease");
    if (multi_array == nullptr) {
        result.reason = "CoreML tensor batch input allocation failed: " +
            error_description(error);
        return result;
    }
    auto* input_data = static_cast<float*>(send<void*>(multi_array, "dataPointer"));
    std::copy(inputs.begin(), inputs.end(), input_data);
    const auto feature_value = send<Object>(
        reinterpret_cast<Object>(objc_getClass("MLFeatureValue")),
        "featureValueWithMultiArray:", multi_array);
    const auto dictionary = send<Object>(
        reinterpret_cast<Object>(objc_getClass("NSDictionary")),
        "dictionaryWithObject:forKey:", feature_value, make_string("input"));
    const auto provider = send<Object>(
        send<Object>(reinterpret_cast<Object>(
            objc_getClass("MLDictionaryFeatureProvider")), "alloc"),
        "initWithDictionary:error:", dictionary, &error);
    if (provider != nullptr) send<Object>(provider, "autorelease");
    if (provider == nullptr) {
        result.reason = "CoreML tensor batch provider creation failed: " +
            error_description(error);
        return result;
    }
    result.upload_us = elapsed_us(upload_started);
    const auto kernel_started = Clock::now();
    const auto prediction = send<Object>(
        cached_model->model, "predictionFromFeatures:error:", provider, &error);
    result.kernel_us = elapsed_us(kernel_started);
    if (prediction == nullptr) {
        result.reason = "CoreML Neural Engine tensor batch prediction failed: " +
            error_description(error);
        return result;
    }
    const auto download_started = Clock::now();
    const auto output_feature = send<Object>(
        prediction, "featureValueForName:", make_string("output"));
    const auto output_array = output_feature != nullptr
        ? send<Object>(output_feature, "multiArrayValue")
        : nullptr;
    auto* output_data = output_array != nullptr
        ? static_cast<float*>(send<void*>(output_array, "dataPointer"))
        : nullptr;
    if (output_data == nullptr) {
        result.reason = "CoreML tensor batch prediction returned no output";
        return result;
    }
    result.output.assign(
        output_data, output_data + batch * output_width);
    result.download_us = elapsed_us(download_started);
    result.executed = true;
    verify_affine_batch_reference(
        inputs, batch, input_width, weights, output_width, bias,
        result, absolute_tolerance, relative_tolerance);
    result.reason = result.verified
        ? "CoreML tensor batch executed one ANE-preferred graph and passed fp64 gates"
        : "CoreML Neural Engine tensor batch failed fp64 reference gates";
    return result;
#endif
}

bool coreml_neural_engine_affine_tensor_batch_is_resident(
    std::size_t batch,
    std::size_t input_width,
    const std::vector<float>& weights,
    std::size_t output_width,
    const std::vector<float>& bias,
    const std::filesystem::path& working_directory) {
#if !defined(SMAVE_HAVE_METAL_RUNTIME)
    static_cast<void>(batch);
    static_cast<void>(input_width);
    static_cast<void>(weights);
    static_cast<void>(output_width);
    static_cast<void>(bias);
    static_cast<void>(working_directory);
    return false;
#else
    if (batch == 0 || input_width == 0 || output_width == 0 ||
        weights.size() != input_width * output_width ||
        bias.size() != output_width) return false;
    const auto bytes = coreml_affine_tensor_batch_model(
        batch, input_width, output_width, weights, bias);
    const auto key = coreml_cache_key(working_directory, bytes);
    std::lock_guard lock(coreml_model_cache_mutex());
    return coreml_model_cache().contains(key);
#endif
}

StructuredDeviceSolveResult accelerate_periodic_helmholtz_2d_batch(
    const std::vector<double>& right_hand_sides,
    std::size_t batch,
    std::size_t width,
    double diffusion_number) {
    StructuredDeviceSolveResult result;
    result.backend = "accelerate-vdsp-real-gcd-periodic-helmholtz-2d-fp64-v3";
    result.device_name = "Apple Accelerate/vDSP";
    result.batch = batch;
    result.width = width;
#if defined(SMAVE_HAVE_ACCELERATE_SPARSE)
    result.available = true;
    if (batch == 0 || width < 2 || (width & (width - 1)) != 0 ||
        right_hand_sides.size() != batch * width * width ||
        !std::isfinite(diffusion_number) || !(diffusion_number >= 0.0) ||
        !std::all_of(right_hand_sides.begin(), right_hand_sides.end(), [](double value) {
            return std::isfinite(value);
        })) {
        result.reason = "invalid Accelerate periodic Helmholtz batch";
        return result;
    }
    const auto setup_started = Clock::now();
    std::size_t log2_width{};
    for (auto value = width; value > 1; value >>= 1U) ++log2_width;
    const auto plane = width * width;
    const auto packed_width = width / 2;
    const auto packed_plane = plane / 2;
    std::vector<double> inverse_eigenvalues(plane);
    constexpr double pi = 3.141592653589793238462643383279502884;
    for (std::size_t row = 0; row < width; ++row) {
        const auto row_angle = 2.0 * pi * static_cast<double>(row) /
            static_cast<double>(width);
        for (std::size_t column = 0; column < width; ++column) {
            const auto column_angle = 2.0 * pi * static_cast<double>(column) /
                static_cast<double>(width);
            const auto eigenvalue = 1.0 + diffusion_number *
                (4.0 - 2.0 * std::cos(row_angle) - 2.0 * std::cos(column_angle));
            inverse_eigenvalues[row * width + column] = 1.0 / eigenvalue;
        }
    }
    std::vector<double> packed_real_scales(packed_plane);
    std::vector<double> packed_imaginary_scales(packed_plane);
    const auto nyquist = width / 2;
    packed_real_scales[0] = inverse_eigenvalues[0];
    packed_imaginary_scales[0] = inverse_eigenvalues[nyquist];
    packed_real_scales[packed_width] =
        inverse_eigenvalues[nyquist * width];
    packed_imaginary_scales[packed_width] =
        inverse_eigenvalues[nyquist * width + nyquist];
    for (std::size_t row = 1; row < nyquist; ++row) {
        const auto even = 2 * row * packed_width;
        const auto odd = even + packed_width;
        packed_real_scales[even] = packed_real_scales[odd] =
            inverse_eigenvalues[row * width];
        packed_imaginary_scales[even] = packed_imaginary_scales[odd] =
            inverse_eigenvalues[row * width + nyquist];
    }
    for (std::size_t row = 0; row < width; ++row) {
        for (std::size_t column = 1; column < packed_width; ++column) {
            const auto packed = row * packed_width + column;
            packed_real_scales[packed] =
                inverse_eigenvalues[row * width + column];
            packed_imaginary_scales[packed] = packed_real_scales[packed];
        }
    }
    result.output.resize(right_hand_sides.size());
    const auto hardware_workers = std::max(1U, std::thread::hardware_concurrency());
    const auto worker_count = std::min<std::size_t>(batch, hardware_workers);
    struct Workspace {
        FFTSetupD setup{};
        std::vector<double> real;
        std::vector<double> imaginary;

        ~Workspace() {
            if (setup != nullptr) vDSP_destroy_fftsetupD(setup);
        }
    };
    std::vector<Workspace> workspaces(worker_count);
    for (auto& workspace : workspaces) {
        workspace.setup = vDSP_create_fftsetupD(
            static_cast<vDSP_Length>(log2_width), FFT_RADIX2);
        if (workspace.setup == nullptr) {
            result.reason = "vDSP FFT setup creation failed";
            return result;
        }
        workspace.real.resize(packed_plane);
        workspace.imaginary.resize(packed_plane);
    }
    result.setup_us = elapsed_us(setup_started);
    const auto kernel_started = Clock::now();
#if defined(__APPLE__)
    struct DispatchContext {
        const std::vector<double>* right_hand_sides;
        const std::vector<double>* packed_real_scales;
        const std::vector<double>* packed_imaginary_scales;
        std::vector<double>* output;
        std::vector<Workspace>* workspaces;
        std::size_t batch;
        std::size_t worker_count;
        std::size_t width;
        std::size_t plane;
        std::size_t packed_width;
        std::size_t packed_plane;
        std::size_t log2_width;
    } context{
        &right_hand_sides, &packed_real_scales, &packed_imaginary_scales,
        &result.output, &workspaces,
        batch, worker_count, width, plane, packed_width, packed_plane,
        log2_width};
    dispatch_apply_f(
        worker_count, dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0),
        &context, [](void* raw_context, std::size_t worker) {
            auto& state = *static_cast<DispatchContext*>(raw_context);
            auto& workspace = (*state.workspaces)[worker];
            DSPDoubleSplitComplex spectrum{
                workspace.real.data(), workspace.imaginary.data()};
            for (std::size_t item = worker; item < state.batch;
                 item += state.worker_count) {
                const auto offset = item * state.plane;
                for (std::size_t index = 0; index < state.packed_plane; ++index) {
                    workspace.real[index] =
                        (*state.right_hand_sides)[offset + 2 * index];
                    workspace.imaginary[index] =
                        (*state.right_hand_sides)[offset + 2 * index + 1];
                }
                vDSP_fft2d_zripD(
                    workspace.setup, &spectrum, 1,
                    static_cast<vDSP_Stride>(state.packed_width),
                    static_cast<vDSP_Length>(state.log2_width),
                    static_cast<vDSP_Length>(state.log2_width), FFT_FORWARD);
                for (std::size_t index = 0; index < state.packed_plane; ++index) {
                    workspace.real[index] *= (*state.packed_real_scales)[index];
                    workspace.imaginary[index] *=
                        (*state.packed_imaginary_scales)[index];
                }
                vDSP_fft2d_zripD(
                    workspace.setup, &spectrum, 1,
                    static_cast<vDSP_Stride>(state.packed_width),
                    static_cast<vDSP_Length>(state.log2_width),
                    static_cast<vDSP_Length>(state.log2_width), FFT_INVERSE);
                const auto inverse_plane =
                    1.0 / (2.0 * static_cast<double>(state.plane));
                for (std::size_t index = 0; index < state.packed_plane; ++index) {
                    (*state.output)[offset + 2 * index] =
                        workspace.real[index] * inverse_plane;
                    (*state.output)[offset + 2 * index + 1] =
                        workspace.imaginary[index] * inverse_plane;
                }
            }
        });
#else
    std::vector<std::thread> workers;
    workers.reserve(worker_count);
    for (std::size_t worker = 0; worker < worker_count; ++worker) {
        workers.emplace_back([&, worker] {
            auto& workspace = workspaces[worker];
            DSPDoubleSplitComplex spectrum{
                workspace.real.data(), workspace.imaginary.data()};
            for (std::size_t item = worker; item < batch; item += worker_count) {
                const auto offset = item * plane;
                std::copy_n(right_hand_sides.data() + offset, plane,
                    workspace.real.data());
                std::fill(workspace.imaginary.begin(),
                    workspace.imaginary.end(), 0.0);
                vDSP_fft2d_zipD(
                    workspace.setup, &spectrum, 1, 0,
                    static_cast<vDSP_Length>(log2_width),
                    static_cast<vDSP_Length>(log2_width), FFT_FORWARD);
                for (std::size_t index = 0; index < plane; ++index) {
                    workspace.real[index] *= inverse_eigenvalues[index];
                    workspace.imaginary[index] *= inverse_eigenvalues[index];
                }
                vDSP_fft2d_zipD(
                    workspace.setup, &spectrum, 1, 0,
                    static_cast<vDSP_Length>(log2_width),
                    static_cast<vDSP_Length>(log2_width), FFT_INVERSE);
                const auto inverse_plane = 1.0 / static_cast<double>(plane);
                for (std::size_t index = 0; index < plane; ++index) {
                    result.output[offset + index] =
                        workspace.real[index] * inverse_plane;
                }
            }
        });
    }
    for (auto& worker : workers) worker.join();
#endif
    result.kernel_us = elapsed_us(kernel_started);
    result.executed = true;
    result.verified = std::all_of(result.output.begin(), result.output.end(), [](double value) {
        return std::isfinite(value);
    });
    result.reason = result.verified ? "completed" : "non-finite spectral solution";
#else
    (void)right_hand_sides;
    (void)diffusion_number;
    result.reason = "Apple Accelerate is unavailable";
#endif
    return result;
}

struct AcceleratePeriodicHelmholtz2DPlan::Impl {
    struct Workspace {
        std::vector<double> real;
        std::vector<double> imaginary;
    };

    std::size_t width{};
    std::size_t plane{};
    std::size_t packed_width{};
    std::size_t packed_plane{};
    std::size_t log2_width{};
    double setup_us{};
    std::string reason;
    std::vector<double> inverse_eigenvalues;
    std::vector<double> packed_real_scales;
    std::vector<double> packed_imaginary_scales;
    std::vector<Workspace> workspaces;
#if defined(SMAVE_HAVE_ACCELERATE_SPARSE)
    FFTSetupD setup{};
#endif

    ~Impl() {
#if defined(SMAVE_HAVE_ACCELERATE_SPARSE)
        if (setup != nullptr) vDSP_destroy_fftsetupD(setup);
#endif
    }
};

AcceleratePeriodicHelmholtz2DPlan::AcceleratePeriodicHelmholtz2DPlan(
    std::size_t width,
    double diffusion_number)
    : impl_(std::make_unique<Impl>()) {
    impl_->width = width;
    impl_->plane = width * width;
    impl_->packed_width = width / 2;
    impl_->packed_plane = impl_->plane / 2;
#if defined(SMAVE_HAVE_ACCELERATE_SPARSE)
    const auto started = Clock::now();
    if (width < 2 || (width & (width - 1)) != 0 ||
        !std::isfinite(diffusion_number) || diffusion_number < 0.0) {
        impl_->reason = "invalid periodic Helmholtz plan parameters";
        return;
    }
    for (auto value = width; value > 1; value >>= 1U) ++impl_->log2_width;
    impl_->inverse_eigenvalues.resize(impl_->plane);
    constexpr double pi = 3.141592653589793238462643383279502884;
    for (std::size_t row = 0; row < width; ++row) {
        const auto row_angle = 2.0 * pi * static_cast<double>(row) /
            static_cast<double>(width);
        for (std::size_t column = 0; column < width; ++column) {
            const auto column_angle = 2.0 * pi * static_cast<double>(column) /
                static_cast<double>(width);
            const auto eigenvalue = 1.0 + diffusion_number *
                (4.0 - 2.0 * std::cos(row_angle) - 2.0 * std::cos(column_angle));
            impl_->inverse_eigenvalues[row * width + column] = 1.0 / eigenvalue;
        }
    }
    impl_->packed_real_scales.resize(impl_->packed_plane);
    impl_->packed_imaginary_scales.resize(impl_->packed_plane);
    const auto nyquist = width / 2;
    impl_->packed_real_scales[0] = impl_->inverse_eigenvalues[0];
    impl_->packed_imaginary_scales[0] = impl_->inverse_eigenvalues[nyquist];
    impl_->packed_real_scales[impl_->packed_width] =
        impl_->inverse_eigenvalues[nyquist * width];
    impl_->packed_imaginary_scales[impl_->packed_width] =
        impl_->inverse_eigenvalues[nyquist * width + nyquist];
    for (std::size_t row = 1; row < nyquist; ++row) {
        const auto even = 2 * row * impl_->packed_width;
        const auto odd = even + impl_->packed_width;
        impl_->packed_real_scales[even] = impl_->packed_real_scales[odd] =
            impl_->inverse_eigenvalues[row * width];
        impl_->packed_imaginary_scales[even] =
            impl_->packed_imaginary_scales[odd] =
                impl_->inverse_eigenvalues[row * width + nyquist];
    }
    for (std::size_t row = 0; row < width; ++row) {
        for (std::size_t column = 1; column < impl_->packed_width; ++column) {
            const auto packed = row * impl_->packed_width + column;
            impl_->packed_real_scales[packed] =
                impl_->inverse_eigenvalues[row * width + column];
            impl_->packed_imaginary_scales[packed] =
                impl_->packed_real_scales[packed];
        }
    }
    const auto worker_count = std::max(1U, std::thread::hardware_concurrency());
    impl_->setup = vDSP_create_fftsetupD(
        static_cast<vDSP_Length>(impl_->log2_width), FFT_RADIX2);
    if (impl_->setup == nullptr) {
        impl_->reason = "vDSP FFT setup creation failed";
        return;
    }
    impl_->workspaces.resize(worker_count);
    for (auto& workspace : impl_->workspaces) {
        workspace.real.resize(impl_->packed_plane);
        workspace.imaginary.resize(impl_->packed_plane);
    }
    impl_->setup_us = elapsed_us(started);
    impl_->reason = "ready";
#else
    (void)diffusion_number;
    impl_->reason = "Apple Accelerate is unavailable";
#endif
}

AcceleratePeriodicHelmholtz2DPlan::~AcceleratePeriodicHelmholtz2DPlan() = default;
AcceleratePeriodicHelmholtz2DPlan::AcceleratePeriodicHelmholtz2DPlan(
    AcceleratePeriodicHelmholtz2DPlan&&) noexcept = default;
AcceleratePeriodicHelmholtz2DPlan&
AcceleratePeriodicHelmholtz2DPlan::operator=(
    AcceleratePeriodicHelmholtz2DPlan&&) noexcept = default;

bool AcceleratePeriodicHelmholtz2DPlan::available() const {
#if defined(SMAVE_HAVE_ACCELERATE_SPARSE)
    return impl_ != nullptr && impl_->setup != nullptr &&
        !impl_->workspaces.empty();
#else
    return false;
#endif
}

const std::string& AcceleratePeriodicHelmholtz2DPlan::reason() const {
    return impl_->reason;
}

double AcceleratePeriodicHelmholtz2DPlan::setup_us() const {
    return impl_->setup_us;
}

bool AcceleratePeriodicHelmholtz2DPlan::solve(
    const std::vector<double>& right_hand_side,
    std::vector<double>& solution,
    double* kernel_us) {
#if defined(SMAVE_HAVE_ACCELERATE_SPARSE)
    if (!available() || right_hand_side.size() != impl_->plane ||
        !std::all_of(right_hand_side.begin(), right_hand_side.end(), [](double value) {
            return std::isfinite(value);
        })) {
        return false;
    }
    return solve_batch(right_hand_side, 1, solution, kernel_us);
#else
    (void)right_hand_side;
    (void)solution;
    (void)kernel_us;
    return false;
#endif
}

bool AcceleratePeriodicHelmholtz2DPlan::solve_batch(
    const std::vector<double>& right_hand_sides,
    std::size_t batch,
    std::vector<double>& solutions,
    double* kernel_us) {
#if defined(SMAVE_HAVE_ACCELERATE_SPARSE)
    if (!available() || batch == 0 ||
        right_hand_sides.size() != batch * impl_->plane ||
        !std::all_of(right_hand_sides.begin(), right_hand_sides.end(),
                     [](double value) { return std::isfinite(value); })) {
        return false;
    }
    solutions.resize(right_hand_sides.size());
    const auto started = Clock::now();
    const auto worker_count = impl_->plane <= 1024 && batch <= 4
        ? std::size_t{1}
        : std::min(batch, impl_->workspaces.size());
    const auto run_worker = [&](std::size_t worker) {
        auto& workspace = impl_->workspaces[worker];
        const auto scale = 1.0 / (2.0 * static_cast<double>(impl_->plane));
        for (std::size_t solve = worker; solve < batch; solve += worker_count) {
            const auto offset = solve * impl_->plane;
            for (std::size_t index = 0; index < impl_->packed_plane; ++index) {
                workspace.real[index] = right_hand_sides[offset + 2 * index];
                workspace.imaginary[index] = right_hand_sides[offset + 2 * index + 1];
            }
            DSPDoubleSplitComplex spectrum{
                workspace.real.data(), workspace.imaginary.data()};
            vDSP_fft2d_zripD(
                impl_->setup, &spectrum, 1,
                static_cast<vDSP_Stride>(impl_->packed_width),
                static_cast<vDSP_Length>(impl_->log2_width),
                static_cast<vDSP_Length>(impl_->log2_width), FFT_FORWARD);
            vDSP_vmulD(
                workspace.real.data(), 1, impl_->packed_real_scales.data(), 1,
                workspace.real.data(), 1,
                static_cast<vDSP_Length>(impl_->packed_plane));
            vDSP_vmulD(
                workspace.imaginary.data(), 1,
                impl_->packed_imaginary_scales.data(), 1,
                workspace.imaginary.data(), 1,
                static_cast<vDSP_Length>(impl_->packed_plane));
            vDSP_fft2d_zripD(
                impl_->setup, &spectrum, 1,
                static_cast<vDSP_Stride>(impl_->packed_width),
                static_cast<vDSP_Length>(impl_->log2_width),
                static_cast<vDSP_Length>(impl_->log2_width), FFT_INVERSE);
            for (std::size_t index = 0; index < impl_->packed_plane; ++index) {
                solutions[offset + 2 * index] = workspace.real[index] * scale;
                solutions[offset + 2 * index + 1] =
                    workspace.imaginary[index] * scale;
            }
        }
    };
#if defined(__APPLE__)
    struct Context {
        const decltype(run_worker)* worker;
    } context{&run_worker};
    dispatch_apply_f(
        worker_count, dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0),
        &context, [](void* raw_context, std::size_t worker) {
            const auto& context = *static_cast<Context*>(raw_context);
            (*context.worker)(worker);
        });
#else
    std::vector<std::thread> workers;
    workers.reserve(worker_count);
    for (std::size_t worker = 0; worker < worker_count; ++worker) {
        workers.emplace_back(run_worker, worker);
    }
    for (auto& worker : workers) worker.join();
#endif
    if (kernel_us != nullptr) *kernel_us += elapsed_us(started);
    return std::all_of(solutions.begin(), solutions.end(), [](double value) {
        return std::isfinite(value);
    });
#else
    (void)right_hand_sides;
    (void)batch;
    (void)solutions;
    (void)kernel_us;
    return false;
#endif
}

#if defined(SMAVE_HAVE_CUDA_RUNTIME)

namespace {

struct CudaRuntime {
    bool available{false};
    std::string device_name;
    std::string error;

    CudaRuntime() {
        int device_count = 0;
        const auto status = cudaGetDeviceCount(&device_count);
        if (status != cudaSuccess || device_count == 0) {
            error = "no CUDA device found";
            return;
        }
        cudaDeviceProp properties{};
        if (cudaGetDeviceProperties(&properties, 0) != cudaSuccess) {
            error = "cudaGetDeviceProperties failed";
            return;
        }
        device_name = properties.name;
        available = true;
    }
};

CudaRuntime& cuda_runtime() {
    static CudaRuntime runtime;
    return runtime;
}

}  // namespace

bool cuda_gpu_available() {
    return cuda_runtime().available;
}

std::string cuda_gpu_device_name() {
    return cuda_runtime().device_name;
}

DeviceExecutionResult cuda_gpu_affine_batch(
    const std::vector<float>& inputs,
    std::size_t batch,
    std::size_t input_width,
    const std::vector<float>& weights,
    std::size_t output_width,
    const std::vector<float>& bias,
    double absolute_tolerance,
    double relative_tolerance) {
    DeviceExecutionResult result;
    result.backend = "cuda-affine-batch-gpu-v1";
    result.batch = batch;
    result.input_width = input_width;
    result.output_width = output_width;
    if (batch == 0 || input_width == 0 || output_width == 0 ||
        inputs.size() != batch * input_width ||
        weights.size() != input_width * output_width ||
        bias.size() != output_width ||
        !(absolute_tolerance > 0.0) || !std::isfinite(absolute_tolerance) ||
        !(relative_tolerance > 0.0) || !std::isfinite(relative_tolerance)) {
        result.reason = "invalid CUDA affine batch shape or numeric contract";
        return result;
    }
    const auto finite = [](const std::vector<float>& values) {
        return std::all_of(values.begin(), values.end(), [](float value) {
            return std::isfinite(value);
        });
    };
    if (!finite(inputs) || !finite(weights) || !finite(bias)) {
        result.reason = "CUDA affine inputs must be finite";
        return result;
    }
    auto& runtime = cuda_runtime();
    if (!runtime.available) {
        result.available = false;
        result.reason = "CUDA runtime unavailable: " + runtime.error;
        return result;
    }
    result.available = true;
    result.device_name = runtime.device_name;

    float* device_inputs = nullptr;
    float* device_weights = nullptr;
    float* device_bias = nullptr;
    float* device_outputs = nullptr;
    const auto inputs_bytes = inputs.size() * sizeof(float);
    const auto weights_bytes = weights.size() * sizeof(float);
    const auto bias_bytes = bias.size() * sizeof(float);
    const auto outputs_bytes = batch * output_width * sizeof(float);
    auto allocate = [&](float** pointer, std::size_t bytes) {
        return cudaMalloc(reinterpret_cast<void**>(pointer), bytes) == cudaSuccess;
    };
    if (!allocate(&device_inputs, inputs_bytes) ||
        !allocate(&device_weights, weights_bytes) ||
        !allocate(&device_bias, bias_bytes) ||
        !allocate(&device_outputs, outputs_bytes)) {
        result.reason = "CUDA allocation failed";
        cudaFree(device_inputs); cudaFree(device_weights);
        cudaFree(device_bias); cudaFree(device_outputs);
        return result;
    }
    const auto upload_started = Clock::now();
    auto copy_to = [](float* dst, const float* src, std::size_t bytes) {
        return cudaMemcpy(dst, src, bytes, cudaMemcpyHostToDevice) == cudaSuccess;
    };
    const bool uploads_ok =
        copy_to(device_inputs, inputs.data(), inputs_bytes) &&
        copy_to(device_weights, weights.data(), weights_bytes) &&
        copy_to(device_bias, bias.data(), bias_bytes);
    result.upload_us = elapsed_us(upload_started);
    if (!uploads_ok) {
        result.reason = "CUDA upload memcpy failed";
        cudaFree(device_inputs); cudaFree(device_weights);
        cudaFree(device_bias); cudaFree(device_outputs);
        return result;
    }

    const auto kernel_started = Clock::now();
    const unsigned int block_size = 256;
    const unsigned int grid_size = static_cast<unsigned int>(
        (batch * output_width + block_size - 1) / block_size);
    {
        const auto total_threads = batch * output_width;
        const auto i_width = input_width;
        const auto o_width = output_width;
        const float* in_ptr = device_inputs;
        const float* w_ptr = device_weights;
        const float* b_ptr = device_bias;
        float* out_ptr = device_outputs;
        cuda_launch_affine(in_ptr, w_ptr, b_ptr, out_ptr,
                           static_cast<unsigned int>(total_threads),
                           static_cast<unsigned int>(i_width),
                           static_cast<unsigned int>(o_width),
                           grid_size, block_size);
    }
    const auto sync_status = cudaDeviceSynchronize();
    result.kernel_us = elapsed_us(kernel_started);
    if (sync_status != cudaSuccess) {
        result.reason = "CUDA affine kernel failed";
        cudaFree(device_inputs); cudaFree(device_weights);
        cudaFree(device_bias); cudaFree(device_outputs);
        return result;
    }
    result.executed = true;

    const auto download_started = Clock::now();
    result.output.resize(batch * output_width);
    const auto download_ok = cudaMemcpy(result.output.data(), device_outputs,
        outputs_bytes, cudaMemcpyDeviceToHost) == cudaSuccess;
    result.download_us = elapsed_us(download_started);
    cudaFree(device_inputs); cudaFree(device_weights);
    cudaFree(device_bias); cudaFree(device_outputs);
    if (!download_ok) {
        result.reason = "CUDA download memcpy failed";
        return result;
    }

    // CPU fp64 reference gate
    for (std::size_t item = 0; item < batch; ++item) {
        for (std::size_t row = 0; row < output_width; ++row) {
            double expected = static_cast<double>(bias[row]);
            for (std::size_t column = 0; column < input_width; ++column) {
                expected += static_cast<double>(
                    weights[row * input_width + column]) *
                    static_cast<double>(inputs[item * input_width + column]);
            }
            const double actual =
                static_cast<double>(result.output[item * output_width + row]);
            const double absolute_error = std::abs(actual - expected);
            const double relative_error =
                absolute_error / std::max(1.0, std::abs(expected));
            result.maximum_absolute_error =
                std::max(result.maximum_absolute_error, absolute_error);
            result.maximum_relative_error =
                std::max(result.maximum_relative_error, relative_error);
        }
    }
    result.verified = result.maximum_absolute_error <= absolute_tolerance ||
        result.maximum_relative_error <= relative_tolerance;
    result.reason = result.verified
        ? "CUDA kernel completed and CPU fp64 reference gate passed"
        : "CUDA kernel completed but CPU fp64 reference gate failed";
    return result;
}

MetalStencilSolveResult cuda_weighted_jacobi_2d_batch(
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
    double residual_tolerance) {
    MetalStencilSolveResult result;
    result.backend = "cuda-fused-weighted-jacobi-2d-fp32-v1";
    result.batch = batch;
    result.width = width;
    result.iterations = iterations;
    const auto size = width * width;
    const auto items = batch * size;
    if (batch == 0 || width == 0 || iterations == 0 || items == 0 ||
        west.size() != items || east.size() != items || south.size() != items ||
        north.size() != items || inverse_diagonal.size() != items ||
        right_hand_sides.size() != items || !(relaxation > 0.0) ||
        !(relaxation < 1.0) || !std::isfinite(relaxation) ||
        !(residual_tolerance > 0.0) || !std::isfinite(residual_tolerance)) {
        result.reason = "invalid CUDA stencil shape or numeric contract";
        return result;
    }
    const auto finite_input = [](const std::vector<double>& values) {
        return std::all_of(values.begin(), values.end(), [](double value) {
            return std::isfinite(value);
        });
    };
    if (!finite_input(west) || !finite_input(east) || !finite_input(south) ||
        !finite_input(north) || !finite_input(inverse_diagonal) ||
        !finite_input(right_hand_sides) ||
        std::any_of(inverse_diagonal.begin(), inverse_diagonal.end(),
                    [](double value) { return !(value > 0.0); })) {
        result.reason =
            "CUDA stencil inputs must be finite with positive diagonal inverse";
        return result;
    }
    auto& runtime = cuda_runtime();
    if (!runtime.available) {
        result.available = false;
        result.reason = "CUDA runtime unavailable: " + runtime.error;
        return result;
    }
    result.available = true;
    result.device_name = runtime.device_name;

    const auto setup_started = Clock::now();
    std::vector<float> west_f(west.begin(), west.end());
    std::vector<float> east_f(east.begin(), east.end());
    std::vector<float> south_f(south.begin(), south.end());
    std::vector<float> north_f(north.begin(), north.end());
    std::vector<float> inverse_diagonal_f(
        inverse_diagonal.begin(), inverse_diagonal.end());
    std::vector<float> right_f(right_hand_sides.begin(), right_hand_sides.end());
    const auto buffer_bytes = items * sizeof(float);
    float* d_west = nullptr;
    float* d_east = nullptr;
    float* d_south = nullptr;
    float* d_north = nullptr;
    float* d_inv = nullptr;
    float* d_right = nullptr;
    float* d_current = nullptr;
    float* d_next = nullptr;
    float* d_output = nullptr;
    auto alloc = [&](float** ptr) {
        return cudaMalloc(reinterpret_cast<void**>(ptr), buffer_bytes) ==
               cudaSuccess;
    };
    if (!alloc(&d_west) || !alloc(&d_east) || !alloc(&d_south) ||
        !alloc(&d_north) || !alloc(&d_inv) || !alloc(&d_right) ||
        !alloc(&d_current) || !alloc(&d_next) || !alloc(&d_output)) {
        result.reason = "CUDA stencil allocation failed";
        cudaFree(d_west); cudaFree(d_east); cudaFree(d_south);
        cudaFree(d_north); cudaFree(d_inv); cudaFree(d_right);
        cudaFree(d_current); cudaFree(d_next); cudaFree(d_output);
        return result;
    }
    const auto upload_started = Clock::now();
    auto copy_to = [&](float* dst, const std::vector<float>& src) {
        return cudaMemcpy(dst, src.data(), buffer_bytes,
                          cudaMemcpyHostToDevice) == cudaSuccess;
    };
    const bool uploads_ok =
        copy_to(d_west, west_f) && copy_to(d_east, east_f) &&
        copy_to(d_south, south_f) && copy_to(d_north, north_f) &&
        copy_to(d_inv, inverse_diagonal_f) && copy_to(d_right, right_f) &&
        cudaMemset(d_current, 0, buffer_bytes) == cudaSuccess;
    const double upload_elapsed = elapsed_us(upload_started);
    if (!uploads_ok) {
        result.reason = "CUDA stencil upload failed";
        cudaFree(d_west); cudaFree(d_east); cudaFree(d_south);
        cudaFree(d_north); cudaFree(d_inv); cudaFree(d_right);
        cudaFree(d_current); cudaFree(d_next); cudaFree(d_output);
        return result;
    }
    result.setup_us = elapsed_us(setup_started);
    (void)upload_elapsed;

    const auto kernel_started = Clock::now();
    const unsigned int block_size = static_cast<unsigned int>(
        std::min<std::size_t>(256, size));
    cuda_launch_weighted_jacobi_2d(
        d_west, d_east, d_south, d_north, d_inv, d_right,
        d_current, d_next, d_output,
        static_cast<unsigned int>(width),
        static_cast<unsigned int>(iterations),
        static_cast<float>(relaxation),
        static_cast<unsigned int>(batch),
        block_size);
    const auto sync_status = cudaDeviceSynchronize();
    result.kernel_us = elapsed_us(kernel_started);
    if (sync_status != cudaSuccess) {
        result.reason = "CUDA weighted Jacobi kernel failed";
        cudaFree(d_west); cudaFree(d_east); cudaFree(d_south);
        cudaFree(d_north); cudaFree(d_inv); cudaFree(d_right);
        cudaFree(d_current); cudaFree(d_next); cudaFree(d_output);
        return result;
    }
    result.executed = true;

    const auto download_started = Clock::now();
    std::vector<float> output_f(items);
    const auto download_ok = cudaMemcpy(output_f.data(), d_output, buffer_bytes,
        cudaMemcpyDeviceToHost) == cudaSuccess;
    result.download_us = elapsed_us(download_started);
    cudaFree(d_west); cudaFree(d_east); cudaFree(d_south);
    cudaFree(d_north); cudaFree(d_inv); cudaFree(d_right);
    cudaFree(d_current); cudaFree(d_next); cudaFree(d_output);
    if (!download_ok) {
        result.reason = "CUDA stencil download failed";
        return result;
    }
    result.output.assign(output_f.begin(), output_f.end());

    for (std::size_t batch_index = 0; batch_index < batch; ++batch_index) {
        double residual_inf{};
        double right_inf{1.0};
        const auto base = batch_index * size;
        for (std::size_t row = 0; row < width; ++row) {
            for (std::size_t column = 0; column < width; ++column) {
                const auto index = row * width + column;
                auto value = result.output[base + index] /
                    inverse_diagonal[base + index];
                if (column > 0) value -= west[base + index] *
                    result.output[base + index - 1];
                if (column + 1 < width) value -= east[base + index] *
                    result.output[base + index + 1];
                if (row > 0) value -= south[base + index] *
                    result.output[base + index - width];
                if (row + 1 < width) value -= north[base + index] *
                    result.output[base + index + width];
                residual_inf = std::max(residual_inf,
                    std::abs(right_hand_sides[base + index] - value));
                right_inf = std::max(right_inf,
                    std::abs(right_hand_sides[base + index]));
            }
        }
        result.maximum_relative_residual = std::max(
            result.maximum_relative_residual, residual_inf / right_inf);
    }
    result.verified = result.maximum_relative_residual <= residual_tolerance;
    result.reason = result.verified
        ? "CUDA fused stencil completed and FP64 original residual gate passed"
        : "CUDA fused stencil completed but FP64 original residual gate failed";
    return result;
}

#endif  // SMAVE_HAVE_CUDA_RUNTIME
#if !defined(SMAVE_HAVE_CUDA_RUNTIME)

bool cuda_gpu_available() {
    return false;
}

std::string cuda_gpu_device_name() {
    return "";
}

DeviceExecutionResult cuda_gpu_affine_batch(
    const std::vector<float>&,
    std::size_t, std::size_t,
    const std::vector<float>&,
    std::size_t,
    const std::vector<float>&,
    double, double) {
    DeviceExecutionResult result;
    result.backend = "cuda-affine-batch-gpu-v1";
    result.reason = "CUDA runtime unavailable in this build";
    return result;
}

MetalStencilSolveResult cuda_weighted_jacobi_2d_batch(
    const std::vector<double>&,
    const std::vector<double>&,
    const std::vector<double>&,
    const std::vector<double>&,
    const std::vector<double>&,
    const std::vector<double>&,
    std::size_t, std::size_t, std::size_t,
    double, double) {
    MetalStencilSolveResult result;
    result.backend = "cuda-fused-weighted-jacobi-2d-fp32-v1";
    result.reason = "CUDA runtime unavailable in this build";
    return result;
}

#endif  // !SMAVE_HAVE_CUDA_RUNTIME

}  // namespace smave

