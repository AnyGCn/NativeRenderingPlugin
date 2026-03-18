add_subdirectory(Unity)

target_sources(RenderingPlugin
PRIVATE
    RenderAPI.cpp
)

if (WIN32)
    target_sources(RenderingPlugin
        PRIVATE
            RenderAPI_D3D.cpp
            RenderAPI_D3D11.cpp
            RenderAPI_D3D12.cpp
            SLWrapper.cpp
    )
endif()

if (Apple)
    target_sources(RenderingPlugin
        PRIVATE
            RenderAPI_Metal.mm
    )
endif()

if (Android)
    add_subdirectory(gl3w)
    target_sources(RenderingPlugin
        PRIVATE
            RenderAPI_OpenGLCoreES.cpp
    )
endif()