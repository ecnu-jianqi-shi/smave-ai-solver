using MadNLP
using NLPModels
using NLPModelsJuMP
using Printf
using JuMP

include(joinpath(@__DIR__, "cops_julia_models.jl"))
include(joinpath(@__DIR__, "cops_madnlp_smave_solver.jl"))
using .SmaveCOPSModels
using .SmaveMadNLPSolver

length(ARGS) == 3 || error(
    "usage: run_cops_madnlp_case.jl CASE IPOPT_BASELINE OUTPUT")

function accepted(status)
    return status in (MadNLP.SOLVE_SUCCEEDED, MadNLP.SOLVED_TO_ACCEPTABLE_LEVEL)
end

function run_solver(case_name, linear_solver; native = false)
    model = MathOptNLPModel(SmaveCOPSModels.build_case(case_name))
    started = time_ns()
    solver = MadNLP.MadNLPSolver(
        model;
        linear_solver = linear_solver,
        inertia_correction_method = MadNLP.InertiaFree,
        richardson_max_iter = 2,
        rethrow_error = false,
        print_level = MadNLP.ERROR,
        max_iter = 3000,
    )
    previous_native = get(ENV, "SMAVE_COPS_NATIVE", nothing)
    ENV["SMAVE_COPS_NATIVE"] = native ? "1" : "0"
    result = try
        MadNLP.solve!(solver)
    finally
        if previous_native === nothing
            delete!(ENV, "SMAVE_COPS_NATIVE")
        else
            ENV["SMAVE_COPS_NATIVE"] = previous_native
        end
    end
    wall_seconds = (time_ns() - started) / 1.0e9
    algorithm_seconds = solver.cnt.total_time
    return (; result, solver, wall_seconds, algorithm_seconds)
end

function warmup_solver(linear_solver)
    model = JuMP.Model()
    @variable(model, x >= 0.0, start = 0.5)
    @NLobjective(model, Min, (x - 1.0)^2)
    nlp = MathOptNLPModel(model)
    solver = MadNLP.MadNLPSolver(
        nlp;
        linear_solver = linear_solver,
        inertia_correction_method = MadNLP.InertiaFree,
        richardson_max_iter = 2,
        rethrow_error = false,
        print_level = MadNLP.ERROR,
        max_iter = 50,
    )
    result = MadNLP.solve!(solver)
    accepted(result.status) || error("solver warmup failed: $(result.status)")
end

case_name, ipopt_baseline_path, output_path = ARGS
warmup_solver(MadNLP.UmfpackSolver)
warmup_solver(SmaveLinearSolver)
probe_model = MathOptNLPModel(SmaveCOPSModels.build_case(case_name))
estimated_kkt_unknowns = NLPModels.get_nvar(probe_model) +
    NLPModels.get_ncon(probe_model)
case_specific_warmup = estimated_kkt_unknowns <= 5000
traditional = nothing
if case_specific_warmup
    run_solver(case_name, MadNLP.UmfpackSolver)
end
traditional = run_solver(case_name, MadNLP.UmfpackSolver)
smave = run_solver(case_name, SmaveLinearSolver; native = case_specific_warmup)
traditional_ok = accepted(traditional.result.status)
smave_ok = accepted(smave.result.status)
traditional_objective = traditional.result.objective
objective_scale = max(1.0, abs(traditional_objective))
objective_error = abs(smave.result.objective - traditional_objective) /
    objective_scale
solution_error = maximum(abs.(smave.result.solution .- traditional.result.solution)) /
    max(1.0, maximum(abs.(traditional.result.solution)))
linear_solver = smave.solver.kkt.linear_solver
agreement = traditional_ok && smave_ok && linear_solver.attempts > 0 &&
    linear_solver.maximum_relative_residual <= 1.0e-8 &&
    smave.result.primal_feas <= 1.0e-6 && smave.result.dual_feas <= 1.0e-6 &&
    objective_error <= 1.0e-7 && solution_error <= 1.0e-6
fallback_only = linear_solver.solves == 0 &&
    linear_solver.external_fallback_solves > 0

open(output_path, "w") do output
    println(output, "SMAVE_COPS_MADNLP_CASE 12")
    println(output, "CASE \"$case_name\"")
    println(output, "ITERATIVE_REFINEMENT_MAX_STEPS 2")
    println(output, "CASE_SPECIFIC_WARMUP $(case_specific_warmup ? 1 : 0)")
    println(output, "ESTIMATED_KKT_UNKNOWNS $estimated_kkt_unknowns")
    println(output, "SMAVE_NATIVE_KKT_LIMIT 5000")
    traditional_status = string(traditional.result.status)
    println(output, "TRADITIONAL_STATUS \"$traditional_status\"")
    println(output, "SMAVE_STATUS \"$(smave.result.status)\"")
    traditional_seconds = traditional.algorithm_seconds
    @printf(output, "TRADITIONAL_SECONDS %.17g\n", traditional_seconds)
    @printf(output, "SMAVE_SECONDS %.17g\n", smave.algorithm_seconds)
    traditional_wall = traditional.wall_seconds
    @printf(output, "TRADITIONAL_PROCESS_WALL_SECONDS %.17g\n", traditional_wall)
    @printf(output, "SMAVE_PROCESS_WALL_SECONDS %.17g\n", smave.wall_seconds)
    traditional_iterations = traditional.solver.cnt.k
    println(output, "TRADITIONAL_ITERATIONS $traditional_iterations")
    println(output, "SMAVE_ITERATIONS $(smave.solver.cnt.k)")
    traditional_linear = traditional.solver.cnt.linear_solver_time
    @printf(output, "TRADITIONAL_LINEAR_SOLVER_SECONDS %.17g\n", traditional_linear)
    @printf(output, "SMAVE_LINEAR_SOLVER_SECONDS %.17g\n",
        smave.solver.cnt.linear_solver_time)
    traditional_evaluation = traditional.solver.cnt.eval_function_time
    @printf(output, "TRADITIONAL_EVALUATION_SECONDS %.17g\n", traditional_evaluation)
    @printf(output, "SMAVE_EVALUATION_SECONDS %.17g\n",
        smave.solver.cnt.eval_function_time)
    @printf(output, "TRADITIONAL_OBJECTIVE %.17g\n", traditional_objective)
    @printf(output, "SMAVE_OBJECTIVE %.17g\n", smave.result.objective)
    @printf(output, "OBJECTIVE_RELATIVE_ERROR %.17g\n", objective_error)
    @printf(output, "SOLUTION_RELATIVE_INF_ERROR %.17g\n", solution_error)
    @printf(output, "SMAVE_PRIMAL_FEASIBILITY %.17g\n", smave.result.primal_feas)
    @printf(output, "SMAVE_DUAL_FEASIBILITY %.17g\n", smave.result.dual_feas)
    println(output, "SMAVE_KKT_ATTEMPTS $(linear_solver.attempts)")
    println(output, "SMAVE_KKT_SOLVES $(linear_solver.solves)")
    println(output, "SMAVE_INDUSTRIAL_SOLVES $(linear_solver.industrial_solves)")
    println(output, "SMAVE_SUPERLU_SOLVES $(linear_solver.superlu_solves)")
    println(output, "SMAVE_ITERATIVE_SOLVES $(linear_solver.iterative_solves)")
    println(output, "EXTERNAL_FALLBACK_SOLVES $(linear_solver.external_fallback_solves)")
    println(output, "FALLBACK_ACTIVATION_ATTEMPT $(linear_solver.fallback_activation_attempt)")
    println(output, "RESOURCE_GATED $(linear_solver.resource_gated ? 1 : 0)")
    println(output, "NATIVE_DISABLED $(linear_solver.native_disabled ? 1 : 0)")
    println(output, "FALLBACK_ONLY $(fallback_only ? 1 : 0)")
    @printf(output, "SMAVE_MAX_KKT_RELATIVE_RESIDUAL %.17g\n",
        linear_solver.maximum_relative_residual)
    println(output, "CORRECTNESS_AGREEMENT $(agreement ? 1 : 0)")
    println(output, "END")
end

exit(agreement ? 0 : 4)
