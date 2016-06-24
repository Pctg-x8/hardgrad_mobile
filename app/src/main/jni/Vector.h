//
// Created by S on 2016/06/23.
//

#ifndef HARDGRAD_MOBILE_VECTOR_H
#define HARDGRAD_MOBILE_VECTOR_H

// Represents 4 vector of float(or the quaternion)
struct Vector4
{
	float x, y, z, w;
};
// Represents 4x4 matrix of float
struct Matrix4
{
	float m[16];
};

// Manipulation Functions
void Vector4_UnitQuaternion(Vector4& self, float x, float y, float z, float th_deg);
void Vector4_ZRotated(Vector4& self, float ax, float ay, float az, float aw, float th_deg);
void Matrix4_SetOrthoProjection(Matrix4& self, float left, float right, float top, float bottom, float far, float near);
void Matrix4_SetPerspectiveProjection(Matrix4& self, float left, float right, float top, float bottom, float far, float near);
void Matrix4_TLRotY(Matrix4& self, float x, float y, float z, float th_deg);
void Matrix4_Translation(Matrix4& self, float x, float y, float z);
void Matrix4_TLScale(Matrix4& self, float x, float y, float z, float scale);

#endif //HARDGRAD_MOBILE_VECTOR_H
