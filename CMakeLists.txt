cmake_minimum_required(VERSION 3.20)
project(MoonCrypto VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(mooncyt
    src/main.cpp
    src/project.cpp
    src/lexer.cpp
    src/ast.cpp
)

target_include_directories(mooncyt PRIVATE src)

if(MSVC)
    target_compile_options(mooncyt PRIVATE /W4 /permissive-)
else()
    target_compile_options(mooncyt PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    target_link_libraries(mooncyt PRIVATE bcrypt)
else()
    find_package(OpenSSL QUIET COMPONENTS Crypto)
    if(OpenSSL_FOUND)
        target_compile_definitions(mooncyt PRIVATE MOONCRYPTO_USE_OPENSSL=1)
        target_link_libraries(mooncyt PRIVATE OpenSSL::Crypto)
    else()
        message(WARNING "OpenSSL não encontrado: crypto.sha256 ficará indisponível neste alvo")
    endif()
endif()
