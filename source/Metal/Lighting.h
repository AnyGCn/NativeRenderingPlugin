#include "ShaderDefinition.h"

#include <metal_stdlib>
using namespace metal;

struct LightParameter
{
    half3  direction;
    float   distanceAttenuation; // full-float precision required on some platforms
    half3   color;
    half    shadowAttenuation;
};

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

struct Varyings
{
    float3 worldPosition;
    half3 normal;
    half3 tangent;
    half3 bitangent;
    float2 texCoord;
};

constexpr sampler linearSampler (address::repeat,
                                 mip_filter::linear,
                                 mag_filter::linear,
                                 min_filter::linear);

constexpr sampler shadowSampler (address::clamp_to_edge,
                                 mip_filter::linear,
                                 mag_filter::linear,
                                 min_filter::linear,
                                 compare_func::greater);

float3 SafeNormalize(float3 inVec)
{
    float dp3 = max(FLT_MIN, dot(inVec, inVec));
    return inVec * rsqrt(dp3);
}

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

float3 computeNormalMap(Varyings in, texture2d<float> normalMapTexture)
{
    float4 encodedNormal = normalMapTexture.sample(linearSampler, float2(in.texCoord));
    float4 normalMap = float4(normalize(encodedNormal.xyz * 2.0 - float3(1,1,1)), 0.0);
    return float3(normalize(in.normal * normalMap.z + in.tangent * normalMap.x + in.bitangent * normalMap.y));
}

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

half PerceptualRoughnessToMipmapLevel(half perceptualRoughness, uint maxMipLevel)
{
    perceptualRoughness = perceptualRoughness * (1.7 - 0.7 * perceptualRoughness);

    return perceptualRoughness * maxMipLevel;
}

half PerceptualRoughnessToMipmapLevel(half perceptualRoughness)
{
    return PerceptualRoughnessToMipmapLevel(perceptualRoughness, 6);
}

/// Initialize Material Data
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
    outMaterialData.roughness           = max(PerceptualRoughnessToRoughness(outMaterialData.perceptualRoughness), sqrt(HALF_EPSILON));
    outMaterialData.roughness2          = max(outMaterialData.roughness * outMaterialData.roughness, HALF_EPSILON);
    outMaterialData.grazingTerm         = saturate(outMaterialData.smoothness + outMaterialData.reflectivity);
    outMaterialData.normalizationTerm   = outMaterialData.roughness * 4.0 + 2.0;
    outMaterialData.roughness2MinusOne  = outMaterialData.roughness2 - 1.0;
    return outMaterialData;
}

/// Lighting Function
float DirectBRDFSpecular(MaterialParameter brdfData, half3 normalWS, half3 lightDirectionWS, half3 viewDirectionWS)
{
    float3 lightDirectionWSFloat3 = float3(lightDirectionWS);
    float3 halfDir = SafeNormalize(lightDirectionWSFloat3 + float3(viewDirectionWS));

    float NoH = saturate(dot(float3(normalWS), halfDir));
    float LoH = saturate(dot(lightDirectionWSFloat3, halfDir));

    // GGX Distribution multiplied by combined approximation of Visibility and Fresnel
    // BRDFspec = (D * V * F) / 4.0
    // D = roughness^2 / ( NoH^2 * (roughness^2 - 1) + 1 )^2
    // V * F = 1.0 / ( LoH^2 * (roughness + 0.5) )
    // See "Optimizing PBR for Mobile" from Siggraph 2015 moving mobile graphics course
    // https://community.arm.com/events/1155

    // Final BRDFspec = roughness^2 / ( NoH^2 * (roughness^2 - 1) + 1 )^2 * (LoH^2 * (roughness + 0.5) * 4.0)
    // We further optimize a few light invariant terms
    // brdfData.normalizationTerm = (roughness + 0.5) * 4.0 rewritten as roughness * 4.0 + 2.0 to a fit a MAD.
    float d = NoH * NoH * brdfData.roughness2MinusOne + 1.00001f;

    float LoH2 = LoH * LoH;
    float specularTerm = brdfData.roughness2 / ((d * d) * max(0.1, LoH2) * brdfData.normalizationTerm);

    return specularTerm;
}

half3 PBRDirectBRDFSpecular( MaterialParameter brdfData, half3 normalWS, half3 lightDirectionWS, half3 viewDirectionWS)
{
    return brdfData.specular * DirectBRDFSpecular(brdfData, normalWS, lightDirectionWS, viewDirectionWS);
}

half3 BRDFDataToLightingResult(MaterialParameter material, LightParameter light, half3 viewDirectionWS)
{
    half NdotL = saturate(dot(material.normalWS, light.direction));

    half3 radiance = light.color * (light.distanceAttenuation * light.shadowAttenuation * NdotL);
    half3 diffuse = material.diffuse * radiance;
    half3 specular = radiance * PBRDirectBRDFSpecular(material, material.normalWS, light.direction, viewDirectionWS);
    return diffuse + specular;
}

/// Light Parameter Function
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

int ComputeCascadeIndex(AAPLRenderParameter shadowData, float3 positionWS)
{
    float3 fromCenter0 = positionWS - shadowData.shadowSplitSphere0.xyz;
    float3 fromCenter1 = positionWS - shadowData.shadowSplitSphere1.xyz;
    float3 fromCenter2 = positionWS - shadowData.shadowSplitSphere2.xyz;
    float3 fromCenter3 = positionWS - shadowData.shadowSplitSphere3.xyz;
    float4 distances2 = float4(dot(fromCenter0, fromCenter0), dot(fromCenter1, fromCenter1), dot(fromCenter2, fromCenter2), dot(fromCenter3, fromCenter3));

    float4 cascadeSplitSphereRadii = float4(shadowData.shadowSplitSphere0.w, shadowData.shadowSplitSphere1.w, shadowData.shadowSplitSphere2.w, shadowData.shadowSplitSphere3.w);
    half4 weights = half4(distances2 < cascadeSplitSphereRadii * cascadeSplitSphereRadii);
    weights.yzw = saturate(weights.yzw - weights.xyz);

    return round(4.0 - dot(weights, half4(4, 3, 2, 1)));
}

#define BEYOND_SHADOW_FAR(shadowCoord) shadowCoord.z <= 0.0 || shadowCoord.z >= 1.0

half MainLightShadow(AAPLRenderParameter shadowData, depth2d<float> shadowMap, float3 positionWS)
{
    int cascadeIndex = ComputeCascadeIndex(shadowData, positionWS);
    float4 shadowCoord = shadowData.shadowMatrix[cascadeIndex] * float4(positionWS, 1.0);
    shadowCoord.xyz /= shadowCoord.w;
    half shadowStrength = shadowData.shadowParams.x;
    half attenuation = shadowMap.sample_compare(shadowSampler, shadowCoord.xy, shadowCoord.z);
    attenuation = mix(1, attenuation, shadowStrength);
    return BEYOND_SHADOW_FAR(shadowCoord) ? 1.0 : attenuation;
}

float4 TransformWorldToShadowCoord(AAPLRenderParameter shadowData, float3 positionWS)
{
    int cascadeIndex = ComputeCascadeIndex(shadowData, positionWS);
    float4 shadowCoord = shadowData.shadowMatrix[cascadeIndex] * float4(positionWS, 1.0);
    return float4(shadowCoord.xyz, 0);
}

/// Indirect Light Function
float3 SHEvalLinearL0L1(float3 N, float4 shAr, float4 shAg, float4 shAb)
{
    float4 vA = float4(N, 1.0);

    float3 x1;
    // Linear (L1) + constant (L0) polynomial terms
    x1.r = dot(shAr, vA);
    x1.g = dot(shAg, vA);
    x1.b = dot(shAb, vA);

    return x1;
}

float3 SHEvalLinearL1(float3 N, float3 shAr, float3 shAg, float3 shAb)
{
    float3 x1;
    x1.r = dot(shAr, N);
    x1.g = dot(shAg, N);
    x1.b = dot(shAb, N);

    return x1;
}

float3 SHEvalLinearL2(float3 N, float4 shBr, float4 shBg, float4 shBb, float4 shC)
{
    float3 x2;
    // 4 of the quadratic (L2) polynomials
    float4 vB = N.xyzz * N.yzzx;
    x2.r = dot(shBr, vB);
    x2.g = dot(shBg, vB);
    x2.b = dot(shBb, vB);

    // Final (5th) quadratic (L2) polynomial
    float vC = N.x * N.x - N.y * N.y;
    float3 x3 = shC.rgb * vC;

    return x2 + x3;
}

half3 SampleSH9(AAPLRenderParameter lightData, float3 N)
{
    // Linear + constant polynomial terms
    float3 res = SHEvalLinearL0L1(N, lightData.unity_SHAr, lightData.unity_SHAg, lightData.unity_SHAb);

    // Quadratic polynomials
    res += SHEvalLinearL2(N, lightData.unity_SHBr, lightData.unity_SHBg, lightData.unity_SHBb, lightData.unity_SHC);

    return half3(res);
}

half4 DiffuseGI(AAPLRenderParameter lightData, float3 normalWS)
{
    half4 diffuseGI = half4(0, 0, 0, 1);
    diffuseGI.xyz += SampleSH9(lightData, normalWS);
    diffuseGI.w = dot(diffuseGI.xyz, half3(0.299, 0.587, 0.114));
    return diffuseGI;
}

half3 DecodeHDREnvironment(half4 encodedIrradiance, half4 decodeInstructions)
{
    // Take into account texture alpha if decodeInstructions.w is true(the alpha value affects the RGB channels)
    half alpha = max(decodeInstructions.w * (encodedIrradiance.a - 1.0) + 1.0, 0.0);

    // If Linear mode is not supported we can skip exponent part
    return (decodeInstructions.x * pow(abs(alpha), decodeInstructions.y)) * encodedIrradiance.rgb;
}

half3 GetEnvironmentReflectionFromSkyCube(float3 reflectVector, half perceptualRoughness, texturecube<half> skyCube, float4 decodeValues)
{
    half mip = PerceptualRoughnessToMipmapLevel(perceptualRoughness);
    half4 encodedIrradiance = skyCube.sample(linearSampler, reflectVector, (level)mip);
    half3 irradiance = DecodeHDREnvironment(encodedIrradiance, half4(decodeValues));
    return irradiance;
}

half3 EnvironmentBRDFSpecular(MaterialParameter brdfData, half fresnelTerm)
{
    half surfaceReduction = 1.0 / (brdfData.roughness2 + 1.0);
    return half3(surfaceReduction * mix(brdfData.specular, brdfData.grazingTerm, fresnelTerm));
}
