#include <metal_stdlib>
#include <simd/simd.h>

#include "Lighting.h"

// Include the header that this Metal shader code shares with the Swift/C code that executes Metal API commands.
using namespace metal;
using raytracing::instancing;
using raytracing::instance_acceleration_structure;
using raytracing::intersection_function_table;
using raytracing::triangle_data;

float3x4 LoadVertexDataDimension2(constant uint8_t* pData, uint32_t i0, uint32_t i1, uint32_t i2, uint32_t stride, bool isHalf)
{
    float3x4 dataArray;
    if ( isHalf )
    {
        dataArray.columns[0] = float4(*((constant half4 *)(pData + i0 * stride)));
        dataArray.columns[1] = float4(*((constant half4 *)(pData + i1 * stride)));
        dataArray.columns[2] = float4(*((constant half4 *)(pData + i2 * stride)));
    }
    else
    {
        dataArray.columns[0] = float4(*((constant float2 *)(pData + i0 * stride)), 1, 1);
        dataArray.columns[1] = float4(*((constant float2 *)(pData + i1 * stride)), 1, 1);
        dataArray.columns[2] = float4(*((constant float2 *)(pData + i2 * stride)), 1, 1);
    }

    return dataArray;
}

float3x4 LoadVertexDataDimension3(constant uint8_t* pData, uint32_t i0, uint32_t i1, uint32_t i2, uint32_t stride, bool isHalf)
{
    float3x4 dataArray;
    if ( isHalf )
    {
        dataArray.columns[0] = float4(*((constant half4 *)(pData + i0 * stride)));
        dataArray.columns[1] = float4(*((constant half4 *)(pData + i1 * stride)));
        dataArray.columns[2] = float4(*((constant half4 *)(pData + i2 * stride)));
    }
    else
    {
        dataArray.columns[0] = float4(*((constant float3 *)(pData + i0 * stride)), 1);
        dataArray.columns[1] = float4(*((constant float3 *)(pData + i1 * stride)), 1);
        dataArray.columns[2] = float4(*((constant float3 *)(pData + i2 * stride)), 1);
    }

    return dataArray;
}

float3x4 LoadVertexDataDimension4(constant uint8_t* pData, uint32_t i0, uint32_t i1, uint32_t i2, uint32_t stride, bool isHalf)
{
    float3x4 dataArray;
    if ( isHalf )
    {
        dataArray.columns[0] = float4(*((constant half4 *)(pData + i0 * stride)));
        dataArray.columns[1] = float4(*((constant half4 *)(pData + i1 * stride)));
        dataArray.columns[2] = float4(*((constant half4 *)(pData + i2 * stride)));
    }
    else
    {
        dataArray.columns[0] = *((constant float4 *)(pData + i0 * stride));
        dataArray.columns[1] = *((constant float4 *)(pData + i1 * stride));
        dataArray.columns[2] = *((constant float4 *)(pData + i2 * stride));
    }

    return dataArray;
}

Varyings LoadVertexData(uint primitive_id, float3 bary3, AAPLInstance instance, AAPLMesh mesh)
{
    uint32_t i0, i1, i2;
    if (IsIndexHalf(mesh.vertexParameters))
    {
        constant uint16_t* pIndices = (constant uint16_t *)mesh.indices;
        i0 = pIndices[ primitive_id * 3 + 0];
        i1 = pIndices[ primitive_id * 3 + 1];
        i2 = pIndices[ primitive_id * 3 + 2];
    }
    else
    {
        constant uint32_t* pIndices = (constant uint32_t *)mesh.indices;
        i0 = pIndices[ primitive_id * 3 + 0];
        i1 = pIndices[ primitive_id * 3 + 1];
        i2 = pIndices[ primitive_id * 3 + 2];
    }

    Varyings vertexOutput = {};
    constant uint8_t* pPositions = mesh.positions;
    constant uint8_t* pGenerics = mesh.generics;
    uint32_t positionStride = GetPositionStride(mesh.vertexParameters);
    uint32_t genericStride = GetGenericStride(mesh.vertexParameters);

    // Position
    uint32_t stride = positionStride;
    bool isHalf = IsPositionHalf(mesh.vertexParameters);
    float3x4 dataArray = LoadVertexDataDimension3(pPositions, i0, i1, i2, stride, isHalf);
    vertexOutput.worldPosition = (instance.transform * (dataArray * bary3)).xyz;
    pPositions += isHalf ? 8 : 12;

    // Normal
    bool isGeneric = IsNormalInGeneric(mesh.vertexParameters);
    stride = isGeneric ? genericStride : positionStride;
    isHalf = IsNormalHalf(mesh.vertexParameters);
    dataArray = LoadVertexDataDimension3(pGenerics, i0, i1, i2, stride, isHalf);
    vertexOutput.normal = half4(dataArray * bary3).xyz;
    pPositions += isGeneric ? 0 : (isHalf ? 8 : 12);
    pGenerics += isGeneric ? (isHalf ? 8 : 12) : 0;

    // Tangent
    isGeneric = IsTangentInGeneric(mesh.vertexParameters);
    stride = isGeneric ? genericStride : positionStride;
    isHalf = IsTangentHalf(mesh.vertexParameters);
    dataArray = LoadVertexDataDimension4(pGenerics, i0, i1, i2, stride, isHalf);
    float4 tangentW = dataArray * bary3;
    vertexOutput.tangent = half4(tangentW).xyz;
    pPositions += isGeneric ? 0 : (isHalf ? 8 : 12);
    pGenerics += isGeneric ? (isHalf ? 8 : 12) : 0;

    // Bitangent
    float4x4 mv = instance.transform;
    half3x3 normalMx = half3x3(half3(mv.columns[0].xyz), half3(mv.columns[1].xyz), half3(mv.columns[2].xyz));
    vertexOutput.normal = normalize(normalMx * vertexOutput.normal);
    vertexOutput.tangent = normalize(normalMx * vertexOutput.tangent);
    vertexOutput.bitangent = cross(vertexOutput.normal, vertexOutput.tangent) * sign(tangentW.w);

    // Color
    isGeneric = IsColorInGeneric(mesh.vertexParameters);
    bool isColorExists = IsColorExists(mesh.vertexParameters);
    pPositions += isGeneric ? 0 : (isColorExists ? 4 : 0);
    pGenerics += isGeneric ? (isColorExists ? 4 : 0) : 0;

    // Texture coordinates (maybe memory access out of bounds)
    isGeneric = IsUVInGeneric(mesh.vertexParameters);
    stride = isGeneric ? genericStride : positionStride;
    isHalf = IsUVHalf(mesh.vertexParameters);
    dataArray = LoadVertexDataDimension2(pGenerics, i0, i1, i2, stride, isHalf);
    vertexOutput.texCoord = (dataArray * bary3).xy;

    return vertexOutput;
}

half MainLightShadowUsingRaytracing(instance_acceleration_structure accelerationStructure, intersection_function_table<instancing, triangle_data> functionTable, float3 worldPos, float3 lightDir)
{
    raytracing::ray r;
    r.origin = worldPos;
    r.direction = lightDir;
    r.min_distance = 0.1;
    r.max_distance = FLT_MAX;
    
    raytracing::intersector<raytracing::instancing, raytracing::triangle_data> inter;
    inter.force_opacity( raytracing::forced_opacity::opaque );
    inter.assume_geometry_type( raytracing::geometry_type::triangle );
    auto intersection = inter.intersect( r, accelerationStructure, AAPLRaytracingMaskShadow, functionTable );
    return intersection.type == raytracing::intersection_type::triangle ? 0.0 : 1.0;
}

[[intersection(triangle, instancing, triangle_data)]]
bool alphaTestIntersection(
    // 自动注入的内联参数
    uint primitive_id                      [[primitive_id]],
    uint instance_id                       [[instance_id]],
    float2 barycentric_coords              [[barycentric_coord]],
    // 开发者传入的自定义资源场景数据
    constant AAPLScene* pScene             [[buffer(AAPLBufferIndexScene)]]
)
{
    constant AAPLInstance& instance = pScene->instances[ instance_id ];
    constant AAPLMesh& mesh = pScene->meshes[instance.meshIndex];
    constant AAPLMaterial& material = pScene->materials[instance.materialIndex];
    float2 bary2 = barycentric_coords;
    float3 bary3 = float3( 1.0 - (bary2.x + bary2.y), bary2.x, bary2.y );
    Varyings vertexOutput = LoadVertexData(primitive_id, bary3, instance, mesh);
    MaterialParameter matData = InitializeMaterialData(vertexOutput, material);
    float alphaCutoff = 0.5;
    return (matData.alpha >= alphaCutoff);
}

kernel void rtReflection(
             texture2d< half, access::write >       outImage                [[texture(AAPLRaytracingOutImageIndex)]],
             texture2d< float >                     depth                   [[texture(AAPLRaytracingGBufferDepthIndex)]],
             texture2d< half >                      normalMap               [[texture(AAPLRaytracingGBufferNormalIndex)]],
             texture2d< half >                      maskMap                 [[texture(AAPLRaytracingGBufferMaskIndex)]],
             depth2d< float >                       mainlightShadowMap      [[texture(AAPLRaytracingMainLightShadowMap)]],
             texturecube< half >                    skyCube                 [[texture(AAPLRaytracingSkyCubeMap)]],
             constant AAPLRenderParameter&          renderData              [[buffer(AAPLBufferIndexRenderParameter)]],
             constant AAPLScene*                    pScene                  [[buffer(AAPLBufferIndexScene)]],
             instance_acceleration_structure        accelerationStructure   [[buffer(AAPLBufferIndexAccelerationStructure)]],
             intersection_function_table<instancing, triangle_data> functionTable [[buffer(AAPLBufferIndexIntersectionFunctionTable)]],
             uint2 tid [[thread_position_in_grid]])
{
    uint2 outputSize = uint2(outImage.get_width(), outImage.get_height());
    if (all(tid < outputSize))
    {
        half4 finalColor = half4( 0.0, 0.0, 0.0, 1.0 );
        if (is_null_instance_acceleration_structure(accelerationStructure))
        {
            finalColor = half4( 0.0, 0.0, 0.0, 0.0 );
        }
        else
        {
            // Reconstruct world-space position from depth
            uint2 inputSize = uint2(depth.get_width(), depth.get_height());
            uint2 inputCoord = tid * inputSize / outputSize;
            float2 uv = (float2(inputCoord) + 0.5f) / float2(inputSize); // pixel center -> [0,1]
            float depth01 = depth.read(inputCoord).x;                    // depth in [0,1] clip space
            float ndcZ = depth01;                                        // to NDC z
            float4 clipPos = float4(uv * 2.0f - 1.0f, ndcZ, 1.0f);       // NDC xy,z
            clipPos.y = -clipPos.y;
            float4 worldPosH = renderData.MatrixVP_Inv * clipPos;  // homogeneous world
            float3 worldPos = worldPosH.xyz / worldPosH.w;

            // Decode oct-encoded normal from the normal map
            float3 normalWS = UnpackOctNormal(normalMap.read(inputCoord).xyz);
            float3 viewDir = normalize(renderData.cameraPosition.xyz - worldPos);
            float3 reflectDir = reflect(-viewDir, normalWS);

            raytracing::ray r;
            r.origin = worldPos;
            r.direction = reflectDir;
            r.min_distance = 0.1;
            r.max_distance = FLT_MAX;

            raytracing::intersector<instancing, triangle_data> inter;
            inter.force_opacity( raytracing::forced_opacity::opaque );
            inter.assume_geometry_type( raytracing::geometry_type::triangle );
            auto intersection = inter.intersect( r, accelerationStructure, AAPLRaytracingMaskNormal, functionTable );
            if ( intersection.type == raytracing::intersection_type::triangle )
            {
                float2 bary2 = intersection.triangle_barycentric_coord;
                float3 bary3 = float3( 1.0 - (bary2.x + bary2.y), bary2.x, bary2.y );
                
                constant AAPLInstance& instance = pScene->instances[ intersection.instance_id ];
                constant AAPLMesh& mesh = pScene->meshes[instance.meshIndex];
                constant AAPLMaterial& material = pScene->materials[instance.materialIndex];
                Varyings vertexOutput = LoadVertexData(intersection.primitive_id, bary3, instance, mesh);

//                float3 cameraPosition = r.origin;
                vertexOutput.worldPosition = r.origin + r.direction * intersection.distance;
                MaterialParameter matData = InitializeMaterialData(vertexOutput, material);
                normalWS = float3(matData.normalWS);
                viewDir = -r.direction;
                reflectDir = reflect(-viewDir, normalWS);
                finalColor = half4(0, 0, 0, 1.0);
                // Indirect light
                {
                    half3 indirectDiffuse = DiffuseGI(renderData, normalWS).rgb * matData.diffuse;
                    half NoV = saturate(dot(normalWS, viewDir));
                    half fresnelTerm = 1.0 - NoV;
                    fresnelTerm *= fresnelTerm;
                    fresnelTerm *= fresnelTerm;
                    half3 indirectSpecular = GetEnvironmentReflectionFromSkyCube(reflectDir, matData.perceptualRoughness, skyCube, renderData.skyCubeHDRDecodeValues) * EnvironmentBRDFSpecular(matData, fresnelTerm);
                    finalColor.xyz += indirectDiffuse + indirectSpecular;
                }
                
                // Main light
                if (renderData.lightCount > 0)
                {
                    LightParameter lightParam = GetLightParameter(renderData.lights[0], vertexOutput.worldPosition);
                    lightParam.shadowAttenuation = renderData.hasMainLightShadow ? MainLightShadowUsingRaytracing(accelerationStructure, functionTable, vertexOutput.worldPosition, float3(lightParam.direction)) : 1.0;
                    finalColor.xyz += BRDFDataToLightingResult(matData, lightParam, half3(viewDir));
                }

                for (uint32_t lightIndex = 1; lightIndex < renderData.lightCount; ++lightIndex)
                {
                    LightParameter lightParam = GetLightParameter(renderData.lights[lightIndex], vertexOutput.worldPosition);
                    finalColor.xyz += BRDFDataToLightingResult(matData, lightParam, half3(viewDir));
                }
            }
            else if ( intersection.type == raytracing::intersection_type::none )
            {
                finalColor = half4( 0.0f, 0.0f, 0.0f, 0.0f );
            }
        }
        outImage.write( finalColor, tid );
    }
}
