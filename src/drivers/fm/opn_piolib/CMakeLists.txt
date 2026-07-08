add_library(opn_piolib STATIC
        src/fm_bus.c
        src/fm_opn.c
)

# Generate PIO header from fm_bus.pio (produces fm_bus.pio.h)
pico_generate_pio_header(opn_piolib
        ${CMAKE_CURRENT_LIST_DIR}/src/fm_bus.pio
)

# Generated pio header + internal header visibility
target_include_directories(opn_piolib PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src   # fm_bus_internal.h
)

target_include_directories(opn_piolib PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(opn_piolib PUBLIC
        pico_stdlib
        hardware_pio
        hardware_sync
        hardware_clocks
        hardware_gpio
)
