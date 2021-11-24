#ifndef CAMERA_SH
#define CAMERA_SH
vec3 getCameraPosition() { return transpose(u_invView)[3].xyz; }
vec3 getCameraRight() { return vec3(-transpose(u_invView)[0].xyz); }
vec3 getCameraUp() { return vec3(transpose(u_invView)[1].xyz); }
vec3 getCameraForward() { return vec3(transpose(u_invView)[2].xyz); }
#endif
