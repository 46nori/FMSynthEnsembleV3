add_library(midi_panel STATIC
    MidiPanelDriverFactory.cpp
)

target_include_directories(midi_panel PUBLIC ${CMAKE_CURRENT_LIST_DIR})

if(BUILD_MIDI_PANEL)
    target_sources(midi_panel PRIVATE OpnMidiPanelDriver.cpp)
    target_compile_definitions(midi_panel PUBLIC BUILD_MIDI_PANEL=1)
else()
    target_compile_definitions(midi_panel PUBLIC BUILD_MIDI_PANEL=0)
endif()

target_link_libraries(midi_panel PUBLIC fm pico_stdlib)
