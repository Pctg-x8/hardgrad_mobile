#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(std140, binding = 0) uniform ProjectionMatrix
{
    mat4 projection_matrix;
};
layout(std140, binding = 1) uniform ObjectMatrix
{
    mat4 object_matrix;
};

layout(location = 0) in vec3 position;
out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
	gl_Position = vec4(position - vec3(0.0f, 0.0f, 0.375f) * gl_InstanceIndex, 1.0) * object_matrix * projection_matrix;
}
