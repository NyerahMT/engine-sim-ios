include_guard(GLOBAL)

function(engine_sim_require_sdl3)
    #
    # SDL 3.2.8 has an iOS SDL_MAIN_USE_CALLBACKS bug that caps
    # SDL_AppIterate() at 60 Hz even on ProMotion displays.
    #
    # SDL 3.4.0 fixes the global CADisplayLink used by the callback
    # application host so iOS can actually drive us above 60 Hz.
    #
    # Pin the known-good release so CI does not silently pick an
    # arbitrary runner-installed SDL version.
    #
    find_package(
        SDL3
        3.4.0
        EXACT
        CONFIG
        QUIET
    )

    if(
        NOT TARGET SDL3::SDL3
        AND ENGINE_SIM_FETCH_SDL3
    )
        FetchContent_Declare(
            SDL3

            GIT_REPOSITORY
            https://github.com/libsdl-org/SDL.git

            GIT_TAG
            release-3.4.0

            GIT_SHALLOW
            TRUE

            EXCLUDE_FROM_ALL
        )

        FetchContent_MakeAvailable(
            SDL3
        )
    endif()

    if(
        NOT TARGET SDL3::SDL3
    )
        message(
            FATAL_ERROR

            "SDL3 3.4.0 was not found. "
            "Install SDL3 3.4.0 or configure with "
            "-DENGINE_SIM_FETCH_SDL3=ON."
        )
    endif()
endfunction()
