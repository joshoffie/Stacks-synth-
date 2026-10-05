cmake_minimum_required(VERSION 3.22)

# Oldest macOS the plugin will run on. Must be set before project().
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")

project(Stacks VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# JUCE is used straight from the copy installed in /Applications when there is
# one; otherwise (CI, Windows) the pinned release is fetched.
set(JUCE_DIR "/Applications/JUCE" CACHE PATH "Path to the JUCE source tree")
include(FetchContent)
if(EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
else()
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.12
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---- llama.cpp: the on-board language model runtime -------------------------
# Pinned release, built as static libraries and linked into the plug-in. Metal
# shaders are embedded in the binary so nothing has to be shipped beside it.
set(LLAMA_BUILD_TESTS     OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES  OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER    OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TOOLS     OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_COMMON    OFF CACHE BOOL "" FORCE)
set(LLAMA_CURL            OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS     OFF CACHE BOOL "" FORCE)
set(GGML_NATIVE           OFF CACHE BOOL "" FORCE)   # generic CPU of its kind, so every M1..M4 (or x86-64) runs it
set(GGML_OPENMP           OFF CACHE BOOL "" FORCE)
if(APPLE)
    set(GGML_METAL            ON  CACHE BOOL "" FORCE)
    set(GGML_METAL_EMBED_LIBRARY ON CACHE BOOL "" FORCE)
else()
    # Windows / Linux: CPU inference with the AVX2 path every x86-64 since 2013 has.
    set(GGML_METAL            OFF CACHE BOOL "" FORCE)
    set(GGML_AVX              ON  CACHE BOOL "" FORCE)
    set(GGML_AVX2             ON  CACHE BOOL "" FORCE)
    set(GGML_FMA              ON  CACHE BOOL "" FORCE)
    set(GGML_F16C             ON  CACHE BOOL "" FORCE)
endif()
FetchContent_Declare(llama
    GIT_REPOSITORY https://github.com/ggml-org/llama.cpp.git
    GIT_TAG        v0.5.0
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(llama)

if(APPLE)
    set(STACKS_FORMATS AU VST3 Standalone)
    set(STACKS_COPY_PLUGIN TRUE)
else()
    set(STACKS_FORMATS VST3 Standalone)
    set(STACKS_COPY_PLUGIN FALSE)   # no system plug-in folder to write to on a build machine
endif()

juce_add_plugin(Stacks
    PRODUCT_NAME "Stacks"
    COMPANY_NAME "Hoffman Audio"
    BUNDLE_ID com.hoffmanaudio.stacks
    PLUGIN_MANUFACTURER_CODE Hofa
    PLUGIN_CODE Stck
    FORMATS ${STACKS_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    # Install into ~/Library/Audio/Plug-Ins/{Components,VST3} after every build
    # so Logic picks the new version up on rescan.
    COPY_PLUGIN_AFTER_BUILD ${STACKS_COPY_PLUGIN})

juce_generate_juce_header(Stacks)

# The factory presets travel inside every bundle and are installed into the library on first run.
if(APPLE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Factory")
    juce_add_bundle_resources_directory(Stacks Factory)
endif()

target_sources(Stacks PRIVATE
    Source/Parameters.cpp
    Source/Wavetable.cpp
    Source/SynthVoice.cpp
    Source/Patch.cpp
    Source/Effects.cpp
    Source/Arpeggiator.cpp
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
    Source/SettingsPanel.cpp
    Source/TreeView.cpp
    Source/TunerView.cpp
    Source/ExplainView.cpp
    Source/ai/PatchExplainer.cpp
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
    Source/Arpeggiator.cpp
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
