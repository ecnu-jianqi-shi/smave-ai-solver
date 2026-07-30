using Pkg

length(ARGS) == 2 || error("usage: setup_cops_julia.jl ENVIRONMENT SOURCE")

Pkg.activate(ARGS[1])
Pkg.develop(path = ARGS[2])
Pkg.add(PackageSpec(name = "JuMP", version = "1"))
Pkg.add("Ipopt")
Pkg.add("NLPModels")
Pkg.add("NLPModelsJuMP")
Pkg.add(PackageSpec(name = "MadNLP", version = "0.8"))
Pkg.instantiate()
Pkg.precompile()
