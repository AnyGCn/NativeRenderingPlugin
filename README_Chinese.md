# Unity Native Rendering Plugin (C++)

一个演示如何在Unity中使用C++原生插件进行图形渲染的示例项目。

## 项目概述

本示例展示了如何通过[Unity原生插件接口](http://docs.unity3d.com/Manual/NativePluginInterface.html)从C++插件中进行渲染和其他图形相关操作。这是一个高级的Unity原生渲染插件，主要提供三大核心功能：NVIDIA DLSS超分辨率技术、Apple MetalFX超采样技术和Metal光线追踪支持。

## 兼容版本

- **Unity 2023.1+** 使用默认分支的最新版本

## 🎯 核心功能

### 1. NVIDIA DLSS超分辨率技术
- 实时深度学习超采样
- 多种质量模式支持（性能、平衡、质量等）
- 帧生成技术（DLSS Frame Generation）
- 自动最优设置查询

### 2. Apple MetalFX超采样技术
- 空间超采样（MetalFX Spatial）
- 时间超采样（MetalFX Temporal）
- 针对Apple Silicon优化的渲染管线

### 3. Metal光线追踪支持
- 实时光线追踪渲染
- 动态实例和材质管理
- 复杂光照和阴影处理

### 4. NVIDIA Reflex技术
- 低延迟渲染优化
- 帧同步和性能分析

## 🔌 Unity接口函数

### 插件生命周期管理
```cpp
UnityPluginLoad(IUnityInterfaces* unityInterfaces)  // 插件加载入口
UnityPluginUnload()                                 // 插件卸载清理
```

### 渲染事件处理函数
```cpp
GetRenderEventFunc()                                // 获取无数据版本渲染事件回调
GetRenderEventWithDataFunc()                        // 获取带数据版本渲染事件回调
OnRenderEventWithData(int eventID, void* data)      // 渲染事件处理核心函数
```

### 核心功能接口

#### 📊 同步与性能优化
```cpp
Sync_Sleep(int frameID)                             // NVIDIA Reflex睡眠回调
Sync_SimulateBegin(int frameID)                     // 模拟开始回调  
Sync_SimulateEnd(int frameID)                       // 模拟结束回调
Sync_RenderStart(int frameID)                       // 渲染开始回调
Sync_RenderEnd(int frameID)                         // 渲染结束回调
```

#### 🎮 NVIDIA DLSS功能
```cpp
SupportDLSS()                                       // 检查DLSS支持
SupportDLSS_FG()                                    // 检查帧生成支持
SetDLSSOptions(int mode)                            // 设置DLSS模式
QueryDLSSOptimalSettings(...)                       // 查询最优DLSS设置
```

#### 🍎 Apple MetalFX功能
```cpp
SupportMetalFX()                                    // 检查MetalFX支持
```

#### 🌟 光线追踪功能
```cpp
SupportRaytracing()                                 // 检查光线追踪支持
SetRaytracingInstances(...)                         // 设置光线追踪实例
SetRaytracingMaterials(...)                         // 设置材质数据
SetRaytracingMeshes(...)                            // 设置网格数据
SetRaytracingGeometryBuildRequestList(...)          // 设置几何构建请求
SetRaytracingRenderParameters(...)                  // 设置渲染参数
```

#### 📷 相机与纹理管理
```cpp
SetCameraData(void* data)                           // 设置相机数据
SetTexture(int textureType, void* nativeTexture)    // 设置纹理
IsValid()                                           // 检查API有效性
```

## 🎪 渲染事件类型 (RenderEventType)

插件支持9种渲染事件，通过`GL.IssuePluginEvent`调用：

1. **Sync_RenderStart** - 渲染开始同步
2. **Sync_RenderEnd** - 渲染结束同步  
3. **Upscale_DLSS** - DLSS超分处理
4. **Cleanup_DLSS** - DLSS资源清理
5. **Upscale_MetalFX_Spatial** - MetalFX空间超采样
6. **Upscale_MetalFX_Temporal** - MetalFX时间超采样
7. **Cleanup_MetalFX** - MetalFX资源清理
8. **Dispatch_Raytracing** - 光线追踪分发
9. **Cleanup_Raytracing** - 光线追踪资源清理

## 🏗️ 架构设计

### 插件初始化流程
1. **UnityPluginLoad** - Unity加载插件时调用
2. **注册设备事件回调** - 监听图形设备变化
3. **创建渲染API实例** - 根据当前图形API创建对应实现
4. **初始化性能分析器** - 集成Unity性能分析工具

### 多平台支持
- **Windows**: D3D11, D3D12, Vulkan
- **macOS**: Metal, OpenGL  
- **跨平台**: 通过RenderAPI抽象层实现

## 💡 使用方法

### Unity C#端调用示例
```csharp
// 获取渲染事件函数
[DllImport("RenderingPlugin")]
public static extern IntPtr GetRenderEventFunc();

// 调用渲染事件
GL.IssuePluginEvent(GetRenderEventFunc(), (int)RenderEventType.Upscale_DLSS);

// 检查功能支持
[DllImport("RenderingPlugin")]
public static extern bool SupportDLSS();

if (SupportDLSS()) {
    // 启用DLSS功能
    GL.IssuePluginEvent(GetRenderEventFunc(), (int)RenderEventType.Upscale_DLSS);
}
```

### 关键配置步骤
1. **平台检测**: 调用`SupportDLSS()`/`SupportMetalFX()`检查功能支持
2. **参数设置**: 使用`SetDLSSOptions()`/`SetCameraData()`配置参数
3. **资源准备**: 通过`SetTexture()`传递必要的纹理数据
4. **事件触发**: 在适当时机调用对应的渲染事件

## 支持的平台和图形API

### Windows
- D3D11, D3D12, OpenGL, Vulkan
- **注意**: Vulkan和DX12默认未编译
  - Vulkan需要Vulkan SDK，在`PlatformBase.h`中修改`#define SUPPORT_VULKAN 0`为`1`
  - DX12需要额外头文件，在`PlatformBase.h`中修改`#define SUPPORT_D3D12 0`为`1`

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
- Metal（支持模拟器，需Unity 2020+和XCode 11+）

### EmbeddedLinux
- OpenGL, OpenGL ES, Vulkan

### QNX
- OpenGL ES

## 项目结构

### PluginSource - C++插件源代码
- `source/` - 源代码目录
  - `RenderingPlugin.cpp` - 主要逻辑和Unity接口定义
  - `RenderAPI*.*` - 不同API的渲染实现
  - `RenderStructures.h` - 数据结构和枚举定义
- `projects/VisualStudio2022` - Windows插件Visual Studio 2022项目文件
- `projects/UWPVisualStudio2022` - Windows Store (UWP)插件项目文件
- `projects/Xcode` - macOS插件Xcode项目文件（测试版本：Xcode 10.3 on macOS 10.14）
- `projects/GNUMake` - Linux Makefile
- `projects/EmbeddedLinux` - 嵌入式Linux构建脚本
- `projects/QNX` - QNX Makefile（需要安装QNX并设置环境变量）

### UnityProject - Unity项目
- 包含插件示例场景的单个场景文件

## 🚀 技术亮点

1. **高性能**: 直接操作原生图形API，避免托管层开销
2. **模块化**: 不同功能模块独立，便于维护和扩展
3. **跨平台**: 统一的接口设计，支持多种图形后端
4. **可扩展**: 易于添加新的渲染技术和功能模块
5. **性能分析**: 集成Unity Profiler，便于性能优化

## 快速开始

1. 根据目标平台打开对应的IDE项目文件
2. 构建C++插件
3. 在Unity项目中导入生成的插件文件
4. 打开示例场景查看效果
5. 参考接口文档集成到自己的项目中

## 许可证

本项目采用MIT/X11许可证，与大多数Unity示例项目相同。