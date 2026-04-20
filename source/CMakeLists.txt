add_subdirectory(Unity)

target_sources(RenderingPlugin
PRIVATE
    RenderAPI.cpp
)

if (WIN32)
    add_subdirectory(Windows)
endif()

if (Apple)
        add_subdirectory(Metal)
endif()

if (Android)
    add_subdirectory(gl3w)
    target_sources(RenderingPlugin
        PRIVATE
            RenderAPI_OpenGLCoreES.cpp
    )
endif()