cmake_minimum_required(VERSION 3.22)

# Oldest macOS the plugin will run on. Must be set before project().
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")

project(Stacks VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# JUCE is used straight from the copy installed in /Applications.
set(JUCE_DIR "/Applications/JUCE" CACHE PATH "Path to the JUCE source tree")
add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)

juce_add_plugin(Stacks
    PRODUCT_NAME "Stacks"
    COMPANY_NAME "Hoffman Audio"
    BUNDLE_ID com.hoffmanaudio.stacks
    PLUGIN_MANUFACTURER_CODE Hofa
    PLUGIN_CODE Stck
    FORMATS AU VST3 Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    # Install into ~/Library/Audio/Plug-Ins/{Components,VST3} after every build
    # so Logic picks the new version up on rescan.
    COPY_PLUGIN_AFTER_BUILD TRUE)

juce_generate_juce_header(Stacks)

target_sources(Stacks PRIVATE
    Source/Parameters.cpp
    Source/Wavetable.cpp
    Source/SynthVoice.cpp
    Source/Patch.cpp
    Source/PatchGenerator.cpp
    Source/ai/OllamaBackend.cpp
    Source/ai/LlmPatchGenerator.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/SynthPanel.cpp
    Source/LabPanel.cpp)

target_compile_definitions(Stacks PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_link_libraries(Stacks
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
