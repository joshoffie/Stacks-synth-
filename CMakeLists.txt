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

# ---- llama.cpp: the on-board language model runtime -------------------------
# Pinned release, built as static libraries and linked into the plug-in. Metal
# shaders are embedded in the binary so nothing has to be shipped beside it.
include(FetchContent)
set(LLAMA_BUILD_TESTS     OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES  OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER    OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TOOLS     OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_COMMON    OFF CACHE BOOL "" FORCE)
set(LLAMA_CURL            OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS     OFF CACHE BOOL "" FORCE)
set(GGML_NATIVE           OFF CACHE BOOL "" FORCE)   # generic arm64 so M1..M4 all run it
set(GGML_METAL            ON  CACHE BOOL "" FORCE)
set(GGML_METAL_EMBED_LIBRARY ON CACHE BOOL "" FORCE)
set(GGML_OPENMP           OFF CACHE BOOL "" FORCE)
FetchContent_Declare(llama
    GIT_REPOSITORY https://github.com/ggml-org/llama.cpp.git
    GIT_TAG        v0.5.0
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(llama)

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
    Source/Effects.cpp
    Source/PatchGenerator.cpp
    Source/ai/OllamaBackend.cpp
    Source/ai/LlamaBackend.cpp
    Source/ai/ModelManager.cpp
    Source/ai/LlmPatchGenerator.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Controls.cpp
    Source/StacksLookAndFeel.cpp
    Source/Displays.cpp
    Source/SynthPanel.cpp
    Source/ModulatorsPanel.cpp
    Source/LabPanel.cpp
    Source/LibraryPanel.cpp
    Source/GardenView.cpp)

target_compile_definitions(Stacks PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_link_libraries(Stacks
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        llama
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---- Tests: a console app over the engine-side code (no plug-in wrapper) ----
# Run build.nosync/StacksTests_artefacts/Release/StacksTests after a build.
juce_add_console_app(StacksTests PRODUCT_NAME "StacksTests")
target_sources(StacksTests PRIVATE
    Tests/Tests.cpp
    Source/Parameters.cpp
    Source/Wavetable.cpp
    Source/Patch.cpp
    Source/PatchGenerator.cpp
    Source/ai/LlmPatchGenerator.cpp
    Source/ai/LlamaBackend.cpp
    Source/ai/OllamaBackend.cpp
    Source/ai/ModelManager.cpp)
target_include_directories(StacksTests PRIVATE Source)
target_compile_definitions(StacksTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)
target_link_libraries(StacksTests
    PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_formats
        juce::juce_dsp
        llama
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
