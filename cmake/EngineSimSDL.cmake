include_guard(GLOBAL)

function(engine_sim_require_sdl3)
    # Keep Engine Simulator on the SDL release this port is validated against.
    # Without EXACT, CMake may accept a newer installed SDL (for example 3.4.x),
    # which makes CI builds depend on the runner image instead of this pin.
    find_package(SDL3 3.2.8 EXACT CONFIG QUIET)

    if(NOT TARGET SDL3::SDL3 AND ENGINE_SIM_FETCH_SDL3)
        FetchContent_Declare(
            SDL3
            GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
            GIT_TAG release-3.2.8
            GIT_SHALLOW TRUE
            EXCLUDE_FROM_ALL
        )

        FetchContent_MakeAvailable(SDL3)
    endif()

    if(NOT TARGET SDL3::SDL3)
        message(
            FATAL_ERROR
            "SDL3 3.2.8 was not found. Install SDL3 3.2.8 or configure with -DENGINE_SIM_FETCH_SDL3=ON."
        )
    endif()
endfunction()
