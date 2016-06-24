#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location = 0) out vec4 color_target;
layout(push_constant) uniform WireColor
{
    vec4 color;
} wire_color;

void main()
{
    color_target = wire_color.color;
}