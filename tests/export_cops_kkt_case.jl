using Ipopt
using JuMP
using NLPModels
using NLPModelsJuMP
using Printf

include(joinpath(@__DIR__, "cops_julia_models.jl"))
using .SmaveCOPSModels

length(ARGS) == 3 || error(
    "usage: export_cops_kkt_case.jl CASE MATRIX OUTPUT")

function write_kkt(case_name, matrix_path, output_path)
    model = SmaveCOPSModels.build_case(case_name)
    set_optimizer(model, Ipopt.Optimizer)
    set_silent(model)
    set_optimizer_attribute(model, "max_iter", 3000)
    set_optimizer_attribute(model, "print_level", 0)
    started = time_ns()
    optimize!(model)
    optimize_seconds = (time_ns() - started) / 1.0e9
    termination_status(model) in (
        JuMP.MOI.OPTIMAL,
        JuMP.MOI.LOCALLY_SOLVED,
        JuMP.MOI.ALMOST_OPTIMAL,
        JuMP.MOI.ALMOST_LOCALLY_SOLVED,
    ) || error("Ipopt did not solve $case_name: $(termination_status(model))")

    nlp = MathOptNLPModel(model)
    variables = all_variables(model)
    x = value.(variables)
    n = get_nvar(nlp)
    m = get_ncon(nlp)
    length(x) == n || error("JuMP/NLPModels variable order mismatch")
    multipliers = zeros(m)

    hessian_rows, hessian_columns = hess_structure(nlp)
    hessian_values = hess_coord(nlp, x, multipliers)
    jacobian_rows, jacobian_columns = jac_structure(nlp)
    jacobian_values = jac_coord(nlp, x)
    length(hessian_rows) == length(hessian_values) || error("invalid Hessian")
    length(jacobian_rows) == length(jacobian_values) || error("invalid Jacobian")

    rows = Int[]
    columns = Int[]
    values = Float64[]
    row_absolute_sum = zeros(n)
    constraint_absolute_sum = zeros(m)
    for (row, column, value) in
        zip(hessian_rows, hessian_columns, hessian_values)
        isfinite(value) || error("non-finite Hessian value")
        lower_row, lower_column = row >= column ? (row, column) : (column, row)
        push!(rows, lower_row)
        push!(columns, lower_column)
        push!(values, value)
        row_absolute_sum[row] += abs(value)
        if row != column
            row_absolute_sum[column] += abs(value)
        end
    end
    for (row, column, value) in
        zip(jacobian_rows, jacobian_columns, jacobian_values)
        isfinite(value) || error("non-finite Jacobian value")
        push!(rows, n + row)
        push!(columns, column)
        push!(values, value)
        row_absolute_sum[column] += abs(value)
        constraint_absolute_sum[row] += abs(value)
    end
    for index in 1:n
        push!(rows, index)
        push!(columns, index)
        push!(values, row_absolute_sum[index] + 1.0)
    end
    for index in 1:m
        push!(rows, n + index)
        push!(columns, n + index)
        push!(values, -(constraint_absolute_sum[index] + 1.0))
    end

    open(matrix_path, "w") do matrix
        println(matrix, "%%MatrixMarket matrix coordinate real symmetric")
        println(matrix, "% SMAVE COPS regularized KKT $case_name")
        println(matrix, "$(n + m) $(n + m) $(length(values))")
        for index in eachindex(values)
            @printf(matrix, "%d %d %.17g\n",
                rows[index], columns[index], values[index])
        end
    end
    open(output_path, "w") do output
        println(output, "SMAVE_COPS_KKT_EXPORT 1")
        println(output, "CASE \"$case_name\"")
        println(output, "PRIMAL_VARIABLES $n")
        println(output, "CONSTRAINTS $m")
        println(output, "KKT_UNKNOWNS $(n + m)")
        println(output, "KKT_STORED_ENTRIES $(length(values))")
        @printf(output, "IPOPT_SECONDS %.17g\n", optimize_seconds)
        @printf(output, "OBJECTIVE %.17g\n", objective_value(model))
        println(output, "REGULARIZATION \"full-KKT strict row diagonal dominance margin one\"")
        println(output, "END")
    end
end

write_kkt(ARGS[1], ARGS[2], ARGS[3])
