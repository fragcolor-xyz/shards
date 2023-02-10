// #pragma once
// #include "lib/const.glsl"
// #include "lib/env.glsl"
// #include "lib/utils.glsl"
// #include "lib/material.glsl"
#version 460
#define PI 3.1416
#define PI2 6.2832
#define INF 99999999999.0

#define EXPOSURE 1.0

// Theta is the polar angle
// Phi is the azimuthal angle
// Result is Z=up at theta=0
vec3 localHemisphereDirectionHelper(float cosTheta, float sinTheta, float phi) {
	return vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

#define saturate(x) clamp(x, 0, 1)

float max3(vec3 v) {
	return max(max(v.x, v.y), v.z);
}

struct MaterialInfo {
	vec3 baseColor;
	
	// specular color at angles 0(direct) and 90(grazing) 
	vec3 specularColor0;
	vec3 specularColor90;
	
	float perceptualRoughness; 
	float metallic;
    
    float ior;
    
    vec3 diffuseColor;
	
	float specularWeight;
};

struct LightingGeneralParams {
	vec3 surfaceNormal;
	vec3 viewDirection; // from object to view
};

struct ImportanceSample {
	float pdf;
	vec3 localDirection;
};

float normalDistributionLambert(float nDotH) {
	return PI;
}

ImportanceSample importanceSampleLambert(vec2 uv) {
	ImportanceSample result;
	float phi = uv.x * 2.0 * PI;
	float cosTheta = sqrt(1.0 - uv.y);
	float sinTheta = sqrt(uv.y);

	result.pdf = cosTheta / PI;
	result.localDirection = localHemisphereDirectionHelper(cosTheta, sinTheta, phi);
	return result;
}

float normalDistributionGGX(float nDotH, float roughness) {
	float roughnessSq = roughness * roughness;
	float f = (nDotH * nDotH) * (roughnessSq - 1.0) + 1.0;
	return roughnessSq / (PI * f * f);
}

ImportanceSample importanceSampleGGX(vec2 uv, float roughness) {
	LightingVectorSample result;
	float roughnessSq = roughness * roughness;
	float phi = 2.0 * PI * uv.x;
	float cosTheta = sqrt((1.0 - uv.y) / (1.0 + (roughnessSq * roughnessSq - 1.0) * uv.y));
	float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

	result.pdf = normalDistributionGGX(cosTheta, roughness * roughness);
	result.localDirection = localHemisphereDirectionHelper(cosTheta, sinTheta, phi);
	return result;
}

float visibilityGGX(float nDotL, float nDotV, float roughness) {
	float roughnessSq = roughness * roughness;
	float GGXV = nDotL * sqrt(nDotV * nDotV * (1.0 - roughnessSq) + roughnessSq);
	float GGXL = nDotV * sqrt(nDotL * nDotL * (1.0 - roughnessSq) + roughnessSq);
	return 0.5 / (GGXV + GGXL);
}

float normalDistributionCharlie(float sheenRoughness, float NdotH) {
	sheenRoughness = max(sheenRoughness, 0.000001); //clamp (0,1]
	float alphaG = sheenRoughness * sheenRoughness;
	float invR = 1.0 / alphaG;
	float cos2h = NdotH * NdotH;
	float sin2h = 1.0 - cos2h;
	return (2.0 + invR) * pow(sin2h, invR * 0.5) / (2.0 * PI);
}

float lambdaSheenNumericHelper(float x, float alphaG) {
	float oneMinusAlphaSq = (1.0 - alphaG) * (1.0 - alphaG);
	float a = mix(21.5473, 25.3245, oneMinusAlphaSq);
	float b = mix(3.82987, 3.32435, oneMinusAlphaSq);
	float c = mix(0.19823, 0.16801, oneMinusAlphaSq);
	float d = mix(-1.97760, -1.27393, oneMinusAlphaSq);
	float e = mix(-4.32054, -4.85967, oneMinusAlphaSq);
	return a / (1.0 + b * pow(x, c)) + d * x + e;
}

float lambdaSheen(float cosTheta, float alphaG) {
	if (abs(cosTheta) < 0.5) {
		return exp(lambdaSheenNumericHelper(cosTheta, alphaG));
	}
	else {
		return exp(2.0 * lambdaSheenNumericHelper(0.5, alphaG) - lambdaSheenNumericHelper(1.0 - cosTheta, alphaG));
	}
}

float visiblitySheen(float nDotL, float nDotV, float sheenRoughness) {
	sheenRoughness = max(sheenRoughness, 0.000001); //clamp (0,1]
	float alphaG = sheenRoughness * sheenRoughness;

	return clamp(1.0 / ((1.0 + lambdaSheen(nDotV, alphaG) + lambdaSheen(nDotL, alphaG)) * (4.0 * nDotV * nDotL)), 0.0, 1.0);
}

vec3 fresnelSchlick(vec3 f0, vec3 f90, float vDotH) {
	return f0 + (f90 - f0) * pow(clamp(1.0 - vDotH, 0.0, 1.0), 5.0);
}

vec3 getReflectionVector(vec3 viewDir, vec3 normal) {
	float proj = dot(viewDir, normal);
	return normal * proj * 2.0 - viewDir;
}

vec3 getIBLRadianceGGX(vec3 ggxSample, vec2 ggxLUT, vec3 fresnelColor, float specularWeight) {
	vec3 FssEss = fresnelColor * ggxLUT.x + ggxLUT.y;
	return specularWeight * ggxSample * FssEss;
}

vec3 getIBLRadianceLambertian(vec3 lambertianSample, vec2 ggxLUT, vec3 fresnelColor, vec3 diffuseColor, vec3 F0, float specularWeight) {
	vec3 FssEss = specularWeight * fresnelColor * ggxLUT.x + ggxLUT.y; // <--- GGX / specular light contribution (scale it down if the specularWeight is low)

	// Multiple scattering, from Fdez-Aguera
	float Ems = (1.0 - (ggxLUT.x + ggxLUT.y));
	vec3 F_avg = specularWeight * (F0 + (1.0 - F0) / 21.0);
	vec3 FmsEms = Ems * FssEss * F_avg / (1.0 - F_avg * Ems);
	vec3 k_D = diffuseColor * (1.0 - FssEss + FmsEms); // we use +FmsEms as indicated by the formula in the blog post (might be a typo in the implementation)

	return (FmsEms + k_D) * lambertianSample;
}

layout(set = 0, binding = 0) uniform texture2D envTexLambert;
layout(set = 0, binding = 1) uniform texture2D envTexGGX;
layout(set = 0, binding = 2) uniform texture2D envTexGGXLUT;
layout(set = 0, binding = 3) uniform texture2D _sampler;
vec3 computeEnvironmentLighting(in MaterialInfo material, in LightingGeneralParams params) {
	vec3 viewDir = params.viewDirection;
	vec3 reflDir = getReflectionVector(viewDir, params.surfaceNormal);

	float nDotV = dot(viewDir, params.surfaceNormal);

	material.perceptualRoughness = clamp(material.perceptualRoughness, 0, 1);
	material.metallic = clamp(material.metallic, 0, 1);

	float roughness = material.perceptualRoughness * material.perceptualRoughness;
	float reflectance = max(max(material.specularColor0.r, material.specularColor0.g), material.specularColor0.b);

	material.specularColor90 = vec3(1.0);

	// TODO
	float numMipLevels = 6.0;
	vec2 ggxLUT = vec2(0.0);// texture(envTexGGXLUT, _sampler, vec2(roughness, nDotV)).xy;
	vec3 ggxSample = vec3(0.0);// sampleEnvironmentLod(envTexGGX, reflDir, numMipLevels * material.perceptualRoughness);
	vec3 lambertianSample = vec3(0.0);//sampleEnvironmentLod(envTexLambert, params.surfaceNormal, 0);

	vec3 specularColor90 = max(vec3(1.0 - roughness), material.specularColor0);
	vec3 fresnelColor = fresnelSchlick(material.specularColor0, specularColor90, nDotV);

	vec3 specularLight = vec3(0.0);
	vec3 diffuseLight = vec3(0.0);
	
	specularLight += getIBLRadianceGGX(ggxSample, ggxLUT, fresnelColor, material.specularWeight);
	diffuseLight += getIBLRadianceLambertian(lambertianSample, ggxLUT, fresnelColor, material.diffuseColor, material.specularColor0, material.specularWeight);

	return diffuseLight + specularLight;
}


MaterialInfo getMatrialParamsDefault() {
	MaterialInfo material;
	material.baseColor = vec3(1, 1, 1);
	material.ior = 1.5;
	material.specularColor0 = vec3(0.04);
	material.specularWeight = 1.0;
	return material;
}

void materialSetIor(inout MaterialInfo info, float u_Ior) {
	info.specularColor0 = vec3(pow((u_Ior - 1.0) / (u_Ior + 1.0), 2.0));
	info.ior = u_Ior;
}

void materialSetSpecularColor(inout MaterialInfo info, vec3 color, float weight) {
	vec3 dielectricSpecularF0 = min(info.specularColor0 * color, vec3(1.0));
	info.specularColor0 = mix(dielectricSpecularF0, info.baseColor, info.metallic);
	info.specularWeight = weight;
	info.diffuseColor = mix(info.baseColor.rgb * (1.0 - max3(dielectricSpecularF0)), vec3(0), info.metallic);
}

void materialSetMetallicRoughness(inout MaterialInfo info, float u_MetallicFactor, float u_RoughnessFactor) {
	info.metallic = u_MetallicFactor;
	info.perceptualRoughness = u_RoughnessFactor;

	// Achromatic specularColor0 based on IOR.
	info.diffuseColor = mix(info.baseColor.rgb * (vec3(1.0) - info.specularColor0), vec3(0), info.metallic);
	info.specularColor0 = mix(info.specularColor0, info.baseColor.rgb, info.metallic);
}


void setSpecularGlossinessInfo(inout MaterialInfo info, vec3 u_SpecularFactor, float u_GlossinessFactor) {
    info.specularColor0 = u_SpecularFactor;
    info.perceptualRoughness = u_GlossinessFactor;

#ifdef HAS_SPECULAR_GLOSSINESS_MAP
    vec4 sgSample = texture(u_SpecularGlossinessSampler, getSpecularGlossinessUV());
    info.perceptualRoughness *= sgSample.a ; // glossiness to roughness
    info.specularColor0 *= sgSample.rgb; // specular
#endif // ! HAS_SPECULAR_GLOSSINESS_MAP

    info.perceptualRoughness = 1.0 - info.perceptualRoughness; // 1 - glossiness
    info.diffuseColor = info.baseColor.rgb * (1.0 - max(max(info.specularColor0.r, info.specularColor0.g), info.specularColor0.b));
}

void main() {
}
