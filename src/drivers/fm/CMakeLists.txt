add_subdirectory(opn_piolib)

add_library(fm STATIC
    OpnBase.cpp
    YM2608.cpp
)

target_include_directories(fm PUBLIC ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(fm PUBLIC opn_piolib)
