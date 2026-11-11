# Outil hors plugin : compile le processeur + la fenetre de SYNTHETIKA sous Linux, teste l'audio et exporte des captures PNG
cmake_minimum_required(VERSION 3.22)
project(SynthetikaRender VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 20)
add_subdirectory(${JUCE_DIR} JUCE)
set(W ${CMAKE_CURRENT_SOURCE_DIR}/../..)
file(GLOB SYNTH_ASSETS ${W}/assets/*.png)
juce_add_binary_data(SynthetikaAssets SOURCES ${SYNTH_ASSETS} ${W}/assets/NineHandsLogoWhite.svg ${W}/assets/Poppins-Bold.ttf ${W}/assets/Poppins-Medium.ttf)
juce_add_console_app(SynthetikaRender PRODUCT_NAME "SynthetikaRender")
target_sources(SynthetikaRender PRIVATE main.cpp ${W}/PluginProcessor.cpp ${W}/PluginEditor.cpp)
target_include_directories(SynthetikaRender PRIVATE ${W})
target_compile_options(SynthetikaRender PRIVATE -O2)
target_compile_definitions(SynthetikaRender PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_ALSA=0 JUCE_JACK=0
  JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XSHM=0 JucePlugin_Name="Synthetika" JucePlugin_Build_VST3=0
  JucePlugin_VersionString="1.0.0" JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(SynthetikaRender PRIVATE SynthetikaAssets juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
