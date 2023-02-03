struct MaterialParameters {
	float metallicFactor;
	float roughnessFactor;
	vec4 baseColorFactor;

	vec3 specularFactor;
	vec4 diffuseFactor;
	float glossinessFactor;

	float sheenRoughnessFactor;
	vec3 sheenColorFactor;

	float clearcoatFactor;
	float clearcoatRoughnessFactor;

	vec3 kHR_materials_specular_specularColorFactor;
	float kHR_materials_specular_specularFactor;

	float transmissionFactor;

	float thicknessFactor;
	vec3 attenuationColor;
	float attenuationDistance;

	float ior;

	float alphaCutoff;
};

struct MaterialInfo {
	float ior;
	float perceptualRoughness; // roughness value, as authored by the model creator (input to shader)
	vec3 f0;				   // full reflectance color (n incidence angle)

	float alphaRoughness; // roughness mapped to a more linear change in the roughness (proposed by [2])
	vec3 c_diff;

	vec3 f90; // reflectance color at grazing angle
	float metallic;

	vec3 baseColor;

	float sheenRoughnessFactor;
	vec3 sheenColorFactor;

	vec3 clearcoatF0;
	vec3 clearcoatF90;
	float clearcoatFactor;
	vec3 clearcoatNormal;
	float clearcoatRoughness;

	// KHR_materials_specular
	float specularWeight; // product of specularFactor and specularTexture.a

	float transmissionFactor;

	float thickness;
	vec3 attenuationColor;
	float attenuationDistance;
};

struct NormalInfo {
    vec3 ng;   // Geometric normal
    vec3 n;    // Pertubed normal
    vec3 t;    // Pertubed tangent
    vec3 b;    // Pertubed bitangent
};

NormalInfo getNormalInfo(vec3 v) {
	vec2 UV = getNormalUV();
	vec3 uv_dx = dFdx(vec3(UV, 0.0));
	vec3 uv_dy = dFdy(vec3(UV, 0.0));

	vec3 t_ = (uv_dy.t * dFdx(v_Position) - uv_dx.t * dFdy(v_Position)) /
			  (uv_dx.s * uv_dy.t - uv_dy.s * uv_dx.t);

	vec3 n, t, b, ng;

	// Compute geometrical TBN:
#ifdef HAS_NORMAL_VEC3
#ifdef HAS_TANGENT_VEC4
	// Trivial TBN computation, present as vertex attribute.
	// Normalize eigenvectors as matrix is linearly interpolated.
	t = normalize(v_TBN[0]);
	b = normalize(v_TBN[1]);
	ng = normalize(v_TBN[2]);
#else
	// Normals are either present as vertex attributes or approximated.
	ng = normalize(v_Normal);
#endif
#else
	ng = normalize(cross(dFdx(v_Position), dFdy(v_Position)));
#endif
	t = normalize(t_ - ng * dot(ng, t_));
	b = cross(ng, t);

	// For a back-facing surface, the tangential basis vectors are negated.
	if (gl_FrontFacing == false) {
		t *= -1.0;
		b *= -1.0;
		ng *= -1.0;
	}

	// Compute pertubed normals:
#ifdef HAS_NORMAL_MAP
	n = texture(material.normalSampler, UV).rgb * 2.0 - vec3(1.0);
	n *= vec3(material.normalScale, material.normalScale, 1.0);
	n = mat3(t, b, ng) * normalize(n);
#else
	n = ng;
#endif

	NormalInfo info;
	info.ng = ng;
	info.t = t;
	info.b = b;
	info.n = n;
	return info;
}

#ifdef MATERIAL_CLEARCOAT
vec3 getClearcoatNormal(NormalInfo normalInfo) {
#ifdef HAS_CLEARCOAT_NORMAL_MAP
	vec3 n = texture(material.clearcoatNormalSampler, getClearcoatNormalUV()).rgb * 2.0 - vec3(1.0);
	n *= vec3(material.clearcoatNormalScale, material.clearcoatNormalScale, 1.0);
	n = mat3(normalInfo.t, normalInfo.b, normalInfo.ng) * normalize(n);
	return n;
#else
	return normalInfo.ng;
#endif
}
#endif

vec4 getBaseColor() {
	vec4 baseColor = vec4(1);

#if defined(MATERIAL_SPECULARGLOSSINESS)
	baseColor = material.diffuseFactor;
#elif defined(MATERIAL_METALLICROUGHNESS)
	baseColor = material.baseColorFactor;
#endif

#if defined(MATERIAL_SPECULARGLOSSINESS) && defined(HAS_DIFFUSE_MAP)
	baseColor *= texture(material.diffuseSampler, getDiffuseUV());
#elif defined(MATERIAL_METALLICROUGHNESS) && defined(HAS_BASE_COLOR_MAP)
	baseColor *= texture(material.baseColorSampler, getBaseColorUV());
#endif

	return baseColor * getVertexColor();
}

#ifdef MATERIAL_SPECULARGLOSSINESS
MaterialInfo getSpecularGlossinessInfo(MaterialInfo info) {
	info.f0 = material.specularFactor;
	info.perceptualRoughness = material.glossinessFactor;

#ifdef HAS_SPECULAR_GLOSSINESS_MAP
	vec4 sgSample = texture(material.specularGlossinessSampler, getSpecularGlossinessUV());
	info.perceptualRoughness *= sgSample.a; // glossiness to roughness
	info.f0 *= sgSample.rgb;				// specular
#endif										// ! HAS_SPECULAR_GLOSSINESS_MAP

	info.perceptualRoughness = 1.0 - info.perceptualRoughness; // 1 - glossiness
	info.c_diff = info.baseColor.rgb * (1.0 - max(max(info.f0.r, info.f0.g), info.f0.b));
	return info;
}
#endif

#ifdef MATERIAL_METALLICROUGHNESS
MaterialInfo getMetallicRoughnessInfo(MaterialInfo info) {
	info.metallic = material.metallicFactor;
	info.perceptualRoughness = material.roughnessFactor;

#ifdef HAS_METALLIC_ROUGHNESS_MAP
	// Roughness is stored in the 'g' channel, metallic is stored in the 'b' channel.
	// This layout intentionally reserves the 'r' channel for (optional) occlusion map data
	vec4 mrSample = texture(material.metallicRoughnessSampler, getMetallicRoughnessUV());
	info.perceptualRoughness *= mrSample.g;
	info.metallic *= mrSample.b;
#endif

	// Achromatic f0 based on IOR.
	info.c_diff = mix(info.baseColor.rgb * (vec3(1.0) - info.f0), vec3(0), info.metallic);
	info.f0 = mix(info.f0, info.baseColor.rgb, info.metallic);
	return info;
}
#endif

#ifdef MATERIAL_SHEEN
MaterialInfo getSheenInfo(MaterialInfo info) {
	info.sheenColorFactor = material.sheenColorFactor;
	info.sheenRoughnessFactor = material.sheenRoughnessFactor;

#ifdef HAS_SHEEN_COLOR_MAP
	vec4 sheenColorSample = texture(material.sheenColorSampler, getSheenColorUV());
	info.sheenColorFactor *= sheenColorSample.rgb;
#endif

#ifdef HAS_SHEEN_ROUGHNESS_MAP
	vec4 sheenRoughnessSample = texture(material.sheenRoughnessSampler, getSheenRoughnessUV());
	info.sheenRoughnessFactor *= sheenRoughnessSample.a;
#endif
	return info;
}
#endif

#ifdef MATERIAL_SPECULAR
MaterialInfo getSpecularInfo(MaterialInfo info) {
	vec4 specularTexture = vec4(1.0);
#ifdef HAS_SPECULAR_MAP
	specularTexture.a = texture(material.specularSampler, getSpecularUV()).a;
#endif
#ifdef HAS_SPECULAR_COLOR_MAP
	specularTexture.rgb = texture(material.specularColorSampler, getSpecularColorUV()).rgb;
#endif

	vec3 dielectricSpecularF0 = min(info.f0 * material.kHR_materials_specular_specularColorFactor * specularTexture.rgb, vec3(1.0));
	info.f0 = mix(dielectricSpecularF0, info.baseColor.rgb, info.metallic);
	info.specularWeight = material.kHR_materials_specular_specularFactor * specularTexture.a;
	info.c_diff = mix(info.baseColor.rgb * (1.0 - max3(dielectricSpecularF0)), vec3(0), info.metallic);
	return info;
}
#endif

#ifdef MATERIAL_TRANSMISSION
MaterialInfo getTransmissionInfo(MaterialInfo info) {
	info.transmissionFactor = material.transmissionFactor;

#ifdef HAS_TRANSMISSION_MAP
	vec4 transmissionSample = texture(material.transmissionSampler, getTransmissionUV());
	info.transmissionFactor *= transmissionSample.r;
#endif
	return info;
}
#endif

#ifdef MATERIAL_VOLUME
MaterialInfo getVolumeInfo(MaterialInfo info) {
	info.thickness = material.thicknessFactor;
	info.attenuationColor = material.attenuationColor;
	info.attenuationDistance = material.attenuationDistance;

#ifdef HAS_THICKNESS_MAP
	vec4 thicknessSample = texture(material.thicknessSampler, getThicknessUV());
	info.thickness *= thicknessSample.g;
#endif
	return info;
}
#endif

#ifdef MATERIAL_CLEARCOAT
MaterialInfo getClearCoatInfo(MaterialInfo info, NormalInfo normalInfo) {
	info.clearcoatFactor = material.clearcoatFactor;
	info.clearcoatRoughness = material.clearcoatRoughnessFactor;
	info.clearcoatF0 = vec3(info.f0);
	info.clearcoatF90 = vec3(1.0);

#ifdef HAS_CLEARCOAT_MAP
	vec4 clearcoatSample = texture(material.clearcoatSampler, getClearcoatUV());
	info.clearcoatFactor *= clearcoatSample.r;
#endif

#ifdef HAS_CLEARCOAT_ROUGHNESS_MAP
	vec4 clearcoatSampleRoughness = texture(material.clearcoatRoughnessSampler, getClearcoatRoughnessUV());
	info.clearcoatRoughness *= clearcoatSampleRoughness.g;
#endif

	info.clearcoatNormal = getClearcoatNormal(normalInfo);
	info.clearcoatRoughness = clamp(info.clearcoatRoughness, 0.0, 1.0);
	return info;
}
#endif

#ifdef MATERIAL_IOR
MaterialInfo getIorInfo(MaterialInfo info) {
	info.f0 = vec3(pow((material.ior - 1.0) / (material.ior + 1.0), 2.0));
	info.ior = material.ior;
	return info;
}
#endif

float albedoSheenScalingLUT(float NdotV, float sheenRoughnessFactor) {
	return texture(material.sheenELUT, vec2(NdotV, sheenRoughnessFactor)).r;
}
