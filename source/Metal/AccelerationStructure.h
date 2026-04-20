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
#include <simd/simd.h>

struct BottomLevelAccelerationStructureDescriptor;
struct TopLevelAccelerationStructureElementDescriptor;

/// Manages Metal ray tracing acceleration structures
class API_AVAILABLE(ios(17.0), macos(14.0)) AccelerationStructure
{
    constexpr static int kPrimitiveAccelerationStructureBuild = 1;
    constexpr static int kInstanceAccelerationStructureBuild = 2;

    bool _initialized = false;
    id<MTLDevice> _device;
    
    id<MTLFunction> _rtReflectionFunction;
    id<MTLComputePipelineState> _rtReflectionPipeline;
    
    id<MTLEvent> _accelerationStructureBuildEvent;
    id<MTLHeap> _accelerationStructureHeap;

    id<MTLBuffer> _sceneArgumentBuffer;
    id<MTLAccelerationStructure> _instanceAccelerationStructure;
    NSArray< id<MTLAccelerationStructure> > *primitiveAccelerationStructures;

    NSMutableArray< MTLPrimitiveAccelerationStructureDescriptor* > *_primitiveAccelerationDescriptors;
    std::vector<MTLAccelerationStructureInstanceDescriptor> _instanceDescriptors;

    NSMutableArray<id<MTLResource>>* _sceneResources;
    NSMutableArray<id<MTLHeap>>* _sceneHeaps;

    MTLAccelerationStructureSizes calculateSizeForPrimitiveAccelerationStructures(NSArray<MTLPrimitiveAccelerationStructureDescriptor*>*primitiveAccelerationDescriptors);
    NSArray<id<MTLAccelerationStructure>>* allocateAndBuildAccelerationStructuresWithDescriptors(id<MTLCommandBuffer> cmd, NSArray<MTLAccelerationStructureDescriptor *>* descriptors, id<MTLHeap> heap, size_t maxScratchSize, id<MTLEvent> event);
    id<MTLAccelerationStructure> allocateAndBuildAccelerationStructureWithDescriptor(MTLAccelerationStructureDescriptor* descriptor, id<MTLCommandBuffer> cmd);

public:
    AccelerationStructure() {}
    ~AccelerationStructure() {}

    void Initialize(id<MTLDevice> device);
    void SetBlasDescriptors(const BottomLevelAccelerationStructureDescriptor* blasDescriptors, const int* pSubmeshCount, int count);
    void SetTlasDescriptors(const TopLevelAccelerationStructureElementDescriptor* tlasDescriptor, int count);
    void BuildBottomLevelAccelerationStructure(id<MTLCommandBuffer> cmd, BottomLevelAccelerationStructureDescriptor* blasDescriptors, int* meshCount, int count);
    void BuildTopLevelAccelerationStructure(id<MTLCommandBuffer> cmd, TopLevelAccelerationStructureElementDescriptor* tlasElements, int instanceCount);
    void DispatchRaytracing(id<MTLCommandBuffer> commandBuffer, id<MTLTexture> rtReflectionMap, id<MTLTexture> positionTexture, id<MTLTexture> directionTexture);
    void CleanupRaytracing();
    bool IsSupported() { return _initialized; }
};
