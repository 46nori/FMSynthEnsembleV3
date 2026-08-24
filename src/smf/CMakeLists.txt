add_library(smf STATIC
    SmfMemoryByteSource.cpp
    SmfParser.cpp
)

target_include_directories(smf PUBLIC ${CMAKE_CURRENT_LIST_DIR})
