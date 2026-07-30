if(NOT DEFINED DIRECTORY)
    message(FATAL_ERROR "DIRECTORY is required")
endif()

foreach(family IN ITEMS
    advection burgers diffusion-sorption darcy shallow-water ns-incompressible)
    set(manifest "${DIRECTORY}/${family}.manifest.txt")
    set(inputs "${DIRECTORY}/${family}.inputs.f32")
    set(targets "${DIRECTORY}/${family}.targets.f32")
    if(NOT EXISTS "${manifest}" OR NOT EXISTS "${inputs}" OR NOT EXISTS "${targets}")
        message(FATAL_ERROR "missing PDEBench training artifact for ${family}")
    endif()
    file(READ "${manifest}" text)
    foreach(field IN ITEMS
        "SMAVE_PDEBENCH_TRAINING_SET 1"
        "FAMILY \"${family}\""
        "TARGET_KIND \"authoritative-next-state-pretraining\""
        "SOLVER_LABEL 0"
        "DISCRETE_OPERATOR_ID \"none\""
        "ORIGINAL_RESIDUAL_CERTIFIED 0"
        "DTYPE \"fp32\""
        "LAYOUT \"sample-major-contiguous\"")
        string(FIND "${text}" "${field}" position)
        if(position EQUAL -1)
            message(FATAL_ERROR "${family} training manifest lacks ${field}")
        endif()
    endforeach()
    string(REGEX MATCH "SAMPLES ([0-9]+)" sample_match "${text}")
    set(samples "${CMAKE_MATCH_1}")
    string(REGEX MATCH "VALUES_PER_SAMPLE ([0-9]+)" value_match "${text}")
    set(values "${CMAKE_MATCH_1}")
    if(NOT sample_match OR NOT value_match OR samples LESS 1 OR values LESS 1)
        message(FATAL_ERROR "invalid PDEBench training shape for ${family}")
    endif()
    math(EXPR expected_bytes "${samples} * ${values} * 4")
    file(SIZE "${inputs}" input_bytes)
    file(SIZE "${targets}" target_bytes)
    if(NOT input_bytes EQUAL expected_bytes OR NOT target_bytes EQUAL expected_bytes)
        message(FATAL_ERROR
            "PDEBench training tensor size mismatch for ${family}: "
            "expected=${expected_bytes}, inputs=${input_bytes}, targets=${target_bytes}")
    endif()
endforeach()

message(STATUS "All six PDEBench pretraining tensor contracts passed")
