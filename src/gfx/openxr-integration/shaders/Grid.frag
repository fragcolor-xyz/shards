layout(location = 0) in vec3 color;
layout(location = 1) in vec3 position;

layout(location = 0) out vec4 outColor;

void main()
{
  const float lineThickness = 0.015;
  float x = mod(position.x, 1.0);
  float z = mod(position.z, 1.0);
  if (x < lineThickness || x > 1.0 - lineThickness || z < lineThickness || z > 1.0 - lineThickness)
  {
    outColor = vec4(color, 1.0);
  }
  else
  {
    outColor = vec4(0.0);
  }
}