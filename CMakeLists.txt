cmake_minimum_required(VERSION 3.29)

project(NativeRenderingPlugin VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

if (MSVC)
    add_compile_options(/utf-8)
endif()

if (APPLE)
    if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
        add_library(RenderingPluginIOS STATIC)
        set(RENDERING_PLUGIN_TARGETS RenderingPluginIOS)

        set_target_properties(RenderingPluginIOS PROPERTIES
            OUTPUT_NAME "RenderingPluginIOS"
            XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
            XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "15.6"
            XCODE_ATTRIBUTE_OTHER_LDFLAGS "-ObjC"
        )
    else()
        add_library(RenderingPlugin MODULE)
        set(RENDERING_PLUGIN_TARGETS RenderingPlugin)

        set_target_properties(RenderingPlugin PROPERTIES
            BUNDLE TRUE
            BUNDLE_EXTENSION "bundle"
            PREFIX ""
            OUTPUT_NAME "RenderingPlugin"
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/projects/Xcode/Info.plist"
            XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
            XCODE_ATTRIBUTE_MACOSX_DEPLOYMENT_TARGET "13.5"
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.anyg.renderingPlugin"
        )

        target_link_libraries(RenderingPlugin PRIVATE
            "-framework CoreFoundation"
            "-framework Metal"
            "-framework MetalKit"
            "-framework MetalFX"
        )
    endif()
else()
    add_library(RenderingPlugin SHARED)
    set(RENDERING_PLUGIN_TARGETS RenderingPlugin)

    install(TARGETS RenderingPlugin DESTINATION .)
endif()

add_subdirectory(source)

if (APPLE)
    if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
        get_property(RenderingPluginIOS_METALLIB_PATH GLOBAL PROPERTY RenderingPluginIOS_METALLIB_PATH)
        add_custom_command(TARGET RenderingPluginIOS POST_BUILD
            COMMAND /bin/sh -c "cp -f \"${RenderingPluginIOS_METALLIB_PATH}\" \"$TARGET_BUILD_DIR/default.metallib\""
            VERBATIM
        )
    else()
        get_property(RenderingPlugin_METALLIB_PATH GLOBAL PROPERTY RenderingPlugin_METALLIB_PATH)
        add_custom_command(TARGET RenderingPlugin POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_BUNDLE_DIR:RenderingPlugin>/Contents/Resources
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${RenderingPlugin_METALLIB_PATH} $<TARGET_BUNDLE_DIR:RenderingPlugin>/Contents/Resources/default.metallib
            VERBATIM
        )
    endif()
endif()

add_subdirectory(external)

if (WIN32)
    find_package(directx-headers CONFIG REQUIRED)
    target_link_libraries(RenderingPlugin PRIVATE Microsoft::DirectX-Headers)
    target_link_libraries(RenderingPlugin PRIVATE d3d12.lib dxgi.lib dxguid.lib)
    target_link_libraries(RenderingPlugin PRIVATE streamline)
endif()