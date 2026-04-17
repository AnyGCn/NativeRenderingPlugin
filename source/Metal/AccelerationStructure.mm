//
//  AccelerationStructure.mm
//  RenderingPlugin
//
//  Created by 郭昱宁 on 2026/4/13.
//
#include "AccelerationStructure.h"
#include "RenderAPI_Metal.h"
#include "ShaderDefinition.h"

void AccelerationStructure::Initialize(id<MTLDevice> device)
{
    _device = device;
    id<MTLLibrary> defaultLibrary = [_device newDefaultLibrary];
    _rtReflectionFunction = [defaultLibrary newFunctionWithName:@"rtReflection"];

    NSError* error;
    _rtReflectionPipeline = [_device newComputePipelineStateWithFunction:_rtReflectionFunction error:&error];
    if (error)
    {
        RenderAPI::LogError("Failed to create RT reflection compute pipeline state: %s", error.localizedDescription.UTF8String);
    }
}

MTLAccelerationStructureSizes AccelerationStructure::calculateSizeForPrimitiveAccelerationStructures(NSArray<MTLPrimitiveAccelerationStructureDescriptor*>*primitiveAccelerationDescriptors)
{
    MTLAccelerationStructureSizes totalSizes = (MTLAccelerationStructureSizes){0, 0, 0};
    for ( MTLPrimitiveAccelerationStructureDescriptor* desc in primitiveAccelerationDescriptors )
    {
        MTLSizeAndAlign sizeAndAlign = [_device heapAccelerationStructureSizeAndAlignWithDescriptor:desc];
        MTLAccelerationStructureSizes sizes = [_device accelerationStructureSizesWithDescriptor:desc];
        totalSizes.accelerationStructureSize += (sizeAndAlign.size + sizeAndAlign.align);
        totalSizes.buildScratchBufferSize = MAX( sizes.buildScratchBufferSize, totalSizes.buildScratchBufferSize );
        totalSizes.refitScratchBufferSize = MAX( sizes.refitScratchBufferSize, totalSizes.refitScratchBufferSize);
    }
    return totalSizes;
}

NSArray<id<MTLAccelerationStructure>>* AccelerationStructure::allocateAndBuildAccelerationStructuresWithDescriptors(id<MTLCommandBuffer> cmd, NSArray<MTLAccelerationStructureDescriptor *>* descriptors, id<MTLHeap> heap, size_t maxScratchSize, id<MTLEvent> event)
{
    NSMutableArray< id<MTLAccelerationStructure> >* accelStructures = [NSMutableArray arrayWithCapacity:descriptors.count];

    id<MTLBuffer> scratch = [_device newBufferWithLength:maxScratchSize options:MTLResourceStorageModePrivate];
    id<MTLAccelerationStructureCommandEncoder> enc = [cmd accelerationStructureCommandEncoder];

    for ( MTLPrimitiveAccelerationStructureDescriptor* descriptor in descriptors )
    {
        MTLSizeAndAlign sizes = [_device heapAccelerationStructureSizeAndAlignWithDescriptor:descriptor];
        id<MTLAccelerationStructure> accelStructure = [heap newAccelerationStructureWithSize:sizes.size];
        [enc buildAccelerationStructure:accelStructure descriptor:descriptor scratchBuffer:scratch scratchBufferOffset:0];
        [accelStructures addObject:accelStructure];
    }

    [enc endEncoding];
    [cmd encodeSignalEvent:event value:kPrimitiveAccelerationStructureBuild];

    return accelStructures;
}

id<MTLAccelerationStructure> AccelerationStructure::allocateAndBuildAccelerationStructureWithDescriptor(MTLAccelerationStructureDescriptor* descriptor, id<MTLCommandBuffer> cmd)
{
    MTLAccelerationStructureSizes sizes = [_device accelerationStructureSizesWithDescriptor:descriptor];
    id<MTLBuffer> scratch = [_device newBufferWithLength:sizes.buildScratchBufferSize options:MTLResourceStorageModePrivate];
    id<MTLAccelerationStructure> accelStructure = [_device newAccelerationStructureWithSize:sizes.accelerationStructureSize];

    id<MTLAccelerationStructureCommandEncoder> enc = [cmd accelerationStructureCommandEncoder];
    [enc buildAccelerationStructure:accelStructure descriptor:descriptor scratchBuffer:scratch scratchBufferOffset:0];
    [enc endEncoding];

    return accelStructure;
}

void AccelerationStructure::SetBlasDescriptors(const BottomLevelAccelerationStructureDescriptor* blasDescriptors, const int* pSubmeshCount, int meshCount)
{
    _primitiveAccelerationDescriptors = [NSMutableArray arrayWithCapacity:meshCount];
    for (int meshIndex = 0; meshIndex < meshCount; ++meshIndex)
    {
        int subMeshCount = pSubmeshCount[meshIndex];
        NSMutableArray< MTLAccelerationStructureTriangleGeometryDescriptor* >* geometries = [NSMutableArray arrayWithCapacity:subMeshCount];
        for (int subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex)
        {
            const BottomLevelAccelerationStructureDescriptor& blasDesc = *(blasDescriptors++);
            MTLAccelerationStructureTriangleGeometryDescriptor* g = [MTLAccelerationStructureTriangleGeometryDescriptor descriptor];
            g.vertexBuffer = (__bridge id<MTLBuffer>)blasDesc.positionBuffer;
            g.vertexBufferOffset = blasDesc.vertexBufferOffset;
            g.vertexFormat = blasDesc.vertexFormat == 0 ? MTLAttributeFormatFloat3 : MTLAttributeFormatHalf4;
            g.vertexStride = blasDesc.vertexStride;

            g.indexBuffer = (__bridge id<MTLBuffer>)blasDesc.indexBuffer;
            g.indexBufferOffset = blasDesc.indexBufferOffset;
            g.indexType = (MTLIndexType)blasDesc.indexType;
            g.triangleCount = blasDesc.indexCount / 3;
            [geometries addObject:g];
        }

        MTLPrimitiveAccelerationStructureDescriptor* primDesc = [MTLPrimitiveAccelerationStructureDescriptor descriptor];
        primDesc.geometryDescriptors = geometries;
        [_primitiveAccelerationDescriptors addObject:primDesc];
    }
}

void AccelerationStructure::SetTlasDescriptors(const TopLevelAccelerationStructureElementDescriptor *tlasDescriptor, int count)
{
    _instanceDescriptors.resize(count);
    for (NSUInteger i = 0; i < count; ++i)
    {
        _instanceDescriptors[i].accelerationStructureIndex = tlasDescriptor[i].meshIndex;
        _instanceDescriptors[i].intersectionFunctionTableOffset = 0;
        _instanceDescriptors[i].mask = 0xFF;
        _instanceDescriptors[i].options = MTLAccelerationStructureInstanceOptionNone;

        MTLPackedFloat4x3 transformationMatrix{};
        _instanceDescriptors[i].transformationMatrix = transformationMatrix;
    }
}

void AccelerationStructure::BuildBottomLevelAccelerationStructure(id<MTLCommandBuffer> cmd, BottomLevelAccelerationStructureDescriptor* blasDescriptors, int* pSubmeshCount, int meshCount)
{
    if ( [_device supportsFamily:MTLGPUFamilyMetal3] )
    {
        MTLAccelerationStructureSizes storageSizes = calculateSizeForPrimitiveAccelerationStructures(_primitiveAccelerationDescriptors);
        MTLHeapDescriptor* heapDesc = [[MTLHeapDescriptor alloc] init];
        heapDesc.size = storageSizes.accelerationStructureSize;
        _accelerationStructureHeap = [_device newHeapWithDescriptor:heapDesc];
        primitiveAccelerationStructures = allocateAndBuildAccelerationStructuresWithDescriptors(cmd, _primitiveAccelerationDescriptors, _accelerationStructureHeap, storageSizes.buildScratchBufferSize, _accelerationStructureBuildEvent);
    }

    [_sceneHeaps addObject:_accelerationStructureHeap];
}

void AccelerationStructure::BuildTopLevelAccelerationStructure(id<MTLCommandBuffer> cmd, TopLevelAccelerationStructureElementDescriptor* tlasElements, int instanceCount)
{
    MTLInstanceAccelerationStructureDescriptor* instanceAccelStructureDesc = [MTLInstanceAccelerationStructureDescriptor descriptor];
    instanceAccelStructureDesc.instancedAccelerationStructures = primitiveAccelerationStructures;

    instanceAccelStructureDesc.instanceCount = instanceCount;

    // Load instance data (two fire trucks + one sphere + floor):
    size_t bufferLength = sizeof(MTLAccelerationStructureInstanceDescriptor) * instanceCount;
    id<MTLBuffer> instanceDescriptorBuffer = [_device newBufferWithLength:bufferLength options:MTLResourceStorageModeShared];
    memcpy(instanceDescriptorBuffer.contents, _instanceDescriptors.data(), bufferLength);

    instanceAccelStructureDesc.instanceDescriptorBuffer = instanceDescriptorBuffer;

    [cmd encodeWaitForEvent:_accelerationStructureBuildEvent value:kPrimitiveAccelerationStructureBuild];
    _instanceAccelerationStructure = allocateAndBuildAccelerationStructureWithDescriptor(instanceAccelStructureDesc, cmd);
    [cmd encodeSignalEvent:_accelerationStructureBuildEvent value:kInstanceAccelerationStructureBuild];
}

void AccelerationStructure::DispatchRaytracing(id<MTLCommandBuffer> commandBuffer, id<MTLTexture> rtReflectionMap, id<MTLTexture> positionTexture, id<MTLTexture> directionTexture)
{
    [commandBuffer encodeWaitForEvent:_accelerationStructureBuildEvent value:kInstanceAccelerationStructureBuild];
    id<MTLComputeCommandEncoder> compEnc = [commandBuffer computeCommandEncoder];
    compEnc.label = @"RaytracedReflectionsComputeEncoder";
    [compEnc setTexture:rtReflectionMap atIndex:OutImageIndex];
    [compEnc setTexture:positionTexture atIndex:ThinGBufferPositionIndex];
    [compEnc setTexture:directionTexture atIndex:ThinGBufferDirectionIndex];

    // Bind the root of the argument buffer for the scene.
    [compEnc setBuffer:_sceneArgumentBuffer offset:0 atIndex:SceneIndex];

    // Bind the prebuilt acceleration structure.
    [compEnc setAccelerationStructure:_instanceAccelerationStructure atBufferIndex:AccelerationStructureIndex];

    // Set the ray tracing reflection kernel.
    [compEnc setComputePipelineState:_rtReflectionPipeline];
}
