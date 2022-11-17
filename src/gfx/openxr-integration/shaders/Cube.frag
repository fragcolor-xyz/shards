layout(location = 0) in vec3 color;
layout(location = 1) in vec3 position;

layout(location = 0) out vec4 outColor;

void main()
{
  outColor = vec4(color - position.y * 0.001, 1.0);
}