target_sources(RenderingPlugin
PRIVATE
    IUnityLog.h
    IUnityGraphics.h
    IUnityInterface.h
)

if (WIN32)
    target_sources(RenderingPlugin
        PRIVATE
            IUnityGraphicsD3D11.h
            IUnityGraphicsD3D12.h
    )
endif()

if (Apple)
    target_sources(RenderingPlugin
        PRIVATE
            IUnityGraphicsMetal.h
    )
endif()
