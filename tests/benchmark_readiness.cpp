#include "smave/benchmark/suite_manifest.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    try {
        if (argc != 3) {
            throw std::invalid_argument(
                "usage: smave_benchmark_readiness BENCHMARK_ROOT OUTPUT");
        }
        const auto statuses = smave::benchmark::inspect_benchmark_suite(argv[1]);
        smave::benchmark::write_benchmark_manifest(statuses, argv[2]);
        for (const auto& status : statuses) {
            std::cout << status.family << ' ' << status.status << ' '
                      << status.executable << '/' << status.discovered << '\n';
        }
        return statuses.size() == 5 ? 0 : 2;
    } catch (const std::exception& error) {
        std::cerr << "benchmark readiness failure: " << error.what() << '\n';
        return 1;
    }
}
