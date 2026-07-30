#include <hdf5.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

class Handle {
public:
    Handle(hid_t value, herr_t (*close)(hid_t)) : value_(value), close_(close) {
        if (value_ < 0) throw std::runtime_error("HDF5 handle creation failed");
    }
    ~Handle() { if (value_ >= 0) close_(value_); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    [[nodiscard]] hid_t get() const { return value_; }
private:
    hid_t value_;
    herr_t (*close_)(hid_t);
};

std::vector<float> read_slice(
    hid_t file, const std::string& path,
    const std::vector<hsize_t>& start,
    const std::vector<hsize_t>& count) {
    Handle dataset(H5Dopen2(file, path.c_str(), H5P_DEFAULT), H5Dclose);
    Handle file_space(H5Dget_space(dataset.get()), H5Sclose);
    const auto rank = H5Sget_simple_extent_ndims(file_space.get());
    if (rank != static_cast<int>(start.size()) || start.size() != count.size()) {
        throw std::runtime_error("PDEBench slice rank mismatch: " + path);
    }
    if (H5Sselect_hyperslab(file_space.get(), H5S_SELECT_SET, start.data(), nullptr,
                            count.data(), nullptr) < 0) {
        throw std::runtime_error("PDEBench hyperslab selection failed: " + path);
    }
    Handle memory_space(H5Screate_simple(rank, count.data(), nullptr), H5Sclose);
    const auto values = std::accumulate(
        count.begin(), count.end(), std::size_t{1},
        [](std::size_t left, hsize_t right) { return left * right; });
    std::vector<float> output(values);
    if (H5Dread(dataset.get(), H5T_NATIVE_FLOAT, memory_space.get(), file_space.get(),
                H5P_DEFAULT, output.data()) < 0) {
        throw std::runtime_error("PDEBench slice read failed: " + path);
    }
    return output;
}

std::vector<float> downsample_1d(
    const std::vector<float>& source, std::size_t width) {
    std::vector<float> output(width);
    for (std::size_t index = 0; index < width; ++index) {
        output[index] = source[index * source.size() / width];
    }
    return output;
}

std::vector<float> downsample_2d(
    const std::vector<float>& source, std::size_t source_width,
    std::size_t width, std::size_t channels = 1) {
    std::vector<float> output(width * width * channels);
    for (std::size_t row = 0; row < width; ++row) {
        const auto source_row = row * source_width / width;
        for (std::size_t column = 0; column < width; ++column) {
            const auto source_column = column * source_width / width;
            for (std::size_t channel = 0; channel < channels; ++channel) {
                output[(row * width + column) * channels + channel] =
                    source[(source_row * source_width + source_column) * channels + channel];
            }
        }
    }
    return output;
}

std::string group(std::size_t sample) {
    std::ostringstream output;
    output << '/' << std::setw(4) << std::setfill('0') << sample << "/data";
    return output.str();
}

struct Pair { std::vector<float> input; std::vector<float> target; };

Pair extract(hid_t file, const std::string& family, std::size_t sample,
             std::size_t width) {
    if (family == "advection" || family == "burgers") {
        return {
            downsample_1d(read_slice(file, "/tensor", {sample, 0, 0}, {1, 1, 1024}), width),
            downsample_1d(read_slice(file, "/tensor", {sample, 1, 0}, {1, 1, 1024}), width)};
    }
    if (family == "diffusion-sorption") {
        return {
            downsample_1d(read_slice(file, group(sample), {0, 0, 0}, {1, 1024, 1}), width),
            downsample_1d(read_slice(file, group(sample), {1, 0, 0}, {1, 1024, 1}), width)};
    }
    if (family == "darcy") {
        return {
            downsample_2d(read_slice(file, "/nu", {sample, 0, 0}, {1, 128, 128}), 128, width),
            downsample_2d(read_slice(file, "/tensor", {sample, 0, 0, 0}, {1, 1, 128, 128}), 128, width)};
    }
    if (family == "shallow-water") {
        return {
            downsample_2d(read_slice(file, group(sample), {0, 0, 0, 0}, {1, 128, 128, 1}), 128, width),
            downsample_2d(read_slice(file, group(sample), {1, 0, 0, 0}, {1, 128, 128, 1}), 128, width)};
    }
    if (family == "ns-incompressible") {
        return {
            downsample_2d(read_slice(file, "/velocity", {sample, 0, 0, 0, 0}, {1, 1, 512, 512, 2}), 512, width, 2),
            downsample_2d(read_slice(file, "/velocity", {sample, 1, 0, 0, 0}, {1, 1, 512, 512, 2}), 512, width, 2)};
    }
    throw std::invalid_argument("unsupported PDEBench family: " + family);
}

std::uint64_t checksum(const std::vector<float>& values, std::uint64_t hash) {
    for (const auto value : values) {
        const auto* bytes = reinterpret_cast<const unsigned char*>(&value);
        for (std::size_t index = 0; index < sizeof(value); ++index) {
            hash ^= bytes[index];
            hash *= 1099511628211ULL;
        }
    }
    return hash;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 6) {
            throw std::invalid_argument(
                "usage: smave_pdebench_training_extract FAMILY FILE OUTPUT_PREFIX WIDTH SAMPLES");
        }
        const std::string family(argv[1]);
        const std::filesystem::path input(argv[2]);
        const std::filesystem::path prefix(argv[3]);
        const auto width = std::stoull(argv[4]);
        const auto samples = std::stoull(argv[5]);
        if (width < 2 || samples == 0) throw std::invalid_argument("invalid extraction shape");
        Handle file(H5Fopen(input.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT), H5Fclose);
        std::vector<float> inputs;
        std::vector<float> targets;
        std::size_t values_per_sample{};
        for (std::size_t sample = 0; sample < samples; ++sample) {
            auto pair = extract(file.get(), family, sample, width);
            if (pair.input.size() != pair.target.size() || pair.input.empty() ||
                !std::all_of(pair.input.begin(), pair.input.end(), [](float v) { return std::isfinite(v); }) ||
                !std::all_of(pair.target.begin(), pair.target.end(), [](float v) { return std::isfinite(v); })) {
                throw std::runtime_error("invalid PDEBench training pair");
            }
            if (sample == 0) values_per_sample = pair.input.size();
            if (pair.input.size() != values_per_sample) throw std::runtime_error("inconsistent pair shape");
            inputs.insert(inputs.end(), pair.input.begin(), pair.input.end());
            targets.insert(targets.end(), pair.target.begin(), pair.target.end());
        }
        std::filesystem::create_directories(prefix.parent_path());
        std::ofstream input_output(prefix.string() + ".inputs.f32", std::ios::binary);
        std::ofstream target_output(prefix.string() + ".targets.f32", std::ios::binary);
        input_output.write(reinterpret_cast<const char*>(inputs.data()),
                           static_cast<std::streamsize>(inputs.size() * sizeof(float)));
        target_output.write(reinterpret_cast<const char*>(targets.data()),
                            static_cast<std::streamsize>(targets.size() * sizeof(float)));
        if (!input_output || !target_output) throw std::runtime_error("cannot write training tensors");
        auto hash = checksum(inputs, 1469598103934665603ULL);
        hash = checksum(targets, hash);
        std::ofstream manifest(prefix.string() + ".manifest.txt");
        manifest << "SMAVE_PDEBENCH_TRAINING_SET 1\n"
                 << "FAMILY \"" << family << "\"\n"
                 << "SOURCE \"" << input.string() << "\"\n"
                 << "SAMPLES " << samples << "\n"
                 << "VALUES_PER_SAMPLE " << values_per_sample << "\n"
                 << "TARGET_KIND \"authoritative-next-state-pretraining\"\n"
                 << "SOLVER_LABEL 0\n"
                 << "DISCRETE_OPERATOR_ID \"none\"\n"
                 << "ORIGINAL_RESIDUAL_CERTIFIED 0\n"
                 << "DTYPE \"fp32\"\n"
                 << "LAYOUT \"sample-major-contiguous\"\n"
                 << "CHECKSUM \"" << std::hex << std::setw(16) << std::setfill('0') << hash << "\"\nEND\n";
        if (!manifest) throw std::runtime_error("cannot write training manifest");
        std::cout << "PDEBench training extraction family=" << family
                  << " samples=" << samples << " values=" << values_per_sample << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "PDEBench training extraction failed: " << error.what() << '\n';
        return 2;
    }
}
