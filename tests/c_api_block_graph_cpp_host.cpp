#include "smave/c_api.h"

#include <array>
#include <iostream>
#include <string_view>

int main() {
    constexpr std::array nodes{
        smave_block_node_desc{sizeof(smave_block_node_desc), SMAVE_ABI_VERSION,
            SMAVE_BLOCK_CONSTANT, 0, 0, 1, 0.1, 0.0, 2.0, 0.0,
            nullptr, nullptr, nullptr, nullptr, nullptr},
        smave_block_node_desc{sizeof(smave_block_node_desc), SMAVE_ABI_VERSION,
            SMAVE_BLOCK_GAIN, 0, 1, 1, 0.1, 0.0, 4.0, 0.0,
            nullptr, nullptr, nullptr, nullptr, nullptr}};
    constexpr std::array connections{
        smave_block_connection_desc{sizeof(smave_block_connection_desc),
            SMAVE_ABI_VERSION, 0, 0, 1, 0}};
    const smave_block_graph_desc descriptor{
        sizeof(descriptor), SMAVE_ABI_VERSION, nodes.data(), nodes.size(),
        connections.data(), connections.size(), 0.1, 0.1};
    smave_library* library{};
    smave_problem* problem{};
    smave_solver* solver{};
    smave_result* result{};
    smave_result_info info{};
    info.struct_size = sizeof(info);
    info.abi_version = SMAVE_ABI_VERSION;
    smave_block_graph_result_info graph_info{};
    graph_info.struct_size = sizeof(graph_info);
    graph_info.abi_version = SMAVE_ABI_VERSION;
    std::array<double, 2> outputs{};
    std::size_t required{};
    const char* service{};
    const char* plan{};
    const char* family{};
    const bool success =
        smave_library_create(nullptr, &library) == SMAVE_STATUS_OK &&
        smave_block_graph_problem_create(library, &descriptor, &problem) == SMAVE_STATUS_OK &&
        smave_problem_finalize(problem) == SMAVE_STATUS_OK &&
        smave_solver_create(problem, nullptr, &solver) == SMAVE_STATUS_OK &&
        smave_solver_solve(solver, &result) == SMAVE_STATUS_OK &&
        smave_result_get_info(result, &info) == SMAVE_STATUS_OK && info.success &&
        smave_result_get_block_graph_info(result, &graph_info) == SMAVE_STATUS_OK &&
        smave_result_get_provenance(result, &service, &plan, &family) == SMAVE_STATUS_OK &&
        smave_result_copy_solution(result, outputs.data(), outputs.size(), &required) ==
            SMAVE_STATUS_OK && required == outputs.size() && outputs[0] == 2.0 &&
        outputs[1] == 8.0 && graph_info.node_count == 2 && service != nullptr &&
        std::string_view(service) == "smave.verified-block-graph-solve.v1" &&
        family != nullptr && std::string_view(family) == "scalar-multiphysics-block-graph" &&
        plan != nullptr && plan[0] != '\0';
    smave_result_destroy(result);
    smave_solver_destroy(solver);
    smave_problem_destroy(problem);
    const bool destroyed = smave_library_destroy(library) == SMAVE_STATUS_OK;
    if (!success || !destroyed) return 1;
    std::cout << "SMAVE_CPP_BLOCK_GRAPH_HOST 1\n"
                 "CPP_HEADER_ONLY_PUBLIC_ABI 1\n"
                 "CPP_BLOCK_GRAPH_SOLVE 1\n";
    return 0;
}
