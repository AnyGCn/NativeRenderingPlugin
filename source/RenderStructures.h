#pragma once

struct float2
{
    float x, y;
};

struct float3
{
    float x, y, z;
};

struct float4
{
    float x, y, z, w;
};

struct float4x4
{
    float4 columns[4];
};

struct int2
{
    int x, y;
};

enum ShadowCastingMode
{
    ShadowCastingOff = 0,
    ShadowCastingOn = 1,
    ShadowCastingTwoSided = 2,
    ShadowCastingShadowsOnly = 3,
};

enum CullMode
{
    CullNone = 0,
    CullFront = 1,
    CullBack = 2,
};

enum RaytracingInstanceFlagMask
{
    RTInstBitOpaque = 0,
    RTInstBitCullMode = 1,
    RTInstBitShadowCastingMode = 3,
    RTInstMaskOpaque = 0b0001,
    RTInstMaskCullMode = 0b0110,
    RTInstMaskShadowCastingMode = 0b011000,
};

// Keep sync with AAPLInstance in Metal/ShaderDefinition.h
struct InstanceDescriptor
{
    int materialIndex;
    int meshIndex;
    int renderFlag;
    int padding;
    float4x4 transformMatrix;
};

// Keep sync with AAPLLightStruct in Metal/ShaderDefinition.h
struct LightDescriptor
{
    float4 attenuation;
    float4 color;
    float4 direction;
    float4 position;
};

#define MAX_LIGHTS_COUNT 32

// Keep sync with AAPLRenderParameter in Metal/ShaderDefinition.h
struct RaytracingRenderParameters
{
    // Warning: aligned of float4
    // Camera Data
    float4x4 MatrixVP;
    float4x4 MatrixVP_Inv;
    float4 cameraPosition;

    // Light Data
    LightDescriptor lights[MAX_LIGHTS_COUNT];

    // Main light shadow data
    float4x4 shadowMatrix0;
    float4x4 shadowMatrix1;
    float4x4 shadowMatrix2;
    float4x4 shadowMatrix3;
    float4x4 shadowMatrix4;
    float4 shadowSplitSphere0;
    float4 shadowSplitSphere1;
    float4 shadowSplitSphere2;
    float4 shadowSplitSphere3;
    float4 shadowBorder;
    float4 shadowMapSize;
    float4 shadowParams;

    // Sky light
    float4 unity_SHAr;
    float4 unity_SHAg;
    float4 unity_SHAb;
    float4 unity_SHBr;
    float4 unity_SHBg;
    float4 unity_SHBb;
    float4 unity_SHC;
    
    float4 skyCubeHDRDecodeValues;
    
    int lightCount;
    int hasMainLightShadow;
};

struct MeshDescriptor
{
    void* positionBuffer;
    void* genericBuffer;
    void* indexBuffer;

    int vertexParameter;
    int positionStride;
    int genericStride;
    int genericOffset;
    int indexBufferOffset;
    int indexCount;
};

struct MaterialDscriptor
{
    void* BaseMap;
    void* NormalMap;
    void* MaskMap;
    void* EmissionMap;
    float4 BaseColor;
    float4 Emission;
    float BumpScale;
    float Metallic;
    float Roughness;
    float Occlusion;
};

struct CameraData
{
    //! Specifies matrix transformation from the camera view to the clip space.
    float4x4 cameraViewToClip;
    //! Specifies matrix transformation from the clip space to the camera view space.
    float4x4 clipToCameraView;
    //! Optional - Specifies matrix transformation describing lens distortion in clip space.
    float4x4 worldToClip;
    float4x4 clipToWorld;
    // float4x4 clipToLensClip;
    //! Specifies matrix transformation from the current clip to the previous clip space.
    //! clipToPrevClip = clipToView * viewToViewPrev * viewToClipPrev
    //! Sample code can be found in sl_matrix_helpers.h
    float4x4 clipToPrevClip;
    //! Specifies matrix transformation from the previous clip to the current clip space.
    //! prevClipToClip = clipToPrevClip.inverse()
    float4x4 prevClipToClip;

    //! Specifies pixel space jitter offset
    float2 jitterOffset;
    //! Specifies scale factors used to normalize motion vectors (so the values are in [-1,1] range)
    float2 mvecScale;
    //! Optional - Specifies camera pinhole offset if used.
    // float2 cameraPinholeOffset;
    //! Specifies camera position in world space.
    float3 cameraPos;
    //! Specifies camera up vector in world space.
    float3 cameraUp;
    //! Specifies camera right vector in world space.
    float3 cameraRight;
    //! Specifies camera forward vector in world space.
    float3 cameraFwd;

    //! Specifies camera near view plane distance.
    float cameraNear;
    //! Specifies camera far view plane distance.
    float cameraFar;
    //! Specifies camera field of view in radians.
    float cameraFOV;
    //! Specifies camera aspect ratio defined as view space width divided by height.
    // float cameraAspectRatio;
    //! Specifies which value represents an invalid (un-initialized) value in the motion vectors buffer
    //! NOTE: This is only required if `cameraMotionIncluded` is set to false and SL needs to compute it.
    // float motionVectorsInvalidValue;

    int viewHandle = 0;

    //! Specifies input texture size
    int2 inputSize;

    //! Specifies output texture size
    int2 outputSize;

    //! Specifies if depth values are inverted (value closer to the camera is higher) or not.
    bool depthInverted = true;
    //! Specifies if camera motion is included in the MVec buffer.
    // bool cameraMotionIncluded = true;
    //! Specifies if motion vectors are 3D or not.
    // bool motionVectors3D = false;
    //! Specifies if previous frame has no connection to the current one (i.e. motion vectors are invalid)
    bool reset = false;
    //! Specifies if orthographic projection is used or not.
    // bool orthographicProjection = false;
    //! Specifies if motion vectors are already dilated or not.
    // bool motionVectorsDilated = false;
    //! Specifies if motion vectors are jittered or not.
    // bool motionVectorsJittered = true;

    bool colorBuffersHDR = true;

    bool alphaUpscalingEnabled = false;
};

enum DLSSMode
{
    eOff,
    eMaxPerformance,
    eBalanced,
    eMaxQuality,
    eUltraPerformance,
    eUltraQuality,
    eDLAA,
    eDLSSModeCount,
};

enum TextureType
{
    eDepth,
    eMotionVectors,
    eHUDLessColor,
    eScalingInputColor,
    eScalingOutputColor,
    eNormal,
    eGBufferMask,
    eRaytracingOutput,
    eMainLightShadowMap,
    eSkyCube,
    eTextureTypeCount,
};

// Plugin event IDs passed to IssuePluginEvent / IssuePluginEventAndData from the Unity side.
// Must stay in sync with the C# enum on the Unity side.
enum RenderEventType
{
    Sync_RenderStart,
    Sync_RenderEnd,
    Upscale_DLSS,
    Cleanup_DLSS,
    Upscale_MetalFX_Spatial,
    Upscale_MetalFX_Temporal,
    Cleanup_MetalFX,
    Dispatch_Raytracing,
    Cleanup_Raytracing,
    RenderEventCount,
};

struct DLSSSettings
{
    int optimalRenderSizeX;
    int optimalRenderSizeY;
    int minRenderSizeX;
    int minRenderSizeY;
    int maxRenderSizeX;
    int maxRenderSizeY;
    float sharpness;
};
