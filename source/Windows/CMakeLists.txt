foreach(rendering_plugin_target IN LISTS RENDERING_PLUGIN_TARGETS)
    target_sources(${rendering_plugin_target}
        PRIVATE
            RenderAPI_D3D.cpp
            RenderAPI_D3D11.cpp
            RenderAPI_D3D12.cpp
            SLWrapper.cpp
    )
endforeach()
