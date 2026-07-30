#include "smave/benchmark/sparse_suite.hpp"

#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

std::string option(int argc, char** argv, const std::string& name) {
    for (int index = 1; index + 1 < argc; ++index) {
        if (argv[index] == name) return argv[index + 1];
    }
    return {};
}

struct ChildResult {
    int exit_code{};
    bool timed_out{false};
    double elapsed_seconds{};
};

ChildResult run_child(
    const std::vector<std::string>& arguments,
    int timeout_seconds,
    const std::filesystem::path& log_path) {
    const auto started = std::chrono::steady_clock::now();
    const auto child = fork();
    if (child < 0) throw std::runtime_error("fork failed");
    if (child == 0) {
        const auto log = std::fopen(log_path.c_str(), "w");
        if (log == nullptr) _exit(126);
        dup2(fileno(log), STDOUT_FILENO);
        dup2(fileno(log), STDERR_FILENO);
        std::vector<char*> raw;
        raw.reserve(arguments.size() + 1);
        for (const auto& argument : arguments) {
            raw.push_back(const_cast<char*>(argument.c_str()));
        }
        raw.push_back(nullptr);
        execv(raw.front(), raw.data());
        _exit(127);
    }
    ChildResult result;
    int status{};
    while (true) {
        const auto waited = waitpid(child, &status, WNOHANG);
        if (waited == child) break;
        if (waited < 0) throw std::runtime_error("waitpid failed");
        const auto elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - started).count();
        if (elapsed >= timeout_seconds) {
            kill(child, SIGTERM);
            std::this_thread::sleep_for(std::chrono::seconds(2));
            if (waitpid(child, &status, WNOHANG) == 0) kill(child, SIGKILL);
            waitpid(child, &status, 0);
            result.timed_out = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    result.elapsed_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - started).count();
    result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 128;
    return result;
}

bool reusable_checkpoint(
    const std::filesystem::path& checkpoint,
    const smave::benchmark::SparseCase& test_case,
    const std::filesystem::path& runner,
    const std::filesystem::path& configuration_path,
    bool configuration_matches) {
    if (!configuration_matches || !std::filesystem::exists(checkpoint)) return false;
    try {
        const auto checkpoint_time = std::filesystem::last_write_time(checkpoint);
        if (checkpoint_time < std::filesystem::last_write_time(runner) ||
            checkpoint_time < std::filesystem::last_write_time(configuration_path) ||
            checkpoint_time < std::filesystem::last_write_time(test_case.matrix_path) ||
            (!test_case.right_hand_side_path.empty() &&
             checkpoint_time < std::filesystem::last_write_time(
                 test_case.right_hand_side_path))) return false;
        const auto result = smave::benchmark::read_sparse_case_result(checkpoint);
        return result.test_case.name == test_case.name &&
            !result.solve_plan_id.empty() &&
            std::filesystem::equivalent(
                std::filesystem::weakly_canonical(result.test_case.matrix_path),
                std::filesystem::weakly_canonical(test_case.matrix_path));
    } catch (...) {
        return false;
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto runner = option(argc, argv, "--case-runner");
        const auto suite = option(argc, argv, "--suite");
        const auto output = option(argc, argv, "--output");
        if (runner.empty() || suite.empty() || output.empty()) {
            throw std::invalid_argument(
                "usage: smave_sparse_suite_benchmark --case-runner FILE "
                "--suite DIR --output DIR [--timeout-seconds N] "
                "[--max-iterations N] [--restart N] [--direct-limit N]");
        }
        const int timeout_seconds = option(argc, argv, "--timeout-seconds").empty()
            ? 1800 : std::stoi(option(argc, argv, "--timeout-seconds"));
        const auto maximum_iterations = option(argc, argv, "--max-iterations").empty()
            ? "1000" : option(argc, argv, "--max-iterations");
        const auto restart = option(argc, argv, "--restart").empty()
            ? "40" : option(argc, argv, "--restart");
        const auto direct_limit = option(argc, argv, "--direct-limit").empty()
            ? "250000" : option(argc, argv, "--direct-limit");
        const auto only = option(argc, argv, "--only");
        auto cases = smave::benchmark::discover_suite_sparse_cases(suite);
        if (!only.empty()) {
            std::erase_if(cases, [&](const auto& test_case) {
                return test_case.name != only;
            });
        }
        if (cases.empty()) throw std::runtime_error("SuiteSparse suite is empty");
        const std::filesystem::path output_directory(output);
        const auto checkpoint_directory = output_directory / "cases";
        const auto log_directory = output_directory / "logs";
        std::filesystem::create_directories(checkpoint_directory);
        std::filesystem::create_directories(log_directory);
        const auto configuration_path = output_directory / "configuration.txt";
        const std::string configuration =
            "SMAVE_SPARSE_CONFIGURATION 1\nMAX_ITERATIONS " + maximum_iterations +
            "\nRESTART " + restart + "\nDIRECT_LIMIT " + direct_limit + "\nEND\n";
        std::string prior_configuration;
        if (std::ifstream input(configuration_path); input) {
            prior_configuration.assign(
                std::istreambuf_iterator<char>(input),
                std::istreambuf_iterator<char>());
        }
        const bool configuration_matches = prior_configuration == configuration;
        if (!configuration_matches) {
            std::ofstream(configuration_path, std::ios::trunc) << configuration;
        }
        std::size_t reused{};
        std::size_t launched{};
        std::size_t timed_out{};
        std::size_t child_failures{};
        for (std::size_t index = 0; index < cases.size(); ++index) {
            const auto& test_case = cases[index];
            const auto checkpoint = checkpoint_directory / (test_case.name + ".txt");
            if (reusable_checkpoint(
                    checkpoint, test_case, runner, configuration_path,
                    configuration_matches)) {
                ++reused;
                std::cout << '[' << index + 1 << '/' << cases.size() << "] reuse "
                          << test_case.name << '\n';
                continue;
            }
            std::vector<std::string> arguments{
                std::filesystem::absolute(runner).string(),
                "--matrix", std::filesystem::absolute(test_case.matrix_path).string(),
                "--output", std::filesystem::absolute(checkpoint).string(),
                "--max-iterations", maximum_iterations,
                "--restart", restart,
                "--direct-limit", direct_limit,
            };
            if (!test_case.right_hand_side_path.empty()) {
                arguments.push_back("--rhs");
                arguments.push_back(
                    std::filesystem::absolute(test_case.right_hand_side_path).string());
            }
            std::cout << '[' << index + 1 << '/' << cases.size() << "] run "
                      << test_case.name << std::endl;
            const auto child = run_child(
                arguments, timeout_seconds, log_directory / (test_case.name + ".log"));
            ++launched;
            timed_out += child.timed_out;
            child_failures += !child.timed_out && child.exit_code != 0;
            std::ofstream timing(
                log_directory / (test_case.name + ".runner"), std::ios::trunc);
            timing << std::setprecision(17)
                   << "exit_code=" << child.exit_code << '\n'
                   << "timed_out=" << child.timed_out << '\n'
                   << "elapsed_seconds=" << child.elapsed_seconds << '\n';
            if (!std::filesystem::exists(checkpoint)) {
                smave::benchmark::SparseCaseResult failed;
                failed.test_case = test_case;
                failed.value_kind = "unreadable";
                failed.symmetry = "unknown";
                failed.right_hand_side_kind = "unavailable";
                failed.equation_family = "invalid-input";
                failed.solve_plan_id = "shared-input-gate-v1";
                failed.backend_chain = {"shared-input-gate"};
                failed.smave.backend = "smave-input-gate";
                failed.smave.status = child.timed_out ? "timed-out" : "invalid-asset";
                failed.smave.reason = child.timed_out
                    ? "case exceeded configured process timeout"
                    : "case runner rejected or could not read the benchmark asset";
                failed.references.push_back({
                    .backend = "superlu-dgssv-7.0.1",
                    .status = "skipped-invalid-input",
                    .reason = "benchmark asset did not pass the shared input gate",
                });
                failed.references.push_back({
                    .backend = "petsc-ksp-3.25.3",
                    .status = "skipped-invalid-input",
                    .reason = "benchmark asset did not pass the shared input gate",
                });
                smave::benchmark::write_sparse_case_result(failed, checkpoint);
            }
        }
        std::size_t checkpoints{};
        std::size_t smave_converged{};
        std::size_t reference_converged{};
        std::size_t agreements{};
        std::size_t performance_comparisons{};
        std::size_t no_common_success{};
        std::size_t reference_skipped{};
        std::size_t invalid_assets{};
        for (const auto& test_case : cases) {
            const auto checkpoint = checkpoint_directory / (test_case.name + ".txt");
            if (!std::filesystem::exists(checkpoint)) continue;
            const auto result = smave::benchmark::read_sparse_case_result(checkpoint);
            ++checkpoints;
            smave_converged += result.smave.status == "converged";
            invalid_assets += result.smave.status == "invalid-asset";
            agreements += result.correctness_agreement;
            bool any_reference_converged = false;
            for (const auto& reference : result.references) {
                reference_converged += reference.status == "converged";
                any_reference_converged = any_reference_converged ||
                    reference.status == "converged";
                reference_skipped += reference.status.starts_with("skipped") ||
                    reference.status == "unavailable";
            }
            const bool comparable = result.smave.status == "converged" &&
                any_reference_converged;
            performance_comparisons += comparable;
            no_common_success += !comparable;
        }
        std::ofstream summary(output_directory / "summary.txt");
        summary << "SMAVE_SUITESPARSE_BENCHMARK 1\n"
                << "ASSET_FILES "
                << std::distance(
                       std::filesystem::recursive_directory_iterator(suite),
                       std::filesystem::recursive_directory_iterator{})
                << '\n'
                << "SYSTEM_CASES " << cases.size() << '\n'
                << "CHECKPOINTS " << checkpoints << '\n'
                << "REUSED " << reused << '\n'
                << "LAUNCHED " << launched << '\n'
                << "TIMED_OUT " << timed_out << '\n'
                << "CHILD_FAILURES " << child_failures << '\n'
                << "SMAVE_CONVERGED " << smave_converged << '\n'
                << "REFERENCE_CONVERGED " << reference_converged << '\n'
                << "REFERENCE_SKIPPED " << reference_skipped << '\n'
                << "INVALID_ASSETS " << invalid_assets << '\n'
                << "CORRECTNESS_AGREEMENTS " << agreements << '\n'
                << "PERFORMANCE_COMPARISONS " << performance_comparisons << '\n'
                << "NO_COMMON_SUCCESS " << no_common_success << '\n'
                << "COMPLETE " << (checkpoints == cases.size()) << "\nEND\n";
        std::cout << "SuiteSparse checkpoints " << checkpoints << '/' << cases.size()
                  << ", SMAVE converged " << smave_converged
                  << ", agreements " << agreements << '\n';
        return checkpoints == cases.size() ? 0 : 5;
    } catch (const std::exception& error) {
        std::cerr << "SuiteSparse benchmark failure: " << error.what() << '\n';
        return 2;
    }
}
