#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 tex_coord;

layout(location = 0) out vec3 frag_color;
layout(location = 1) out vec3 frag_tex_coord;

layout(binding = 0) uniform UniformBufferObject
{
	mat4 M;
	mat4 V;
	mat4 P;
} ubo;

void main ()
{
	gl_Position = ubo.P * ubo.V * ubo.M * vec4 (pos, 1.0);
	frag_color = color;
	frag_tex_coord = tex_coord;
}
