if(NOT DEFINED SOURCE)
    message(FATAL_ERROR "SOURCE is required")
endif()

set(commit "2986f5570d0b5a7696e4bb7a9dbb991f06e279ae")
if(NOT EXISTS "${SOURCE}/Project.toml")
    get_filename_component(parent "${SOURCE}" DIRECTORY)
    file(MAKE_DIRECTORY "${parent}")
    execute_process(
        COMMAND git clone https://github.com/MadNLP/COPSBenchmark.jl "${SOURCE}"
        RESULT_VARIABLE clone_result)
    if(NOT clone_result EQUAL 0)
        message(FATAL_ERROR "unable to clone COPSBenchmark.jl")
    endif()
endif()
execute_process(
    COMMAND git checkout --detach "${commit}"
    WORKING_DIRECTORY "${SOURCE}"
    RESULT_VARIABLE checkout_result)
if(NOT checkout_result EQUAL 0)
    message(FATAL_ERROR "unable to checkout COPSBenchmark.jl ${commit}")
endif()
file(READ "${SOURCE}/Project.toml" project)
string(FIND "${project}" "\n[sources]\n" sources_start)
if(sources_start GREATER -1)
    string(FIND "${project}" "\n[compat]\n" compat_start)
    if(compat_start LESS 0)
        message(FATAL_ERROR "COPSBenchmark Project.toml has no compat section")
    endif()
    string(SUBSTRING "${project}" 0 ${sources_start} project_before)
    string(SUBSTRING "${project}" ${compat_start} -1 project_after)
    set(project "${project_before}${project_after}")
endif()
file(WRITE "${SOURCE}/Project.toml" "${project}")
