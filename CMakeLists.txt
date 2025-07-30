cmake_minimum_required(VERSION 3.12)
project(CryptoTradingInfra LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (APPLE)
    set(CMAKE_CXX_FLAGS "-I/opt/homebrew/opt/llvm/include ${CMAKE_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L/opt/homebrew/opt/llvm/lib")

    set(CMAKE_C_COMPILER "/opt/homebrew/opt/llvm/bin/clang")
    set(CMAKE_CXX_COMPILER "/opt/homebrew/opt/llvm/bin/clang++")
endif()

# Configuration options
option(BUILD_TESTS "Build test programs" ON)
option(BUILD_BENCHMARKS "Build benchmark programs" OFF)

add_subdirectory(utils)

# Main application
add_executable(trading_engine
    app/main.cpp
)
target_link_libraries(trading_engine utils)

# Testing (conditional build)
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Installation targets
install(TARGETS trading_engine DESTINATION bin)
