cmake_minimum_required(VERSION 3.22)
project(Shimmerstuff VERSION 0.2.0 LANGUAGES C CXX)

include(FetchContent)
FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.16-security GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(juce)
FetchContent_Declare(cat_ui GIT_REPOSITORY https://github.com/SpeakingColors/cat-ui GIT_TAG main)
FetchContent_MakeAvailable(cat_ui)

juce_add_plugin(Shimmerstuff
    COMPANY_NAME "Studio 87"
    PLUGIN_MANUFACTURER_CODE Spco
    PLUGIN_CODE Shst
    FORMATS VST3 Standalone
    PRODUCT_NAME "shimmerstuff"
    IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE
    VST3_CATEGORIES Fx Reverb
    COPY_PLUGIN_AFTER_BUILD FALSE)

enable_testing()
find_package(Threads REQUIRED)
foreach(t fx_test reverb_test preset_test)
  add_executable(${t} tests/${t}.cpp)
  target_include_directories(${t} PRIVATE src)
  target_compile_features(${t} PRIVATE cxx_std_17)
  target_link_libraries(${t} PRIVATE Threads::Threads)
  add_test(NAME ${t} COMMAND ${t})
endforeach()

target_sources(Shimmerstuff PRIVATE src/PluginProcessor.cpp src/PluginEditor.cpp)
target_include_directories(Shimmerstuff PRIVATE src)
target_compile_definitions(Shimmerstuff PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(Shimmerstuff PRIVATE cat_ui juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
