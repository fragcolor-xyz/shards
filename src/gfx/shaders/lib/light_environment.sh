#if 0
struct LightingGeneralParams {
	vec3 surfaceNormal;
	vec3 viewDirection; // from object to view
};

vec3 getIBLRadianceLambertian(vec3 lambertianSample, vec2 ggxLUT, vec3 fresnelColor, vec3 diffuseColor, vec3 F0, float specularWeight) {
	vec3 FssEss = specularWeight * fresnelColor * ggxLUT.x + ggxLUT.y; // <--- GGX / specular light contribution (scale it down if the specularWeight is low)

	// Multiple scattering, from Fdez-Aguera
	float Ems = (1.0 - (ggxLUT.x + ggxLUT.y));
	vec3 F_avg = specularWeight * (F0 + (1.0 - F0) / 21.0);
	vec3 FmsEms = Ems * FssEss * F_avg / (1.0 - F_avg * Ems);
	vec3 k_D = diffuseColor * (1.0 - FssEss + FmsEms); // we use +FmsEms as indicated by the formula in the blog post (might be a typo in the implementation)

	return (FmsEms + k_D) * lambertianSample;
}

vec3 computeEnvironmentLighting(in MaterialInfo material, in LightingGeneralParams params, in sampler2D envTexLambert, in sampler2D envTexGGX, in sampler2D envTexGGXLUT) {
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
	vec2 ggxLUT = texture(envTexGGXLUT, vec2(roughness, nDotV)).xy;
	vec3 ggxSample = sampleEnvironmentLod(envTexGGX, reflDir, numMipLevels * material.perceptualRoughness);
	vec3 lambertianSample = sampleEnvironmentLod(envTexLambert, params.surfaceNormal, 0);

	vec3 specularColor90 = max(vec3(1.0 - roughness), material.specularColor0);
	vec3 fresnelColor = fresnelSchlick(material.specularColor0, specularColor90, nDotV);

	vec3 specularLight = vec3(0.0);
	vec3 diffuseLight = vec3(0.0);

	specularLight += getIBLRadianceGGX(ggxSample, ggxLUT, fresnelColor, material.specularWeight);
	diffuseLight += getIBLRadianceLambertian(lambertianSample, ggxLUT, fresnelColor, material.diffuseColor, material.specularColor0, material.specularWeight);

	return diffuseLight + specularLight;
}
#endif

vec3 environmentLight(vec3 worldPosition, vec3 viewDirection, vec3 normal) {
	return normal;
}
