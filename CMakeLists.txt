cmake_minimum_required(VERSION 3.24)

project(engine-sim VERSION 0.2.0 LANGUAGES CXX)

include(CTest)
include(FetchContent)
include(cmake/EngineSimShaders.cmake)

option(ENGINE_SIM_BUILD_DESKTOP "Build the SDL3 desktop host" OFF)
option(ENGINE_SIM_BUILD_SCRIPTING "Build the Piranha script interpreter" OFF)
option(ENGINE_SIM_FETCH_SDL3 "Fetch pinned SDL3 when it is not installed" ON)
option(ENGINE_SIM_MACOS_APP_BUNDLE "Build the desktop host as a macOS .app bundle" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "cmake")

set(ENGINE_SIM_SUBMODULE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/submodules")
foreach(submodule IN ITEMS simple-2d-constraint-solver csv-io piranha)
    if(NOT EXISTS "${ENGINE_SIM_SUBMODULE_DIR}/${submodule}/CMakeLists.txt")
        message(FATAL_ERROR
            "Missing ${submodule} submodule. Run: git submodule update --init --recursive")
    endif()
endforeach()

# The simulator owns the constraint solver, but has no desktop, graphics, or
# audio-device dependency. Keep third-party test executables out of the normal
# build while retaining their library target for the core.
file(GLOB ENGINE_SIM_SCS_SOURCES CONFIGURE_DEPENDS
    "${ENGINE_SIM_SUBMODULE_DIR}/simple-2d-constraint-solver/src/*.cpp")
add_library(simple-2d-constraint-solver STATIC ${ENGINE_SIM_SCS_SOURCES})
target_include_directories(simple-2d-constraint-solver
    PUBLIC "${ENGINE_SIM_SUBMODULE_DIR}/simple-2d-constraint-solver/include")
set_property(TARGET simple-2d-constraint-solver PROPERTY FOLDER "third_party/scs")
# The pinned solver was authored for MSVC and exposes __forceinline in public
# headers. Map it to standard inline for Clang and GCC without changing the
# checked-out third-party source.
if(NOT MSVC)
    target_compile_definitions(simple-2d-constraint-solver
        PUBLIC __forceinline=inline)
    # The retained upstream solver has a few legacy include-order omissions.
    # Preinclude their compatibility header without modifying the pinned
    # submodule checkout.
    target_compile_options(simple-2d-constraint-solver PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/cmake/EngineSimLegacySolverCompatibility.h")
endif()

set(ENGINE_SIM_CORE_SOURCES
    src/camshaft.cpp
    src/combustion_chamber.cpp
    src/connecting_rod.cpp
    src/convolution_filter.cpp
    src/crankshaft.cpp
    src/cylinder_bank.cpp
    src/cylinder_head.cpp
    src/delay_filter.cpp
    src/derivative_filter.cpp
    src/direct_throttle_linkage.cpp
    src/dynamometer.cpp
    src/engine.cpp
    src/exhaust_system.cpp
    src/feedback_comb_filter.cpp
    src/filter.cpp
    src/fuel.cpp
    src/function.cpp
    src/gas_system.cpp
    src/gaussian_filter.cpp
    src/governor.cpp
    src/ignition_module.cpp
    src/impulse_response.cpp
    src/intake.cpp
    src/jitter_filter.cpp
    src/leveling_filter.cpp
    src/low_pass_filter.cpp
    src/part.cpp
    src/piston.cpp
    src/piston_engine_simulator.cpp
    src/simulator.cpp
    src/standard_valvetrain.cpp
    src/starter_motor.cpp
    src/synthesizer.cpp
    src/throttle.cpp
    src/transmission.cpp
    src/utilities.cpp
    src/valvetrain.cpp
    src/vehicle.cpp
    src/vehicle_drag_constraint.cpp
    src/vtec_valvetrain.cpp)

add_library(engine-sim-core STATIC ${ENGINE_SIM_CORE_SOURCES})
add_library(engine-sim::core ALIAS engine-sim-core)
target_include_directories(engine-sim-core
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")
# The solver's types appear in public simulator headers, so downstream users
# must receive its include-time compatibility definitions as well.
target_link_libraries(engine-sim-core PUBLIC simple-2d-constraint-solver)
target_compile_features(engine-sim-core PUBLIC cxx_std_17)

add_library(engine-sim-render-support STATIC
    src/geometry_generator.cpp
    src/runtime_paths.cpp
    src/shaders.cpp
    src/ui_math.cpp)
target_include_directories(engine-sim-render-support
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_link_libraries(engine-sim-render-support PUBLIC engine-sim::core)
target_compile_features(engine-sim-render-support PUBLIC cxx_std_17)

# The UI and simulation-view objects have no platform ownership. Compile them
# separately so SDL remains a desktop-host concern and the migration can keep
# their existing application-facing draw calls intact.
add_library(engine-sim-visualization STATIC
    src/afr_cluster.cpp
    src/combustion_chamber_object.cpp
    src/connecting_rod_object.cpp
    src/crankshaft_object.cpp
    src/cylinder_bank_object.cpp
    src/cylinder_head_object.cpp
    src/cylinder_pressure_gauge.cpp
    src/cylinder_temperature_gauge.cpp
    src/desktop_application.cpp
    src/engine_view.cpp
    src/firing_order_display.cpp
    src/fuel_cluster.cpp
    src/gauge.cpp
    src/info_cluster.cpp
    src/labeled_gauge.cpp
    src/load_simulation_cluster.cpp
    src/authored_mesh_library.cpp
    src/mixer_cluster.cpp
    src/oscilloscope.cpp
    src/oscilloscope_cluster.cpp
    src/performance_cluster.cpp
    src/piston_object.cpp
    src/right_gauge_cluster.cpp
    src/simulation_object.cpp
    src/throttle_display.cpp
    src/text_renderer.cpp
    src/ui_button.cpp
    src/ui_element.cpp
    src/ui_manager.cpp
    src/ui_utilities.cpp)
target_include_directories(engine-sim-visualization
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb")
target_link_libraries(engine-sim-visualization PUBLIC engine-sim-render-support)
target_compile_features(engine-sim-visualization PUBLIC cxx_std_17)

if(ENGINE_SIM_BUILD_SCRIPTING)
    # Piranha's upstream CMake project unconditionally fetches a second
    # GoogleTest copy, even when only the library is wanted. Build its parser
    # library directly so scripting stays offline/reproducible with the same
    # dependency policy as the rest of this project.
    find_package(FLEX REQUIRED)
    # The Piranha grammar declares `%require "3.2"`; fail at configure time
    # rather than emitting a late parser-generation error from an old system
    # Bison (notably macOS's bundled 2.3).
    find_package(BISON 3.2 REQUIRED)
    set(ENGINE_SIM_PIRANHA_DIR "${ENGINE_SIM_SUBMODULE_DIR}/piranha")
    flex_target(engine_sim_piranha_lexer
        "${ENGINE_SIM_PIRANHA_DIR}/flex-bison/scanner.l"
        "${CMAKE_CURRENT_BINARY_DIR}/piranha/scanner.auto.cpp"
        DEFINES_FILE "${CMAKE_CURRENT_BINARY_DIR}/piranha/scanner.auto.h")
    bison_target(engine_sim_piranha_parser
        "${ENGINE_SIM_PIRANHA_DIR}/flex-bison/specification.y"
        "${CMAKE_CURRENT_BINARY_DIR}/piranha/parser.auto.cpp"
        DEFINES_FILE "${CMAKE_CURRENT_BINARY_DIR}/piranha/parser.auto.h")
    add_flex_bison_dependency(engine_sim_piranha_lexer engine_sim_piranha_parser)
    file(GLOB ENGINE_SIM_PIRANHA_SOURCES CONFIGURE_DEPENDS
        "${ENGINE_SIM_PIRANHA_DIR}/src/*.cpp")
    list(REMOVE_ITEM ENGINE_SIM_PIRANHA_SOURCES "${ENGINE_SIM_PIRANHA_DIR}/src/path.cpp")
    add_library(piranha STATIC
        ${ENGINE_SIM_PIRANHA_SOURCES}
        src/piranha_path_compat.cpp
        ${BISON_engine_sim_piranha_parser_OUTPUTS}
        ${FLEX_engine_sim_piranha_lexer_OUTPUTS})
    target_include_directories(piranha
        PUBLIC "${ENGINE_SIM_SUBMODULE_DIR}"
            "${ENGINE_SIM_PIRANHA_DIR}/include"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include"
            "${CMAKE_CURRENT_BINARY_DIR}/piranha"
            "${ENGINE_SIM_PIRANHA_DIR}/dependencies/libraries/flex/include")
    target_compile_features(piranha PUBLIC cxx_std_17)
    if(NOT MSVC)
        target_compile_definitions(piranha PUBLIC __int64=long)
    endif()
    set_property(TARGET piranha PROPERTY FOLDER "third_party/piranha")

    add_library(engine-sim-scripting STATIC
        scripting/src/channel_types.cpp
        scripting/src/compiler.cpp
        scripting/src/engine_context.cpp
        scripting/src/language_rules.cpp)
    add_library(engine-sim::scripting ALIAS engine-sim-scripting)
    target_include_directories(engine-sim-scripting
        PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/scripting/include>")
    target_link_libraries(engine-sim-scripting
        PUBLIC engine-sim::core piranha)
    target_compile_features(engine-sim-scripting PUBLIC cxx_std_17)

    # EngineSimApplication lives in engine-sim-visualization, not in the SDL
    # executable. Give its owning target both the scripting dependency and the
    # feature flag so loadScript() is present in desktop builds.
    target_link_libraries(engine-sim-visualization PUBLIC engine-sim-scripting)
    target_compile_definitions(engine-sim-visualization PRIVATE ATG_ENGINE_SIM_PIRANHA_ENABLED)
endif()

if(ENGINE_SIM_BUILD_DESKTOP)
    find_package(SDL3 3.2 CONFIG QUIET)
    if(NOT TARGET SDL3::SDL3 AND ENGINE_SIM_FETCH_SDL3)
        FetchContent_Declare(SDL3
            GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
            GIT_TAG release-3.2.8
            GIT_SHALLOW TRUE
            EXCLUDE_FROM_ALL)
        FetchContent_MakeAvailable(SDL3)
    endif()
    if(NOT TARGET SDL3::SDL3)
        message(FATAL_ERROR
            "SDL3 was not found. Install SDL3 or configure with -DENGINE_SIM_FETCH_SDL3=ON.")
    endif()

    # This intentionally small host proves the SDL3 dependency and target
    # boundary. Step 2 replaces its loop with EngineSimApplication adapters.
    add_executable(engine-sim-desktop
        src/desktop_main.cpp
        src/desktop_platform_sdl.cpp
        src/sdl_audio_output.cpp
        src/sdl_gpu_renderer.cpp)
    target_link_libraries(engine-sim-desktop PRIVATE SDL3::SDL3 engine-sim-visualization)
    target_compile_features(engine-sim-desktop PRIVATE cxx_std_17)
    target_compile_definitions(engine-sim-desktop PRIVATE
        ENGINE_SIM_SHADER_DIRECTORY="${CMAKE_CURRENT_BINARY_DIR}/shaders"
        ENGINE_SIM_SOURCE_ASSET_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/assets")
    if(ENGINE_SIM_BUILD_SCRIPTING)
        target_link_libraries(engine-sim-desktop PRIVATE engine-sim-scripting)
        target_compile_definitions(engine-sim-desktop PRIVATE ATG_ENGINE_SIM_PIRANHA_ENABLED)
    endif()
    engine_sim_add_shader_artifacts(engine-sim-shaders
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/engine_sim.hlsl"
        "${CMAKE_CURRENT_BINARY_DIR}/shaders")
    if(TARGET engine-sim-shaders)
        add_dependencies(engine-sim-desktop engine-sim-shaders)
    endif()

    # Keep the normal executable in bin so RuntimePaths' established ../assets
    # layout works without a launcher or compile-time path. Packaging can opt
    # into a real macOS app bundle without changing the developer executable.
    # SDL3 is the only non-system runtime dependency of this SDL host, so
    # install it explicitly; CMake's generic dependency scanner omits
    # build-tree targets.
    if(APPLE)
        set_target_properties(engine-sim-desktop PROPERTIES INSTALL_RPATH "@executable_path")
    elseif(UNIX)
        set_target_properties(engine-sim-desktop PROPERTIES INSTALL_RPATH "$ORIGIN")
    endif()
    if(APPLE AND ENGINE_SIM_MACOS_APP_BUNDLE)
        set_target_properties(engine-sim-desktop PROPERTIES
            MACOSX_BUNDLE TRUE
            OUTPUT_NAME "engine-sim")
        set(ENGINE_SIM_INSTALL_ASSET_DIRECTORY "engine-sim.app/Contents/assets")
        set(ENGINE_SIM_INSTALL_RUNTIME_DIRECTORY "engine-sim.app/Contents/MacOS")
        install(TARGETS engine-sim-desktop BUNDLE DESTINATION .)
    else()
        set(ENGINE_SIM_INSTALL_ASSET_DIRECTORY "assets")
        set(ENGINE_SIM_INSTALL_RUNTIME_DIRECTORY "bin")
        install(TARGETS engine-sim-desktop RUNTIME DESTINATION "${ENGINE_SIM_INSTALL_RUNTIME_DIRECTORY}")
    endif()
    install(FILES "$<TARGET_FILE:SDL3::SDL3>" DESTINATION "${ENGINE_SIM_INSTALL_RUNTIME_DIRECTORY}")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/" DESTINATION "${ENGINE_SIM_INSTALL_ASSET_DIRECTORY}"
        PATTERN ".DS_Store" EXCLUDE)
    if(TARGET engine-sim-shaders)
        install(FILES
            "${CMAKE_CURRENT_BINARY_DIR}/shaders/engine_sim.vertex.spv"
            "${CMAKE_CURRENT_BINARY_DIR}/shaders/engine_sim.fragment.spv"
            "${CMAKE_CURRENT_BINARY_DIR}/shaders/engine_sim.vertex.dxil"
            "${CMAKE_CURRENT_BINARY_DIR}/shaders/engine_sim.fragment.dxil"
            "${CMAKE_CURRENT_BINARY_DIR}/shaders/engine_sim.vertex.msl"
            "${CMAKE_CURRENT_BINARY_DIR}/shaders/engine_sim.fragment.msl"
            DESTINATION "${ENGINE_SIM_INSTALL_ASSET_DIRECTORY}/shaders")
    endif()
endif()

if(TARGET engine-sim-desktop)
    set(CPACK_PACKAGE_NAME "open-engine-simulator")
    set(CPACK_PACKAGE_VENDOR "Open Engine Simulator contributors")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
        "Portable SDL desktop fork of AngeTheGreat's Engine Simulator")
    set(CPACK_PACKAGE_FILE_NAME
        "open-engine-simulator-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
    set(CPACK_MONOLITHIC_INSTALL ON)
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
    if(WIN32 OR APPLE)
        # A ZIP keeps the Windows runtime together and preserves the macOS app
        # bundle as one portable artifact without requiring signing on CI.
        set(CPACK_GENERATOR "ZIP")
    elseif(UNIX)
        set(CPACK_GENERATOR "TGZ")
    endif()
    include(CPack)
endif()

if(BUILD_TESTING)
    add_library(csv-io STATIC
        "${ENGINE_SIM_SUBMODULE_DIR}/csv-io/src/information.cpp"
        "${ENGINE_SIM_SUBMODULE_DIR}/csv-io/src/csv_data.cpp")
    target_include_directories(csv-io
        PUBLIC "${ENGINE_SIM_SUBMODULE_DIR}/csv-io/include")
    set_property(TARGET csv-io PROPERTY FOLDER "third_party/csv")

    find_package(GTest CONFIG QUIET)
    if(NOT TARGET GTest::gtest_main)
        FetchContent_Declare(googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG 58d77fa8070e8cec2dc1ed015d66b454c8d78850
            GIT_SHALLOW TRUE
            EXCLUDE_FROM_ALL)
        FetchContent_MakeAvailable(googletest)
    endif()

    add_executable(engine-sim-core-tests
        test/function_test.cpp
        test/gas_system_tests.cpp
        test/geometry_generator_test.cpp
        test/runtime_paths_test.cpp
        test/shaders_test.cpp
        test/synthesizer_tests.cpp
        test/text_renderer_test.cpp
        test/authored_mesh_library_test.cpp
        src/text_renderer.cpp
        src/authored_mesh_library.cpp)
    if(ENGINE_SIM_BUILD_SCRIPTING)
        target_sources(engine-sim-core-tests PRIVATE test/script_compile_test.cpp)
        target_link_libraries(engine-sim-core-tests PRIVATE engine-sim-scripting)
        target_compile_definitions(engine-sim-core-tests PRIVATE
            ENGINE_SIM_TEST_ASSET_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/assets")
    endif()
    target_link_libraries(engine-sim-core-tests
        PRIVATE engine-sim::core engine-sim-render-support csv-io gtest_main)
    target_include_directories(engine-sim-core-tests
        PRIVATE
            "${ENGINE_SIM_SUBMODULE_DIR}/csv-io/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb")
    target_compile_definitions(engine-sim-core-tests PRIVATE
        ENGINE_SIM_TEST_SOURCE_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}")
    include(GoogleTest)
    gtest_discover_tests(engine-sim-core-tests)

    if(ENGINE_SIM_BUILD_DESKTOP)
        # Runs against SDL's dummy device, so WAV conversion and stream
        # lifetime can be checked on CI without a physical audio endpoint.
        add_executable(engine-sim-desktop-audio-tests
            test/sdl_audio_output_test.cpp
            src/sdl_audio_output.cpp)
        target_link_libraries(engine-sim-desktop-audio-tests
            PRIVATE SDL3::SDL3 engine-sim::core gtest_main)
        target_compile_definitions(engine-sim-desktop-audio-tests PRIVATE
            ENGINE_SIM_TEST_ASSET_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/assets")
        gtest_discover_tests(engine-sim-desktop-audio-tests)
    endif()
endif()
