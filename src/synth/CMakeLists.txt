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

# debugger.h（DPRINTF）は app/ にある。config.h は fmse_config 経由で src/ から参照する
target_include_directories(synth PRIVATE
    ${CMAKE_SOURCE_DIR}/src/app
)

target_link_libraries(synth PUBLIC fm midi midi_panel platform fmse_config)
