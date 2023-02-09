#define PI 3.1416
#define PI2 6.2832
#define INF 99999999999.0

#define EXPOSURE 1.0

#define MONTECARLO_NUM_SAMPLES 1024

// Hammersley Points on the Hemisphere
// CC BY 3.0 (Holger Dammertz)
// http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
// with adapted interface
float radicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

// hammersley2d describes a sequence of points in the 2d unit square [0,1)^2
// that can be used for quasi Monte Carlo integration
vec2 hammersley2d(int i, int N) {
    return vec2(float(i)/float(N), radicalInverse_VdC(uint(i)));
}


// Conversion to spherical coordinates with r=1
vec2 directionToLongLat(vec3 dir) {
	vec2 result;
	if (dir.x >= 0) {
		result.x = atan(dir.z / dir.x);
	}
	else {
		result.x = atan(dir.z / dir.x) + PI;
	}
	result.y = acos(dir.y);
	return result;
}

// Conversion from long-lat uv to direction vector
vec3 uvToDirection(vec2 uv) {
	float theta = uv.x * PI2; // azimuthal angle
	float phi = uv.y * PI; // polar angle
	
	vec3 result;
	result.x = cos(theta) * sin(phi);
	result.z = sin(theta) * sin(phi);
	result.y = cos(phi);
	return result;
}

vec3 sampleEnvironmentLod(in texture2D inTexture, vec3 dir, float lod) {
	vec2 polar = directionToLongLat(dir);
	vec2 uv = vec2(
		(polar.x / PI2), // azimuthal angle
		(polar.y / PI) // polar angle 
	);
	return vec3(0.0);
}

vec3 sampleEnvironment(in texture2D inTexture, vec3 dir) {
	return sampleEnvironmentLod(inTexture, dir, 8.0);
}



struct IntegrateInput {
	vec3 baseDirection; // Direction which is being sampled
	vec2 coord;			// Uniform coord in the unit rectangle
    vec2 uvCoord; // raw uv coordinate
};

struct IntegrateOutput {
	vec3 localDirection;
	float pdf;
	float sampleWeight;
    float sampleScale;
};

layout(binding=0) uniform FilterParameters {
	vec2 inputDimensions;
	vec2 outputDimensions;
}
filterParameters;

float getWeightedLod(float pdf) {
	// https://cgg.mff.cuni.cz/~jaroslav/papers/2007-sketch-fis/Final_sap_0073.pdf
	vec2 inputDims = filterParameters.inputDimensions;
	return 0.5 * log2(6.0 * float(inputDims.x) * float(inputDims.x) / (float(MONTECARLO_NUM_SAMPLES) * pdf));
}

mat3 generateFrameFromZDirection(vec3 normal) {
	vec3 bitangent = vec3(0.0, 1.0, 0.0);

	float NdotUp = dot(normal, vec3(0.0, 1.0, 0.0));
	float epsilon = 0.00001;
	if (1.0 - abs(NdotUp) <= epsilon) {
		// Sampling +Y or -Y, so we need a more robust bitangent.
		if (NdotUp > 0.0) {
			bitangent = vec3(0.0, 0.0, 1.0);
		}
		else {
			bitangent = vec3(0.0, 0.0, -1.0);
		}
	}

	vec3 tangent = normalize(cross(bitangent, normal));
	bitangent = cross(normal, tangent);

	return mat3(tangent, bitangent, normal);
}

IntegrateOutput MONTECARLO_FUNCTION(in IntegrateInput input) {
	IntegrateOutput empty;
	return empty;
}

vec3 montecarlo(in vec2 inputUV, in texture2D inputTexture) {
	vec3 baseDir = vec3(0, 1, 2);
	mat3 tbn = generateFrameFromZDirection(baseDir);

	float weight = 0.0;
	vec3 result = vec3(0, 0, 0);
	for (int sampleIndex = 0; sampleIndex < MONTECARLO_NUM_SAMPLES; sampleIndex++) {
		IntegrateInput mci;
		mci.baseDirection = baseDir;
		mci.coord = hammersley2d(sampleIndex, MONTECARLO_NUM_SAMPLES);
		mci.uvCoord = inputUV;

		IntegrateOutput mco = MONTECARLO_FUNCTION(mci);

		float lod = getWeightedLod(mco.pdf);

		vec3 direction = tbn * mco.localDirection;
		vec3 s = sampleEnvironmentLod(inputTexture, direction, lod);
		result += s * mco.sampleScale;
		weight += mco.sampleWeight;
	}
	result /= weight;

	return result;
}

void main() {
}