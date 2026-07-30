#include "smave/routing.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 2) throw std::invalid_argument("usage: cascade_ordering_evidence OUTPUT");
    std::vector<smave::SolveStep> stages{
        {.expert_version = "a", .estimated_cost_us = 2.0, .pass_probability = 0.20},
        {.expert_version = "b", .estimated_cost_us = 6.0, .pass_probability = 0.90},
        {.expert_version = "c", .estimated_cost_us = 1.5, .pass_probability = 0.35},
        {.expert_version = "d", .estimated_cost_us = 4.0, .pass_probability = 0.55},
    };
    constexpr double terminal_cost_us = 10.0;
    auto selected = stages;
    smave::order_cascade_steps(selected);
    const double selected_cost = smave::expected_cascade_cost(selected, terminal_cost_us);
    std::sort(stages.begin(), stages.end(), [](const auto& left, const auto& right) {
        return left.expert_version < right.expert_version;
    });
    std::size_t permutations = 0;
    double exhaustive_minimum = INFINITY;
    do {
        exhaustive_minimum = std::min(
            exhaustive_minimum,
            smave::expected_cascade_cost(stages, terminal_cost_us));
        ++permutations;
    } while (std::next_permutation(
        stages.begin(), stages.end(), [](const auto& left, const auto& right) {
            return left.expert_version < right.expert_version;
        }));
    const bool optimal = std::abs(selected_cost - exhaustive_minimum) <= 1.0e-12;
    std::ofstream output(argv[1]);
    if (!output) throw std::runtime_error("cannot write cascade ordering evidence");
    output << std::setprecision(17)
           << "SMAVE_CASCADE_ORDERING_EVIDENCE 1\n"
           << "stages=" << selected.size() << '\n'
           << "permutations=" << permutations << '\n'
           << "terminal_cost_us=" << terminal_cost_us << '\n'
           << "selected_order=";
    for (std::size_t index = 0; index < selected.size(); ++index) {
        if (index != 0) output << ',';
        output << selected[index].expert_version;
    }
    output << '\n'
           << "selected_expected_cost=" << selected_cost << '\n'
           << "exhaustive_minimum_cost=" << exhaustive_minimum << '\n'
           << "exhaustive_optimum_match=" << optimal << '\n'
           << "END\n";
    std::cout << "SMAVE_CASCADE_ORDERING_EVIDENCE " << optimal
              << " permutations=" << permutations << '\n';
    return optimal ? 0 : 1;
}
