//
//  AccelerationStructure.mm
//  RenderingPlugin
//
//  Created by 郭昱宁 on 2026/4/13.
//
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

// 将一个指向 16 个 float 的指针转换成 matrix_float4x4（按列主序填充）
static inline matrix_float4x4 MatrixFromFloatPointer(const float* m)
{
    return (matrix_float4x4){
        (simd_float4){ m[0],  m[1],  m[2],  m[3]  },
        (simd_float4){ m[4],  m[5],  m[6],  m[7]  },
        (simd_float4){ m[8],  m[9],  m[10], m[11] },
        (simd_float4){ m[12], m[13], m[14], m[15] }
    };
}

MTLPackedFloat4x3 matrix4x4_drop_last_row(matrix_float4x4 m)
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

    _rtReflectionFunction = [defaultLibrary newFunctionWithName:@"rtReflection"];
    if (_rtReflectionFunction == nil)
    {
        RenderAPI::LogError("Failed to load default metallib from plugin bundle: %s", error.localizedDescription.UTF8String);
        return;
    }

    _rtReflectionPipeline = [_device newComputePipelineStateWithFunction:_rtReflectionFunction error:&error];
    if (error)
    {
        RenderAPI::LogError("Failed to create RT reflection compute pipeline state: %s", error.localizedDescription.UTF8String);
        return;
    }

    for (int i = 0; i < kMaxBuffersInFlight; i++)
    {
        _cameraDataBuffers[i] = [_device newBufferWithLength:sizeof(AAPLCameraData)
                                                 options:MTLResourceStorageModeShared];

        _cameraDataBuffers[i].label = [NSString stringWithFormat:@"CameraDataBuffer %d", i];
    }

    _accelerationStructureBuildEvent = [_device newEvent];
    _sceneResources = [[NSMutableArray alloc] init];
    _sceneHeaps = [[NSMutableArray alloc] init];
    _initialized = true;
    _accelerationStructureDirty = false;
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

void AccelerationStructure::SetMaterials(const MaterialDscriptor *materials, int count)
{
    _materialDescriptors.resize(count);
    memcpy(_materialDescriptors.data(), materials, count * sizeof(MaterialDscriptor));
}

void AccelerationStructure::SetMeshes(const MeshDescriptor* meshes, int meshCount)
{
    _meshDescriptors.resize(meshCount);
    memcpy(_meshDescriptors.data(), meshes, meshCount * sizeof(MeshDescriptor));
    _accelerationStructureDirty = true;
}

void AccelerationStructure::SetInstances(const InstanceDescriptor *instances, int count)
{
    _instanceDescriptors.resize(count);
    memcpy(_instanceDescriptors.data(), instances, count * sizeof(InstanceDescriptor));
    _accelerationStructureDirty = true;
}

void AccelerationStructure::BuildBottomLevelAccelerationStructure(id<MTLCommandBuffer> cmd)
{
    constexpr int subMeshCount = 1;
    NSMutableArray<MTLPrimitiveAccelerationStructureDescriptor*> *_primitiveAccelerationDescriptors = [NSMutableArray arrayWithCapacity:_meshDescriptors.size()];
    for (int meshIndex = 0; meshIndex < _meshDescriptors.size(); ++meshIndex)
    {
        NSMutableArray< MTLAccelerationStructureTriangleGeometryDescriptor* >* geometries = [NSMutableArray arrayWithCapacity:subMeshCount];
        for (int subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex)
        {
            const MeshDescriptor& mesh = _meshDescriptors[meshIndex];
            MTLAccelerationStructureTriangleGeometryDescriptor* g = [MTLAccelerationStructureTriangleGeometryDescriptor descriptor];
            g.vertexBuffer = (__bridge id<MTLBuffer>)mesh.positionBuffer;
            g.vertexBufferOffset = 0;
            g.vertexFormat = IsPositionHalf(mesh.vertexParameter) ? MTLAttributeFormatHalf4 : MTLAttributeFormatFloat3;
            g.vertexStride = GetPositionStride(mesh.vertexParameter);

            g.indexBuffer = (__bridge id<MTLBuffer>)mesh.indexBuffer;
            g.indexBufferOffset = mesh.indexBufferOffset;
            g.indexType = IsIndexHalf(mesh.vertexParameter) ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32;
            g.triangleCount = mesh.indexCount / 3;
            [geometries addObject:g];
        }

        MTLPrimitiveAccelerationStructureDescriptor* primDesc = [MTLPrimitiveAccelerationStructureDescriptor descriptor];
        primDesc.geometryDescriptors = geometries;
        [_primitiveAccelerationDescriptors addObject:primDesc];
    }
    
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

void AccelerationStructure::BuildTopLevelAccelerationStructure(id<MTLCommandBuffer> cmd)
{
    MTLInstanceAccelerationStructureDescriptor* instanceAccelStructureDesc = [MTLInstanceAccelerationStructureDescriptor descriptor];
    instanceAccelStructureDesc.instancedAccelerationStructures = primitiveAccelerationStructures;

    NSUInteger instanceCount = _instanceDescriptors.size();
    instanceAccelStructureDesc.instanceCount = instanceCount;

    // Load instance data (two fire trucks + one sphere + floor):
    size_t bufferLength = sizeof(MTLAccelerationStructureInstanceDescriptor) * instanceCount;
    id<MTLBuffer> instanceDescriptorBuffer = [_device newBufferWithLength:bufferLength options:MTLResourceStorageModeShared];
    MTLAccelerationStructureInstanceDescriptor* instanceDescriptors = (MTLAccelerationStructureInstanceDescriptor *)instanceDescriptorBuffer.contents;
    for (NSUInteger i = 0; i < _instanceDescriptors.size(); ++i)
    {
        instanceDescriptors[i].accelerationStructureIndex = _instanceDescriptors[i].meshIndex;
        instanceDescriptors[i].intersectionFunctionTableOffset = 0;
        instanceDescriptors[i].mask = 0xFF;
        instanceDescriptors[i].options = MTLAccelerationStructureInstanceOptionNone;

        MTLPackedFloat4x3 transformationMatrix{};
        instanceDescriptors[i].transformationMatrix = matrix4x4_drop_last_row(MatrixFromFloatPointer(_instanceDescriptors[i].transformMatrix));
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
    NSUInteger instanceArgumentSize = sizeof( struct Instance ) * _instanceDescriptors.size();
    id<MTLBuffer> instanceArgumentBuffer = newBufferWithLabel(@"instanceArgumentBuffer",
                                                             instanceArgumentSize,
                                                             storageMode);

    // Encode the instances array in `Scene` (`Scene::instances`).
    for ( NSUInteger i = 0; i < _instanceDescriptors.size(); ++i )
    {
        struct Instance* pInstance = ((struct Instance *)instanceArgumentBuffer.contents) + i;
        pInstance->meshIndex = _instanceDescriptors[i].meshIndex;
        pInstance->materialIndex = _instanceDescriptors[i].materialIndex;
        pInstance->transform = MatrixFromFloatPointer(_instanceDescriptors[i].transformMatrix);
    }

#if TARGET_MACOS
    [instanceArgumentBuffer didModifyRange:NSMakeRange(0, instanceArgumentBuffer.length)];
#endif

    NSUInteger meshArgumentSize = sizeof( struct Mesh ) * _meshDescriptors.size();
    id<MTLBuffer> meshArgumentBuffer = newBufferWithLabel(@"meshArgumentBuffer",
                                                             meshArgumentSize,
                                                             storageMode);

    // Encode the meshes array in Scene (Scene::meshes).
    for ( NSUInteger i = 0; i < _meshDescriptors.size(); ++i )
    {
        MeshDescriptor mesh = _meshDescriptors[i];
        struct Mesh* pMesh = ((struct Mesh *)meshArgumentBuffer.contents) + i;

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
    
    NSUInteger materialArgumentSize = sizeof( struct Mesh ) * _materialDescriptors.size();
    id<MTLBuffer> materialArgumentBuffer = newBufferWithLabel(@"materialArgumentBuffer",
                                                              materialArgumentSize,
                                                             storageMode);
    for ( NSUInteger i = 0; i < _materialDescriptors.size(); ++i )
    {
        struct Material* pMaterial = ((struct Material *)materialArgumentBuffer.contents) + i;
        id<MTLTexture> baseMap = (__bridge id<MTLTexture>)_materialDescriptors[i].BaseMap;
        id<MTLTexture> normalMap = (__bridge id<MTLTexture>)_materialDescriptors[i].NormalMap;
        id<MTLTexture> maskMap = (__bridge id<MTLTexture>)_materialDescriptors[i].MaskMap;
        pMaterial->textures[AAPLTextureIndexBaseColor] = baseMap.gpuResourceID;
        pMaterial->textures[AAPLTextureIndexNormal] = normalMap.gpuResourceID;
        pMaterial->textures[AAPLTextureIndexMask] = maskMap.gpuResourceID;
        [_sceneResources addObject:baseMap];
        [_sceneResources addObject:normalMap];
        [_sceneResources addObject:maskMap];
    }

    id<MTLBuffer> sceneArgumentBuffer = newBufferWithLabel(@"sceneArgumentBuffer",
                                                           instanceArgumentSize,
                                                           storageMode);

    // Set `Scene::instances`.
    ((struct Scene *)sceneArgumentBuffer.contents)->instances = instanceArgumentBuffer.gpuAddress;

    // Set `Scene::meshes`.
    ((struct Scene *)sceneArgumentBuffer.contents)->meshes = meshArgumentBuffer.gpuAddress;

    // Set `Scene::materials`.
    ((struct Scene *)sceneArgumentBuffer.contents)->materials = materialArgumentBuffer.gpuAddress;

#if TARGET_MACOS
    [instanceArgumentBuffer didModifyRange:NSMakeRange(0, instanceArgumentBuffer.length)];
    [meshArgumentBuffer didModifyRange:NSMakeRange(0, meshArgumentBuffer.length)];
    [materialArgumentBuffer didModifyRange:NSMakeRange(0, materialArgumentBuffer.length)];
    [sceneArgumentBuffer didModifyRange:NSMakeRange(0, sceneArgumentBuffer.length)];
#endif

    _sceneArgumentBuffer = sceneArgumentBuffer;
}

void AccelerationStructure::DispatchRaytracing(id<MTLCommandBuffer> commandBuffer, const CameraData& cameraData, id<MTLTexture> rtReflectionMap, id<MTLTexture> depthTexture, id<MTLTexture> normalTexture)
{
    if (_accelerationStructureDirty)
    {
        [_sceneResources removeAllObjects];
        [_sceneHeaps removeAllObjects];
        BuildBottomLevelAccelerationStructure(commandBuffer);
        BuildTopLevelAccelerationStructure(commandBuffer);
        BuildSceneArgumentBuffer(commandBuffer);
        [commandBuffer encodeWaitForEvent:_accelerationStructureBuildEvent value:kInstanceAccelerationStructureBuild];
        _accelerationStructureDirty = false;
    }

    id<MTLComputeCommandEncoder> compEnc = [commandBuffer computeCommandEncoder];
    compEnc.label = @"RaytracedReflectionsComputeEncoder";
    [compEnc setTexture:rtReflectionMap atIndex:OutImageIndex];
    [compEnc setTexture:depthTexture atIndex:GBufferDepthIndex];
    [compEnc setTexture:normalTexture atIndex:GBufferNormalIndex];

    // Bind the root of the argument buffer for the scene.
//    [compEnc setBuffer:_sceneArgumentBuffer offset:0 atIndex:SceneIndex];

    // Update Projection Matrix
    _cameraBufferIndex = ( _cameraBufferIndex + 1 ) % kMaxBuffersInFlight;
    AAPLCameraData* pCameraData = (AAPLCameraData *)_cameraDataBuffers[_cameraBufferIndex].contents;
    pCameraData->MatrixVP = MatrixFromFloatPointer(cameraData.worldToClip);
    pCameraData->MatrixVP_Inv = MatrixFromFloatPointer(cameraData.clipToWorld);
    pCameraData->cameraPosition = vector_float3{ cameraData.cameraPos[0], cameraData.cameraPos[1], cameraData.cameraPos[2] };
    [compEnc setBuffer:_cameraDataBuffers[_cameraBufferIndex] offset:0 atIndex:AAPLBufferIndexCameraData];

    [compEnc setBuffer:_sceneArgumentBuffer offset:0 atIndex:AAPLBufferIndexScene];

    // Bind the prebuilt acceleration structure.
    [compEnc setAccelerationStructure:_instanceAccelerationStructure atBufferIndex:AAPLBufferIndexAccelerationStructure];

    // Set the ray tracing reflection kernel.
    [compEnc setComputePipelineState:_rtReflectionPipeline];
    
    // Flag residency for indirectly referenced heaps to make the driver put them into GPU memory.
    arrayToBatchMethodHelper(_sceneHeaps, ^(__unsafe_unretained id *data, NSUInteger count)
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
    MTLSize threadsPerGrid = MTLSizeMake(rtReflectionMap.width, rtReflectionMap.height, 1);

    [compEnc dispatchThreads:threadsPerGrid threadsPerThreadgroup:threadsPerThreadgroup];

    [compEnc endEncoding];
}

void AccelerationStructure::CleanupRaytracing()
{
    // Release acceleration structure resources
    _instanceAccelerationStructure = nil;
    primitiveAccelerationStructures = nil;
    _accelerationStructureHeap = nil;

    // Release argument buffer
    _sceneArgumentBuffer = nil;

    // Clear C++ containers
    _instanceDescriptors.clear();
}

