if(NOT DEFINED SOURCE OR NOT DEFINED OUTPUT OR NOT DEFINED AMPL_BIN OR
   NOT DEFINED SOLVER_BIN_DIR)
    message(FATAL_ERROR "SOURCE, OUTPUT, AMPL_BIN, and SOLVER_BIN_DIR are required")
endif()

file(MAKE_DIRECTORY "${OUTPUT}/cases" "${OUTPUT}/logs")
file(GLOB model_files "${SOURCE}/models/*/*.mod")
list(SORT model_files)
set(total 0)
set(passed 0)
set(failed 0)
set(blocked_license 0)
set(timed_out 0)
set(case_lines "")

foreach(model_file IN LISTS model_files)
    get_filename_component(model_dir "${model_file}" DIRECTORY)
    get_filename_component(model_name "${model_file}" NAME_WE)
    file(GLOB parameter_files "${model_dir}/${model_name}.par[0-9]*")
    list(SORT parameter_files COMPARE NATURAL)
    foreach(parameter_file IN LISTS parameter_files)
        get_filename_component(parameter_name "${parameter_file}" NAME)
        string(REGEX REPLACE ".*\\.par" "" parameter_index "${parameter_name}")
        set(case_name "${model_name}-par${parameter_index}")
        set(run_file "${OUTPUT}/cases/${case_name}.ampl")
        set(log_file "${OUTPUT}/logs/${case_name}.log")
        file(WRITE "${run_file}"
            "model ${model_name}.mod;\n"
            "data ${parameter_name};\n"
            "data ${model_name}.dat;\n"
            "option solver ipopt;\n"
            "option ipopt_options 'max_iter=300 print_level=0';\n"
            "solve;\n"
            "display solve_result, solve_result_num, _nvars, _ncons, _obj;\n")
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E env
                "PATH=${SOLVER_BIN_DIR}:$ENV{PATH}"
                "${AMPL_BIN}" "${run_file}"
            WORKING_DIRECTORY "${model_dir}"
            RESULT_VARIABLE result
            OUTPUT_VARIABLE stdout
            ERROR_VARIABLE stderr
            TIMEOUT 900)
        set(combined_log "${stdout}${stderr}")
        file(WRITE "${log_file}" "${combined_log}")
        math(EXPR total "${total}+1")
        if(result MATCHES "timeout")
            math(EXPR timed_out "${timed_out}+1")
            string(APPEND case_lines "CASE \"${case_name}\" STATUS timeout\n")
        elseif(combined_log MATCHES "demo license")
            math(EXPR blocked_license "${blocked_license}+1")
            string(APPEND case_lines "CASE \"${case_name}\" STATUS blocked-license\n")
        elseif(result EQUAL 0 AND combined_log MATCHES "solve_result = solved")
            math(EXPR passed "${passed}+1")
            string(APPEND case_lines "CASE \"${case_name}\" STATUS executed\n")
        else()
            math(EXPR failed "${failed}+1")
            string(APPEND case_lines "CASE \"${case_name}\" STATUS failed EXIT ${result}\n")
        endif()
    endforeach()
endforeach()

file(WRITE "${OUTPUT}/summary.txt"
    "SMAVE_COPS_BENCHMARK 1\n"
    "AMPL_BIN \"${AMPL_BIN}\"\n"
    "CASES ${total}\n"
    "EXECUTED ${passed}\n"
    "FAILED ${failed}\n"
    "BLOCKED_LICENSE ${blocked_license}\n"
    "TIMED_OUT ${timed_out}\n"
    "${case_lines}END\n")
message(STATUS
    "COPS cases=${total} executed=${passed} failed=${failed} "
    "blocked-license=${blocked_license} timeout=${timed_out}")
