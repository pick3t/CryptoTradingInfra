# Utility tests
add_executable(test
    test_ring_buffer.cpp
    test_market_updates_recv.cpp
    test_order_book.cpp
    test_execution_engine.cpp
    test_main.cpp
)
target_link_libraries(test PRIVATE utils data app)

# Test suite aggregation
add_custom_target(run_tests
    DEPENDS 
    test
    COMMAND ctest --output-on-failure
)

enable_testing()
add_test(NAME test COMMAND test)
