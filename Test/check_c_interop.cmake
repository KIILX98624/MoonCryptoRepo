execute_process(
    COMMAND "${MONC}" run "${FIXTURE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE actual
    ERROR_VARIABLE error_output
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "MonC C interop fixture failed (exit ${result}): ${error_output}")
endif()

string(STRIP "${actual}" actual)
set(expected_prefix "sqrt=5
pow=32
sin=0
abs=17
atoi=123")
string(FIND "${actual}" "${expected_prefix}" prefix_position)
if(NOT prefix_position EQUAL 0)
    message(FATAL_ERROR "Unexpected C interop output prefix.\nExpected:\n${expected_prefix}\nActual:\n${actual}")
endif()

string(REGEX MATCH "rand_first=([0-9]+)" first_match "${actual}")
set(first_value "${CMAKE_MATCH_1}")
string(REGEX MATCH "rand_second=([0-9]+)" second_match "${actual}")
set(second_value "${CMAKE_MATCH_1}")
if(first_match STREQUAL "" OR second_match STREQUAL "" OR NOT first_value STREQUAL second_value)
    message(FATAL_ERROR "srand did not reset rand() to the same sequence.\nActual:\n${actual}")
endif()
