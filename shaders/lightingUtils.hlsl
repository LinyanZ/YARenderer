#ifndef __LIGHTING_UTILS_HLSL__
#define __LIGHTING_UTILS_HLSL__

#include "constants.hlsl"
#include "utils.hlsl"
#include "structs.hlsl"

float3 DoDirectLighting(Light light, float3 albedo, float3 normal, float metalness, float roughness, float3 L, float3 V)
{
    // Angle between surface normal and outgoing light direction.
    float NdotV = max(0.0, dot(normal, V));
    
    // Specular reflection vector.
    float3 Lr = 2.0 * NdotV * normal - V;

	// Fresnel reflectance at normal incidence (for metals use albedo color).
    float3 F0 = lerp(Fdielectric, albedo, metalness);
    
    // Half-vector between L and V.
    float3 H = normalize(L + V);

    // Calculate angles between surface normal and various light vectors.
    float NdotL = max(0.0, dot(normal, L));
    float NdotH = max(0.0, dot(normal, H));

    // Calculate Fresnel term for direct lighting. 
    float3 F = FresnelSchlick(F0, max(0.0, dot(H, V)));
    // Calculate normal distribution for specular BRDF.
    float D = NDFGGX(NdotH, roughness);
    // Calculate geometric attenuation for specular BRDF.
    float G = GASchlickGGX(NdotL, NdotV, roughness);

    // Diffuse scattering happens due to light being refracted multiple times by a dielectric medium.
    // Metals on the other hand either reflect or absorb energy, so diffuse contribution is always zero.
    // To be energy conserving we must scale diffuse BRDF contribution based on Fresnel factor & metalness.
    float3 kd = lerp(float3(1, 1, 1) - F, float3(0, 0, 0), metalness);

    // Lambert diffuse BRDF.
    // We don't scale by 1/PI for lighting & material units to be more convenient.
    // See: https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
    float3 diffuseBRDF = kd * albedo;

    // Cook-Torrance specular microfacet BRDF.
    float3 specularBRDF = (F * D * G) / max(Epsilon, 4.0 * NdotL * NdotV);
    
    return (diffuseBRDF + specularBRDF) * light.Color.xyz * NdotL;
}

float3 DoDirectionalLight(Light light, float3 albedo, float3 normal, float metalness, float roughness, float3 PositionV)
{
    // Everything is in view space.
    float4 eyePos = { 0, 0, 0, 1 };
    
    float3 V = normalize(eyePos.xyz - PositionV);
    float3 L = normalize(-light.DirectionVS.xyz);
    
    return DoDirectLighting(light, albedo, normal, metalness, roughness, L, V) * light.Intensity;
}

float DoAttenuation(Light light, float distance)
{
    return 1.0f - smoothstep(light.Range * 0.75f, light.Range, distance);
}

float3 DoPointLight(Light light, float3 albedo, float3 normal, float metalness, float roughness, float3 PositionV)
{
    // Everything is in view space.
    float4 eyePos = { 0, 0, 0, 1 };
    
    float3 V = normalize(eyePos.xyz - PositionV);
    
    float3 L = light.PositionVS.xyz - PositionV;
    float distance = length(L);
    L /= distance;
    
    float attenuation = DoAttenuation(light, distance);
    
    return DoDirectLighting(light, albedo, normal, metalness, roughness, L, V) * attenuation * light.Intensity;
}

float DoSpotCone(Light light, float3 L)
{
    // If the cosine angle of the light's direction 
    // vector and the vector from the light source to the point being 
    // shaded is less than minCos, then the spotlight contribution will be 0.
    float minCos = cos(radians(light.SpotlightAngle));
    // If the cosine angle of the light's direction vector
    // and the vector from the light source to the point being shaded
    // is greater than maxCos, then the spotlight contribution will be 1.
    float maxCos = lerp(minCos, 1, 0.5f);
    float cosAngle = dot(light.DirectionVS.xyz, -L);
    // Blend between the minimum and maximum cosine angles.
    return smoothstep(minCos, maxCos, cosAngle);
}

float3 DoSpotLight(Light light, float3 albedo, float3 normal, float metalness, float roughness, float3 PositionV)
{
    // Everything is in view space.
    float4 eyePos = { 0, 0, 0, 1 };
    
    float3 V = normalize(eyePos.xyz - PositionV);
    
    float3 L = light.PositionVS.xyz - PositionV;
    float distance = length(L);
    L /= distance;
    
    float attenuation = DoAttenuation(light, distance);
    float spotIntensity = DoSpotCone(light, L);
    
    return DoDirectLighting(light, albedo, normal, metalness, roughness, L, V) * attenuation * spotIntensity * light.Intensity;
}

#endif // __LIGHTING_UTILS_HLSL__