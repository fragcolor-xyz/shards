#pragma once

layout(std140) uniform Camera {
	mat4 view;
	mat4 invView;
	mat4 proj;
	mat4 invProj;
	vec2 dimensions;
}
camera;

vec4 cameraProject(vec4 v) {
	return camera.proj * camera.view * v;
}

vec4 cameraDeproject(vec4 v) {
	vec4 result = camera.invView * camera.invProj * v;
	return result / result.w;
}

vec3 getCameraPosition() {
	return vec3(camera.invView[3].xyz);
}

vec3 getCameraForward() {
	return vec3(-camera.invView[2].xyz);
}
