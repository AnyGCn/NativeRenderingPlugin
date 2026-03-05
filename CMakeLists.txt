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

add_compile_options(/utf-8)

# Export the plugin as dll
add_library(RenderingPlugin SHARED source/RenderingPlugin.cpp)

install(TARGETS RenderingPlugin DESTINATION .)

if (WIN32)
    find_package(directx-headers CONFIG REQUIRED)
    target_link_libraries(RenderingPlugin Microsoft::DirectX-Headers)
    target_link_libraries(RenderingPlugin d3d12.lib dxgi.lib dxguid.lib)
    target_link_libraries(RenderingPlugin streamline)
endif()

add_subdirectory(source)
add_subdirectory(external)