# Utility tests
add_executable(test_ring_buffer
    test_ring_buffer.cpp
)
target_link_libraries(test_ring_buffer PRIVATE utils)

# Test suite aggregation
add_custom_target(run_tests
    DEPENDS 
    test_ring_buffer
    COMMAND ctest --output-on-failure
)

enable_testing()
add_test(NAME ring_buffer COMMAND test_ring_buffer)
