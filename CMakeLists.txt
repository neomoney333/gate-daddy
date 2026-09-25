cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")
endif()

project(GateDaddy VERSION 1.0.0)

option(GATEDADDY_COPY_AFTER_BUILD "Install plugins into the system plugin folders after building" ON)

# Use a local JUCE checkout if present, otherwise download it.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(GateDaddy
    COMPANY_NAME "Gate Daddy Labs"
    BUNDLE_ID "com.gatedaddylabs.gatedaddy"
    PLUGIN_MANUFACTURER_CODE Gdlb
    PLUGIN_CODE GtDd
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Gate Daddy"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${GATEDADDY_COPY_AFTER_BUILD}
    VST3_CATEGORIES Fx Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Gate Daddy needs audio input to process sound in standalone mode.")

target_sources(GateDaddy PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(GateDaddy PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    GATEDADDY_REPO="neomoney333/gate-daddy"
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_compile_features(GateDaddy PUBLIC cxx_std_20)

target_link_libraries(GateDaddy
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline test harness (not shipped)
option(GATEDADDY_BUILD_TESTS "Build the offline test harness" ON)
if(GATEDADDY_BUILD_TESTS)
juce_add_console_app(GateDaddyTest PRODUCT_NAME "GateDaddyTest")
target_sources(GateDaddyTest PRIVATE tests/Harness.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(GateDaddyTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Gate Daddy" JucePlugin_VersionString="${PROJECT_VERSION}" GATEDADDY_REPO="neomoney333/gate-daddy")
target_compile_features(GateDaddyTest PRIVATE cxx_std_20)
set_target_properties(GateDaddyTest PROPERTIES OSX_ARCHITECTURES arm64)
target_link_libraries(GateDaddyTest PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
