#include <bgfx_shader.sh>
#include <shaderlib.sh>
#include <lib/constants.sh>
#include <lib/env_texture.sh>
#include <lib/camera.sh>

float max3(vec3 v) {
	return max(max(v.x, v.y), v.z);
}
