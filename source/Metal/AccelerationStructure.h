//
//  AccelerationStructure.h
//  RenderingPlugin
//
//  Created by 郭昱宁 on 2026/4/13.
//
#pragma once

#import <Metal/Metal.h>
#import <MetalFX/MetalFX.h>
#include <vector>
#include <map>
#include <simd/simd.h>

#include "RenderStructures.h"

#define kMaxBuffersInFlight 3

/// Manages Metal ray tracing acceleration structures
class API_AVAILABLE(ios(17.0), macos(14.0)) AccelerationStructure
{
    constexpr static int kPrimitiveAccelerationStructureBuild = 1;
    constexpr static int kInstanceAccelerationStructureBuild = 2;

    bool _accelerationStructureDirty;
    bool _initialized = false;
    uint8_t _cameraBufferIndex = 0;

    id<MTLDevice> _device;
    
    id<MTLFunction> _rtReflectionFunction;
    id<MTLComputePipelineState> _rtReflectionPipeline;
    
    id<MTLEvent> _accelerationStructureBuildEvent;
    id<MTLHeap> _accelerationStructureHeap;

    id<MTLBuffer> _cameraDataBuffers[kMaxBuffersInFlight];
    id<MTLBuffer> _sceneArgumentBuffer;
    id<MTLAccelerationStructure> _instanceAccelerationStructure;
    NSArray< id<MTLAccelerationStructure> > *primitiveAccelerationStructures;
    
    std::vector<MaterialDscriptor> _materialDescriptors;
    std::vector<MeshDescriptor> _meshDescriptors;
    std::vector<InstanceDescriptor> _instanceDescriptors;
    
    NSMutableArray<id<MTLResource>>* _sceneResources;
    NSMutableArray<id<MTLHeap>>* _sceneHeaps;

    id<MTLBuffer> newBufferWithLabel(NSString *label, NSUInteger length, MTLResourceOptions options);
    MTLAccelerationStructureSizes calculateSizeForPrimitiveAccelerationStructures(NSArray<MTLPrimitiveAccelerationStructureDescriptor*>*primitiveAccelerationDescriptors);
    NSArray<id<MTLAccelerationStructure>>* allocateAndBuildAccelerationStructuresWithDescriptors(id<MTLCommandBuffer> cmd, NSArray<MTLAccelerationStructureDescriptor *>* descriptors, id<MTLHeap> heap, size_t maxScratchSize, id<MTLEvent> event);
    id<MTLAccelerationStructure> allocateAndBuildAccelerationStructureWithDescriptor(MTLAccelerationStructureDescriptor* descriptor, id<MTLCommandBuffer> cmd);

public:
    AccelerationStructure() {}
    ~AccelerationStructure() {}

    void Initialize(id<MTLDevice> device);
    void SetMeshes(const MeshDescriptor* meshes, int count);
    void SetInstances(const InstanceDescriptor* instances, int count);
    void SetMaterials(const MaterialDscriptor* materials, int count);

    void BuildBottomLevelAccelerationStructure(id<MTLCommandBuffer> cmd);
    void BuildTopLevelAccelerationStructure(id<MTLCommandBuffer> cmd);
    void BuildSceneArgumentBuffer(id<MTLCommandBuffer> cmd);
    void DispatchRaytracing(id<MTLCommandBuffer> commandBuffer, const CameraData& cameraData, id<MTLTexture> rtReflectionMap, id<MTLTexture> depthTexture, id<MTLTexture> normalTexture);
    void CleanupRaytracing();
    bool IsSupported() { return _initialized; }
};
