add_library(display STATIC
    Rw1063Display.cpp
)

target_include_directories(display PUBLIC ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(display PUBLIC pico_stdlib hardware_i2c)
