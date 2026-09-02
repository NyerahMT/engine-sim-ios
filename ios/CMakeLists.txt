cmake_minimum_required(VERSION 3.24)

project(
    EngineSimIOSHost
    VERSION 0.4.1
    LANGUAGES
        C
        CXX
        OBJC
        OBJCXX
)

set(
    CMAKE_CXX_STANDARD
    17
)

set(
    CMAKE_CXX_STANDARD_REQUIRED
    ON
)

set(
    CMAKE_CXX_EXTENSIONS
    OFF
)

if(NOT IOS)
    message(
        FATAL_ERROR
        "EngineSimIOSHost must be built for iOS."
    )
endif()

#
# ============================================================
# EngineSim configuration
# ============================================================
#

set(
    BUILD_TESTING
    OFF
    CACHE BOOL
    ""
    FORCE
)

set(
    ENGINE_SIM_BUILD_DESKTOP
    OFF
    CACHE BOOL
    ""
    FORCE
)

set(
    ENGINE_SIM_BUILD_WEB
    OFF
    CACHE BOOL
    ""
    FORCE
)

set(
    ENGINE_SIM_BUILD_SCRIPTING
    ON
    CACHE BOOL
    ""
    FORCE
)

set(
    ENGINE_SIM_FETCH_SDL3
    ON
    CACHE BOOL
    ""
    FORCE
)

set(
    ENGINE_SIM_MACOS_APP_BUNDLE
    OFF
    CACHE BOOL
    ""
    FORCE
)

#
# ============================================================
# EngineSim libraries
# ============================================================
#

add_subdirectory(
    "${CMAKE_CURRENT_LIST_DIR}/.."
    "${CMAKE_CURRENT_BINARY_DIR}/engine-sim"
    EXCLUDE_FROM_ALL
)

#
# The visualization target contains EngineSimApplication::tick().
#
# Mark that target as iOS as well so it can select the
# mobile-specific presentation cadence.
#

target_compile_definitions(
    engine-sim-visualization

    PRIVATE

    ENGINE_SIM_IOS=1
)

#
# ============================================================
# SDL3
# ============================================================
#

include(
    "${CMAKE_CURRENT_LIST_DIR}/../cmake/EngineSimSDL.cmake"
)

engine_sim_require_sdl3()

#
# ============================================================
# iOS resources
# ============================================================
#

set(
    ENGINE_SIM_IOS_ASSET_CATALOG
    "${CMAKE_CURRENT_LIST_DIR}/Assets.xcassets"
)

set_source_files_properties(
    "${ENGINE_SIM_IOS_ASSET_CATALOG}"

    PROPERTIES

    MACOSX_PACKAGE_LOCATION
    "Resources"
)

#
# ============================================================
# Native application
# ============================================================
#

add_executable(
    EngineSimIOS

    MACOSX_BUNDLE

    main.cpp

    document_open_bridge.mm

    png_decode_bridge.mm

    ios_platform_sdl.cpp

    "${CMAKE_CURRENT_LIST_DIR}/../src/sdl_gpu_renderer.cpp"

    "${CMAKE_CURRENT_LIST_DIR}/../src/sdl_audio_output.cpp"

    "${CMAKE_CURRENT_LIST_DIR}/../src/sdl_audio_util.cpp"

    "${ENGINE_SIM_IOS_ASSET_CATALOG}"
)

target_compile_features(
    EngineSimIOS

    PRIVATE

    cxx_std_17
)

target_compile_definitions(
    EngineSimIOS

    PRIVATE

    ENGINE_SIM_IOS=1
)

target_include_directories(
    EngineSimIOS

    PRIVATE

    "${CMAKE_CURRENT_LIST_DIR}"

    "${CMAKE_CURRENT_LIST_DIR}/../include"

    "${CMAKE_CURRENT_LIST_DIR}/../scripting/include"
)

#
# ============================================================
# Link actual EngineSim
# ============================================================
#

target_link_libraries(
    EngineSimIOS

    PRIVATE

    SDL3::SDL3

    engine-sim::core

    engine-sim-visualization

    engine-sim-scripting

    "-framework UIKit"

    "-framework Foundation"

    "-framework CoreGraphics"

    "-framework QuartzCore"

    "-framework Metal"

    "-framework MetalKit"

    "-framework GameController"

    "-framework AudioToolbox"

    "-framework AVFoundation"

    "-framework CoreAudio"
)

if(TARGET SDL3::SDL3main)

    target_link_libraries(
        EngineSimIOS

        PRIVATE

        SDL3::SDL3main
    )

endif()

#
# ============================================================
# Bundle assets
# ============================================================
#

add_custom_command(
    TARGET
    EngineSimIOS

    POST_BUILD

    COMMAND
    "${CMAKE_COMMAND}"
    -E
    rm
    -rf
    "$<TARGET_BUNDLE_DIR:EngineSimIOS>/assets"

    COMMAND
    "${CMAKE_COMMAND}"
    -E
    copy_directory
    "${CMAKE_CURRENT_LIST_DIR}/../assets"
    "$<TARGET_BUNDLE_DIR:EngineSimIOS>/assets"

    COMMAND
    /usr/libexec/PlistBuddy
    -c
    "Set :CFBundleDisplayName Engine Simulator"
    "$<TARGET_BUNDLE_DIR:EngineSimIOS>/Info.plist"

    COMMAND
    /usr/libexec/PlistBuddy
    -c
    "Set :CFBundleName Engine Simulator"
    "$<TARGET_BUNDLE_DIR:EngineSimIOS>/Info.plist"

    COMMAND
    /usr/libexec/PlistBuddy
    -c
    "Print :CFBundleDisplayName"
    "$<TARGET_BUNDLE_DIR:EngineSimIOS>/Info.plist"

    COMMENT
    "Bundling EngineSim runtime assets and applying iOS display name"
)

#
# ============================================================
# iOS bundle configuration
# ============================================================
#

set_target_properties(
    EngineSimIOS

    PROPERTIES

    OUTPUT_NAME
    "EngineSim"

    MACOSX_BUNDLE
    TRUE

    MACOSX_BUNDLE_INFO_PLIST
    "${CMAKE_CURRENT_LIST_DIR}/Info.plist"

    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER
    "com.nyeraht.engine-sim-ios"

    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY
    "1,2"

    XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET
    "15.0"

    XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED
    "NO"

    XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED
    "NO"

    XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY
    ""

    XCODE_ATTRIBUTE_SUPPORTED_PLATFORMS
    "iphoneos"

    XCODE_ATTRIBUTE_SUPPORTS_MACCATALYST
    "NO"

    XCODE_ATTRIBUTE_ENABLE_BITCODE
    "NO"

    XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC
    "YES"

    XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME
    "AppIcon"

    XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_INCLUDE_ALL_APPICON_ASSETS
    "YES"
)
