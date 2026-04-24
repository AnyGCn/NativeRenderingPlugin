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
float3 UnpackOctNormal(half3 pn)
{
    float2 remappedOctNormalWS = Unpack888ToFloat2(float3(pn));
    float2 octNormalWS = remappedOctNormalWS * 2.0f - 1.0f;
    return UnpackNormalOctQuadEncode(octNormalWS);
}

struct LightParameter
{
    half3  direction;
    float   distanceAttenuation; // full-float precision required on some platforms
    half3   color;
    half    shadowAttenuation;
};

// Matches Unity Vanilla HINT_NICE_QUALITY attenuation
// Attenuation smoothly decreases to light range.
float DistanceAttenuation(float distanceSqr, half2 distanceAttenuation)
{
    // We use a shared distance attenuation for additional directional and puctual lights
    // for directional lights attenuation will be 1
    float lightAtten = 1.0f / distanceSqr;
    float2 distanceAttenuationFloat = float2(distanceAttenuation);

    // Use the smoothing factor also used in the Unity lightmapper.
    half factor = half(distanceSqr * distanceAttenuationFloat.x);
    half smoothFactor = saturate(half(1.0) - factor * factor);
    smoothFactor = smoothFactor * smoothFactor;

    return lightAtten * smoothFactor;
}

half AngleAttenuation(half3 spotDirection, half3 lightDirection, half2 spotAttenuation)
{
    // Spot Attenuation with a linear falloff can be defined as
    // (SdotL - cosOuterAngle) / (cosInnerAngle - cosOuterAngle)
    // This can be rewritten as
    // invAngleRange = 1.0 / (cosInnerAngle - cosOuterAngle)
    // SdotL * invAngleRange + (-cosOuterAngle * invAngleRange)
    // SdotL * spotAttenuation.x + spotAttenuation.y

    // If we precompute the terms in a MAD instruction
    half SdotL = dot(spotDirection, lightDirection);
    half atten = saturate(SdotL * spotAttenuation.x + spotAttenuation.y);
    return atten * atten;
}

// Fills a light struct given a perObjectLightIndex
LightParameter GetLightParameter(AAPLLightStruct lightData, float3 positionWS)
{
    // Abstraction over Light input constants
    float4 lightPositionWS = lightData.position;
    half3 color = half3(lightData.color.rgb);
    half4 distanceAndSpotAttenuation = half4(lightData.attenuation);
    half4 spotDirection = half4(lightData.direction);

    // Directional lights store direction in lightPosition.xyz and have .w set to 0.0.
    // This way the following code will work for both directional and punctual lights.
    float3 lightVector = lightPositionWS.xyz - positionWS * lightPositionWS.w;
    float distanceSqr = max(dot(lightVector, lightVector), FLT_MIN);

    half3 lightDirection = half3(lightVector * rsqrt(distanceSqr));
    // full-float precision required on some platforms
    float attenuation = DistanceAttenuation(distanceSqr, distanceAndSpotAttenuation.xy) * AngleAttenuation(spotDirection.xyz, lightDirection, distanceAndSpotAttenuation.zw);

    LightParameter light;
    light.direction = lightDirection;
    light.distanceAttenuation = attenuation;
    light.shadowAttenuation = 1.0; // This value can later be overridden in GetAdditionalLight(uint i, float3 positionWS, half4 shadowMask)
    light.color = color;
    return light;
}

typedef struct
{
    float3 worldPosition;
    half3 normal;
    half3 tangent;
    half3 bitangent;
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
    vertexOutput.normal = half4(dataArray * bary3).xyz;
    pGenerics += isHalf ? 8 : 12;

    // Tangent
    isHalf = IsTangentHalf(mesh.vertexParameters);
    dataArray = LoadVertexDataDimension4(pGenerics, i0, i1, i2, stride, isHalf);
    float4 tangentW = dataArray * bary3;
    vertexOutput.tangent = half4(tangentW).xyz;
    pGenerics += isHalf ? 8 : 16;

    // Bitangent
    float4x4 mv = instance.transform;
    half3x3 normalMx = half3x3(half3(mv.columns[0].xyz), half3(mv.columns[1].xyz), half3(mv.columns[2].xyz));
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

struct MaterialParameter
{
    half3 albedo;
    half  alpha;
    half3 normalWS;
    half  smoothness;
    half3 specular;
    half  metallic;
    half3 emission;
    half  occlusion;
    uint materialFlags;
    
    half3 diffuse;
    half reflectivity;
    half perceptualRoughness;
    half roughness;
    half roughness2;
    half grazingTerm;

    // We save some light invariant BRDF terms so we don't have to recompute
    // them in the light loop. Take a look at DirectBRDF function for detailed explaination.
    half normalizationTerm;     // roughness * 4.0 + 2.0
    half roughness2MinusOne;    // roughness^2 - 1.0
};

half3 UnpackNormalAG(half4 packedNormal, half scale = 1.0)
{
    half3 normal;
    normal.xy = packedNormal.ag * 2.0 - 1.0;
    normal.z = max(1.0e-16, sqrt(1.0 - saturate(dot(normal.xy, normal.xy))));

    // must scale after reconstruction of normal.z which also
    // mirrors UnpackNormalRGB(). This does imply normal is not returned
    // as a unit length vector but doesn't need it since it will get normalized after TBN transformation.
    // If we ever need to blend contributions with built-in shaders for URP
    // then we should consider using UnpackDerivativeNormalAG() instead like
    // HDRP does since derivatives do not use renormalization and unlike tangent space
    // normals allow you to blend, accumulate and scale contributions correctly.
    normal.xy *= scale;
    return normal;
}

half3 UnpackNormalScale(half4 packedNormal, half scale = 1.0)
{
    // Convert to (?, y, 0, x)
    packedNormal.a *= packedNormal.r;
    return UnpackNormalAG(packedNormal, scale);
}

#define kDielectricSpec half4(0.04, 0.04, 0.04, 1.0 - 0.04) // standard dielectric reflectivity coef at incident angle (= 4%)

half OneMinusReflectivityMetallic(half metallic)
{
    // We'll need oneMinusReflectivity, so
    //   1-reflectivity = 1-lerp(dielectricSpec, 1, metallic) = lerp(1-dielectricSpec, 0, metallic)
    // store (1-dielectricSpec) in kDielectricSpec.a, then
    //   1-reflectivity = lerp(alpha, 0, metallic) = alpha + metallic*(0 - alpha) =
    //                  = alpha - metallic * alpha
    half oneMinusDielectricSpec = kDielectricSpec.a;
    return oneMinusDielectricSpec - metallic * oneMinusDielectricSpec;
}

half PerceptualRoughnessToRoughness(half perceptualRoughness)
{
    return perceptualRoughness * perceptualRoughness;
}

half RoughnessToPerceptualRoughness(half roughness)
{
    return sqrt(roughness);
}

half RoughnessToPerceptualSmoothness(half roughness)
{
    return 1.0 - sqrt(roughness);
}

half PerceptualSmoothnessToRoughness(half perceptualSmoothness)
{
    return (1.0 - perceptualSmoothness) * (1.0 - perceptualSmoothness);
}

half PerceptualSmoothnessToPerceptualRoughness(half perceptualSmoothness)
{
    return (1.0 - perceptualSmoothness);
}

MaterialParameter InitializeMaterialData(Varyings in, AAPLMaterial materialData)
{
    MaterialParameter outMaterialData;
    half4 albedoAlpha = materialData.textures[AAPLTextureIndexBaseColor].sample(linearSampler, in.texCoord.xy);
    outMaterialData.alpha = albedoAlpha.a * materialData._BaseColor.a;
    outMaterialData.albedo = albedoAlpha.rgb * half3(materialData._BaseColor.rgb);
    
    half4 ARM = materialData.textures[AAPLTextureIndexMask].sample(linearSampler, in.texCoord.xy);
    outMaterialData.occlusion = ARM.r;
    outMaterialData.metallic = ARM.b * materialData._Metallic;
    outMaterialData.smoothness = 1.0f - (ARM.g * materialData._Roughness);
    outMaterialData.specular = half3(0.0, 0.0, 0.0);
    
    half3 normalTS = UnpackNormalScale(materialData.textures[AAPLTextureIndexNormal].sample(linearSampler, in.texCoord.xy), materialData._BumpScale);
    outMaterialData.emission = materialData.textures[AAPLTextureIndexNormal].sample(linearSampler, in.texCoord.xy).rgb * half3(materialData._Emission.rgb) * materialData._Emission.a;
    half3x3 tangentToWorld = half3x3(in.tangent.xyz, in.bitangent.xyz, in.normal.xyz);
    outMaterialData.normalWS = tangentToWorld * normalTS;
    half oneMinusReflectivity = OneMinusReflectivityMetallic(outMaterialData.metallic);
    half reflectivity = half(1.0) - oneMinusReflectivity;
    outMaterialData.diffuse = outMaterialData.albedo * oneMinusReflectivity;
    outMaterialData.specular = mix(kDielectricSpec.rgb, outMaterialData.albedo, outMaterialData.metallic);
    outMaterialData.reflectivity = reflectivity;
    outMaterialData.perceptualRoughness = PerceptualSmoothnessToPerceptualRoughness(outMaterialData.smoothness);
    outMaterialData.roughness           = max(PerceptualRoughnessToRoughness(outMaterialData.perceptualRoughness), sqrt(HALF_MIN));
    outMaterialData.roughness2          = max(outMaterialData.roughness * outMaterialData.roughness, HALF_MIN);
    outMaterialData.grazingTerm         = saturate(outMaterialData.smoothness + outMaterialData.reflectivity);
    outMaterialData.normalizationTerm   = outMaterialData.roughness * 4.0 + 2.0;
    outMaterialData.roughness2MinusOne  = outMaterialData.roughness2 - 1.0;
    return outMaterialData;
}

kernel void rtReflection(
             texture2d< half, access::write >      outImage                [[texture(AAPLRaytracingOutImageIndex)]],
             texture2d< float >                     depth                   [[texture(AAPLRaytracingGBufferDepthIndex)]],
             texture2d< half >                     normalMap               [[texture(AAPLRaytracingGBufferNormalIndex)]],
             constant AAPLCameraData&               cameraData              [[buffer(AAPLBufferIndexCameraData)]],
             constant AAPLLightData&                lightData               [[buffer(AAPLBufferIndexLightData)]],
             constant AAPLScene*                    pScene                  [[buffer(AAPLBufferIndexScene)]],
             instance_acceleration_structure        accelerationStructure   [[buffer(AAPLBufferIndexAccelerationStructure)]],
             uint2 tid [[thread_position_in_grid]])
{
    uint w = outImage.get_width();
    uint h = outImage.get_height();
    if ( tid.x < w && tid.y < h )
    {
        half4 finalColor = half4( 0.0, 0.0, 0.0, 1.0 );
        if (is_null_instance_acceleration_structure(accelerationStructure))
        {
            finalColor = half4( 1.0, 0.0, 1.0, 1.0 );
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
                
                constant AAPLInstance& instance = pScene->instances[ intersection.instance_id ];
                constant AAPLMesh& mesh = pScene->meshes[instance.meshIndex];
                constant AAPLMaterial& material = pScene->materials[instance.materialIndex];
                Varyings vertexOutput = LoadVertexData(intersection.primitive_id, bary3, instance, mesh);

                AAPLCameraData cd( cameraData );
                cd.cameraPosition = r.origin;
                vertexOutput.worldPosition = r.origin + r.direction * intersection.distance;
                MaterialParameter matData = InitializeMaterialData(vertexOutput, material);
                finalColor = half4(matData.albedo, 1.0);
            }
            else if ( intersection.type == raytracing::intersection_type::none )
            {
                finalColor = half4( 0.0f, 0.0f, 0.0f, 0.0f );
            }
        }
        outImage.write( finalColor, tid );
    }
}
