#version 460

layout (set = 1, binding = 0) uniform u
{
  mat4 transform;
};

layout(location = 0) out vec2 uv;

vec2 positions[4] = vec2[](
  vec2(-1.0,  1.0),  // Bottom left
  vec2(-1.0, -1.0),  // Top left
  vec2( 1.0,  1.0),  // Bottom right
  vec2( 1.0, -1.0)   // Top right
);

vec2 texCoords[4] = vec2[](
  vec2(0.0, 1.0),  // Bottom left
  vec2(0.0, 0.0),  // Top left
  vec2(1.0, 1.0),  // Bottom right
  vec2(1.0, 0.0)   // Top right
);

void main()
{
  gl_Position = transform * vec4(positions[gl_VertexIndex], 0.0, 1.0);
  uv = texCoords[gl_VertexIndex];
}
