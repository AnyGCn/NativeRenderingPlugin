#include "ShaderDefinition.h"

#include <metal_stdlib>
#include <simd/simd.h>

// Include the header that this Metal shader code shares with the Swift/C code that executes Metal API commands.
using namespace metal;
using raytracing::instance_acceleration_structure;

// Unpack 2 floats (12-bit each) packed into RGB888
float2 Unpack888UIntToFloat2(uint3 x)
{
    // 8 bits in lo, 4 bits in hi
    uint hi = x.z >> 4;
    uint lo = x.z & 15;
    uint2 cb = x.xy | uint2(lo << 8, hi << 8);
    return float2(cb) / 4095.0f;
}

// Unpack 2 floats (12-bit each) from normalized RGB888
float2 Unpack888ToFloat2(float3 x)
{
    // +0.5 to mitigate precision issues on some GPUs
    uint3 i = uint3(x * 255.5f);
    return Unpack888UIntToFloat2(i);
}

// Octahedral decode back to a unit vector
float3 UnpackNormalOctQuadEncode(float2 f)
{
    // NOTE: Do NOT use abs() in this line. It causes miscompilations. (UUM-62216, UUM-70600)
    float3 n = float3(f.x, f.y, 1.0f - (f.x < 0 ? -f.x : f.x) - (f.y < 0 ? -f.y : f.y));

    float t = max(-n.z, 0.0f);
    n.xy += float2(n.x >= 0.0f ? -t : t, n.y >= 0.0f ? -t : t);

    return normalize(n);
}

// HLSL-style UnpackOctNormal translated to Metal
float3 UnpackOctNormal(float3 pn)
{
    float2 remappedOctNormalWS = Unpack888ToFloat2(pn);
    float2 octNormalWS = remappedOctNormalWS * 2.0f - 1.0f;
    return UnpackNormalOctQuadEncode(octNormalWS);
}

typedef struct
{
    float3 worldPosition;
    float3 normal;
    float3 tangent;
    float3 bitangent;
    float2 texCoord;
} Varyings;

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

Varyings LoadVertexData(uint primitive_id, float3 bary3, Instance instance, Mesh mesh)
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

    // Position
    uint32_t stride = GetPositionStride(mesh.vertexParameters);
    bool isHalf = IsPositionHalf(mesh.vertexParameters);
    float3x4 dataArray = LoadVertexDataDimension3(mesh.positions, i0, i1, i2, stride, isHalf);
    vertexOutput.worldPosition = (instance.transform * (dataArray * bary3)).xyz;

    // Generics
    constant uint8_t* pGenerics = mesh.generics + GetGenericOffset(mesh.vertexParameters);

    // Normal
    stride = GetGenericStride(mesh.vertexParameters);
    isHalf = IsNormalHalf(mesh.vertexParameters);
    dataArray = LoadVertexDataDimension3(pGenerics, i0, i1, i2, stride, isHalf);
    vertexOutput.normal = (dataArray * bary3).xyz;
    pGenerics += isHalf ? 8 : 12;

    // Tangent
    isHalf = IsTangentHalf(mesh.vertexParameters);
    dataArray = LoadVertexDataDimension4(pGenerics, i0, i1, i2, stride, isHalf);
    float4 tangentW = dataArray * bary3;
    vertexOutput.tangent = tangentW.xyz;
    pGenerics += isHalf ? 8 : 16;

    // Bitangent
    float4x4 mv = instance.transform;
    float3x3 normalMx = float3x3(mv.columns[0].xyz, mv.columns[1].xyz, mv.columns[2].xyz);
    vertexOutput.normal = normalize(normalMx * vertexOutput.normal);
    vertexOutput.tangent = normalize(normalMx * vertexOutput.tangent);
    vertexOutput.bitangent = cross(vertexOutput.normal, vertexOutput.tangent) * sign(tangentW.w);

    // Color
    pGenerics += IsColorExists(mesh.vertexParameters) ? 4 : 0;

    // Texture coordinates (maybe memory access out of bounds)
    isHalf = IsUVHalf(mesh.vertexParameters);
    dataArray = LoadVertexDataDimension2(pGenerics, i0, i1, i2, stride, isHalf);
    vertexOutput.texCoord = (dataArray * bary3).xy;

    return vertexOutput;
}

struct LightingParameters
{
    float3  lightDir;
    float3  viewDir;
    float3  halfVector;
    float3  reflectedVector;
    float3  normal;
    float3  reflectedColor;
    float3  irradiatedColor;
    float4  baseColor;
    float   nDoth;
    float   nDotv;
    float   nDotl;
    float   hDotl;
    float   metalness;
    float   roughness;
    float   ambientOcclusion;
};

constexpr sampler linearSampler (address::repeat,
                                 mip_filter::linear,
                                 mag_filter::linear,
                                 min_filter::linear);

float3 computeNormalMap(Varyings in, texture2d<float> normalMapTexture)
{
    float4 encodedNormal = normalMapTexture.sample(linearSampler, float2(in.texCoord));
    float4 normalMap = float4(normalize(encodedNormal.xyz * 2.0 - float3(1,1,1)), 0.0);
    return float3(normalize(in.normal * normalMap.z + in.tangent * normalMap.x + in.bitangent * normalMap.y));
}

LightingParameters calculateParameters(Varyings in,
                                       AAPLCameraData cameraData,
                                       constant AAPLLightData& lightData,
                                       texture2d<float>   baseColorMap,
                                       texture2d<float>   normalMap,
                                       texture2d<float>   maskMap)
{
    LightingParameters parameters;

    parameters.baseColor = baseColorMap.sample(linearSampler, in.texCoord.xy);

    parameters.normal = computeNormalMap(in, normalMap);

    parameters.viewDir = normalize(cameraData.cameraPosition - float3(in.worldPosition));

    float4 maskValue = maskMap.sample(linearSampler, in.texCoord.xy);
    parameters.roughness = max(maskValue.y, 0.001f) * 0.8;

    parameters.metalness = max(maskValue.z, 0.1);

    parameters.ambientOcclusion = maskValue.x;

    parameters.reflectedVector = reflect(-parameters.viewDir, parameters.normal);
    
//    constexpr sampler linearFilterSampler(coord::normalized, address::clamp_to_edge, filter::linear);
//    float3 c = equirectangularSample(parameters.reflectedVector, linearFilterSampler, skydomeMap).rgb;
//    parameters.irradiatedColor = clamp(c, 0.f, kMaxHDRValue);

    parameters.lightDir = lightData.directionalLightInvDirection;
    parameters.nDotl = max(0.001f,saturate(dot(parameters.normal, parameters.lightDir)));

    parameters.halfVector = normalize(parameters.lightDir + parameters.viewDir);
    parameters.nDoth = max(0.001f,saturate(dot(parameters.normal, parameters.halfVector)));
    parameters.nDotv = max(0.001f,saturate(dot(parameters.normal, parameters.viewDir)));
    parameters.hDotl = max(0.001f,saturate(dot(parameters.lightDir, parameters.halfVector)));

    return parameters;
}

kernel void rtReflection(
             texture2d< float, access::write >      outImage                [[texture(OutImageIndex)]],
             texture2d< float >                     depth                   [[texture(GBufferDepthIndex)]],
             texture2d< float >                     normalMap               [[texture(GBufferNormalIndex)]],
             constant AAPLCameraData&               cameraData              [[buffer(AAPLBufferIndexCameraData)]],
             constant AAPLLightData&                lightData               [[buffer(AAPLBufferIndexLightData)]],
             constant Scene*                        pScene                  [[buffer(AAPLBufferIndexScene)]],
             instance_acceleration_structure        accelerationStructure   [[buffer(AAPLBufferIndexAccelerationStructure)]],
             uint2 tid [[thread_position_in_grid]])
{
    uint w = outImage.get_width();
    uint h = outImage.get_height();
    if ( tid.x < w && tid.y < h )
    {
        float4 finalColor = float4( 0.0, 0.0, 0.0, 1.0 );
        if (is_null_instance_acceleration_structure(accelerationStructure))
        {
            finalColor = float4( 1.0, 0.0, 1.0, 1.0 );
        }
        else
        {
            // Reconstruct world-space position from depth
            float depth01 = depth.read(tid).x;                     // depth in [0,1] clip space
            float2 uv = (float2(tid) + 0.5f) / float2(w, h);       // pixel center -> [0,1]
            float ndcZ = depth01;                                  // to NDC z
            float4 clipPos = float4(uv * 2.0f - 1.0f, ndcZ, 1.0f); // NDC xy,z
            clipPos.y = -clipPos.y;
            float4 worldPosH = cameraData.MatrixVP_Inv * clipPos;  // homogeneous world
            float3 worldPos = worldPosH.xyz / worldPosH.w;

            // Decode oct-encoded normal from the normal map
            float3 normalWS = UnpackOctNormal(normalMap.read(tid).xyz);
            float3 viewDir = normalize(cameraData.cameraPosition - worldPos);
            float3 reflectDir = reflect(-viewDir, normalWS);

            raytracing::ray r;
            r.origin = worldPos;
            r.direction = reflectDir;
            r.min_distance = 0.1;
            r.max_distance = FLT_MAX;

            raytracing::intersector<raytracing::instancing, raytracing::triangle_data> inter;
            inter.assume_geometry_type( raytracing::geometry_type::triangle );
            auto intersection = inter.intersect( r, accelerationStructure, 0xFF );
            if ( intersection.type == raytracing::intersection_type::triangle )
            {
                float2 bary2 = intersection.triangle_barycentric_coord;
                float3 bary3 = float3( 1.0 - (bary2.x + bary2.y), bary2.x, bary2.y );
                
                constant Instance& instance = pScene->instances[ intersection.instance_id ];
                constant Mesh& mesh = pScene->meshes[instance.meshIndex];
                constant Material& material = pScene->materials[instance.materialIndex];
                Varyings vertexOutput = LoadVertexData(intersection.primitive_id, bary3, instance, mesh);

                AAPLCameraData cd( cameraData );
                cd.cameraPosition = r.origin;
                vertexOutput.worldPosition = r.origin + r.direction * intersection.distance;
                
                LightingParameters params = calculateParameters(vertexOutput,
                                                                cd,
                                                                lightData,
                                                                material.textures[AAPLTextureIndexBaseColor],
                                                                material.textures[AAPLTextureIndexNormal],
                                                                material.textures[AAPLTextureIndexMask]);
                
                finalColor = params.baseColor;
            }
            else if ( intersection.type == raytracing::intersection_type::none )
            {
                finalColor = float4( 0.0f, 0.0f, 0.0f, 1.0f );
            }
        }
        outImage.write( finalColor, tid );
    }
}
