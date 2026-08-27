include_guard(GLOBAL)

function(engine_sim_require_sdl3)
    #
    # SDL 3.2.8 has an iOS SDL_MAIN_USE_CALLBACKS bug that caps
    # SDL_AppIterate() at 60 Hz even on ProMotion displays.
    #
    # SDL 3.4.0 fixes the global CADisplayLink used by the callback
    # application host so iOS can actually drive us above 60 Hz.
    #
    # SDL 3.4.10 also fixes iOS cold-start document opens where the
    # SDL_EVENT_DROP_FILE was previously emitted before SDL's event system
    # was ready and silently lost. Use the current 3.4.x bugfix release so
    # tapping/opening an .mr file can launch Engine Simulator and deliver
    # the file to our import-and-save path.
    #
    # Pin the known-good release so CI does not silently pick an
    # arbitrary runner-installed SDL version.
    #
    find_package(
        SDL3
        3.4.12
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
            release-3.4.12

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

            "SDL3 3.4.12 was not found. "
            "Install SDL3 3.4.12 or configure with "
            "-DENGINE_SIM_FETCH_SDL3=ON."
        )
    endif()
endfunction()
