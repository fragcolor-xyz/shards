struct LightingVectorSample {
	float pdf;
	vec3 localDirection;
};

// Theta is the polar angle
// Phi is the azimuthal angle
// Result is Z=up at theta=0
vec3 localHemisphereDirectionHelper(float cosTheta, float sinTheta, float phi) {
	return vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

float normalDistributionLambert(float nDotH) {
	return PI;
}

LightingVectorSample importanceSampleLambert(vec2 uv) {
	LightingVectorSample result;
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

LightingVectorSample importanceSampleGGX(vec2 uv, float roughness) {
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
