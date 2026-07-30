if(NOT DEFINED SOURCE OR NOT DEFINED OUTPUT OR NOT DEFINED PETSC_ROOT)
    message(FATAL_ERROR "SOURCE, OUTPUT, and PETSC_ROOT are required")
endif()

file(MAKE_DIRECTORY "${OUTPUT}/bin" "${OUTPUT}/logs")
file(GLOB cases "${SOURCE}/ex*.c")
list(SORT cases COMPARE NATURAL)
list(LENGTH cases case_count)
set(compiled 0)
set(executed 0)
set(passed 0)
set(compile_failed 0)
set(run_failed 0)
set(timed_out 0)
set(case_lines "")

foreach(source_file IN LISTS cases)
    get_filename_component(name "${source_file}" NAME_WE)
    set(binary "${OUTPUT}/bin/${name}")
    set(compile_log "${OUTPUT}/logs/${name}.compile.log")
    set(compile_source "${source_file}")
    if(name STREQUAL "ex2" OR name STREQUAL "ex9" OR name STREQUAL "ex10")
        file(READ "${source_file}" source_text)
        if(name STREQUAL "ex2")
            string(REGEX REPLACE
                "  PetscCall\\(PetscFunctionListGet\\(TSList, &types, &ntypes\\)\\);.*  PetscCall\\(PetscFree\\(types\\)\\);"
                ""
                source_text "${source_text}")
        elseif(name STREQUAL "ex9")
            string(REPLACE "PetscInt    I;" "PetscInt    global_index;" source_text "${source_text}")
            string(REPLACE "I = 2 * rank;" "global_index = 2 * rank;" source_text "${source_text}")
            string(REPLACE "I = 2 * rank + 1;" "global_index = 2 * rank + 1;" source_text "${source_text}")
            string(REPLACE "&I, PETSC_COPY_VALUES" "&global_index, PETSC_COPY_VALUES" source_text "${source_text}")
        else()
            string(REPLACE
                "(*tsdae->setfromoptions)(PetscOptionsObject, tsdae)"
                "(*tsdae->setfromoptions)(tsdae, NULL)"
                source_text "${source_text}")
            string(REPLACE
                "VecCreateFromOptions(tsdae->comm, NULL, nU + nV, PETSC_DETERMINE, &tsrhs)"
                "VecCreateFromOptions(tsdae->comm, NULL, 1, nU + nV, PETSC_DETERMINE, &tsrhs)"
                source_text "${source_text}")
        endif()
        set(compile_source "${OUTPUT}/bin/${name}-portable.c")
        file(WRITE "${compile_source}" "${source_text}")
    endif()
    execute_process(
        COMMAND /usr/bin/clang
            "${compile_source}"
            "-I${PETSC_ROOT}/include"
            "-L${PETSC_ROOT}/lib"
            -O2 -Wno-deprecated-declarations
            -lpetsc -lf2clapack -lf2cblas -lc++ -lm
            -o "${binary}"
        RESULT_VARIABLE compile_result
        OUTPUT_VARIABLE compile_stdout
        ERROR_VARIABLE compile_stderr
        TIMEOUT 300)
    file(WRITE "${compile_log}" "${compile_stdout}${compile_stderr}")
    if(NOT compile_result EQUAL 0)
        math(EXPR compile_failed "${compile_failed}+1")
        string(APPEND case_lines
            "CASE \"${name}\" COMPILE failed RUN not-run EXIT ${compile_result}\n")
        continue()
    endif()
    math(EXPR compiled "${compiled}+1")
    set(run_log "${OUTPUT}/logs/${name}.run.log")
    set(run_arguments
        -draw_type null
        -ts_monitor_cancel
        -snes_monitor_cancel
        -ksp_monitor_cancel)
    if(name STREQUAL "ex2")
        list(APPEND run_arguments -ts_type beuler -nest 0)
    elseif(name STREQUAL "ex4")
        list(APPEND run_arguments -ts_fd -ts_type beuler)
    elseif(name STREQUAL "ex5")
        file(COPY "${SOURCE}/ex5_control.txt"
            DESTINATION "${OUTPUT}/bin")
        list(APPEND run_arguments -ts_max_steps 130 -monitor_interval 60)
    elseif(name STREQUAL "ex9")
        list(APPEND run_arguments -ts_max_steps 10)
    elseif(name STREQUAL "ex10")
        list(APPEND run_arguments -ts_max_steps 10)
    elseif(name STREQUAL "ex27")
        list(APPEND run_arguments
            -N 3 -dm_plex_dim 2 -dm_plex_simplex 0
            -dm_plex_box_faces 1,1 -dm_plex_box_lower -1,-1
            -dm_plex_box_upper 1,1 -ts_type theta -ts_theta_theta 0.5
            -snes_fd)
    elseif(name STREQUAL "ex28")
        list(APPEND run_arguments
            -particles_per_cell 1 -output_step 10 -ts_type euler
            -dm_plex_dim 1 -dm_plex_box_faces 200
            -dm_plex_box_lower -10 -dm_plex_box_upper 10)
    elseif(name STREQUAL "ex30")
        list(APPEND run_arguments
            -dim 2 -petscspace_degree 3 -dm_landau_num_species_grid 1
            -dm_refine 0 -number_particles_per_dimension 5
            -dm_landau_batch_size 1 -number_spatial_vertices 1
            -dm_landau_n 1 -dm_landau_thermal_temps 1
            -ts_time_step 0.1 -ts_max_steps 1 -ts_type beuler
            -dm_landau_device_type cpu -pc_type jacobi)
    endif()
    execute_process(
        COMMAND "${binary}" ${run_arguments}
        WORKING_DIRECTORY "${OUTPUT}/bin"
        RESULT_VARIABLE run_result
        OUTPUT_VARIABLE run_stdout
        ERROR_VARIABLE run_stderr
        TIMEOUT 180)
    file(WRITE "${run_log}" "${run_stdout}${run_stderr}")
    math(EXPR executed "${executed}+1")
    if(run_result MATCHES "timeout")
        math(EXPR timed_out "${timed_out}+1")
        string(APPEND case_lines
            "CASE \"${name}\" COMPILE passed RUN timeout EXIT -1\n")
    elseif(run_result EQUAL 0)
        math(EXPR passed "${passed}+1")
        string(APPEND case_lines
            "CASE \"${name}\" COMPILE passed RUN passed EXIT 0\n")
    else()
        math(EXPR run_failed "${run_failed}+1")
        string(APPEND case_lines
            "CASE \"${name}\" COMPILE passed RUN failed EXIT ${run_result}\n")
    endif()
endforeach()

file(WRITE "${OUTPUT}/summary.txt"
    "SMAVE_PETSC_TS_BENCHMARK 1\n"
    "PETSC_ROOT \"${PETSC_ROOT}\"\n"
    "CASES ${case_count}\n"
    "COMPILED ${compiled}\n"
    "EXECUTED ${executed}\n"
    "PASSED ${passed}\n"
    "COMPILE_FAILED ${compile_failed}\n"
    "RUN_FAILED ${run_failed}\n"
    "TIMED_OUT ${timed_out}\n"
    "${case_lines}END\n")

message(STATUS
    "PETSc TS cases=${case_count} compiled=${compiled} passed=${passed} "
    "compile_failed=${compile_failed} run_failed=${run_failed} timeout=${timed_out}")
