# =====================================================================
#  4GE UPDATER - a small tray app (Windows): keeps the latest 4GE test build (GitHub Releases "4ge-latest") installed in
#  C:\Program Files\Common Files\VST3, silently, in the background. The 4GE look + the sonicsystems badge.
#  Built by .github/workflows/build-4ge-updater.yml; JUCE 8.0.10 like the plugins.
# =====================================================================
cmake_minimum_required(VERSION 3.22)
project(FourGEUpdater VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(SHARED "${CMAKE_CURRENT_LIST_DIR}/../../../Shared")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/JUCE" JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.10 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_gui_app(FourGEUpdater PRODUCT_NAME "4GE Updater" COMPANY_NAME "sonicsystems" VERSION 1.0.0)
juce_add_binary_data(FourGEUpdaterData SOURCES
    ${SHARED}/Resources/Fonts/BarlowSemiCondensed-Medium.ttf
    ${SHARED}/Resources/Fonts/BarlowSemiCondensed-Bold.ttf)
target_sources(FourGEUpdater PRIVATE Source/Main.cpp)
target_include_directories(FourGEUpdater PRIVATE ${SHARED}/Source)
target_compile_definitions(FourGEUpdater PRIVATE JUCE_WEB_BROWSER=0 $<$<NOT:$<PLATFORM_ID:Windows>>:JUCE_USE_CURL=0> JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:FourGEUpdater,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:FourGEUpdater,JUCE_VERSION>" JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(FourGEUpdater PRIVATE FourGEUpdaterData juce::juce_gui_extra juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
if(MSVC)
    # writing into Program Files needs administrator rights: the app always asks Windows for them (the logon task runs it with
    # the highest rights, so there is no prompt in the background)
    target_link_options(FourGEUpdater PRIVATE "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'")
endif()
