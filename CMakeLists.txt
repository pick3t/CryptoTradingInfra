cmake_minimum_required(VERSION 3.12)
project(CryptoTradingInfra LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Diagnosis
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ compiler version: ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C++ compiler id: ${CMAKE_CXX_COMPILER_ID}")

# Configuration options
option(BUILD_TESTS "Build test programs" ON)
option(BUILD_BENCHMARKS "Build benchmark programs" OFF)

add_subdirectory(utils)
add_subdirectory(data)

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
