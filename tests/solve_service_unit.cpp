#include "smave/solve_service.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <string>

namespace {

bool correct_solution(const smave::VerifiedLinearSolveResult& result) {
    return result.success && result.solution.size() == 3 &&
        std::abs(result.solution[0] - 1.0) <= 1.0e-12 &&
        std::abs(result.solution[1] - 1.0) <= 1.0e-12 &&
        std::abs(result.solution[2] - 1.0) <= 1.0e-12 &&
        result.residual_inf == 0.0 && result.backward_error == 0.0 &&
        !result.plan_id.empty() && !result.equation_family.empty() &&
        result.diagnostic.starts_with(smave::verified_linear_solve_service_v1);
}

smave::LinearSystem dense_fixture() {
    smave::LinearSystem system;
    system.unknowns = {"x0", "x1", "x2"};
    system.matrix = {{4, -1, 0}, {-1, 4, -1}, {0, -1, 3}};
    system.right_hand_side = {3, 2, 2};
    smave::classify_linear_system(system);
    return system;
}

smave::LinearSystem sparse_fixture() {
    smave::LinearSystem system;
    system.unknowns = {"x0", "x1", "x2"};
    system.sparsity.row_count = 3;
    system.sparsity.column_count = 3;
    system.sparsity.row_offsets = {0, 2, 5, 7};
    system.sparsity.column_indices = {0, 1, 0, 1, 2, 1, 2};
    system.sparse_values = {4, -1, -1, 4, -1, -1, 3};
    system.right_hand_side = {3, 2, 2};
    smave::classify_linear_system(system);
    return system;
}

smave::LinearSystem dense_spd_portfolio_fixture() {
    smave::LinearSystem system;
    system.unknowns = {"x0", "x1", "x2", "x3"};
    system.matrix = {
        {5.0, -1.0, -0.25, 0.0},
        {-1.0, 4.0, -1.0, 0.0},
        {-0.25, -1.0, 4.0, -1.0},
        {0.0, 0.0, -1.0, 3.0},
    };
    system.right_hand_side = {3.75, 2.0, 1.75, 2.0};
    smave::classify_linear_system(system);
    return system;
}

smave::LinearSystem nonsymmetric_sparse_fixture() {
    smave::LinearSystem system;
    system.unknowns = {"x0", "x1", "x2", "x3"};
    system.sparsity.row_count = 4;
    system.sparsity.column_count = 4;
    system.sparsity.row_offsets = {0, 2, 5, 8, 11};
    system.sparsity.column_indices = {0, 1, 0, 1, 2, 1, 2, 3, 0, 2, 3};
    system.sparse_values = {4.0, 1.0, -1.0, 4.0, 1.0, -1.0, 4.0, 1.0,
                            1.0, -1.0, 3.0};
    system.right_hand_side = {5.0, 4.0, 4.0, 3.0};
    smave::classify_linear_system(system);
    return system;
}

smave::RoutingConfig only_expert(const std::string& expert) {
    smave::RoutingConfig routing;
    routing.top_k = 1;
    routing.expert_allowlist = {expert};
    return routing;
}

smave::RequestConditionedRoutingModel single_budget_model(
    const std::string& expert, int work_iterations) {
    smave::RequestConditionedRoutingModel model;
    model.feature_names = {"sparse:log_rows"};
    model.feature_means = {0.0};
    model.feature_scales = {1.0};
    model.actions[expert] = {smave::RouteActionPredictor{
        .work_iterations = work_iterations,
        .training_samples = 1,
        .independent_training_groups = 1,
        .independent_calibration_groups = 1,
        .log_cost_coefficients = {0.0, 0.0},
        .pass_logit_coefficients = {20.0, 0.0},
    }};
    return model;
}

}

int main() {
    const auto dense = smave::verified_linear_solve(dense_fixture());
    if (!correct_solution(dense)) {
        std::cerr << "dense verified service solve failed\n";
        return 1;
    }
    const auto sparse = smave::verified_linear_solve(sparse_fixture());
    if (!correct_solution(sparse) || sparse.backend != dense.backend ||
        sparse.used_fallback != dense.used_fallback) {
        std::cerr << "CSR verified service solve diverged from dense\n";
        return 1;
    }

    const auto forced_pcg = smave::verified_linear_solve(
        dense_spd_portfolio_fixture(),
        {.maximum_work_iterations = 8,
         .routing = only_expert("pcg-jacobi-cpu-v1")});
    if (!forced_pcg.success || forced_pcg.used_fallback ||
        forced_pcg.backend != "pcg-jacobi-cpu-v1" ||
        forced_pcg.attempts.empty() ||
        forced_pcg.attempts.front().backend != "pcg-jacobi-cpu-v1" ||
        forced_pcg.attempts.front().work_iterations != 8 ||
        forced_pcg.attempts.front().executed_iterations <= 0 ||
        forced_pcg.attempts.front().status != "accepted" ||
        !(forced_pcg.attempts.front().wall_us >= 0.0)) {
        std::cerr << "forced PCG action was not executed and traced\n";
        return 1;
    }

    const auto forced_gmres = smave::verified_linear_solve(
        nonsymmetric_sparse_fixture(),
        {.maximum_work_iterations = 20,
         .restart_dimension = 4,
         .routing = only_expert("gmres-ilu0-cpu-v1")});
    if (!forced_gmres.success || forced_gmres.used_fallback ||
        forced_gmres.backend != "gmres-ilu0-cpu-v1" ||
        forced_gmres.attempts.empty() ||
        forced_gmres.attempts.front().backend != "gmres-ilu0-cpu-v1" ||
        forced_gmres.attempts.front().work_iterations != 20 ||
        forced_gmres.attempts.front().executed_iterations <= 0 ||
        forced_gmres.attempts.front().status != "accepted" ||
        !(forced_gmres.attempts.front().wall_us >= 0.0)) {
        std::cerr << "forced GMRES action was not executed and traced\n";
        return 1;
    }

    const auto forced_ilut = smave::verified_linear_solve(
        nonsymmetric_sparse_fixture(),
        {.maximum_work_iterations = 20,
         .restart_dimension = 4,
         .routing = only_expert("gmres-ilut-cpu-v1")});
    if (!forced_ilut.success || forced_ilut.used_fallback ||
        forced_ilut.backend != "gmres-ilut-cpu-v1" ||
        forced_ilut.attempts.empty() ||
        forced_ilut.attempts.front().backend != "gmres-ilut-cpu-v1" ||
        forced_ilut.attempts.front().status != "accepted") {
        std::cerr << "sparse ILUT-GMRES action was not executed\n";
        return 1;
    }

    auto budget_routing = only_expert("pcg-jacobi-cpu-v1");
    budget_routing.request_conditioned_model =
        single_budget_model("pcg-jacobi-cpu-v1", 1);
    budget_routing.calibrated_terminal_fallback_cost_us = 1.0e6;
    const auto budgeted_pcg = smave::verified_linear_solve(
        dense_spd_portfolio_fixture(),
        {.maximum_work_iterations = 8, .routing = budget_routing});
    if (!budgeted_pcg.success || !budgeted_pcg.used_fallback ||
        budgeted_pcg.backend != "pcg-jacobi-cpu-v1" ||
        budgeted_pcg.attempts.size() < 2 ||
        budgeted_pcg.attempts.front().work_iterations != 1 ||
        budgeted_pcg.attempts.front().status != "gate-rejected" ||
        budgeted_pcg.attempts.back().work_iterations != 8 ||
        budgeted_pcg.attempts.back().status != "accepted" ||
        budgeted_pcg.plan_id.find("terminal-numerical-linear-cascade-v1") ==
            std::string::npos) {
        std::cerr << "budget rejection did not continue to the full numerical fallback\n";
        return 1;
    }
    const auto terminal_suffix = budgeted_pcg.plan_id.find(
        "|terminal-numerical-linear-cascade-v1");
    if (terminal_suffix == std::string::npos ||
        budgeted_pcg.plan_id.substr(0, terminal_suffix) == forced_pcg.plan_id) {
        std::cerr << "work budget was not included in the plan identity\n";
        return 1;
    }

    auto invalid_feature_routing = only_expert("pcg-jacobi-cpu-v1");
    auto invalid_feature_model = single_budget_model("pcg-jacobi-cpu-v1", 4);
    invalid_feature_model.feature_names = {"context:unavailable"};
    invalid_feature_routing.request_conditioned_model = invalid_feature_model;
    const auto invalid_feature = smave::verified_linear_solve(
        dense_spd_portfolio_fixture(), {.routing = invalid_feature_routing});
    if (invalid_feature.success ||
        invalid_feature.diagnostic_code !=
            smave::VerifiedSolveDiagnosticCode::invalid_contract ||
        invalid_feature.diagnostic.find("unavailable for sparse profile") ==
            std::string::npos) {
        std::cerr << "invalid sparse routing feature was not rejected\n";
        return 1;
    }

    auto invalid_action_routing = only_expert("pcg-jacobi-cpu-v1");
    invalid_action_routing.request_conditioned_model =
        single_budget_model("pcg-jacobi-cpu-v1", 9);
    const auto invalid_action_budget = smave::verified_linear_solve(
        dense_spd_portfolio_fixture(),
        {.maximum_work_iterations = 8, .routing = invalid_action_routing});
    if (invalid_action_budget.success ||
        invalid_action_budget.diagnostic_code !=
            smave::VerifiedSolveDiagnosticCode::invalid_contract ||
        invalid_action_budget.diagnostic.find("outside request limit") ==
            std::string::npos) {
        std::cerr << "out-of-contract action budget was not rejected\n";
        return 1;
    }

    auto invalid_direct_routing = only_expert("dense-direct-cpu-v1");
    invalid_direct_routing.request_conditioned_model =
        single_budget_model("dense-direct-cpu-v1", 1);
    const auto invalid_direct_budget = smave::verified_linear_solve(
        dense_spd_portfolio_fixture(), {.routing = invalid_direct_routing});
    if (invalid_direct_budget.success ||
        invalid_direct_budget.diagnostic_code !=
            smave::VerifiedSolveDiagnosticCode::invalid_contract ||
        invalid_direct_budget.diagnostic.find("requires zero work budget") ==
            std::string::npos) {
        std::cerr << "nonzero direct-solver budget was not rejected\n";
        return 1;
    }

    const auto invalid_work_budget = smave::verified_linear_solve(
        dense_spd_portfolio_fixture(), {.maximum_work_iterations = 0});
    const auto invalid_restart = smave::verified_linear_solve(
        dense_spd_portfolio_fixture(), {.restart_dimension = 0});
    if (invalid_work_budget.success || invalid_restart.success ||
        invalid_work_budget.diagnostic.find("work budget") == std::string::npos ||
        invalid_restart.diagnostic.find("work budget") == std::string::npos) {
        std::cerr << "invalid linear iteration controls were not rejected\n";
        return 1;
    }

    std::size_t cancellation_checks{};
    std::size_t cancelled_external_calls{};
    const auto cancelled_linear = smave::verified_linear_solve(
        dense_spd_portfolio_fixture(),
        {.maximum_work_iterations = 8,
         .routing = only_expert("pcg-jacobi-cpu-v1"),
         .cancellation_requested = [&] { return ++cancellation_checks >= 3; },
         .external_fallback = [&](std::vector<double>&) {
             ++cancelled_external_calls;
             return false;
         }});
    if (cancelled_linear.success ||
        cancelled_linear.diagnostic_code !=
            smave::VerifiedSolveDiagnosticCode::cancelled ||
        cancelled_external_calls != 0 || cancelled_linear.attempts.size() != 1 ||
        cancelled_linear.attempts.front().status != "cancelled") {
        std::cerr << "linear cancellation continued into another solver path\n";
        return 1;
    }

    auto singular = dense_fixture();
    singular.matrix = {{0, 0}, {0, 0}};
    singular.unknowns = {"x0", "x1"};
    singular.right_hand_side = {0, 0};
    smave::classify_linear_system(singular);
    const auto singular_result = smave::verified_linear_solve(singular);
    if (singular_result.success ||
        !singular_result.diagnostic.starts_with(smave::verified_linear_solve_service_v1)) {
        std::cerr << "singular system was not rejected\n";
        return 1;
    }
    std::size_t external_fallback_calls{};
    const auto external_fallback_result = smave::verified_linear_solve(
        singular,
        {.external_fallback = [&](std::vector<double>& candidate) {
            ++external_fallback_calls;
            candidate = {1.0, 1.0};
            return true;
        }});
    if (!external_fallback_result.success || !external_fallback_result.used_fallback ||
        external_fallback_result.backend != "caller-linear-fallback-v1" ||
        external_fallback_calls != 1 || external_fallback_result.residual_inf != 0.0 ||
        external_fallback_result.plan_id.find("caller-linear-fallback-v1") ==
            std::string::npos) {
        std::cerr << "verified external linear fallback failed\n";
        return 1;
    }
    const auto rejected_external_fallback = smave::verified_linear_solve(
        singular,
        {.external_fallback = [](std::vector<double>& candidate) {
            candidate = {std::numeric_limits<double>::infinity(), 0.0};
            return true;
        }});
    if (rejected_external_fallback.success ||
        rejected_external_fallback.diagnostic_code !=
            smave::VerifiedSolveDiagnosticCode::original_gate_rejected) {
        std::cerr << "external linear fallback bypassed the original gate\n";
        return 1;
    }
    const auto failed_external_fallback = smave::verified_linear_solve(
        singular,
        {.external_fallback = [](std::vector<double>&) { return false; }});
    if (failed_external_fallback.success ||
        failed_external_fallback.diagnostic_code !=
            smave::VerifiedSolveDiagnosticCode::callback_failure) {
        std::cerr << "external linear fallback failure was not classified\n";
        return 1;
    }
    std::size_t unnecessary_fallback_calls{};
    const auto built_in_wins = smave::verified_linear_solve(
        dense_fixture(),
        {.external_fallback = [&](std::vector<double>&) {
            ++unnecessary_fallback_calls;
            return false;
        }});
    if (!built_in_wins.success || unnecessary_fallback_calls != 0 ||
        built_in_wins.backend == "caller-linear-fallback-v1") {
        std::cerr << "caller fallback ran before a verified built-in candidate\n";
        return 1;
    }

    smave::RoutingConfig oversized_built_in_direct_routing;
    oversized_built_in_direct_routing.expert_allowlist = {
        "sparse-ordered-threshold-pivot-cpu-v2"};
    const auto oversized_built_in_direct = smave::verified_linear_solve(
        sparse_fixture(),
        {.built_in_sparse_direct_row_limit = 1,
         .routing = oversized_built_in_direct_routing});
    if (oversized_built_in_direct.success ||
        oversized_built_in_direct.diagnostic.find(
            "no requested linear expert is available") == std::string::npos) {
        std::cerr << "built-in sparse direct row eligibility was ignored\n";
        return 1;
    }

    auto malformed = sparse_fixture();
    malformed.sparsity.row_offsets = {0, 5, 4, 7};
    const auto malformed_result = smave::verified_linear_solve(malformed);
    if (malformed_result.success ||
        malformed_result.diagnostic.find("invalid linear system") == std::string::npos) {
        std::cerr << "malformed CSR was not rejected\n";
        return 1;
    }

    const auto invalid_tolerance = smave::verified_linear_solve(
        dense_fixture(),
        {.absolute_tolerance = std::numeric_limits<double>::quiet_NaN(),
         .relative_tolerance = 1.0e-10});
    if (invalid_tolerance.success ||
        invalid_tolerance.diagnostic.find("tolerance") == std::string::npos) {
        std::cerr << "invalid tolerance was not rejected\n";
        return 1;
    }

    smave::VerifiedNonlinearSolveProblem nonlinear;
    nonlinear.initial_state = {1.5, 1.5};
    nonlinear.residual = [](const std::vector<double>& state, std::vector<double>& residual) {
        if (state.size() != 2) return false;
        residual = {
            state[0] * state[0] + state[1] - 5.0,
            state[0] + state[1] * state[1] - 3.0,
        };
        return true;
    };
    nonlinear.jacobian = [](const std::vector<double>& state,
                            std::vector<std::vector<double>>& jacobian) {
        if (state.size() != 2) return false;
        jacobian.assign(2, std::vector<double>(2, 0.0));
        return true;
    };
    const auto nonlinear_result = smave::verified_nonlinear_solve(nonlinear);
    if (!nonlinear_result.success || !nonlinear_result.used_fallback ||
        nonlinear_result.solution.size() != 2 ||
        std::abs(nonlinear_result.solution[0] - 2.0) > 1.0e-8 ||
        std::abs(nonlinear_result.solution[1] - 1.0) > 1.0e-8 ||
        !nonlinear_result.diagnostic.starts_with(
            smave::verified_nonlinear_solve_service_v1)) {
        std::cerr << "nonlinear verified service fallback failed\n";
        return 1;
    }
    smave::VerifiedNonlinearSolveProblem external_nonlinear;
    external_nonlinear.initial_state = {0.0};
    external_nonlinear.residual = [](const std::vector<double>& state,
                                     std::vector<double>& residual) {
        residual = {state.at(0) * state.at(0) - 4.0};
        return true;
    };
    external_nonlinear.jacobian = [](const std::vector<double>&,
                                     std::vector<std::vector<double>>&) {
        return false;
    };
    std::size_t nonlinear_fallback_calls{};
    const auto external_nonlinear_result = smave::verified_nonlinear_solve(
        external_nonlinear,
        {.maximum_iterations = 1,
         .external_fallback = [&](std::vector<double>& candidate) {
             ++nonlinear_fallback_calls;
             candidate.at(0) = 2.0;
             return true;
         }});
    if (!external_nonlinear_result.success ||
        !external_nonlinear_result.used_fallback ||
        external_nonlinear_result.backend != "caller-nonlinear-fallback-v1" ||
        external_nonlinear_result.solution != std::vector<double>{2.0} ||
        nonlinear_fallback_calls != 1 ||
        external_nonlinear_result.plan_id.find("caller-nonlinear-fallback-v1") ==
            std::string::npos) {
        std::cerr << "verified external nonlinear fallback failed\n";
        return 1;
    }
    const auto rejected_nonlinear_fallback = smave::verified_nonlinear_solve(
        external_nonlinear,
        {.maximum_iterations = 1,
         .external_fallback = [](std::vector<double>& candidate) {
             candidate.at(0) = 0.0;
             return true;
         }});
    if (rejected_nonlinear_fallback.success ||
        rejected_nonlinear_fallback.diagnostic_code !=
            smave::VerifiedSolveDiagnosticCode::original_gate_rejected) {
        std::cerr << "external nonlinear fallback bypassed original gate\n";
        return 1;
    }
    const auto failed_nonlinear_fallback = smave::verified_nonlinear_solve(
        external_nonlinear,
        {.maximum_iterations = 1,
         .external_fallback = [](std::vector<double>&) { return false; }});
    if (failed_nonlinear_fallback.success ||
        failed_nonlinear_fallback.diagnostic_code !=
            smave::VerifiedSolveDiagnosticCode::callback_failure) {
        std::cerr << "external nonlinear fallback failure was not classified\n";
        return 1;
    }
    std::size_t unnecessary_nonlinear_fallback_calls{};
    const auto built_in_nonlinear_wins = smave::verified_nonlinear_solve(
        nonlinear,
        {.external_fallback = [&](std::vector<double>&) {
             ++unnecessary_nonlinear_fallback_calls;
             return false;
         }});
    if (!built_in_nonlinear_wins.success ||
        unnecessary_nonlinear_fallback_calls != 0 ||
        built_in_nonlinear_wins.backend == "caller-nonlinear-fallback-v1") {
        std::cerr << "nonlinear caller fallback ran before a verified built-in\n";
        return 1;
    }

    smave::VerifiedOdeSolveProblem ode;
    ode.initial_state = {1.0};
    ode.right_hand_side = [](double, const std::vector<double>& state,
                             std::vector<double>& derivative) {
        derivative = {-state.at(0)};
        return true;
    };
    const auto ode_result = smave::verified_ode_solve(
        ode, {.start_time = 0.0, .end_time = 1.0, .maximum_step = 0.1,
              .absolute_tolerance = 1.0e-10, .relative_tolerance = 1.0e-8,
              .maximum_steps = 100000});
    if (!ode_result.success || ode_result.used_fallback ||
        ode_result.backend != "adaptive-rk4-step-doubling-v1" ||
        ode_result.equation_family != "explicit-ode-smooth" ||
        std::abs(ode_result.solution.at(0) - std::exp(-1.0)) > 1.0e-7 ||
        ode_result.maximum_scaled_local_error > 1.0 ||
        !ode_result.diagnostic.starts_with(smave::verified_ode_solve_service_v1)) {
        std::cerr << "verified ODE primary path failed\n";
        return 1;
    }

    smave::VerifiedOdeSolveProblem fallback_ode;
    fallback_ode.initial_state = {0.0};
    fallback_ode.right_hand_side = [](double time, const std::vector<double>&,
                                      std::vector<double>& derivative) {
        if (std::abs(time - 0.05) < 1.0e-14) return false;
        derivative = {1.0};
        return true;
    };
    const auto fallback_ode_result = smave::verified_ode_solve(
        fallback_ode,
        {.start_time = 0.0, .end_time = 0.1, .maximum_step = 0.1,
         .absolute_tolerance = 1.0, .relative_tolerance = 0.0, .maximum_steps = 100});
    if (!fallback_ode_result.success || !fallback_ode_result.used_fallback ||
        fallback_ode_result.backend != "adaptive-heun-euler-fallback-v1" ||
        std::abs(fallback_ode_result.solution.at(0) - 0.1) > 1.0e-12) {
        std::cerr << "verified ODE fallback path failed\n";
        return 1;
    }

    const auto invalid_ode = smave::verified_ode_solve(
        ode, {.start_time = 1.0, .end_time = 0.0});
    if (invalid_ode.success || invalid_ode.diagnostic.find("invalid explicit ODE") ==
            std::string::npos) {
        std::cerr << "invalid ODE interval was not rejected\n";
        return 1;
    }
    auto nonfinite_ode = ode;
    nonfinite_ode.right_hand_side = [](double, const std::vector<double>&,
                                       std::vector<double>& derivative) {
        derivative = {std::numeric_limits<double>::infinity()};
        return true;
    };
    if (smave::verified_ode_solve(nonfinite_ode).success) {
        std::cerr << "non-finite ODE RHS was not rejected\n";
        return 1;
    }
    smave::VerifiedOdeSolveProblem external_ode;
    external_ode.initial_state = {1.0};
    external_ode.right_hand_side = [](double, const std::vector<double>& state,
                                      std::vector<double>& derivative) {
        derivative = {state.at(0)};
        return true;
    };
    std::size_t external_ode_calls{};
    const auto external_ode_result = smave::verified_ode_solve(
        external_ode,
        {.start_time = 0.0,
         .end_time = 1.0,
         .maximum_step = 1.0,
         .absolute_tolerance = 3.0e-5,
         .relative_tolerance = 0.0,
         .maximum_steps = 1,
         .external_step_fallback = [&](double from_time,
                                       const std::vector<double>& previous,
                                       double to_time,
                                       std::vector<double>& quarter,
                                       std::vector<double>& midpoint,
                                       std::vector<double>& three_quarter,
                                       std::vector<double>& next) {
             ++external_ode_calls;
             const double step = to_time - from_time;
             quarter = {previous.at(0) * std::exp(0.25 * step)};
             midpoint = {previous.at(0) * std::exp(0.5 * step)};
             three_quarter = {previous.at(0) * std::exp(0.75 * step)};
             next = {previous.at(0) * std::exp(step)};
             return true;
         }});
    if (!external_ode_result.success || !external_ode_result.used_fallback ||
        external_ode_result.backend != "caller-ode-dense-stepper-fallback-v1" ||
        external_ode_calls != 1 ||
        std::abs(external_ode_result.solution.at(0) - std::exp(1.0)) > 1.0e-12 ||
        external_ode_result.maximum_scaled_local_error > 1.0) {
        std::cerr << "verified external ODE dense stepper failed\n";
        return 1;
    }
    smave::VerifiedDaeSolveProblem external_dae;
    external_dae.differential_mask = {1};
    external_dae.initial_state = {1.0};
    external_dae.initial_derivative = {1.0};
    external_dae.residual = [](double,
                               const std::vector<double>& state,
                               const std::vector<double>& derivative,
                               std::vector<double>& residual) {
        residual = {derivative.at(0) - state.at(0) * state.at(0)};
        return true;
    };
    external_dae.jacobian = [](double,
                               const std::vector<double>&,
                               const std::vector<double>&,
                               double,
                               std::vector<std::vector<double>>&) {
        return false;
    };
    std::size_t external_dae_calls{};
    const auto external_dae_result = smave::verified_dae_solve(
        external_dae,
        {.start_time = 0.0,
         .end_time = 0.1,
         .maximum_step = 0.1,
         .absolute_tolerance = 1.0e-10,
         .relative_tolerance = 1.0e-10,
         .maximum_iterations = 1,
         .maximum_newton_iterations = 1,
         .external_step_fallback = [&](double from_time,
                                       const std::vector<double>& previous_state,
                                       const std::vector<double>&,
                                       double to_time,
                                       std::vector<double>& next_state,
                                       std::vector<double>& next_derivative) {
             ++external_dae_calls;
             const double step = to_time - from_time;
             const double discriminant = 1.0 -
                 4.0 * step * previous_state.at(0);
             next_state = {
                 (1.0 - std::sqrt(discriminant)) / (2.0 * step)};
             next_derivative = {
                 (next_state.at(0) - previous_state.at(0)) / step};
             return true;
         }});
    if (!external_dae_result.success || !external_dae_result.used_fallback ||
        external_dae_result.backend != "caller-dae-stepper-fallback-v1" ||
        external_dae_calls != 1 || external_dae_result.maximum_residual_inf > 1.0e-9) {
        std::cerr << "verified external DAE stepper failed\n";
        return 1;
    }

    std::cout << "SMAVE_SOLVE_SERVICE_UNIT 1\n"
              << "DENSE 1\n"
              << "CSR 1\n"
              << "SINGULAR_REJECTED 1\n"
              << "EXTERNAL_LINEAR_FALLBACK 1\n"
              << "EXTERNAL_LINEAR_FALLBACK_GATE 1\n"
              << "EXTERNAL_LINEAR_FALLBACK_ORDER 1\n"
              << "MALFORMED_CSR_REJECTED 1\n"
              << "INVALID_TOLERANCE_REJECTED 1\n"
              << "NONLINEAR_FALLBACK 1\n"
              << "EXTERNAL_NONLINEAR_FALLBACK 1\n"
              << "EXTERNAL_NONLINEAR_FALLBACK_GATE 1\n"
              << "EXTERNAL_NONLINEAR_FALLBACK_ORDER 1\n"
              << "ODE_PRIMARY 1\n"
              << "ODE_FALLBACK 1\n"
              << "ODE_INVALID_REJECTED 1\n"
              << "EXTERNAL_ODE_DENSE_STEPPER 1\n"
              << "EXTERNAL_DAE_STEPPER 1\n";
    return 0;
}
