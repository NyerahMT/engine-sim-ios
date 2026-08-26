include_guard(GLOBAL)

function(engine_sim_require_sdl3)
    #
    # SDL 3.4+ is required for correct ProMotion behavior when using
    # SDL_MAIN_USE_CALLBACKS on iOS.
    #
    # SDL 3.2.x used a separate CADisplayLink for SDL_AppIterate that
    # remained capped at 60 Hz. SDL 3.4 fixes that display link and
    # requests the native high-refresh-rate range.
    #
    find_package(SDL3 3.4 CONFIG QUIET)

    if(NOT TARGET SDL3::SDL3 AND ENGINE_SIM_FETCH_SDL3)
        FetchContent_Declare(
            SDL3

            GIT_REPOSITORY
            https://github.com/libsdl-org/SDL.git

            GIT_TAG
            release-3.4.12

            GIT_SHALLOW
            TRUE

            EXCLUDE_FROM_ALL
        )

        FetchContent_MakeAvailable(SDL3)
    endif()

    if(NOT TARGET SDL3::SDL3)
        message(
            FATAL_ERROR
            "SDL3 3.4 or newer was not found. "
            "Install SDL3 3.4+ or configure with "
            "-DENGINE_SIM_FETCH_SDL3=ON."
        )
    endif()
endfunction()
