#include "AccelerationStructure.h"
#include "RenderAPI_Metal.h"
#include "ShaderDefinition.h"

void arrayToBatchMethodHelper(NSArray *array, void (^callback)(__unsafe_unretained id *, NSUInteger))
{
#define bufferLength 16
    __unsafe_unretained id buffer[bufferLength];
    NSFastEnumerationState state = {};

    NSUInteger count;

    while ((count = [array countByEnumeratingWithState:&state objects:buffer count:bufferLength]) > 0)
    {
        callback(state.itemsPtr, count);
    }
#undef bufferLength
}

MTLPackedFloat4x3 matrix4x4_drop_last_row(const float4x4 m)
{
    return (MTLPackedFloat4x3){
        MTLPackedFloat3Make( m.columns[0].x, m.columns[0].y, m.columns[0].z ),
        MTLPackedFloat3Make( m.columns[1].x, m.columns[1].y, m.columns[1].z ),
        MTLPackedFloat3Make( m.columns[2].x, m.columns[2].y, m.columns[2].z ),
        MTLPackedFloat3Make( m.columns[3].x, m.columns[3].y, m.columns[3].z )
    };
}

// Helper Objective-C class to locate the plugin's bundle via bundleForClass:
@interface _RenderingPluginBundleLocator : NSObject
@end
@implementation _RenderingPluginBundleLocator
@end

void AccelerationStructure::Initialize(id<MTLDevice> device)
{
    _device = device;

    // Find the bundle that contains this plugin's compiled code
    NSBundle* pluginBundle = [NSBundle bundleForClass:[_RenderingPluginBundleLocator class]];

    NSError* error = nil;
    id<MTLLibrary> defaultLibrary = [_device newDefaultLibraryWithBundle:pluginBundle error:&error];
    if (error)
    {
        RenderAPI::LogError("Failed to load default metallib from plugin bundle: %s", error.localizedDescription.UTF8String);
        return;
    }

    id<MTLFunction> rtReflectionFunction = [defaultLibrary newFunctionWithName:@"rtReflection"];
    if (rtReflectionFunction == nil)
    {
        RenderAPI::LogError("Failed to load default metallib from plugin bundle: %s", error.localizedDescription.UTF8String);
        return;
    }

    MTLIntersectionFunctionDescriptor *intersectionDesc = [[MTLIntersectionFunctionDescriptor alloc] init];
    intersectionDesc.name = @"alphaTestIntersection";
    id<MTLFunction> rtAnyHitFunction = [defaultLibrary newIntersectionFunctionWithDescriptor:intersectionDesc error:&error];
    if (rtAnyHitFunction == nil)
    {
        RenderAPI::LogError("Failed to load intersection function 'alphaTestIntersection' from metallib: %s", error.localizedDescription.UTF8String);
        return;
    }

    MTLLinkedFunctions *linkedFunctions = [[MTLLinkedFunctions alloc] init];
    linkedFunctions.functions = @[ rtAnyHitFunction ]; // 注册 Any-Hit 函数

    MTLComputePipelineDescriptor *pipelineDesc = [[MTLComputePipelineDescriptor alloc] init];
    pipelineDesc.computeFunction = rtReflectionFunction;
    pipelineDesc.linkedFunctions = linkedFunctions;
    
    _rtReflectionPipeline = [_device newComputePipelineStateWithDescriptor:pipelineDesc options:MTLPipelineOptionNone reflection:nil error:&error];
    if (error)
    {
        RenderAPI::LogError("Failed to create RT reflection compute pipeline state: %s", error.localizedDescription.UTF8String);
        return;
    }

    MTLIntersectionFunctionTableDescriptor *tableDesc = [MTLIntersectionFunctionTableDescriptor intersectionFunctionTableDescriptor];
    tableDesc.functionCount = 1;
    
    _intersectionFunctionTable = [_rtReflectionPipeline newIntersectionFunctionTableWithDescriptor:tableDesc];
    [_intersectionFunctionTable setFunction:[_rtReflectionPipeline functionHandleWithFunction:rtAnyHitFunction] atIndex:0];

    for (int i = 0; i < kMaxBuffersInFlight; i++)
    {
        _renderParametersBuffers[i] = [_device newBufferWithLength:sizeof(RaytracingRenderParameters)
                                         options:MTLResourceStorageModeShared];
        _renderParametersBuffers[i].label = [NSString stringWithFormat:@"RenderParametersBuffer %d", i];
    }

    _accelerationStructureBuildEvent = [_device newEvent];
    _accelerationStructureHeaps = [[NSMutableArray alloc] init];
    _sceneResources = [[NSMutableArray alloc] init];
    _primitiveAccelerationStructures = [[NSMutableArray alloc] init];
    _initialized = true;
    _accelerationStructureDirty = false;
}

id<MTLHeap> AccelerationStructure::GetAvailableHeap(const MTLSizeAndAlign& requiredSize)
{
    for (int i = 0; i < _accelerationStructureHeaps.count; i++)
    {
        if ([_accelerationStructureHeaps[i] maxAvailableSizeWithAlignment:requiredSize.align] >= requiredSize.size)
            return _accelerationStructureHeaps[i];
    }
    
    MTLHeapDescriptor* heapDesc = [[MTLHeapDescriptor alloc] init];
    heapDesc.size = fmax(256 * 1024 * 1024, requiredSize.size + requiredSize.align);
    [_accelerationStructureHeaps addObject:[_device newHeapWithDescriptor: heapDesc]];
    return _accelerationStructureHeaps.lastObject;
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

void AccelerationStructure::SetInstances(const InstanceDescriptor *instances, int count)
{
    _instanceDescriptors.resize(count);
    memcpy(_instanceDescriptors.data(), instances, count * sizeof(InstanceDescriptor));
    _accelerationStructureDirty = true;
}

void AccelerationStructure::SetMaterials(const MaterialDscriptor *materials, int count)
{
    _materialDescriptors.resize(count);
    memcpy(_materialDescriptors.data(), materials, count * sizeof(MaterialDscriptor));
    _accelerationStructureDirty = true;
}

void AccelerationStructure::SetMeshes(const MeshDescriptor* meshes, int meshCount)
{
    _meshDescriptors.resize(meshCount);
    memcpy(_meshDescriptors.data(), meshes, meshCount * sizeof(MeshDescriptor));
    _accelerationStructureDirty = true;
}

void AccelerationStructure::SetRaytracingGeometryBuildRequestList(const int* pBuildRequestList, int count)
{
    _geometryBuildRequestList.resize(count);
    memcpy(_geometryBuildRequestList.data(), pBuildRequestList, count * sizeof(int));
    _accelerationStructureDirty = true;
}

void AccelerationStructure::SetRenderParameters(const RaytracingRenderParameters& lightData)
{
    _constantBufferIndex = ( _constantBufferIndex + 1 ) % kMaxBuffersInFlight;
    // Update Projection Matrix
    RaytracingRenderParameters* pRenderParam = (RaytracingRenderParameters *)_renderParametersBuffers[_constantBufferIndex].contents;
    *pRenderParam = lightData;
}

void AccelerationStructure::BuildBottomLevelAccelerationStructure(id<MTLCommandBuffer> cmd)
{
    if (_geometryBuildRequestList.size() == 0)
        return;
    
    // Resize to match the number of meshes
    for (NSUInteger i = _primitiveAccelerationStructures.count; i < _meshDescriptors.size(); ++i)
        [_primitiveAccelerationStructures addObject:(id)[NSNull null]];

    id<MTLAccelerationStructureCommandEncoder> enc = [cmd accelerationStructureCommandEncoder];
    id<MTLBuffer> scratch = [_device newBufferWithLength: 16 * 1024 * 1024 options:MTLResourceStorageModePrivate];
    for (int buildIndex = 0; buildIndex < _geometryBuildRequestList.size(); ++buildIndex)
    {
        int blasIndex = _geometryBuildRequestList[buildIndex];
        MTLPrimitiveAccelerationStructureDescriptor* primDesc = [MTLPrimitiveAccelerationStructureDescriptor descriptor];
        const MeshDescriptor& mesh = _meshDescriptors[blasIndex];
        MTLAccelerationStructureTriangleGeometryDescriptor* geometry = [MTLAccelerationStructureTriangleGeometryDescriptor descriptor];
        geometry.vertexBuffer = (__bridge id<MTLBuffer>)mesh.positionBuffer;
        geometry.vertexBufferOffset = 0;
        geometry.vertexFormat = IsPositionHalf(mesh.vertexParameter) ? MTLAttributeFormatHalf4 : MTLAttributeFormatFloat3;
        geometry.vertexStride = GetPositionStride(mesh.vertexParameter);

        geometry.indexBuffer = (__bridge id<MTLBuffer>)mesh.indexBuffer;
        geometry.indexBufferOffset = mesh.indexBufferOffset;
        geometry.indexType = IsIndexHalf(mesh.vertexParameter) ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32;
        geometry.triangleCount = mesh.indexCount / 3;
        primDesc.geometryDescriptors = @[ geometry ];
        
        MTLSizeAndAlign sizeAndAlign = [_device heapAccelerationStructureSizeAndAlignWithDescriptor:primDesc];
        MTLAccelerationStructureSizes sizes = [_device accelerationStructureSizesWithDescriptor:primDesc];
        if (scratch.length < sizes.buildScratchBufferSize)
            scratch = [_device newBufferWithLength: sizes.buildScratchBufferSize options:MTLResourceStorageModePrivate];
        id<MTLAccelerationStructure> accelStructure = [GetAvailableHeap(sizeAndAlign) newAccelerationStructureWithSize:sizeAndAlign.size];
        [enc buildAccelerationStructure:accelStructure descriptor:primDesc scratchBuffer:scratch scratchBufferOffset:0];
        _primitiveAccelerationStructures[blasIndex] = accelStructure;
    }
    
    [enc endEncoding];
    [cmd encodeSignalEvent:_accelerationStructureBuildEvent value:kPrimitiveAccelerationStructureBuild];
    _geometryBuildRequestList.clear();
}

const static MTLAccelerationStructureInstanceOptions cullingOptions[] =
{
    MTLAccelerationStructureInstanceOptionDisableTriangleCulling,
    MTLAccelerationStructureInstanceOptionTriangleFrontFacingWindingCounterClockwise,
    MTLAccelerationStructureInstanceOptionNone,
};

const static uint32_t shadowCastingMask[] =
{
    AAPLRaytracingMaskNormal,
    AAPLRaytracingMaskNormal | AAPLRaytracingMaskShadow,
    AAPLRaytracingMaskNormal | AAPLRaytracingMaskShadow,
    AAPLRaytracingMaskShadow,
};

inline uint32_t GetRaytracingMask(uint32_t renderFlag)
{
    return shadowCastingMask[(renderFlag & RTInstMaskShadowCastingMode) >> RTInstBitShadowCastingMode];
}

inline MTLAccelerationStructureInstanceOptions GetRaytracingOptions(uint32_t renderFlag)
{
    return cullingOptions[(renderFlag & RTInstMaskCullMode) >> RTInstBitCullMode] |
    (renderFlag & RTInstMaskOpaque ? MTLAccelerationStructureInstanceOptionOpaque : MTLAccelerationStructureInstanceOptionNonOpaque);
}

void AccelerationStructure::BuildTopLevelAccelerationStructure(id<MTLCommandBuffer> cmd)
{
    MTLInstanceAccelerationStructureDescriptor* instanceAccelStructureDesc = [MTLInstanceAccelerationStructureDescriptor descriptor];
    instanceAccelStructureDesc.instancedAccelerationStructures = _primitiveAccelerationStructures;

    NSUInteger instanceCount = _instanceDescriptors.size();
    instanceAccelStructureDesc.instanceCount = instanceCount;

    // Load instance data (two fire trucks + one sphere + floor):
    size_t bufferLength = sizeof(MTLAccelerationStructureInstanceDescriptor) * instanceCount;
    id<MTLBuffer> instanceDescriptorBuffer = [_device newBufferWithLength:bufferLength options:MTLResourceStorageModeShared];
    MTLAccelerationStructureInstanceDescriptor* accelInstanceDescs = (MTLAccelerationStructureInstanceDescriptor *)instanceDescriptorBuffer.contents;
    for (NSUInteger i = 0; i < _instanceDescriptors.size(); ++i)
    {
        const InstanceDescriptor& instance = _instanceDescriptors[i];
        accelInstanceDescs[i].accelerationStructureIndex = instance.meshIndex;
        accelInstanceDescs[i].intersectionFunctionTableOffset = 0;
        accelInstanceDescs[i].mask = GetRaytracingMask(instance.renderFlag);
        accelInstanceDescs[i].options |= GetRaytracingOptions(instance.renderFlag);
        accelInstanceDescs[i].transformationMatrix = matrix4x4_drop_last_row(instance.transformMatrix);
    }

    instanceAccelStructureDesc.instanceDescriptorBuffer = instanceDescriptorBuffer;

    [cmd encodeWaitForEvent:_accelerationStructureBuildEvent value:kPrimitiveAccelerationStructureBuild];
    _instanceAccelerationStructure = allocateAndBuildAccelerationStructureWithDescriptor(instanceAccelStructureDesc, cmd);
    [cmd encodeSignalEvent:_accelerationStructureBuildEvent value:kInstanceAccelerationStructureBuild];
}

id<MTLBuffer> AccelerationStructure::newBufferWithLabel(NSString *label, NSUInteger length, MTLResourceOptions options)
{
    id<MTLBuffer> buffer = [_device newBufferWithLength:length options:options];
    buffer.label = label;

    [_sceneResources addObject:buffer];

    return buffer;
}

void AccelerationStructure::BuildSceneArgumentBuffer(id<MTLCommandBuffer> cmd)
{
    MTLResourceOptions storageMode;
#if TARGET_MACOS
    storageMode = MTLResourceStorageModeManaged;
#else
    storageMode = MTLResourceStorageModeShared;
#endif

    // The renderer builds this structure to match the ray-traced scene structure so the
    // ray-tracing shader navigates it. In particular, Metal represents each submesh as a
    // geometry in the primitive acceleration structure.
    NSUInteger instanceArgumentSize = sizeof( struct InstanceDescriptor ) * _instanceDescriptors.size();
    id<MTLBuffer> instanceArgumentBuffer = newBufferWithLabel(@"instanceArgumentBuffer",
                                                             instanceArgumentSize,
                                                             storageMode);
    memcpy(instanceArgumentBuffer.contents, _instanceDescriptors.data(), instanceArgumentSize);

    NSUInteger meshArgumentSize = sizeof( struct AAPLMesh ) * _meshDescriptors.size();
    id<MTLBuffer> meshArgumentBuffer = newBufferWithLabel(@"meshArgumentBuffer",
                                                             meshArgumentSize,
                                                             storageMode);

    // Encode the meshes array in Scene (Scene::meshes).
    for ( NSUInteger i = 0; i < _meshDescriptors.size(); ++i )
    {
        MeshDescriptor mesh = _meshDescriptors[i];
        struct AAPLMesh* pMesh = ((struct AAPLMesh *)meshArgumentBuffer.contents) + i;

        id<MTLBuffer> positionBuffer = (__bridge id<MTLBuffer>)mesh.positionBuffer;
        id<MTLBuffer> genericBuffer = (__bridge id<MTLBuffer>)mesh.genericBuffer;
        id<MTLBuffer> indexBuffer = (__bridge id<MTLBuffer>)mesh.indexBuffer;
        
        pMesh->positions = positionBuffer.gpuAddress;
        pMesh->generics = genericBuffer.gpuAddress;
        pMesh->indices = indexBuffer.gpuAddress + mesh.indexBufferOffset;
        pMesh->vertexParameters = mesh.vertexParameter;

        [_sceneResources addObject:positionBuffer];
        [_sceneResources addObject:genericBuffer];
        [_sceneResources addObject:indexBuffer];

        // Build submeshes into a buffer and reference it through a pointer in the mesh.
    }
    
    NSUInteger materialArgumentSize = sizeof( struct AAPLMaterial ) * _materialDescriptors.size();
    id<MTLBuffer> materialArgumentBuffer = newBufferWithLabel(@"materialArgumentBuffer",
                                                              materialArgumentSize,
                                                             storageMode);
    for ( NSUInteger i = 0; i < _materialDescriptors.size(); ++i )
    {
        struct AAPLMaterial* pMaterial = ((struct AAPLMaterial *)materialArgumentBuffer.contents) + i;
        id<MTLTexture> baseMap = (__bridge id<MTLTexture>)_materialDescriptors[i].BaseMap;
        id<MTLTexture> normalMap = (__bridge id<MTLTexture>)_materialDescriptors[i].NormalMap;
        id<MTLTexture> maskMap = (__bridge id<MTLTexture>)_materialDescriptors[i].MaskMap;
        id<MTLTexture> emissionMap = (__bridge id<MTLTexture>)_materialDescriptors[i].EmissionMap;
        pMaterial->textures[AAPLTextureIndexBaseColor] = baseMap.gpuResourceID;
        pMaterial->textures[AAPLTextureIndexNormal] = normalMap.gpuResourceID;
        pMaterial->textures[AAPLTextureIndexMask] = maskMap.gpuResourceID;
        pMaterial->textures[AAPLTextureIndexEmission] = emissionMap.gpuResourceID;
        pMaterial->BaseColor = *reinterpret_cast<simd_float4 *>(&_materialDescriptors[i].BaseColor);
        pMaterial->Emission = *reinterpret_cast<simd_float4 *>(&_materialDescriptors[i].Emission);
        pMaterial->SurfaceScale = *reinterpret_cast<simd_float4 *>(&_materialDescriptors[i].SurfaceScale);
        pMaterial->MaterialParam = *reinterpret_cast<simd_float4 *>(&_materialDescriptors[i].MaterialParam);
        [_sceneResources addObject:baseMap];
        [_sceneResources addObject:normalMap];
        [_sceneResources addObject:maskMap];
        [_sceneResources addObject:emissionMap];
    }

    id<MTLBuffer> sceneArgumentBuffer = newBufferWithLabel(@"sceneArgumentBuffer",
                                                           instanceArgumentSize,
                                                           storageMode);

    // Set `Scene::instances`.
    ((struct AAPLScene *)sceneArgumentBuffer.contents)->instances = instanceArgumentBuffer.gpuAddress;

    // Set `Scene::meshes`.
    ((struct AAPLScene *)sceneArgumentBuffer.contents)->meshes = meshArgumentBuffer.gpuAddress;

    // Set `Scene::materials`.
    ((struct AAPLScene *)sceneArgumentBuffer.contents)->materials = materialArgumentBuffer.gpuAddress;

#if TARGET_MACOS
    [instanceArgumentBuffer didModifyRange:NSMakeRange(0, instanceArgumentBuffer.length)];
    [meshArgumentBuffer didModifyRange:NSMakeRange(0, meshArgumentBuffer.length)];
    [materialArgumentBuffer didModifyRange:NSMakeRange(0, materialArgumentBuffer.length)];
    [sceneArgumentBuffer didModifyRange:NSMakeRange(0, sceneArgumentBuffer.length)];
#endif

    _sceneArgumentBuffer = sceneArgumentBuffer;
}

void AccelerationStructure::DispatchRaytracing(id<MTLCommandBuffer> commandBuffer, id<MTLTexture> __unsafe_unretained * textures)
{
    if (_accelerationStructureDirty)
    {
        [_sceneResources removeAllObjects];
        BuildBottomLevelAccelerationStructure(commandBuffer);
        BuildTopLevelAccelerationStructure(commandBuffer);
        BuildSceneArgumentBuffer(commandBuffer);
        [commandBuffer encodeWaitForEvent:_accelerationStructureBuildEvent value:kInstanceAccelerationStructureBuild];
        _accelerationStructureDirty = false;
    }

    id<MTLComputeCommandEncoder> compEnc = [commandBuffer computeCommandEncoder];
    compEnc.label = @"RaytracedReflectionsComputeEncoder";
    [compEnc setTexture:textures[AAPLRaytracingOutImageIndex] atIndex:AAPLRaytracingOutImageIndex];
    [compEnc setTexture:textures[AAPLRaytracingGBufferDepthIndex] atIndex:AAPLRaytracingGBufferDepthIndex];
    [compEnc setTexture:textures[AAPLRaytracingGBufferNormalIndex] atIndex:AAPLRaytracingGBufferNormalIndex];
    [compEnc setTexture:textures[AAPLRaytracingGBufferMaskIndex] atIndex:AAPLRaytracingGBufferMaskIndex];
    [compEnc setTexture:textures[AAPLRaytracingScreenSpaceDiffuse] atIndex:AAPLRaytracingScreenSpaceDiffuse];
    [compEnc setTexture:textures[AAPLRaytracingScreenSpaceAO] atIndex:AAPLRaytracingScreenSpaceAO];
    [compEnc setTexture:textures[AAPLRaytracingSkyCubeMap] atIndex:AAPLRaytracingSkyCubeMap];
    [compEnc setBuffer: _renderParametersBuffers[_constantBufferIndex] offset:0 atIndex:AAPLBufferIndexRenderParameter];

    // Bind the root of the argument buffer for the scene.
    [compEnc setBuffer:_sceneArgumentBuffer offset:0 atIndex:AAPLBufferIndexScene];

    // Bind the prebuilt acceleration structure.
    [compEnc setAccelerationStructure:_instanceAccelerationStructure atBufferIndex:AAPLBufferIndexAccelerationStructure];
    [compEnc setIntersectionFunctionTable:_intersectionFunctionTable atBufferIndex:AAPLBufferIndexIntersectionFunctionTable];

    // Set the ray tracing reflection kernel.
    [compEnc setComputePipelineState:_rtReflectionPipeline];
    
    // Flag residency for indirectly referenced heaps to make the driver put them into GPU memory.
    arrayToBatchMethodHelper(_accelerationStructureHeaps, ^(__unsafe_unretained id *data, NSUInteger count)
    {
        [compEnc useHeaps:data
                    count:count];
    });

    // Flag residency for indirectly referenced resources to make the driver put them into GPU memory.
    arrayToBatchMethodHelper(_sceneResources, ^(__unsafe_unretained id *data, NSUInteger count)
    {
        [compEnc useResources:data
                        count:count
                        usage:MTLResourceUsageRead];
    });
    
    NSUInteger w = _rtReflectionPipeline.threadExecutionWidth;
    NSUInteger h = _rtReflectionPipeline.maxTotalThreadsPerThreadgroup / w;
    MTLSize threadsPerThreadgroup = MTLSizeMake( w, h, 1 );
    MTLSize threadsPerGrid = MTLSizeMake(textures[AAPLRaytracingOutImageIndex].width, textures[AAPLRaytracingOutImageIndex].height, 1);

    [compEnc dispatchThreads:threadsPerGrid threadsPerThreadgroup:threadsPerThreadgroup];

    [compEnc endEncoding];
}

void AccelerationStructure::CleanupRaytracing()
{
    // Release acceleration structure resources
    _instanceAccelerationStructure = nil;
    [_primitiveAccelerationStructures removeAllObjects];
    [_accelerationStructureHeaps removeAllObjects];

    // Release argument buffer
    _sceneArgumentBuffer = nil;
    [_sceneResources removeAllObjects];

    // Clear C++ containers
    _instanceDescriptors.clear();
    _materialDescriptors.clear();
    _meshDescriptors.clear();
    _geometryBuildRequestList.clear();
}
