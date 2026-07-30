module SmaveCOPSModels

using COPSBenchmark

export build_case

const backend = COPSBenchmark.JuMPBackend()

function family_parameters(family, index)
    parameters = Dict(
        "bearing" => [(50, 50), (50, 75), (50, 100)],
        "camshape" => [(800,), (1000,), (1200,)],
        "catmix" => [(100,), (200,), (400,)],
        "chain" => [(200,), (400,), (800,)],
        "channel" => [(200,), (400,), (800,)],
        "dirichlet" => [(10,), (20,), (40,)],
        "elec" => [(50,), (100,), (200,)],
        "gasoil" => [(100,), (200,), (400,)],
        "glider" => [(100,), (200,), (400,)],
        "henon" => [(10,), (20,), (40,)],
        "lane_emden" => [(10,), (20,), (40,)],
        "marine" => [(100,), (200,), (400,)],
        "methanol" => [(100,), (200,), (400,)],
        "minsurf" => [(50, 50), (50, 75), (50, 100)],
        "pinene" => [(100,), (200,), (400,)],
        "polygon" => [(50,), (100,), (200,)],
        "robot" => [(200,), (400,), (800,)],
        "rocket" => [(400,), (800,), (1600,)],
        "steering" => [(200,), (400,), (800,)],
        "torsion" => [(50, 50), (50, 75), (50, 100)],
    )
    haskey(parameters, family) || error("unknown COPS family: $family")
    values = parameters[family]
    1 <= index <= length(values) || error("invalid parameter index")
    return values[index]
end

function build_case(case_name)
    matched = match(r"^(.+)-par([0-9]+)$", case_name)
    matched === nothing && error("invalid COPS case name: $case_name")
    family = matched.captures[1]
    index = parse(Int, matched.captures[2])
    if family == "tetra"
        variants = ["foam5", "gear", "hook", "duct20", "duct15"]
        1 <= index <= length(variants) || error("invalid tetra case")
        constructor = getfield(
            COPSBenchmark, Symbol("tetra_$(variants[index])_model"))
        return constructor(backend)
    end
    if family == "triangle"
        variants = ["deer", "pacman", "turtle"]
        1 <= index <= length(variants) || error("invalid triangle case")
        constructor = getfield(
            COPSBenchmark, Symbol("triangle_$(variants[index])_model"))
        return constructor(backend)
    end
    constructor = getfield(COPSBenchmark, Symbol("$(family)_model"))
    return constructor(backend, family_parameters(family, index)...)
end

end
