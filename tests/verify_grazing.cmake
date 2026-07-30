foreach(required IR REPORT REPEAT_REPORT)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "missing grazing artifact: ${required}")
    endif()
endforeach()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${REPORT}" "${REPEAT_REPORT}"
    RESULT_VARIABLE compare_result)
if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR "grazing simulation is not deterministic")
endif()
file(READ "${REPORT}" report)
foreach(pattern
    "SUCCESS 1"
    "GRAZING_EVENTS 1"
    "EVENTS 1"
    "EVENT \"event-1\" [^\n]* 1 [^\n]*\"y\" 1"
    "REFERENCE_TIME_MATCHED 1")
    if(NOT report MATCHES "${pattern}")
        message(FATAL_ERROR "grazing acceptance evidence missing: ${pattern}")
    endif()
endforeach()
message(STATUS "Grazing root localization, atomic reset, reference, and determinism gates passed")
