#pragma once

#import <Metal/Metal.h>
#import <MetalFX/MetalFX.h>
#include <vector>
#include <map>
#include <simd/simd.h>

#include "ShaderDefinition.h"
#include "RenderStructures.h"

#define kMaxBuffersInFlight 3

/// Manages Metal ray tracing acceleration structures
class API_AVAILABLE(ios(17.0), macos(14.0)) AccelerationStructure
{
    constexpr static int kPrimitiveAccelerationStructureBuild = 1;
    constexpr static int kInstanceAccelerationStructureBuild = 2;
    constexpr static int kInitialAccelerationStructureHeapSize = 256 * 1024 * 1024;

    bool _accelerationStructureDirty;
    bool _initialized = false;
    
    uint8_t _constantBufferIndex = 0;
    id<MTLBuffer> _renderParametersBuffers[kMaxBuffersInFlight];

    id<MTLDevice> _device;
    id<MTLIntersectionFunctionTable> _intersectionFunctionTable;
    id<MTLComputePipelineState> _rtReflectionPipeline;

    id<MTLEvent> _accelerationStructureBuildEvent;

    id<MTLBuffer> _sceneArgumentBuffer;
    id<MTLAccelerationStructure> _instanceAccelerationStructure;
    NSMutableArray< id<MTLAccelerationStructure> > *_primitiveAccelerationStructures;
    NSMutableArray<id<MTLHeap>>* _accelerationStructureHeaps;

    std::vector<InstanceDescriptor> _instanceDescriptors;
    std::vector<MaterialDscriptor> _materialDescriptors;
    std::vector<MeshDescriptor> _meshDescriptors;
    std::vector<int> _geometryBuildRequestList;

    NSMutableArray<id<MTLResource>>* _sceneResources;

    id<MTLBuffer> newBufferWithLabel(NSString *label, NSUInteger length, MTLResourceOptions options);
    MTLAccelerationStructureSizes calculateSizeForPrimitiveAccelerationStructures(NSArray<MTLPrimitiveAccelerationStructureDescriptor*>*primitiveAccelerationDescriptors);
    NSArray<id<MTLAccelerationStructure>>* allocateAndBuildAccelerationStructuresWithDescriptors(id<MTLCommandBuffer> cmd, NSArray<MTLAccelerationStructureDescriptor *>* descriptors, id<MTLHeap> heap, size_t maxScratchSize, id<MTLEvent> event);
    id<MTLAccelerationStructure> allocateAndBuildAccelerationStructureWithDescriptor(MTLAccelerationStructureDescriptor* descriptor, id<MTLCommandBuffer> cmd);
    id<MTLHeap> GetAvailableHeap(const MTLSizeAndAlign& requiredSize);

public:
    AccelerationStructure() {}
    ~AccelerationStructure() {}

    void Initialize(id<MTLDevice> device);
    void SetInstances(const InstanceDescriptor* instances, int count);
    void SetMaterials(const MaterialDscriptor* materials, int count);
    void SetMeshes(const MeshDescriptor* meshes, int count);
    void SetRaytracingGeometryBuildRequestList(const int* buildRequestList, int count);
    void SetRenderParameters(const RaytracingRenderParameters& lightData);

    void BuildBottomLevelAccelerationStructure(id<MTLCommandBuffer> cmd);
    void BuildTopLevelAccelerationStructure(id<MTLCommandBuffer> cmd);
    void BuildSceneArgumentBuffer(id<MTLCommandBuffer> cmd);
    void DispatchRaytracing(id<MTLCommandBuffer> commandBuffer, id<MTLTexture> __unsafe_unretained * textures);
    void CleanupRaytracing();
    bool IsSupported() { return _initialized; }
};
