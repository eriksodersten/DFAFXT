cmake_minimum_required(VERSION 3.22)
project(DFAFXT VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(/Users/eriksode/JUCE JUCE)

juce_add_plugin(DFAFXT
    COMPANY_NAME "eriksodersten"
    PLUGIN_MANUFACTURER_CODE Esod
    PLUGIN_CODE Dfxt
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "DFAF XT"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
)

juce_generate_juce_header(DFAFXT)

target_sources(DFAFXT PRIVATE
    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/DecayEnvelope.h
    Source/MoogLadderFilter.h
    Source/XTSequencer.h
    Source/XTVoice.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

source_group("Header Files" FILES
    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/DecayEnvelope.h
    Source/MoogLadderFilter.h
    Source/XTSequencer.h
    Source/XTVoice.h
)

source_group("Source Files" FILES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(DFAFXT PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(DFAFXT PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
