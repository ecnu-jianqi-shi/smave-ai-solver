if(NOT DEFINED SOURCE OR NOT DEFINED OUTPUT OR NOT DEFINED IMAGE)
    message(FATAL_ERROR "SOURCE, OUTPUT, and IMAGE are required")
endif()

file(MAKE_DIRECTORY "${OUTPUT}")
set(library_home "${OUTPUT}/openmodelica-home")
file(MAKE_DIRECTORY "${library_home}")
set(install_script "${OUTPUT}/install-modelica.mos")
file(WRITE "${install_script}"
    "installPackage(Modelica, \"4.1.0\", exactMatch=true);\n"
    "getErrorString();\n")
execute_process(
    COMMAND docker run --rm
        -e HOME=/omhome
        -v "${library_home}:/omhome"
        -v "${install_script}:/work/install-modelica.mos:ro"
        "${IMAGE}"
        omc /work/install-modelica.mos
    RESULT_VARIABLE install_result
    OUTPUT_VARIABLE install_stdout
    ERROR_VARIABLE install_stderr
    TIMEOUT 600)
file(WRITE "${OUTPUT}/install-modelica.log" "${install_stdout}${install_stderr}")
if(NOT install_result EQUAL 0 OR NOT install_stdout MATCHES "true")
    message(FATAL_ERROR "Unable to install Modelica Standard Library; see ${OUTPUT}/install-modelica.log")
endif()
set(names
    DCPM_Temperature
    HeatExchanger
    HeatingMOSInverter
    HeatingRectifier
    HeatingSystem
    MovingCoilActuator
    SolenoidActuator)
set(classes
    Modelica.Electrical.Machines.Examples.DCMachines.DCPM_Temperature
    Modelica.Fluid.Examples.HeatExchanger.HeatExchangerSimulation
    Modelica.Electrical.Analog.Examples.HeatingMOSInverter
    Modelica.Electrical.Analog.Examples.HeatingRectifier
    Modelica.Fluid.Examples.HeatingSystem
    Modelica.Magnetic.FluxTubes.Examples.MovingCoilActuator.ForceStrokeBehaviour
    Modelica.Magnetic.FluxTubes.Examples.SolenoidActuator.ComparisonQuasiStatic)

set(passed 0)
set(failed 0)
set(timed_out 0)
set(case_lines "")
list(LENGTH names count)
math(EXPR last "${count}-1")
foreach(index RANGE ${last})
    list(GET names ${index} name)
    list(GET classes ${index} class_name)
    set(simulation_flags "")
    set(simulation_flag_label "default")
    if(name STREQUAL "HeatExchanger" OR name STREQUAL "HeatingSystem")
        set(simulation_flags ", simflags=\"-iim=none\"")
        set(simulation_flag_label "-iim=none")
    endif()
    set(case_dir "${OUTPUT}/${name}")
    file(MAKE_DIRECTORY "${case_dir}")
    file(WRITE "${case_dir}/run.mos"
        "loadModel(Modelica);\n"
        "getVersion(Modelica);\n"
        "checkModel(${class_name});\n"
        "simulate(${class_name}, outputFormat=\"csv\"${simulation_flags});\n"
        "getErrorString();\n")
    execute_process(
        COMMAND docker run --rm
            -e HOME=/omhome
            -v "${library_home}:/omhome"
            -v "${case_dir}:/work"
            -w /work
            "${IMAGE}"
            omc /work/run.mos
        RESULT_VARIABLE result
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
        TIMEOUT 900)
    file(WRITE "${case_dir}/openmodelica.log" "${stdout}${stderr}")
    if(result MATCHES "timeout")
        math(EXPR timed_out "${timed_out}+1")
        string(APPEND case_lines
            "CASE \"${name}\" CLASS \"${class_name}\" STATUS timeout\n")
    elseif(result EQUAL 0 AND stdout MATCHES "resultFile = \"[^\"]+\"")
        math(EXPR passed "${passed}+1")
        string(APPEND case_lines
            "CASE \"${name}\" CLASS \"${class_name}\" STATUS passed SIMFLAGS \"${simulation_flag_label}\"\n")
    else()
        math(EXPR failed "${failed}+1")
        string(APPEND case_lines
            "CASE \"${name}\" CLASS \"${class_name}\" STATUS failed EXIT ${result}\n")
    endif()
endforeach()

file(WRITE "${OUTPUT}/summary.txt"
    "SMAVE_OPENMODELICA_MSL_BENCHMARK 1\n"
    "IMAGE \"${IMAGE}\"\n"
    "CASES ${count}\n"
    "PASSED ${passed}\n"
    "FAILED ${failed}\n"
    "TIMED_OUT ${timed_out}\n"
    "${case_lines}END\n")
message(STATUS
    "OpenModelica MSL cases=${count} passed=${passed} failed=${failed} timeout=${timed_out}")
