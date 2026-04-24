#pragma once

struct InstanceDescriptor
{
    int materialIndex;
    int meshIndex;
    float transformMatrix[16];
};

struct LightDescriptor
{
    float attenuation[4];
    float color[4];
    float direction[4];
    float position[4];
};

struct MeshDescriptor
{
    void* positionBuffer;
    void* genericBuffer;
    void* indexBuffer;

    int vertexParameter;
    int indexBufferOffset;
    int indexCount;
};

struct MaterialDscriptor
{
    void* BaseMap;
    void* NormalMap;
    void* MaskMap;
};

struct CameraData
{
    //! Specifies matrix transformation from the camera view to the clip space.
    float cameraViewToClip[16];
    //! Specifies matrix transformation from the clip space to the camera view space.
    float clipToCameraView[16];
    //! Optional - Specifies matrix transformation describing lens distortion in clip space.
    float worldToClip[16];
    float clipToWorld[16];
    // float clipToLensClip[16];
    //! Specifies matrix transformation from the current clip to the previous clip space.
    //! clipToPrevClip = clipToView * viewToViewPrev * viewToClipPrev
    //! Sample code can be found in sl_matrix_helpers.h
    float clipToPrevClip[16];
    //! Specifies matrix transformation from the previous clip to the current clip space.
    //! prevClipToClip = clipToPrevClip.inverse()
    float prevClipToClip[16];

    //! Specifies pixel space jitter offset
    float jitterOffset[2];
    //! Specifies scale factors used to normalize motion vectors (so the values are in [-1,1] range)
    float mvecScale[2];
    //! Optional - Specifies camera pinhole offset if used.
    // float cameraPinholeOffset[2];
    //! Specifies camera position in world space.
    float cameraPos[3];
    //! Specifies camera up vector in world space.
    float cameraUp[3];
    //! Specifies camera right vector in world space.
    float cameraRight[3];
    //! Specifies camera forward vector in world space.
    float cameraFwd[3];

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
    int inputSize[2];

    //! Specifies output texture size
    int outputSize[2];

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
    eRaytracingOutput,
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
