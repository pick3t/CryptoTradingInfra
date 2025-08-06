add_library(app STATIC execution_engine.cpp)

target_include_directories(app PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(app PUBLIC utils data)
