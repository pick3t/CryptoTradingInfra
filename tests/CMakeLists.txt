add_library(test_suite
    test_ring_buffer.cpp
    test_market_updates_recv.cpp
    test_order_book.cpp
    test_execution_engine.cpp
)

target_include_directories(test_suite PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(test_suite PRIVATE utils data app)

target_compile_definitions(test_suite PRIVATE CRYPTO_TRADING_INFRA_BENCHMARK)

# Utility tests
add_executable(test
    test_main.cpp
)
target_link_libraries(test PRIVATE test_suite)

# Test suite aggregation
add_custom_target(run_tests
    DEPENDS 
    test
    COMMAND ctest --output-on-failure
)

enable_testing()
add_test(NAME test COMMAND test)

if (BUILD_BENCHMARKS)
    add_executable(test_benchmark_ring_buffer test_benchmark_ring_buffer.cpp)
    find_package(Boost REQUIRED)
    target_include_directories(test_benchmark_ring_buffer PRIVATE ${Boost_INCLUDE_DIRS})
    target_link_libraries(test_benchmark_ring_buffer test_suite utils pthread
        benchmark::benchmark benchmark::benchmark_main)
    add_custom_target(benchmarks
        COMMAND test_benchmark_ring_buffer
        DEPENDS test_benchmark_ring_buffer
    )
endif()
