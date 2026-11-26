cmake_minimum_required(VERSION 3.20)
project(MoonCrypto VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(monc
    src/main.cpp
    src/project.cpp
    src/lexer.cpp
    src/ast.cpp
)

target_include_directories(monc PRIVATE src)

if(MSVC)
    target_compile_options(monc PRIVATE /W4 /permissive-)
else()
    target_compile_options(monc PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    target_link_libraries(monc PRIVATE bcrypt)
else()
    target_link_libraries(monc PRIVATE ${CMAKE_DL_LIBS})
    find_package(OpenSSL QUIET COMPONENTS Crypto)
    if(OpenSSL_FOUND)
        target_compile_definitions(monc PRIVATE MOONCRYPTO_USE_OPENSSL=1)
        target_link_libraries(monc PRIVATE OpenSSL::Crypto)
    else()
        message(WARNING "OpenSSL não encontrado: crypto.sha256 ficará indisponível neste alvo")
    endif()
endif()

enable_testing()
add_test(
    NAME float_runtime
    COMMAND ${CMAKE_COMMAND}
        -DMONC=$<TARGET_FILE:monc>
        "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/Code MonC/SmokeTest/fixtures/float.monc"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/Code MonC/SmokeTest/check_float.cmake"
)
add_test(
    NAME c_interop_math
    COMMAND ${CMAKE_COMMAND}
        -DMONC=$<TARGET_FILE:monc>
        "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/Code MonC/SmokeTest/fixtures/c_interop.monc"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/Code MonC/SmokeTest/check_c_interop.cmake"
)

add_test(
    NAME bytearray_builtins
    COMMAND ${CMAKE_COMMAND}
        -DMONC=$<TARGET_FILE:monc>
        "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/Code MonC/SmokeTest/fixtures/bytearray_builtins.monc"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/Code MonC/SmokeTest/check_bytearray_builtins.cmake"
)

add_test(
    NAME python_builtin_conversions_and_sequences
    COMMAND ${CMAKE_COMMAND}
        -DMONC=$<TARGET_FILE:monc>
        "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/Code MonC/SmokeTest/fixtures/python_builtins.monc"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/Code MonC/SmokeTest/check_python_builtins.cmake"
)
