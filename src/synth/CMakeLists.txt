add_library(synth STATIC
    MidiFactory.cpp
    MidiPanelController.cpp
    MidiProcessor.cpp
    channel/MidiChannel.cpp
    channel/NoteChannel.cpp
    channel/RhythmChannel.cpp
    voice/Voice.cpp
    voice/VoiceAllocator.cpp
    voice/NoteVoice.cpp
    voice/CsmVoice.cpp
)

target_include_directories(synth PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/channel
    ${CMAKE_CURRENT_LIST_DIR}/voice
)

# config.h と debugger.h は app/ にある
target_include_directories(synth PRIVATE
    ${CMAKE_SOURCE_DIR}/src/app
)

target_link_libraries(synth PUBLIC fm midi midi_panel platform usb pico_stdlib pico_multicore)
