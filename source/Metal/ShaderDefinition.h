#pragma once

#include <simd/simd.h>

#define AAPL_MAX_LIGHTS_COUNT 32

typedef enum AAPLRaytracingMask
{
    AAPLRaytracingMaskNormal = 0x1,
    AAPLRaytracingMaskShadow = 0x2,
} AAPLRaytracingMask;

typedef enum AAPLRTReflectionKernelImageIndex
{
    AAPLRaytracingOutImageIndex                 = 0,
    AAPLRaytracingGBufferDepthIndex             = 1,
    AAPLRaytracingGBufferNormalIndex            = 2,
    AAPLRaytracingGBufferMaskIndex              = 3,
    AAPLRaytracingMainLightShadowMap            = 4,
    AAPLRaytracingSkyCubeMap                    = 5,
    AAPLRaytracingTextureCount,
} AAPLRTReflectionKernelImageIndex;

typedef enum AAPLTextureIndex
{
    AAPLTextureIndexBaseColor,
    AAPLTextureIndexNormal,
    AAPLTextureIndexMask,
    AAPLTextureIndexEmission,
    AAPLMaterialTextureCount,
} AAPLTextureIndex;

typedef enum AAPLRTReflectionKernelBufferIndex
{
    AAPLBufferIndexScene,
    AAPLBufferIndexAccelerationStructure,
    AAPLBufferIndexIntersectionFunctionTable,
    AAPLBufferIndexRenderParameter,
} AAPLRTReflectionKernelBufferIndex;

typedef struct AAPLCameraData
{
    matrix_float4x4 MatrixVP;
    matrix_float4x4 MatrixVP_Inv;
    vector_float3 cameraPosition;
    float metallicBias;
    float roughnessBias;
} AAPLCameraData;

// Keep sync with LightDescriptor in RenderStructures.h
typedef struct AAPLLightStruct
{
    // Per Light Properties
    vector_float4 attenuation;
    vector_float4 color;
    vector_float4 direction;
    vector_float4 position;
} AAPLLightStruct;

typedef enum AAPLArgumentBufferID
{
    AAPLArgumentBufferIDSceneInstances,
    AAPLArgumentBufferIDSceneMeshes,
    AAPLArgumentBufferIDSceneMaterials,

    AAPLArgumentBufferIDMaterialTextures,

    AAPLArgumentBufferIDMeshPositions,
    AAPLArgumentBufferIDMeshGenerics,
    AAPLArgumentBufferIDMeshIndices
} AAPLArgumentBufferID;

typedef enum AAPLVerexFlagMask
{
    AAPLVertexFlagBitPositionStride = 0,
    AAPLVertexFlagBitGenericStride  = 8,
    AAPLVertexFlagMaskPositionStride = 0xFF,
    AAPLVertexFlagMaskGenericStride = 0xFF00,
    AAPLVertexFlagMaskNormalInGeneric = 0x10000,
    AAPLVertexFlagMaskTangentInGeneric = 0x20000,
    AAPLVertexFlagMaskColorInGeneric = 0x40000,
    AAPLVertexFlagMaskUVInGeneric = 0x80000,
    AAPLVertexFlagMaskIndexHalf = 0x1000000,
    AAPLVertexFlagMaskPositionHalf = 0x2000000,
    AAPLVertexFlagMaskNormalHalf = 0x4000000,
    AAPLVertexFlagMaskTangentHalf = 0x8000000,
    AAPLVertexFlagMaskColorExists = 0x10000000,
    AAPLVertexFlagMaskUVHalf = 0x20000000
} AAPLVerexParameterFlags;

inline uint32_t GetPositionStride(uint32_t vertexParameters)
{
    return (vertexParameters & AAPLVertexFlagMaskPositionStride) >> AAPLVertexFlagBitPositionStride;
}

inline uint32_t GetGenericStride(uint32_t vertexParameters)
{
    return (vertexParameters & AAPLVertexFlagMaskGenericStride) >> AAPLVertexFlagBitGenericStride;
}

inline bool IsNormalInGeneric(uint32_t vertexParameters)
{
    return (vertexParameters & AAPLVertexFlagMaskNormalInGeneric) != 0;
}

inline bool IsTangentInGeneric(uint32_t vertexParameters)
{
    return (vertexParameters & AAPLVertexFlagMaskTangentInGeneric) != 0;
}

inline bool IsColorInGeneric(uint32_t vertexParameters)
{
    return (vertexParameters & AAPLVertexFlagMaskColorInGeneric) != 0;
}

inline bool IsUVInGeneric(uint32_t vertexParameters)
{
    return (vertexParameters & AAPLVertexFlagMaskUVInGeneric) != 0;
}

inline bool IsIndexHalf(uint32_t vertexParameters)
{
    return (vertexParameters & AAPLVertexFlagMaskIndexHalf) != 0;
}

inline bool IsPositionHalf(uint32_t vertexParameters)
{
    return (vertexParameters & AAPLVertexFlagMaskPositionHalf) != 0;
}

inline bool IsNormalHalf(uint32_t vertexParameters)
{
    return (vertexParameters & AAPLVertexFlagMaskNormalHalf) != 0;
}

inline bool IsTangentHalf(uint32_t vertexParameters)
{
    return (vertexParameters & AAPLVertexFlagMaskTangentHalf) != 0;
}

inline bool IsColorExists(uint32_t vertexParameters)
{
    return (vertexParameters & AAPLVertexFlagMaskColorExists) != 0;
}

inline bool IsUVHalf(uint32_t vertexParameters)
{
    return (vertexParameters & AAPLVertexFlagMaskUVHalf) != 0;
}

#if __METAL_VERSION__

#include <metal_stdlib>
using namespace metal;

// Keep sync with AAPLInstance in RenderStructures.h
struct AAPLInstance
{
    // A reference to a single mesh in the meshes array stored in structure `Scene`.
    uint32_t materialIndex;
    uint32_t meshIndex;
    uint32_t renderFlag;
    uint32_t padding;

    // The location of the mesh for this instance.
    float4x4 transform;
};

struct AAPLMesh
{
    constant uint8_t* positions;
    constant uint8_t* generics;
    constant uint8_t* indices;
    uint32_t vertexParameters;
    uint32_t padding;
};

struct AAPLMaterial
{
    array<texture2d<half>, AAPLMaterialTextureCount> textures [[ id( AAPLArgumentBufferIDMaterialTextures ) ]];
    float4 _BaseColor;
    float4 _Emission;
    float _BumpScale;
    float _Metallic;
    float _Roughness;
    float _Occlusion;
};

struct AAPLScene
{
    // The array of instances.
    constant AAPLInstance* instances    [[ id( AAPLArgumentBufferIDSceneInstances ) ]];
    constant AAPLMesh* meshes           [[ id( AAPLArgumentBufferIDSceneMeshes )]];
    constant AAPLMaterial* materials    [[ id( AAPLArgumentBufferIDSceneMaterials ) ]];
};

// Keep sync with RaytracingRenderParameters in RenderStructures.h
struct AAPLRenderParameter
{
    // Camera data
    float4x4 MatrixVP;
    float4x4 MatrixVP_Inv;
    float4 cameraPosition;
    
    AAPLLightStruct lights[AAPL_MAX_LIGHTS_COUNT];
    // Shadow map
    float4x4 shadowMatrix[5];
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
    
    uint32_t lightCount;
    uint32_t hasMainLightShadow;
};

#else

#import <Metal/Metal.h>

struct AAPLMesh
{
    uint64_t positions;
    uint64_t generics;
    uint64_t indices;
    uint32_t vertexParameters;
};

struct AAPLMaterial
{
    MTLResourceID textures[AAPLMaterialTextureCount];
    simd_float4 _BaseColor;
    simd_float4 _Emission;
    float _BumpScale;
    float _Metallic;
    float _Roughness;
    float _Occlusion;
};

struct AAPLScene
{
    // The array of instances.
    uint64_t instances;
    uint64_t meshes;
    uint64_t materials;
};

#endif
