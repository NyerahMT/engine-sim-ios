cmake_minimum_required(VERSION 3.24)

project(
    EngineSimIOSHost
    VERSION 0.1.0
    LANGUAGES CXX OBJCXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT IOS)
    message(FATAL_ERROR "EngineSimIOSHost must be built for iOS.")
endif()

#
# Configure the parent EngineSim project for a core-only build.
#
# We deliberately leave desktop, web, scripting, tests and SDL disabled
# during this milestone. The purpose of this target is to prove that an
# actual UIKit application can link against EngineSim and become an iOS
# .app bundle.
#

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

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

set(
    ENGINE_SIM_FETCH_SDL3
    OFF
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
# Bring the EngineSim source tree into this iOS host project.
#
# The parent directory is the engine-sim-ios repository root.
#

add_subdirectory(
    "${CMAKE_CURRENT_LIST_DIR}/.."
    "${CMAKE_CURRENT_BINARY_DIR}/engine-sim"
    EXCLUDE_FROM_ALL
)

#
# Native iOS application.
#

add_executable(
    EngineSimIOS
    MACOSX_BUNDLE
    main.mm
)

target_compile_features(
    EngineSimIOS
    PRIVATE
    cxx_std_17
)

#
# The important part of this milestone:
#
# The UIKit application executable is linked directly against the native
# arm64 EngineSim simulation library that we already proved can compile for
# iPhoneOS.
#

target_link_libraries(
    EngineSimIOS
    PRIVATE
    engine-sim::core
    "-framework UIKit"
    "-framework Foundation"
)

target_include_directories(
    EngineSimIOS
    PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../include"
)

#
# Bundle configuration.
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
)

#
# Useful compile-time marker for everything we add to the native host later.
#

target_compile_definitions(
    EngineSimIOS
    PRIVATE
    ENGINE_SIM_IOS=1
)
