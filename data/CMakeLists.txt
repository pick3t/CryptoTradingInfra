add_library(data STATIC order_book.cpp)

target_include_directories(data PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(data PUBLIC utils)
