# Unity Native Rendering Plugin (C++)

A demonstration project showing how to use C++ native plugins for graphics rendering in Unity.

## Project Overview

This sample demonstrates how to perform rendering and other graphics operations from C++ plugins using the [Unity Native Plugin Interface](http://docs.unity3d.com/Manual/NativePluginInterface.html). This is an advanced Unity native rendering plugin that primarily provides three core features: NVIDIA DLSS super-resolution technology, Apple MetalFX upscaling technology, and Metal ray tracing support.

## Compatibility

- **Unity 2023.1+** uses the latest version from the default branch

## 🎯 Core Features

### 1. NVIDIA DLSS Super-Resolution Technology
- Real-time deep learning super sampling
- Multiple quality mode support (Performance, Balanced, Quality, etc.)
- Frame generation technology (DLSS Frame Generation)
- Automatic optimal settings query

### 2. Apple MetalFX Upscaling Technology
- Spatial upscaling (MetalFX Spatial)
- Temporal upscaling (MetalFX Temporal)
- Rendering pipeline optimized for Apple Silicon

### 3. Metal Ray Tracing Support
- Real-time ray tracing rendering
- Dynamic instance and material management
- Complex lighting and shadow processing

### 4. NVIDIA Reflex Technology
- Low-latency rendering optimization
- Frame synchronization and performance analysis

## 🔌 Unity Interface Functions

### Plugin Lifecycle Management
```cpp
UnityPluginLoad(IUnityInterfaces* unityInterfaces)  // Plugin loading entry point
UnityPluginUnload()                                 // Plugin unloading and cleanup
```

### Render Event Handling Functions
```cpp
GetRenderEventFunc()                                // Get render event callback without data
GetRenderEventWithDataFunc()                        // Get render event callback with data
OnRenderEventWithData(int eventID, void* data)      // Core render event handling function
```

### Core Function Interfaces

#### 📊 Synchronization & Performance Optimization
```cpp
Sync_Sleep(int frameID)                             // NVIDIA Reflex sleep callback
Sync_SimulateBegin(int frameID)                     // Simulation start callback  
Sync_SimulateEnd(int frameID)                       // Simulation end callback
Sync_RenderStart(int frameID)                       // Render start callback
Sync_RenderEnd(int frameID)                         // Render end callback
```

#### 🎮 NVIDIA DLSS Functions
```cpp
SupportDLSS()                                       // Check DLSS support
SupportDLSS_FG()                                    // Check frame generation support
SetDLSSOptions(int mode)                            // Set DLSS mode
QueryDLSSOptimalSettings(...)                       // Query optimal DLSS settings
```

#### 🍎 Apple MetalFX Functions
```cpp
SupportMetalFX()                                    // Check MetalFX support
```

#### 🌟 Ray Tracing Functions
```cpp
SupportRaytracing()                                 // Check ray tracing support
SetRaytracingInstances(...)                         // Set ray tracing instances
SetRaytracingMaterials(...)                         // Set material data
SetRaytracingMeshes(...)                            // Set mesh data
SetRaytracingGeometryBuildRequestList(...)          // Set geometry build requests
SetRaytracingRenderParameters(...)                  // Set render parameters
```

#### 📷 Camera & Texture Management
```cpp
SetCameraData(void* data)                           // Set camera data
SetTexture(int textureType, void* nativeTexture)    // Set texture
IsValid()                                           // Check API validity
```

## 🎪 Render Event Types (RenderEventType)

The plugin supports 9 types of render events, called via `GL.IssuePluginEvent`:

1. **Sync_RenderStart** - Render start synchronization
2. **Sync_RenderEnd** - Render end synchronization  
3. **Upscale_DLSS** - DLSS upscaling processing
4. **Cleanup_DLSS** - DLSS resource cleanup
5. **Upscale_MetalFX_Spatial** - MetalFX spatial upscaling
6. **Upscale_MetalFX_Temporal** - MetalFX temporal upscaling
7. **Cleanup_MetalFX** - MetalFX resource cleanup
8. **Dispatch_Raytracing** - Ray tracing dispatch
9. **Cleanup_Raytracing** - Ray tracing resource cleanup

## 🏗️ Architecture Design

### Plugin Initialization Process
1. **UnityPluginLoad** - Called when Unity loads the plugin
2. **Register device event callbacks** - Monitor graphics device changes
3. **Create render API instance** - Create corresponding implementation based on current graphics API
4. **Initialize profiler** - Integrate Unity performance analysis tools

### Multi-Platform Support
- **Windows**: D3D11, D3D12, Vulkan
- **macOS**: Metal, OpenGL  
- **Cross-platform**: Implemented through RenderAPI abstraction layer

## 💡 Usage Guide

### Unity C# Side Calling Example
```csharp
// Get render event function
[DllImport("RenderingPlugin")]
public static extern IntPtr GetRenderEventFunc();

// Call render event
GL.IssuePluginEvent(GetRenderEventFunc(), (int)RenderEventType.Upscale_DLSS);

// Check feature support
[DllImport("RenderingPlugin")]
public static extern bool SupportDLSS();

if (SupportDLSS()) {
    // Enable DLSS feature
    GL.IssuePluginEvent(GetRenderEventFunc(), (int)RenderEventType.Upscale_DLSS);
}
```

### Key Configuration Steps
1. **Platform Detection**: Call `SupportDLSS()`/`SupportMetalFX()` to check feature support
2. **Parameter Setup**: Use `SetDLSSOptions()`/`SetCameraData()` to configure parameters
3. **Resource Preparation**: Pass necessary texture data via `SetTexture()`
4. **Event Triggering**: Call corresponding render events at appropriate times

## Supported Platforms and Graphics APIs

### Windows
- D3D11, D3D12, OpenGL, Vulkan
- **Note**: Vulkan and DX12 are not compiled by default
  - Vulkan requires Vulkan SDK, modify `#define SUPPORT_VULKAN 0` to `1` in `PlatformBase.h`
  - DX12 requires additional headers, modify `#define SUPPORT_D3D12 0` to `1` in `PlatformBase.h`

### macOS
- Metal, OpenGL

### Linux
- OpenGL, Vulkan

### Windows Store (UWP)
- D3D11, D3D12

### WebGL
- OpenGL ES

### Android
- OpenGL ES, Vulkan

### iOS/tvOS
- Metal (supports simulator, requires Unity 2020+ and XCode 11+)

### EmbeddedLinux
- OpenGL, OpenGL ES, Vulkan

### QNX
- OpenGL ES

## Project Structure

### PluginSource - C++ Plugin Source Code
- `source/` - Source code directory
  - `RenderingPlugin.cpp` - Main logic and Unity interface definitions
  - `RenderAPI*.*` - Rendering implementations for different APIs
  - `RenderStructures.h` - Data structures and enum definitions
- `projects/VisualStudio2022` - Windows plugin Visual Studio 2022 project files
- `projects/UWPVisualStudio2022` - Windows Store (UWP) plugin project files
- `projects/Xcode` - macOS plugin Xcode project files (tested with Xcode 10.3 on macOS 10.14)
- `projects/GNUMake` - Linux Makefile
- `projects/EmbeddedLinux` - Embedded Linux build scripts
- `projects/QNX` - QNX Makefile (requires QNX installation and environment setup)

### UnityProject - Unity Project
- Contains a single scene file with plugin example scenes

## 🚀 Technical Highlights

1. **High Performance**: Direct native graphics API operations, avoiding managed layer overhead
2. **Modular Design**: Independent functional modules for easy maintenance and extension
3. **Cross-Platform**: Unified interface design supporting multiple graphics backends
4. **Extensible**: Easy to add new rendering technologies and functional modules
5. **Performance Analysis**: Integrated Unity Profiler for performance optimization

## Quick Start

1. Open the corresponding IDE project file based on your target platform
2. Build the C++ plugin
3. Import the generated plugin files into your Unity project
4. Open the example scene to see the effects
5. Refer to the interface documentation for integration into your own project

## License

This project uses the MIT/X11 license, same as most Unity sample projects.