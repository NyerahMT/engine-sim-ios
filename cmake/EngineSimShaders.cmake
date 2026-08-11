function(engine_sim_add_shader_artifacts target source output_directory)
    # Accept an explicit local build without requiring a PATH modification.
    # This is useful on Apple Silicon, where vcpkg does not package its DXC
    # dependency even though SDL_shadercross builds natively from source.
    set(ENGINE_SIM_SHADERCROSS_EXECUTABLE "" CACHE FILEPATH
        "Path to the SDL_shadercross offline compiler")
    if(NOT ENGINE_SIM_SHADERCROSS_EXECUTABLE)
        find_program(ENGINE_SIM_SHADERCROSS_EXECUTABLE NAMES shadercross)
    endif()
    if(NOT ENGINE_SIM_SHADERCROSS_EXECUTABLE)
        message(STATUS
            "shadercross was not found; ${target} will not generate SDL GPU shader artifacts. "
            "Set ENGINE_SIM_SHADERCROSS_EXECUTABLE to a locally built SDL_shadercross tool "
            "to enable the drawable desktop renderer.")
        return()
    endif()

    file(MAKE_DIRECTORY "${output_directory}")
    set(outputs)
    foreach(stage IN ITEMS vertex fragment)
        if(stage STREQUAL "vertex")
            set(entrypoint VSMain)
        else()
            set(entrypoint PSMain)
        endif()
        set(spirv "${output_directory}/engine_sim.${stage}.spv")
        set(msl "${output_directory}/engine_sim.${stage}.msl")
        set(dxil "${output_directory}/engine_sim.${stage}.dxil")

        add_custom_command(
            OUTPUT "${spirv}"
            COMMAND "${ENGINE_SIM_SHADERCROSS_EXECUTABLE}" "${source}"
                --source HLSL --dest SPIRV --stage "${stage}" --entrypoint "${entrypoint}"
                --output "${spirv}"
            DEPENDS "${source}"
            VERBATIM)
        add_custom_command(
            OUTPUT "${msl}"
            COMMAND "${ENGINE_SIM_SHADERCROSS_EXECUTABLE}" "${spirv}"
                --source SPIRV --dest MSL --stage "${stage}" --entrypoint "${entrypoint}"
                --output "${msl}"
            DEPENDS "${spirv}"
            VERBATIM)
        add_custom_command(
            OUTPUT "${dxil}"
            COMMAND "${ENGINE_SIM_SHADERCROSS_EXECUTABLE}" "${source}"
                --source HLSL --dest DXIL --stage "${stage}" --entrypoint "${entrypoint}"
                --output "${dxil}"
            DEPENDS "${source}"
            VERBATIM)
        list(APPEND outputs "${spirv}" "${msl}" "${dxil}")
    endforeach()

    add_custom_target(${target} DEPENDS ${outputs})
endfunction()
