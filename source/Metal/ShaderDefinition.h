#pragma once

#include <simd/simd.h>

#define AAPL_MAX_LIGHTS_COUNT 32

typedef enum AAPLRTReflectionKernelImageIndex
{
    AAPLRaytracingOutImageIndex                 = 0,
    AAPLRaytracingGBufferDepthIndex             = 1,
    AAPLRaytracingGBufferNormalIndex            = 2,
    AAPLRaytracingGBufferMaskIndex              = 3
} RTReflectionKernelImageIndex;

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
    AAPLBufferIndexCameraData,
    AAPLBufferIndexLightData,
} AAPLRTReflectionKernelBufferIndex;

typedef struct AAPLCameraData
{
    matrix_float4x4 MatrixVP;
    matrix_float4x4 MatrixVP_Inv;
    vector_float3 cameraPosition;
    float metallicBias;
    float roughnessBias;
} AAPLCameraData;

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
    AAPLVertexFlagBitGenericOffset  = 16,
    AAPLVertexFlagMaskPositionStride = 0xFF,
    AAPLVertexFlagMaskGenericStride = 0xFF00,
    AAPLVertexFlagMaskGenericOffset = 0xFF0000,
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

inline uint32_t GetGenericOffset(uint32_t vertexParameters)
{
    return (vertexParameters & AAPLVertexFlagMaskGenericOffset) >> AAPLVertexFlagBitGenericOffset;
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

struct AAPLInstance
{
    // A reference to a single mesh in the meshes array stored in structure `Scene`.
    uint32_t meshIndex [[id(0)]];

    //constant Mesh* pMesh [[ id( AAPLArgmentBufferIDInstanceMesh ) ]];
    uint32_t materialIndex [[id(1)]];

    // The location of the mesh for this instance.
    float4x4 transform [[id(2)]];
};

struct AAPLMesh
{
    // The arrays of vertices.
    // position stride 8 bit
    // generic stride 8 bit
    // generic offset 8 bit
    // index half flag 1 bit
    // position half flag 1 bit
    // normal half flag 1 bit
    // tangent half flag 1 bit
    // color exists flag 1 bit
    // uv half flag 1 bit
    uint32_t vertexParameters   [[ id(0) ]];
    // Support 4 submesh mostly.
//    uint4    subMeshIndexOffset [[ id(1) ]];
    constant uint8_t* positions [[ id( AAPLArgumentBufferIDMeshPositions ) ]];
    constant uint8_t* generics  [[ id( AAPLArgumentBufferIDMeshGenerics  ) ]];
    constant uint8_t* indices   [[ id( AAPLArgumentBufferIDMeshIndices   ) ]];
};

struct AAPLMaterial
{
    array<texture2d<half>, AAPLMaterialTextureCount> textures [[ id( AAPLArgumentBufferIDMaterialTextures ) ]];
    float4 _BaseColor;
    float4 _Emission;
    float _BumpScale;
    float _Metallic;
    float _Roughness;
};

struct AAPLScene
{
    // The array of instances.
    constant AAPLInstance* instances    [[ id( AAPLArgumentBufferIDSceneInstances ) ]];
    constant AAPLMesh* meshes           [[ id( AAPLArgumentBufferIDSceneMeshes )]];
    constant AAPLMaterial* materials    [[ id( AAPLArgumentBufferIDSceneMaterials ) ]];
};

struct AAPLLightData
{
    uint32_t lightCount;
    array<AAPLLightStruct, AAPL_MAX_LIGHTS_COUNT> lights;
};

#else

struct AAPLInstance
{
    uint32_t meshIndex;
    uint32_t materialIndex;
    matrix_float4x4 transform;
};

struct AAPLMesh
{
    // The arrays of vertices.
    // index format 1 bit
    // position format 1 bit
    // normal format 1 bit
    // tangent format 1 bit
    // color format 1 bit
    // uv format 1 bit
    // position stride 8 bit
    // generic stride 8 bit
    // generic offset 8 bit
    uint32_t vertexParameters;
    // Support 4 submesh mostly.
//    uint4    subMeshIndexOffset     [[ id(1) ]];
    uint64_t positions;
    uint64_t generics;
    uint64_t indices;
};

struct AAPLMaterial
{
    MTLResourceID textures[AAPLMaterialTextureCount];
};

struct AAPLScene
{
    // The array of instances.
    uint64_t instances;
    uint64_t meshes;
    uint64_t materials;
};

struct AAPLLightData
{
    uint32_t lightCount;
    AAPLLightStruct lights[AAPL_MAX_LIGHTS_COUNT];
};

#endif
