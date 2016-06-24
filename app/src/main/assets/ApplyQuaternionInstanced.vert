#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(std140, binding = 0) uniform OrthoProjectionParams
{
    mat4 ortho_projection_matrix;
};
layout(std140, binding = 1) uniform CenterTransform
{
    mat4 center_tf;
};

layout(location = 0) in vec3 pos;
layout(location = 1) in vec4 rot_quat;
out gl_PerVertex
{
    vec4 gl_Position;
};

vec4 qConjugate(vec4 iq) { return vec4(-iq.xyz, iq.w); }
vec4 qMult(vec4 q1, vec4 q2) { return vec4(cross(q1.xyz, q2.xyz) + q2.w * q1.xyz + q1.w * q2.xyz, q1.w * q2.w - dot(q1.xyz, q2.xyz)); }
vec4 qRot(vec3 in_vec, vec4 rq)
{
    vec4 q1 = rq;
    vec4 qp = vec4(in_vec, 0.0f);
    vec4 q2 = qConjugate(rq);
    vec4 qt = qMult(q1, qp);
    return qMult(qt, q2);
}

void main()
{
    gl_Position = vec4(qRot(pos, rot_quat).xyz, 1.0f) * center_tf * ortho_projection_matrix;
}