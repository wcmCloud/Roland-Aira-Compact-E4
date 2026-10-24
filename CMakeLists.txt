cmake_minimum_required(VERSION 3.22)
project(AiraE4 VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(CTest)
add_executable(e4_core_tests Tests/CoreTests.cpp)
target_include_directories(e4_core_tests PRIVATE Source)
add_test(NAME e4_core_tests COMMAND e4_core_tests)
add_executable(e4_clock_tests Tests/ClockTests.cpp)
target_include_directories(e4_clock_tests PRIVATE Source)
add_test(NAME e4_clock_tests COMMAND e4_clock_tests)
add_executable(e4_mapping_tests Tests/MappingTests.cpp)
target_include_directories(e4_mapping_tests PRIVATE Source)
add_test(NAME e4_mapping_tests COMMAND e4_mapping_tests)
find_package(Threads REQUIRED)
target_link_libraries(e4_core_tests PRIVATE Threads::Threads)
add_executable(e4_output_tests Tests/OutputTests.cpp)
target_link_libraries(e4_output_tests PRIVATE Threads::Threads)
target_include_directories(e4_output_tests PRIVATE Source)
add_test(NAME e4_output_tests COMMAND e4_output_tests)
set_tests_properties(e4_output_tests PROPERTIES TIMEOUT 15)
option(E4_BUILD_APPS "Build JUCE plugin and standalone shells" ON)
option(E4_FETCH_JUCE "Download pinned JUCE if no local checkout is supplied" OFF)
set(JUCE_SOURCE_DIR "" CACHE PATH "Existing JUCE source checkout")
if(NOT E4_BUILD_APPS)
    return()
endif()
if(JUCE_SOURCE_DIR)
    add_subdirectory("${JUCE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")
elseif(E4_FETCH_JUCE)
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.10 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
else()
    message(FATAL_ERROR "Set JUCE_SOURCE_DIR or E4_FETCH_JUCE=ON; use E4_BUILD_APPS=OFF for core tests only.")
endif()
juce_add_binary_data(e4_brand NAMESPACE E4BrandData HEADER_NAME E4BrandData.h
    SOURCES assets/icon/png/app-icon-256.png)
set(E4_PLUGIN_FORMATS VST3)
if(APPLE)
    list(APPEND E4_PLUGIN_FORMATS AU)
endif()
juce_add_plugin(AiraE4
    COMPANY_NAME "Chris" BUNDLE_ID "com.chris.aira-e4"
    PLUGIN_MANUFACTURER_CODE Chrs PLUGIN_CODE Aefr
    FORMATS ${E4_PLUGIN_FORMATS} PRODUCT_NAME "E-4 Companion"
    IS_SYNTH TRUE NEEDS_MIDI_INPUT TRUE NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE COPY_PLUGIN_AFTER_BUILD FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Audio device access for the E-4 companion."
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon/png/app-icon-1024.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon/png/app-icon-256.png"
    VST3_CATEGORIES Instrument Tools)
target_sources(AiraE4 PRIVATE Source/PluginProcessor.cpp Source/PluginProcessor.h)
target_compile_definitions(AiraE4 PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(AiraE4 PRIVATE juce::juce_audio_utils PUBLIC e4_brand juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# Dedicated MIDI-only app: does not open or require an audio device.
juce_add_gui_app(AiraE4_Standalone
    PRODUCT_NAME "E-4 Companion" COMPANY_NAME "Chris" BUNDLE_ID "com.chris.aira-e4.standalone"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon/png/app-icon-1024.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon/png/app-icon-256.png")
target_sources(AiraE4_Standalone PRIVATE Source/StandaloneMain.cpp)
set_target_properties(AiraE4_Standalone PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/AiraE4_artefacts/$<CONFIG>/Standalone")
target_link_libraries(AiraE4_Standalone PRIVATE e4_brand juce::juce_audio_utils juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
target_compile_definitions(AiraE4_Standalone PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)

if(APPLE)
    juce_add_gui_app(e4_installer PRODUCT_NAME "E-4 Installer" COMPANY_NAME "Chris"
        BUNDLE_ID "com.chris.aira-e4.installer"
        ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon/png/app-icon-1024.png"
        ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon/png/app-icon-256.png")
    target_sources(e4_installer PRIVATE Tools/InstallerMain.cpp)
    target_compile_definitions(e4_installer PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(e4_installer PRIVATE e4_brand juce::juce_gui_basics juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()

juce_add_console_app(e4_midi_probe PRODUCT_NAME "E4 MIDI Probe")
target_sources(e4_midi_probe PRIVATE Tools/MidiProbe.cpp)
target_include_directories(e4_midi_probe PRIVATE Source)
target_link_libraries(e4_midi_probe PRIVATE juce::juce_audio_devices juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
target_compile_definitions(e4_midi_probe PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)

if(BUILD_TESTING)
    juce_add_console_app(e4_ui_preview PRODUCT_NAME "E4 UI Preview")
    target_sources(e4_ui_preview PRIVATE Tools/UiPreview.cpp Source/StandaloneMain.cpp)
    target_include_directories(e4_ui_preview PRIVATE Source)
    target_compile_definitions(e4_ui_preview PRIVATE E4_UI_PREVIEW=1)
    target_link_libraries(e4_ui_preview PRIVATE AiraE4 juce::juce_audio_utils)
    juce_add_console_app(e4_vst3_check PRODUCT_NAME "E4 VST3 Check")
    target_sources(e4_vst3_check PRIVATE Tools/Vst3Check.cpp)
    target_link_libraries(e4_vst3_check PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
    target_compile_definitions(e4_vst3_check PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_PLUGINHOST_AU=0 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    add_dependencies(e4_vst3_check AiraE4_VST3)
    add_test(NAME e4_vst3_wrapper_check COMMAND e4_vst3_check "${CMAKE_BINARY_DIR}/AiraE4_artefacts/$<CONFIG>/VST3/E-4 Companion.vst3")
    add_test(NAME e4_midi_probe_help COMMAND e4_midi_probe --help)
    add_test(NAME e4_midi_probe_invalid_duration COMMAND e4_midi_probe --capture unused --seconds invalid)
    set_tests_properties(e4_midi_probe_invalid_duration PROPERTIES WILL_FAIL TRUE)
    juce_add_console_app(e4_processor_tests PRODUCT_NAME "E4 Processor Tests")
    target_sources(e4_processor_tests PRIVATE Tests/ProcessorTests.cpp)
    target_include_directories(e4_processor_tests PRIVATE Source)
    target_link_libraries(e4_processor_tests PRIVATE AiraE4 juce::juce_audio_utils)
    add_test(NAME e4_processor_tests COMMAND e4_processor_tests)
endif()
