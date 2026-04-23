#pragma once

#include <simd/simd.h>

typedef enum RTReflectionKernelImageIndex
{
    OutImageIndex                   = 0,
    GBufferDepthIndex               = 1,
    GBufferNormalIndex              = 2,
    IrradianceMapIndex              = 3
} RTReflectionKernelImageIndex;

typedef enum AAPLTextureIndex
{
    AAPLTextureIndexBaseColor,
    AAPLTextureIndexNormal,
    AAPLTextureIndexMask,
//    AAPLTextureIndexOcclusion,
    AAPLMaterialTextureCount,
} AAPLTextureIndex;

typedef enum RTReflectionKernelBufferIndex
{
    AAPLBufferIndexScene,
    AAPLBufferIndexAccelerationStructure,
    AAPLBufferIndexCameraData,
    AAPLBufferIndexLightData,
} RTReflectionKernelBufferIndex;

typedef struct AAPLCameraData
{
    matrix_float4x4 MatrixVP;
    matrix_float4x4 MatrixVP_Inv;
    vector_float3 cameraPosition;
    float metallicBias;
    float roughnessBias;
} AAPLCameraData;

typedef struct
{
    // Per Light Properties
    vector_float3 directionalLightInvDirection;
    float lightIntensity;

} AAPLLightData;

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

struct Instance
{
    // A reference to a single mesh in the meshes array stored in structure `Scene`.
    uint32_t meshIndex [[id(0)]];

    //constant Mesh* pMesh [[ id( AAPLArgmentBufferIDInstanceMesh ) ]];
    uint32_t materialIndex [[id(1)]];

    // The location of the mesh for this instance.
    float4x4 transform [[id(2)]];
};

struct Mesh
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

struct Material
{
    array<texture2d<float>, AAPLMaterialTextureCount> textures [[ id( AAPLArgumentBufferIDMaterialTextures ) ]];
};

struct Scene
{
    // The array of instances.
    constant Instance* instances    [[ id( AAPLArgumentBufferIDSceneInstances ) ]];
    constant Mesh* meshes           [[ id( AAPLArgumentBufferIDSceneMeshes )]];
    constant Material* materials    [[ id( AAPLArgumentBufferIDSceneMaterials ) ]];
};


#else

struct Instance
{
    uint32_t meshIndex;
    uint32_t materialIndex;
    matrix_float4x4 transform;
};

struct Mesh
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

struct Material
{
    MTLResourceID textures[AAPLMaterialTextureCount];
};

struct Scene
{
    // The array of instances.
    uint64_t instances;
    uint64_t meshes;
    uint64_t materials;
};

#endif
