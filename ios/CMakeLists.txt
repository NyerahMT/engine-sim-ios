cmake_minimum_required(VERSION 3.24)

project(
    EngineSimIOSHost
    VERSION 0.2.0
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
# EngineSim configuration
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
    OFF
    CACHE BOOL
    ""
    FORCE
)

#
# SDL is now enabled.
#

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
# Bring EngineSim libraries into the iOS build.
#

add_subdirectory(
    "${CMAKE_CURRENT_LIST_DIR}/.."
    "${CMAKE_CURRENT_BINARY_DIR}/engine-sim"
    EXCLUDE_FROM_ALL
)

#
# Pull the exact SDL3 version already selected by
# the EngineSim project.
#

include(
    "${CMAKE_CURRENT_LIST_DIR}/../cmake/EngineSimSDL.cmake"
)

engine_sim_require_sdl3()

#
# Native SDL3 iOS application.
#
# SDL_MAIN_USE_CALLBACKS lets SDL own the UIKit
# lifecycle, which is the cleanest path on iOS.
#

add_executable(
    EngineSimIOS
    MACOSX_BUNDLE
    main.cpp
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

#
# Force the visualization library through the iOS
# compiler during this build.
#
# Even though the temporary SDL dashboard does not
# call it yet, this catches visualization portability
# failures NOW instead of several milestones later.
#

add_dependencies(
    EngineSimIOS
    engine-sim-visualization
)

target_link_libraries(
    EngineSimIOS
    PRIVATE

    SDL3::SDL3

    engine-sim::core

    "-framework UIKit"
    "-framework Foundation"
    "-framework CoreGraphics"
    "-framework QuartzCore"
    "-framework Metal"
    "-framework GameController"
    "-framework AudioToolbox"
    "-framework AVFoundation"
)

#
# Some SDL configurations expose the main shim as a
# separate CMake target.
#

if(TARGET SDL3::SDL3main)

    target_link_libraries(
        EngineSimIOS
        PRIVATE
        SDL3::SDL3main
    )

endif()

target_include_directories(
    EngineSimIOS
    PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../include"
)

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
)
