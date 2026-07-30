#include "smave/compiler.hpp"
#include "smave/residency.hpp"
#include "smave/runtime.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::invalid_argument("usage: smave_residency_evidence OUTPUT_DIR");
        const std::filesystem::path output_directory = argv[1];
        std::filesystem::create_directories(output_directory);

        smave::ExpertResidencyManager manager(smave::ResidencyConfig{
            .device = "cpu", .capacity_bytes = 8, .minimum_invocations = 1});
        const auto first = manager.request("expert-a", 8);
        const auto blocked = manager.request("expert-b", 8);
        const auto admitted = manager.request("expert-b", 8);
        const auto hit = manager.request("expert-b", 8);
        if (!first.admitted || blocked.admitted || !admitted.admitted ||
            admitted.evicted_experts != std::vector<std::string>{"expert-a"} ||
            !hit.cache_hit) {
            throw std::runtime_error("deterministic residency evidence failed");
        }

        const auto source = output_directory / "ResidencyFallback.mo";
        std::ofstream(source)
            << "model ResidencyFallback\nReal x(start=1);\n"
            << "equation\nx*x = 4;\nend ResidencyFallback;\n";
        const auto model = smave::compile_model(source);
        const smave::Runtime runtime(
            model, {}, {}, smave::ResidencyConfig{
                .device = "cpu", .capacity_bytes = 1, .minimum_invocations = 2});
        const auto cold = runtime.solve({}, output_directory / "cold-traces");
        const auto hot = runtime.solve({}, output_directory / "hot-traces");
        const auto reused = runtime.solve({}, output_directory / "hit-traces");
        if (!cold.success || cold.fallback_count != 1 ||
            cold.residency_rejection_count != 1 || !hot.success ||
            hot.residency_load_count != 1 || !reused.success ||
            reused.residency_hit_count != 1) {
            throw std::runtime_error("runtime residency fallback evidence failed");
        }

        std::cout << "DEVICE cpu\n"
                  << "CAPACITY_BYTES 8\n"
                  << "COLD_REJECTED 1\n"
                  << "HOT_EVICTIONS " << admitted.evicted_experts.size() << '\n'
                  << "CACHE_HITS 1\n"
                  << "RUNTIME_COLD_FALLBACKS " << cold.fallback_count << '\n'
                  << "RUNTIME_LOADS " << hot.residency_load_count << '\n'
                  << "RUNTIME_HITS " << reused.residency_hit_count << '\n'
                  << "GPU_NPU_RESIDENCY_BACKENDS 0\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
