#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(std140, binding = 0) uniform OrthoProjectionParams
{
    mat4 ortho_projection_matrix;
};

layout(location = 0) in vec3 pos;
out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
    gl_Position = vec4(pos, 1.0f) * ortho_projection_matrix;
}