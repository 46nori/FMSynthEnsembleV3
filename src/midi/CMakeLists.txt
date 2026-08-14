add_library(midi STATIC
    MidiParser.cpp
    MidiStreamAssembler.cpp
    MidiSysEx.cpp
)

target_include_directories(midi PUBLIC ${CMAKE_CURRENT_LIST_DIR})
