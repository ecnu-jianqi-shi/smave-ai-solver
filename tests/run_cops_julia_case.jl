using COPSBenchmark
using Ipopt
using JuMP
using Printf

length(ARGS) == 2 || error("usage: run_cops_julia_case.jl CASE OUTPUT")

include(joinpath(@__DIR__, "cops_julia_models.jl"))
using .SmaveCOPSModels

function safe_message(error_value)
    return replace(sprint(showerror, error_value), '\n' => ' ', '"' => '\'')
end

function run_case(case_name, output_path)
    status = "failed"
    termination = "not-run"
    variables = 0
    objective = NaN
    build_seconds = 0.0
    solve_seconds = 0.0
    reason = ""

    try
        started = time_ns()
        model = SmaveCOPSModels.build_case(case_name)
        build_seconds = (time_ns() - started) / 1.0e9
        variables = num_variables(model)
        set_optimizer(model, Ipopt.Optimizer)
        set_silent(model)
    set_optimizer_attribute(model, "max_iter", 3000)
        set_optimizer_attribute(model, "print_level", 0)
        started = time_ns()
        optimize!(model)
        solve_seconds = (time_ns() - started) / 1.0e9
        termination = string(termination_status(model))
        if has_values(model)
            objective = objective_value(model)
        end
        accepted = termination_status(model) in (
            JuMP.MOI.OPTIMAL,
            JuMP.MOI.LOCALLY_SOLVED,
            JuMP.MOI.ALMOST_OPTIMAL,
            JuMP.MOI.ALMOST_LOCALLY_SOLVED,
        )
        status = accepted ? "solved" : "failed"
        reason = accepted ? "Ipopt accepted termination" : "Ipopt did not converge"
    catch error_value
        reason = safe_message(error_value)
    end

    open(output_path, "w") do output
        println(output, "SMAVE_COPS_JULIA_CASE 1")
        println(output, "CASE \"$case_name\"")
        println(output, "STATUS $status")
        println(output, "TERMINATION \"$termination\"")
        println(output, "VARIABLES $variables")
        @printf(output, "BUILD_SECONDS %.17g\n", build_seconds)
        @printf(output, "SOLVE_SECONDS %.17g\n", solve_seconds)
        @printf(output, "OBJECTIVE %.17g\n", objective)
        println(output, "REASON \"$reason\"")
        println(output, "END")
    end
    return status == "solved" ? 0 : 4
end

length(ARGS) == 2 || error("usage: run_cops_julia_case.jl CASE OUTPUT")
exit(run_case(ARGS[1], ARGS[2]))
