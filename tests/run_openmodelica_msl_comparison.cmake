if(NOT DEFINED SOURCE_ROOT OR NOT DEFINED OUTPUT OR NOT DEFINED IMAGE OR NOT DEFINED CXX)
    message(FATAL_ERROR "SOURCE_ROOT, OUTPUT, IMAGE and CXX are required")
endif()
set(names DCPM_Temperature HeatExchanger HeatingMOSInverter HeatingRectifier HeatingSystem MovingCoilActuator SolenoidActuator)
set(executables
    Modelica.Electrical.Machines.Examples.DCMachines.DCPM_Temperature
    Modelica.Fluid.Examples.HeatExchanger.HeatExchangerSimulation
    Modelica.Electrical.Analog.Examples.HeatingMOSInverter
    Modelica.Electrical.Analog.Examples.HeatingRectifier
    Modelica.Fluid.Examples.HeatingSystem
    Modelica.Magnetic.FluxTubes.Examples.MovingCoilActuator.ForceStrokeBehaviour
    Modelica.Magnetic.FluxTubes.Examples.SolenoidActuator.ComparisonQuasiStatic)
file(RELATIVE_PATH output_relative "${SOURCE_ROOT}" "${OUTPUT}")
execute_process(
    COMMAND docker run --rm -v "${SOURCE_ROOT}:/repo" -w /repo "${IMAGE}" sh -lc
        "g++ -std=c++20 -O3 -fPIC -ffunction-sections -fdata-sections -shared tests/openmodelica_smave_lapack_interpose.cpp src/linear.cpp -Iinclude -Wl,--gc-sections -ldl -o /repo/${output_relative}/libsmave_omc_lapack.so"
    RESULT_VARIABLE compile_result
    OUTPUT_VARIABLE compile_stdout
    ERROR_VARIABLE compile_stderr
    TIMEOUT 300)
file(WRITE "${OUTPUT}/smave-adapter-build.log" "${compile_stdout}${compile_stderr}")
if(NOT compile_result EQUAL 0)
    message(FATAL_ERROR "unable to build OpenModelica SMAVE adapter")
endif()
execute_process(
    COMMAND "${CXX}" -std=c++20 -O2
        "${SOURCE_ROOT}/tests/compare_csv_numeric.cpp"
        -o "${OUTPUT}/compare_csv_numeric"
    RESULT_VARIABLE comparator_compile_result)
if(NOT comparator_compile_result EQUAL 0)
    message(FATAL_ERROR "unable to build CSV comparator")
endif()
set(agreements 0)
set(case_lines "")
list(LENGTH names count)
math(EXPR last "${count}-1")
foreach(index RANGE ${last})
    list(GET names ${index} name)
    list(GET executables ${index} executable)
    set(case_dir "${OUTPUT}/${name}")
    set(extra_flags "")
    if(name STREQUAL "HeatExchanger" OR name STREQUAL "HeatingSystem")
        set(extra_flags "-iim=none")
    endif()
    execute_process(
        COMMAND /usr/bin/time -p docker run --rm -v "${OUTPUT}:/bench"
            -w "/bench/${name}" "${IMAGE}" sh -lc
            "./${executable} ${extra_flags} -r=traditional-comparison.csv"
        RESULT_VARIABLE traditional_result
        OUTPUT_VARIABLE traditional_stdout
        ERROR_VARIABLE traditional_stderr
        TIMEOUT 900)
    string(REGEX MATCH "real[ \t]+([0-9.]+)" match "${traditional_stderr}")
    if(NOT match)
        message(FATAL_ERROR "missing traditional timing for ${name}")
    endif()
    set(traditional_seconds "${CMAKE_MATCH_1}")
    file(WRITE "${case_dir}/traditional-comparison.log" "${traditional_stdout}${traditional_stderr}")
    if(NOT traditional_result EQUAL 0)
        message(FATAL_ERROR "traditional MSL comparison failed for ${name}")
    endif()
    execute_process(
        COMMAND /usr/bin/time -p docker run --rm -v "${OUTPUT}:/bench"
            -w "/bench/${name}" "${IMAGE}" sh -lc
            "SMAVE_OMC_LAPACK_REPORT=/bench/${name}/smave-lapack.txt LD_PRELOAD=/bench/libsmave_omc_lapack.so ./${executable} ${extra_flags} -r=smave-comparison.csv"
        RESULT_VARIABLE smave_result
        OUTPUT_VARIABLE smave_stdout
        ERROR_VARIABLE smave_stderr
        TIMEOUT 900)
    string(REGEX MATCH "real[ \t]+([0-9.]+)" match "${smave_stderr}")
    if(NOT match)
        message(FATAL_ERROR "missing SMAVE timing for ${name}")
    endif()
    set(smave_seconds "${CMAKE_MATCH_1}")
    file(WRITE "${case_dir}/smave-comparison.log" "${smave_stdout}${smave_stderr}")
    if(NOT smave_result EQUAL 0)
        message(FATAL_ERROR "SMAVE MSL comparison failed for ${name}")
    endif()
    file(SHA256 "${case_dir}/traditional-comparison.csv" traditional_hash)
    file(SHA256 "${case_dir}/smave-comparison.csv" smave_hash)
    execute_process(
        COMMAND "${OUTPUT}/compare_csv_numeric"
            "${case_dir}/traditional-comparison.csv"
            "${case_dir}/smave-comparison.csv"
        RESULT_VARIABLE comparison_result
        OUTPUT_VARIABLE comparison_output)
    string(REGEX MATCH "^([0-9.eE+-]+) ([0-9]+)" match "${comparison_output}")
    set(trajectory_error "${CMAKE_MATCH_1}")
    set(trajectory_values "${CMAKE_MATCH_2}")
    file(READ "${case_dir}/smave-lapack.txt" adapter)
    foreach(field IN ITEMS CALLS SOLVED FALLBACKS TOTAL_UNKNOWNS)
        string(REGEX MATCH "${field} ([0-9]+)" match "${adapter}")
        if(NOT match)
            message(FATAL_ERROR "missing ${field} for ${name}")
        endif()
        set(${field} "${CMAKE_MATCH_1}")
    endforeach()
    set(agreement 0)
    if(comparison_result EQUAL 0 AND CALLS EQUAL SOLVED AND FALLBACKS EQUAL 0)
        set(agreement 1)
        math(EXPR agreements "${agreements}+1")
    endif()
    execute_process(
        COMMAND awk -v traditional=${traditional_seconds} -v smave=${smave_seconds}
            "BEGIN { printf \"%.9g\", traditional / smave }"
        OUTPUT_VARIABLE speedup
        RESULT_VARIABLE speedup_result)
    if(NOT speedup_result EQUAL 0 OR speedup STREQUAL "")
        message(FATAL_ERROR "unable to calculate speedup for ${name}")
    endif()
    string(APPEND case_lines
        "CASE \"${name}\" AGREEMENT ${agreement} TRAJECTORY_ERROR ${trajectory_error} TRAJECTORY_VALUES ${trajectory_values} TRAJECTORY_SHA256 \"${traditional_hash}\" CALLS ${CALLS} SMAVE_SOLVED ${SOLVED} FALLBACKS ${FALLBACKS} TOTAL_UNKNOWNS ${TOTAL_UNKNOWNS} OPENMODELICA_SECONDS ${traditional_seconds} SMAVE_SECONDS ${smave_seconds} SMAVE_VS_OPENMODELICA_SPEEDUP ${speedup}\n")
endforeach()
file(WRITE "${OUTPUT}/comparison.txt"
    "SMAVE_OPENMODELICA_MSL_COMPARISON 1\nCASES ${count}\nAGREEMENTS ${agreements}\n${case_lines}END\n")
if(NOT agreements EQUAL count)
    message(FATAL_ERROR "MSL comparison agreements ${agreements}/${count}")
endif()
