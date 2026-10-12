cmake_minimum_required(VERSION 3.24)

project(
  OpenLFO
  VERSION 0.1.0
  LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_XCODE_GENERATE_SCHEME OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(JUCE)

set(SOURCE_FILES
    Source/Config.cpp
    Source/Config.h
    Source/FilterEngine.cpp
    Source/FilterEngine.h
    Source/GraphEditor.cpp
    Source/GraphEditor.h
    Source/LfoShape.h
    Source/LookAndFeel.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h)

juce_add_plugin(
  ${PROJECT_NAME}
  COMPANY_NAME
  JezzterInc
  IS_SYNTH
  FALSE
  NEEDS_MIDI_INPUT
  TRUE
  NEEDS_MIDI_OUTPUT
  TRUE
  IS_MIDI_EFFECT
  FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS
  FALSE
  COPY_PLUGIN_AFTER_BUILD
  TRUE
  PLUGIN_MANUFACTURER_CODE
  Jzzt
  PLUGIN_CODE
  LfoT
  FORMATS
  VST3
  AU
  Standalone
  ICON_BIG
  "${CMAKE_CURRENT_SOURCE_DIR}/Assets/icon.png"
  PRODUCT_NAME
  "OpenLFO")

# How we want our SourceFiles to appear in our IDE
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCE_FILES})

# Make the SourceFiles buildable
target_sources(${PROJECT_NAME} PRIVATE ${SOURCE_FILES})

# These are some toggleable options from the JUCE CMake API
target_compile_definitions(
  ${PROJECT_NAME} PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
                         JUCE_VST3_CAN_REPLACE_VST2=0)

# JUCE libraries to bring into our project
target_link_libraries(
  ${PROJECT_NAME}
  PRIVATE juce::juce_audio_basics
          juce::juce_audio_devices
          juce::juce_audio_utils
          juce::juce_core
          juce::juce_data_structures
          juce::juce_dsp
          juce::juce_graphics
          juce::juce_gui_basics
          juce::juce_gui_extra
  PUBLIC juce::juce_recommended_config_flags
         juce::juce_recommended_lto_flags
         juce::juce_recommended_warning_flags)
