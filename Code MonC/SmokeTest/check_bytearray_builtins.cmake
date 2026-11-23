execute_process(
    COMMAND "${MONC}" run "${FIXTURE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE actual
    ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "ByteArray/built-ins fixture failed (${result}): ${error}")
endif()
string(STRIP "${actual}" actual)
set(expected "12,60,42,50
30
true
2
1
1,2,12,42,50
2,12
1,9,9,42,50
2
9,1
true
true
false
0b1011
'Ol\\xe1'
0")
if(NOT actual STREQUAL expected)
    message(FATAL_ERROR "Unexpected ByteArray/built-ins output.\nExpected:\n${expected}\nActual:\n${actual}")
endif()
