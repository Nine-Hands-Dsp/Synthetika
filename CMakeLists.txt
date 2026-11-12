cmake_minimum_required(VERSION 3.22)

# macOS : un seul binaire universel (Apple Silicon + Intel)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")

project(Synthetika VERSION 1.3.0)

if(DEFINED JUCE_DIR)
  add_subdirectory(${JUCE_DIR} juce)
else()
  include(FetchContent)
  FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW ON)
  FetchContent_MakeAvailable(JUCE)
endif()

file(GLOB SYNTH_ASSETS ${CMAKE_CURRENT_SOURCE_DIR}/assets/*.png)
juce_add_binary_data(SynthetikaAssets SOURCES
  ${SYNTH_ASSETS}
  assets/NineHandsLogoWhite.svg assets/Poppins-Bold.ttf assets/Poppins-Medium.ttf)

juce_add_plugin(Synthetika
  COMPANY_NAME "NINE HANDS | DSP"
  PLUGIN_MANUFACTURER_CODE Wiga
  PLUGIN_CODE WgSk
  FORMATS VST3
  VST3_CATEGORIES Fx Synth
  BUNDLE_ID com.ninehandsdsp.synthetika
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  PRODUCT_NAME "Synthetika"
  COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Synthetika PRIVATE PluginProcessor.cpp PluginEditor.cpp)


# verrou de taille de la fenetre du DAW (jamais sous 60 %, jamais au-dela de 150 %)
if(APPLE)
  target_sources(Synthetika PRIVATE WindowMinSize.mm)
  target_link_libraries(Synthetika PRIVATE "-framework Cocoa")
else()
  target_sources(Synthetika PRIVATE WindowMinSize.cpp)
endif()
if(WIN32)
  target_link_libraries(Synthetika PRIVATE comctl32 user32)
endif()
if(MSVC)
  set_source_files_properties(PluginProcessor.cpp PROPERTIES COMPILE_OPTIONS "/fp:fast")
else()
  set_source_files_properties(PluginProcessor.cpp PROPERTIES COMPILE_OPTIONS "-O3;-ffast-math")
endif()

target_compile_definitions(Synthetika PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Synthetika
  PRIVATE
    SynthetikaAssets
    juce::juce_audio_utils
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
