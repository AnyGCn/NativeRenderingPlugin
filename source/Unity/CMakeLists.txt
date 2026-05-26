foreach(rendering_plugin_target IN LISTS RENDERING_PLUGIN_TARGETS)
    target_sources(${rendering_plugin_target}
        PRIVATE
            IUnityLog.h
            IUnityGraphics.h
            IUnityInterface.h
    )

    if (WIN32)
        target_sources(${rendering_plugin_target}
            PRIVATE
                IUnityGraphicsD3D11.h
                IUnityGraphicsD3D12.h
        )
    endif()

    if (APPLE)
        target_sources(${rendering_plugin_target}
            PRIVATE
                IUnityGraphicsMetal.h
        )
    endif()
endforeach()
