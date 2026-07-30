module SmaveMadNLPSolver

using MadNLP
using Libdl
using SparseArrays

import MadNLP: AbstractLinearSolver, default_options, factorize!, improve!,
    input_type, introduce, is_inertia, is_supported, solve!

export SmaveLinearSolver

Base.@kwdef mutable struct SmaveOptions <: MadNLP.AbstractOptions
end

const bridge_path = get(ENV, "SMAVE_COPS_BRIDGE", "")
isempty(bridge_path) && error("SMAVE_COPS_BRIDGE is not set")
const bridge_handle = Libdl.dlopen(bridge_path)
const bridge_solve = Libdl.dlsym(
    bridge_handle, :smave_cops_solve_symmetric_csc)

mutable struct SmaveLinearSolver{T} <: AbstractLinearSolver{T}
    matrix::SparseMatrixCSC{T, Int32}
    fallback::Any
    attempts::Int
    solves::Int
    industrial_solves::Int
    superlu_solves::Int
    iterative_solves::Int
    external_fallback_solves::Int
    fallback_active::Bool
    fallback_activation_attempt::Int
    resource_gated::Bool
    native_disabled::Bool
    maximum_relative_residual::Float64
end

function SmaveLinearSolver(
    matrix::SparseMatrixCSC{T, Int32};
    opt = SmaveOptions(),
    logger = MadNLP.MadNLPLogger(),
) where {T}
    fallback = MadNLP.UmfpackSolver(matrix; logger = logger)
    return SmaveLinearSolver{T}(
        matrix, fallback, 0, 0, 0, 0, 0, 0, false, 0, false, false, 0.0)
end

function factorize!(solver::SmaveLinearSolver)
    native_enabled = get(ENV, "SMAVE_COPS_NATIVE", "0") == "1"
    if !solver.fallback_active && (!native_enabled || solver.matrix.n > 5000)
        solver.attempts += 1
        solver.fallback_active = true
        solver.fallback_activation_attempt = solver.attempts
        solver.resource_gated = true
        solver.native_disabled = !native_enabled
    end
    solver.fallback_active && MadNLP.factorize!(solver.fallback)
    return solver
end

function solve!(solver::SmaveLinearSolver{Float64}, rhs::Vector{Float64})
    if solver.fallback_active
        MadNLP.solve!(solver.fallback, rhs)
        solver.external_fallback_solves += 1
        return rhs
    end
    solver.attempts += 1
    residual = Ref{Cdouble}(Inf)
    backend = Ref{Cint}(0)
    column_offsets = solver.matrix.colptr .- Int32(1)
    row_indices = solver.matrix.rowval .- Int32(1)
    status = ccall(
        bridge_solve,
        Cint,
        (Cint, Ptr{Int32}, Ptr{Int32}, Ptr{Cdouble}, Ptr{Cdouble}, Ref{Cdouble}, Ref{Cint}),
        solver.matrix.n,
        column_offsets,
        row_indices,
        solver.matrix.nzval,
        rhs,
        residual,
        backend,
    )
    if status != 0
        solver.fallback_active = true
        solver.fallback_activation_attempt = solver.attempts
        MadNLP.factorize!(solver.fallback)
        MadNLP.solve!(solver.fallback, rhs)
        solver.external_fallback_solves += 1
        return rhs
    end
    solver.solves += 1
    solver.industrial_solves += backend[] == 1
    solver.superlu_solves += backend[] == 2
    solver.iterative_solves += backend[] == 3
    solver.maximum_relative_residual = max(
        solver.maximum_relative_residual, residual[])
    return rhs
end

is_inertia(::SmaveLinearSolver) = false
MadNLP.inertia(::SmaveLinearSolver) = throw(MadNLP.InertiaException())
input_type(::Type{SmaveLinearSolver}) = :csc
default_options(::Type{SmaveLinearSolver}) = SmaveOptions()
improve!(solver::SmaveLinearSolver) = solver.fallback_active &&
    MadNLP.improve!(solver.fallback)
introduce(::SmaveLinearSolver) = "SMAVE sparse direct benchmark bridge"
is_supported(::Type{SmaveLinearSolver}, ::Type{Float64}) = true

end
