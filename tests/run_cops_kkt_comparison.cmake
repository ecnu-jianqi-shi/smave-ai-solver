if(NOT DEFINED JULIA OR NOT DEFINED ENVIRONMENT OR NOT DEFINED SOURCE_ROOT OR
   NOT DEFINED OUTPUT OR NOT DEFINED CASE_RUNNER)
    message(FATAL_ERROR
        "JULIA, ENVIRONMENT, SOURCE_ROOT, OUTPUT and CASE_RUNNER are required")
endif()

set(families
    bearing camshape catmix chain channel dirichlet elec gasoil glider henon
    lane_emden marine methanol minsurf pinene polygon robot rocket steering
    torsion)
set(cases "")
foreach(family IN LISTS families)
    foreach(index RANGE 1 3)
        list(APPEND cases "${family}-par${index}")
    endforeach()
endforeach()
foreach(index RANGE 1 5)
    list(APPEND cases "tetra-par${index}")
endforeach()
foreach(index RANGE 1 3)
    list(APPEND cases "triangle-par${index}")
endforeach()

file(MAKE_DIRECTORY "${OUTPUT}/matrices" "${OUTPUT}/exports"
    "${OUTPUT}/comparisons" "${OUTPUT}/logs")
set(agreements 0)
set(performance_comparisons 0)
set(case_lines "")
foreach(case_name IN LISTS cases)
    set(matrix "${OUTPUT}/matrices/${case_name}-kkt.mtx")
    set(export "${OUTPUT}/exports/${case_name}.txt")
    set(comparison "${OUTPUT}/comparisons/${case_name}.txt")
    if(NOT EXISTS "${matrix}" OR NOT EXISTS "${export}")
        execute_process(
            COMMAND "${JULIA}" --startup-file=no --project=${ENVIRONMENT}
                "${SOURCE_ROOT}/tests/export_cops_kkt_case.jl"
                "${case_name}" "${matrix}" "${export}"
            RESULT_VARIABLE export_result
            OUTPUT_VARIABLE export_stdout
            ERROR_VARIABLE export_stderr
            TIMEOUT 1800)
        file(WRITE "${OUTPUT}/logs/${case_name}-export.log"
            "${export_stdout}${export_stderr}")
        if(NOT export_result EQUAL 0)
            message(FATAL_ERROR "COPS KKT export failed for ${case_name}")
        endif()
    endif()
    if(NOT EXISTS "${comparison}")
        execute_process(
            COMMAND "${CASE_RUNNER}"
                --matrix "${matrix}"
                --output "${comparison}"
                --max-iterations 3000
                --restart 80
                --direct-limit 100000
            RESULT_VARIABLE comparison_result
            OUTPUT_VARIABLE comparison_stdout
            ERROR_VARIABLE comparison_stderr
            TIMEOUT 1800)
        file(WRITE "${OUTPUT}/logs/${case_name}-comparison.log"
            "${comparison_stdout}${comparison_stderr}")
    endif()
    file(READ "${export}" export_text)
    file(READ "${comparison}" comparison_text)
    string(REGEX MATCH "KKT_UNKNOWNS ([0-9]+)" match "${export_text}")
    set(kkt_unknowns "${CMAKE_MATCH_1}")
    string(REGEX MATCH "KKT_STORED_ENTRIES ([0-9]+)" match "${export_text}")
    set(kkt_entries "${CMAKE_MATCH_1}")
    string(REGEX MATCH "CORRECTNESS_AGREEMENT ([0-9]+)" match
        "${comparison_text}")
    set(agreement "${CMAKE_MATCH_1}")
    if(agreement EQUAL 1)
        math(EXPR agreements "${agreements}+1")
    endif()
    file(STRINGS "${comparison}" observation_lines REGEX "^OBSERVATION ")
    list(GET observation_lines 0 smave_observation)
    set(superlu_observation "")
    foreach(observation IN LISTS observation_lines)
        if(observation MATCHES "^OBSERVATION \"superlu-dgssv-7.0.1\"")
            set(superlu_observation "${observation}")
        endif()
    endforeach()
    string(REGEX MATCH
        "OBSERVATION \"[^\"]+\" \"([^\"]+)\" \"[^\"]*\" [0-9]+ [0-9.eE+-]+ ([0-9.eE+-]+)"
        match "${smave_observation}")
    set(smave_status "${CMAKE_MATCH_1}")
    set(smave_seconds "${CMAKE_MATCH_2}")
    string(REGEX MATCH
        "OBSERVATION \"superlu-dgssv-7.0.1\" \"([^\"]+)\" \"[^\"]*\" [0-9]+ [0-9.eE+-]+ ([0-9.eE+-]+)"
        match "${superlu_observation}")
    set(superlu_status "${CMAKE_MATCH_1}")
    set(superlu_seconds "${CMAKE_MATCH_2}")
    set(speedup 0)
    if(smave_status STREQUAL "converged" AND
       superlu_status STREQUAL "converged")
        execute_process(
            COMMAND awk -v traditional=${superlu_seconds} -v smave=${smave_seconds}
                "BEGIN { printf \"%.9g\", traditional / smave }"
            OUTPUT_VARIABLE speedup)
        math(EXPR performance_comparisons "${performance_comparisons}+1")
    endif()
    string(APPEND case_lines
        "CASE \"${case_name}\" AGREEMENT ${agreement} KKT_UNKNOWNS ${kkt_unknowns} KKT_STORED_ENTRIES ${kkt_entries} SMAVE_STATUS \"${smave_status}\" SUPERLU_STATUS \"${superlu_status}\" SMAVE_SECONDS ${smave_seconds} SUPERLU_SECONDS ${superlu_seconds} SMAVE_VS_SUPERLU_SPEEDUP ${speedup}\n")
endforeach()

list(LENGTH cases total)
file(WRITE "${OUTPUT}/comparison.txt"
    "SMAVE_COPS_KKT_COMPARISON 1\n"
    "CASES ${total}\n"
    "AGREEMENTS ${agreements}\n"
    "PERFORMANCE_COMPARISONS ${performance_comparisons}\n"
    "KKT_CONTRACT \"objective-Hessian plus full constraint-Jacobian; full-KKT strict row diagonal dominance margin one; deterministic manufactured solution\"\n"
    "${case_lines}END\n")
message(STATUS
    "COPS KKT cases=${total} agreements=${agreements} performance=${performance_comparisons}")
if(NOT agreements EQUAL total OR NOT performance_comparisons EQUAL total)
    message(FATAL_ERROR
        "COPS KKT comparison incomplete: agreements=${agreements}, performance=${performance_comparisons}, expected=${total}")
endif()
