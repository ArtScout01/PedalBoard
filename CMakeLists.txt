cmake_minimum_required(VERSION 3.22)
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "")
endif()
project(PedalBoard VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 17)
include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(PedalBoard
    PRODUCT_NAME "PedalBoard"
    COMPANY_NAME "PedalBoard"
    PLUGIN_MANUFACTURER_CODE Pdbd
    PLUGIN_CODE Pbd1
    FORMATS VST3 Standalone
    IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE)

juce_add_binary_data(PedalBoardAssets SOURCES Assets/carpet.jpg)

juce_generate_juce_header(PedalBoard)
target_sources(PedalBoard PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(PedalBoard PRIVATE Source Source/pb)
target_compile_definitions(PedalBoard PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(PedalBoard
    PRIVATE PedalBoardAssets juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
