if(NOT DEFINED JULIA OR NOT DEFINED ENVIRONMENT OR NOT DEFINED SOURCE_ROOT OR
   NOT DEFINED OUTPUT OR NOT DEFINED BRIDGE)
    message(FATAL_ERROR
        "JULIA, ENVIRONMENT, SOURCE_ROOT, OUTPUT and BRIDGE are required")
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

file(MAKE_DIRECTORY "${OUTPUT}/madnlp-cases" "${OUTPUT}/madnlp-logs")
set(attempted 0)
set(both_solved 0)
set(agreements 0)
set(performance_comparisons 0)
set(timed_out 0)
set(kkt_solves 0)
set(industrial_solves 0)
set(superlu_solves 0)
set(iterative_solves 0)
set(external_fallback_solves 0)
set(fallback_only_cases 0)
set(resource_gated_cases 0)
set(case_lines "")
foreach(case_name IN LISTS cases)
    set(checkpoint "${OUTPUT}/madnlp-cases/${case_name}.txt")
    set(valid_checkpoint FALSE)
    if(EXISTS "${checkpoint}")
        file(READ "${checkpoint}" checkpoint_text)
        if(checkpoint_text MATCHES "^SMAVE_COPS_MADNLP_CASE 12" AND
           checkpoint_text MATCHES "END")
            set(valid_checkpoint TRUE)
        endif()
    endif()
    if(NOT valid_checkpoint)
        math(EXPR attempted "${attempted}+1")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E env
                "SMAVE_COPS_BRIDGE=${BRIDGE}"
                "${JULIA}" --startup-file=no --project=${ENVIRONMENT}
                "${SOURCE_ROOT}/tests/run_cops_madnlp_case.jl"
                "${case_name}" "${OUTPUT}/cases/${case_name}.txt" "${checkpoint}"
            RESULT_VARIABLE result
            OUTPUT_VARIABLE stdout
            ERROR_VARIABLE stderr
            TIMEOUT 1800)
        file(WRITE "${OUTPUT}/madnlp-logs/${case_name}.log" "${stdout}${stderr}")
        if(result MATCHES "timeout")
            file(WRITE "${checkpoint}"
                "SMAVE_COPS_MADNLP_CASE 12\n"
                "CASE \"${case_name}\"\n"
                "CASE_TIMEOUT 1\n"
                "TIMEOUT_SECONDS 1800\n"
                "TRADITIONAL_STATUS \"TIMEOUT_OR_INCOMPLETE\"\n"
                "SMAVE_STATUS \"TIMEOUT_OR_NOT_RUN\"\n"
                "SMAVE_KKT_ATTEMPTS 0\n"
                "SMAVE_KKT_SOLVES 0\n"
                "SMAVE_INDUSTRIAL_SOLVES 0\n"
                "SMAVE_SUPERLU_SOLVES 0\n"
                "SMAVE_ITERATIVE_SOLVES 0\n"
                "EXTERNAL_FALLBACK_SOLVES 0\n"
                "FALLBACK_ONLY 0\n"
                "RESOURCE_GATED 0\n"
                "CORRECTNESS_AGREEMENT 0\n"
                "END\n")
            math(EXPR timed_out "${timed_out}+1")
            string(APPEND case_lines "CASE \"${case_name}\" STATUS timeout\n")
            continue()
        endif()
    else()
        math(EXPR attempted "${attempted}+1")
    endif()
    if(NOT EXISTS "${checkpoint}")
        string(APPEND case_lines "CASE \"${case_name}\" STATUS failed-no-checkpoint\n")
        continue()
    endif()
    file(READ "${checkpoint}" case_text)
    if(case_text MATCHES "TRADITIONAL_STATUS \"(SOLVE_SUCCEEDED|IPOPT_BASELINE_SOLVED)\"" AND
       case_text MATCHES "SMAVE_STATUS \"SOLVE_SUCCEEDED\"")
        math(EXPR both_solved "${both_solved}+1")
    endif()
    if(case_text MATCHES "CASE_TIMEOUT 1")
        math(EXPR timed_out "${timed_out}+1")
        set(status timeout)
    elseif(case_text MATCHES "CORRECTNESS_AGREEMENT 1")
        math(EXPR agreements "${agreements}+1")
        set(status agreement)
    else()
        set(status disagreement)
    endif()
    foreach(field IN ITEMS SMAVE_KKT_SOLVES SMAVE_INDUSTRIAL_SOLVES SMAVE_SUPERLU_SOLVES SMAVE_ITERATIVE_SOLVES EXTERNAL_FALLBACK_SOLVES)
        string(REGEX MATCH "${field} ([0-9]+)" match "${case_text}")
        if(CMAKE_MATCH_1)
            if(field STREQUAL "SMAVE_KKT_SOLVES")
                math(EXPR kkt_solves "${kkt_solves}+${CMAKE_MATCH_1}")
            elseif(field STREQUAL "SMAVE_INDUSTRIAL_SOLVES")
                math(EXPR industrial_solves "${industrial_solves}+${CMAKE_MATCH_1}")
            elseif(field STREQUAL "SMAVE_SUPERLU_SOLVES")
                math(EXPR superlu_solves "${superlu_solves}+${CMAKE_MATCH_1}")
            elseif(field STREQUAL "SMAVE_ITERATIVE_SOLVES")
                math(EXPR iterative_solves "${iterative_solves}+${CMAKE_MATCH_1}")
            else()
                math(EXPR external_fallback_solves "${external_fallback_solves}+${CMAKE_MATCH_1}")
            endif()
        endif()
    endforeach()
    if(case_text MATCHES "FALLBACK_ONLY 1")
        math(EXPR fallback_only_cases "${fallback_only_cases}+1")
    elseif(case_text MATCHES "CORRECTNESS_AGREEMENT 1")
        math(EXPR performance_comparisons "${performance_comparisons}+1")
    endif()
    if(case_text MATCHES "RESOURCE_GATED 1")
        math(EXPR resource_gated_cases "${resource_gated_cases}+1")
    endif()
    set(traditional_seconds nan)
    if(case_text MATCHES "TRADITIONAL_SECONDS ([0-9.eE+-]+)")
        set(traditional_seconds "${CMAKE_MATCH_1}")
    endif()
    set(smave_seconds nan)
    if(case_text MATCHES "SMAVE_SECONDS ([0-9.eE+-]+)")
        set(smave_seconds "${CMAKE_MATCH_1}")
    endif()
    string(APPEND case_lines
        "CASE \"${case_name}\" STATUS ${status} TRADITIONAL_SECONDS ${traditional_seconds} SMAVE_SECONDS ${smave_seconds}\n")
endforeach()

list(LENGTH cases total)
file(WRITE "${OUTPUT}/madnlp-summary.txt"
    "SMAVE_COPS_MADNLP_COMPARISON 12\n"
    "ITERATIVE_REFINEMENT_MAX_STEPS 2\n"
    "CASE_SPECIFIC_WARMUP \"native KKT cases only\"\n"
    "SMAVE_NATIVE_KKT_LIMIT 5000\n"
    "CASES ${total}\n"
    "ATTEMPTED ${attempted}\n"
    "BOTH_SOLVED ${both_solved}\n"
    "CORRECTNESS_AGREEMENTS ${agreements}\n"
    "PERFORMANCE_COMPARISONS ${performance_comparisons}\n"
    "TIMED_OUT ${timed_out}\n"
    "SMAVE_KKT_SOLVES ${kkt_solves}\n"
    "SMAVE_INDUSTRIAL_SOLVES ${industrial_solves}\n"
    "SMAVE_SUPERLU_SOLVES ${superlu_solves}\n"
    "SMAVE_ITERATIVE_SOLVES ${iterative_solves}\n"
    "EXTERNAL_FALLBACK_SOLVES ${external_fallback_solves}\n"
    "FALLBACK_ONLY_CASES ${fallback_only_cases}\n"
    "RESOURCE_GATED_CASES ${resource_gated_cases}\n"
    "${case_lines}END\n")
message(STATUS
    "COPS MadNLP cases=${total} attempted=${attempted} both-solved=${both_solved} agreements=${agreements} KKT-solves=${kkt_solves}")
