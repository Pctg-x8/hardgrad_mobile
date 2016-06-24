#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(std140, binding = 0) uniform OrthoProjectionParams
{
    mat4 ortho_projection_matrix;
};
layout(std140, binding = 1) uniform CenterTF
{
    mat4 center_tf;
};

layout(location = 0) in vec3 pos;
layout(location = 1) in vec4 child_tf_0;
layout(location = 2) in vec4 child_tf_1;
layout(location = 3) in vec4 child_tf_2;
layout(location = 4) in vec4 child_tf_3;
out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
    gl_Position = vec4(pos, 1.0f) * mat4(child_tf_0, child_tf_1, child_tf_2, child_tf_3) * center_tf * ortho_projection_matrix;
}