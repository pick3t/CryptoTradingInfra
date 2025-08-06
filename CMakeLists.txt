cmake_minimum_required(VERSION 3.12)
project(CryptoTradingInfra LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_BUILD_TYPE Debug)

# Diagnosis
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ compiler version: ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C++ compiler id: ${CMAKE_CXX_COMPILER_ID}")

# Configuration options
option(BUILD_TESTS "Build test programs" ${RUN_TESTS})
option(BUILD_BENCHMARKS "Build benchmark programs" ${RUN_BENCHMARKS})

add_subdirectory(utils)
add_subdirectory(data)
add_subdirectory(app)

# Main application
add_executable(trading_engine
    app/main.cpp
)
target_link_libraries(trading_engine PRIVATE utils data app)

# Testing (conditional build)
if(BUILD_TESTS)
    if(BUILD_BENCHMARKS)
        set(CMAKE_BUILD_TYPE Release)
        set(BENCHMARK_ENABLE_GTEST_TESTS OFF)
        set(BENCHMARK_ENABLE_TESTING OFF)
        add_subdirectory(external/benchmark)
    endif()
    add_subdirectory(tests)
endif()

# Installation targets
install(TARGETS trading_engine DESTINATION bin)
