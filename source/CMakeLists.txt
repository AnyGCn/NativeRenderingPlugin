if (APPLE)
    find_program(RENDERING_PLUGIN_XCRUN_EXECUTABLE xcrun REQUIRED)

    set(RENDERING_PLUGIN_METAL_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Metal/Raytracing.metal)

    if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
        set(RENDERING_PLUGIN_METAL_SDK iphoneos)
    else()
        set(RENDERING_PLUGIN_METAL_SDK macosx)
    endif()

    function(add_rendering_plugin_metallib target)
        set(metal_intermediate_dir ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${target}/metal)
        set(metal_air ${metal_intermediate_dir}/Raytracing.air)
        set(metal_lib ${metal_intermediate_dir}/default.metallib)
        set(metal_compile_flags
            $<$<CONFIG:Debug>:-g>
            $<$<CONFIG:Debug>:-frecord-sources>
        )

        add_custom_command(
            OUTPUT ${metal_lib}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${metal_intermediate_dir}
            COMMAND ${RENDERING_PLUGIN_XCRUN_EXECUTABLE} -sdk ${RENDERING_PLUGIN_METAL_SDK} metal ${metal_compile_flags} -c ${RENDERING_PLUGIN_METAL_SOURCE} -o ${metal_air}
            COMMAND ${RENDERING_PLUGIN_XCRUN_EXECUTABLE} -sdk ${RENDERING_PLUGIN_METAL_SDK} metallib ${metal_air} -o ${metal_lib}
            DEPENDS ${RENDERING_PLUGIN_METAL_SOURCE}
            COMMAND_EXPAND_LISTS
            VERBATIM
        )

        add_custom_target(${target}_metallib DEPENDS ${metal_lib})
        add_dependencies(${target} ${target}_metallib)
        set_property(GLOBAL PROPERTY ${target}_METALLIB_PATH ${metal_lib})
    endfunction()
endif()

foreach(rendering_plugin_target IN LISTS RENDERING_PLUGIN_TARGETS)
    target_sources(${rendering_plugin_target}
        PRIVATE
            RenderingPlugin.cpp
            RenderAPI.cpp
            PlatformBase.h
            RenderAPI.h
            RenderStructures.h
    )

    target_include_directories(${rendering_plugin_target}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/Unity
            ${CMAKE_CURRENT_SOURCE_DIR}/Metal
    )

    if (APPLE)
        target_sources(${rendering_plugin_target}
            PRIVATE
                Metal/AccelerationStructure.h
                Metal/AccelerationStructure.mm
                Metal/Lighting.h
                Metal/RenderAPI_Metal.h
                Metal/RenderAPI_Metal.mm
                Metal/ShaderDefinition.h
        )

        target_compile_options(${rendering_plugin_target}
            PRIVATE
                $<$<COMPILE_LANGUAGE:OBJC>:-fobjc-arc>
                $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>
        )

        add_rendering_plugin_metallib(${rendering_plugin_target})
    endif()

    if (ANDROID)
        target_sources(${rendering_plugin_target}
            PRIVATE
                RenderAPI_OpenGLCoreES.cpp
        )
    endif()
endforeach()

add_subdirectory(Unity)

if (WIN32)
    add_subdirectory(Windows)
endif()

if (ANDROID)
    add_subdirectory(gl3w)
endif()