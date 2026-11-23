cmake_minimum_required(VERSION 3.22)

project(ChenRecPro VERSION 0.11.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if (APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "" FORCE)
endif()

# Tracktion Engine (+ JUCE fourni en sous-module)
set(TE_ADD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(external/tracktion_engine/modules/juce)
add_subdirectory(external/tracktion_engine/modules)

juce_add_gui_app(ChenRecPro
    PRODUCT_NAME "ChenRecPro"
    COMPANY_NAME "Studio des Chenevieres"
    BUNDLE_ID "fr.studiodeschenevieres.chenrecpro"
    VERSION ${PROJECT_VERSION}
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "ChenRecPro a besoin du micro pour enregistrer."
    NEEDS_CURL FALSE
    NEEDS_WEB_BROWSER FALSE)

set(VV_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/varivox")
juce_add_binary_data(ChenRecProData SOURCES Resources/pistache.png
    "${VV_DIR}/Resources/Cormorant-Italic.ttf" "${VV_DIR}/Resources/Cinzel-Regular.ttf")
set_target_properties(ChenRecProData PROPERTIES POSITION_INDEPENDENT_CODE ON)
juce_generate_juce_header(ChenRecPro)
target_sources(ChenRecPro PRIVATE Source/Main.cpp
    # VariVox (correction de justesse, Studio des Chènevières) : moteur + éditeur de notes
    "${VV_DIR}/Source/Engine/Analysis.cpp"
    "${VV_DIR}/Source/Engine/Render.cpp"
    "${VV_DIR}/Source/Take.cpp"
    "${VV_DIR}/Source/NoteView.cpp"
    "${VV_DIR}/Source/NoteViewLevel.cpp")
target_include_directories(ChenRecPro PRIVATE "${VV_DIR}/Source")

target_compile_definitions(ChenRecPro PRIVATE
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:ChenRecPro,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:ChenRecPro,JUCE_VERSION>"
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=1
    $<$<PLATFORM_ID:Linux>:JUCE_PLUGINHOST_LADSPA=1>
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=1)

target_link_libraries(ChenRecPro PRIVATE
    ChenRecProData
    tracktion::tracktion_core
    tracktion::tracktion_engine
    tracktion::tracktion_graph
    juce::juce_audio_devices
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_recommended_config_flags)

if (MSVC)
    target_compile_options(ChenRecPro PRIVATE /utf-8 /bigobj)
endif()

# ASIO (SDK Steinberg sous licence GPLv3) : indispensable pour les interfaces pro sous Windows
if (WIN32)
    target_include_directories(ChenRecPro PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/external/asiosdk/common")
    target_compile_definitions(ChenRecPro PRIVATE JUCE_ASIO=1)
endif()

if (APPLE)
    target_compile_options(ChenRecPro PRIVATE "-fno-aligned-allocation")
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(ChenRecPro PRIVATE "-latomic")
endif()
