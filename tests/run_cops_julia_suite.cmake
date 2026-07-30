if(NOT DEFINED JULIA OR NOT DEFINED ENVIRONMENT OR NOT DEFINED SOURCE_ROOT OR
   NOT DEFINED OUTPUT)
    message(FATAL_ERROR "JULIA, ENVIRONMENT, SOURCE_ROOT and OUTPUT are required")
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

file(MAKE_DIRECTORY "${OUTPUT}/cases" "${OUTPUT}/logs")
set(solved 0)
set(failed 0)
set(timed_out 0)
set(case_lines "")
foreach(case_name IN LISTS cases)
    set(checkpoint "${OUTPUT}/cases/${case_name}.txt")
    if(EXISTS "${checkpoint}")
        file(READ "${checkpoint}" checkpoint_text)
        if(checkpoint_text MATCHES "^SMAVE_COPS_JULIA_CASE 1" AND
           checkpoint_text MATCHES "STATUS solved")
            math(EXPR solved "${solved}+1")
            string(REGEX MATCH "SOLVE_SECONDS ([0-9.eE+-]+)" match
                "${checkpoint_text}")
            set(solve_seconds "${CMAKE_MATCH_1}")
            string(REGEX MATCH "OBJECTIVE ([^\n]+)" match "${checkpoint_text}")
            set(objective "${CMAKE_MATCH_1}")
            string(APPEND case_lines
                "CASE \"${case_name}\" STATUS solved SOLVE_SECONDS ${solve_seconds} OBJECTIVE ${objective}\n")
            continue()
        endif()
    endif()
    execute_process(
        COMMAND "${JULIA}" --startup-file=no --project=${ENVIRONMENT}
            "${SOURCE_ROOT}/tests/run_cops_julia_case.jl"
            "${case_name}" "${checkpoint}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
        TIMEOUT 1800)
    file(WRITE "${OUTPUT}/logs/${case_name}.log" "${stdout}${stderr}")
    if(result MATCHES "timeout")
        math(EXPR timed_out "${timed_out}+1")
        string(APPEND case_lines "CASE \"${case_name}\" STATUS timeout\n")
    elseif(EXISTS "${checkpoint}")
        file(READ "${checkpoint}" case_text)
        if(case_text MATCHES "STATUS solved")
            math(EXPR solved "${solved}+1")
            string(REGEX MATCH "SOLVE_SECONDS ([0-9.eE+-]+)" match "${case_text}")
            set(solve_seconds "${CMAKE_MATCH_1}")
            string(REGEX MATCH "OBJECTIVE ([^\n]+)" match "${case_text}")
            set(objective "${CMAKE_MATCH_1}")
            string(APPEND case_lines
                "CASE \"${case_name}\" STATUS solved SOLVE_SECONDS ${solve_seconds} OBJECTIVE ${objective}\n")
        else()
            math(EXPR failed "${failed}+1")
            string(APPEND case_lines "CASE \"${case_name}\" STATUS failed\n")
        endif()
    else()
        math(EXPR failed "${failed}+1")
        string(APPEND case_lines
            "CASE \"${case_name}\" STATUS failed EXIT ${result}\n")
    endif()
endforeach()

list(LENGTH cases total)
file(WRITE "${OUTPUT}/summary.txt"
    "SMAVE_COPS_JULIA_BASELINE 1\n"
    "CASES ${total}\n"
    "SOLVED ${solved}\n"
    "FAILED ${failed}\n"
    "TIMED_OUT ${timed_out}\n"
    "${case_lines}END\n")
message(STATUS
    "COPS Julia cases=${total} solved=${solved} failed=${failed} timeout=${timed_out}")
if(NOT solved EQUAL total)
    message(FATAL_ERROR "COPS Julia baseline incomplete: ${solved}/${total}")
endif()
