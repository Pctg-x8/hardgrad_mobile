//
// Created by S on 2016/06/23.
//

#include "Vector.h"
#include <math.h>

void Vector4_UnitQuaternion(Vector4& self, float x, float y, float z, float th_deg)
{
	const auto len2 = powf(x, 2.0f) + powf(y, 2.0f) + powf(z, 2.0f);
	const auto th_rad = 2.0f * atanf(1.0f) * th_deg / 180.0f;
	const float sincos[] = { sinf(th_rad), cosf(th_rad) };

	self.x = sincos[0] * x / sqrtf(len2);
	self.y = sincos[0] * y / sqrtf(len2);
	self.z = sincos[0] * z / sqrtf(len2);
	self.w = sincos[1];
}
void Vector4_ZRotated(Vector4& self, float ax, float ay, float az, float aw, float th_deg)
{
	const auto th_rad = 4.0f * atanf(1.0f) * th_deg / 180.0f;

	self.x = ax * cosf(th_rad) - ay * sinf(th_rad);
	self.y = ax * sinf(th_rad) + ay * cosf(th_rad);
	self.z = az;
	self.w = aw;
}
void Matrix4_SetOrthoProjection(Matrix4& self, float left, float right, float top, float bottom, float far, float near)
{
	for(auto& e : self.m) e = 0.0f;
	self.m[0 * 4 + 0] = 2.0f / (right - left);
	self.m[1 * 4 + 1] = 2.0f / (top - bottom);
	self.m[2 * 4 + 2] = -2.0f / (far - near);
	self.m[3 * 4 + 3] = 1.0f;
	self.m[0 * 4 + 3] = -(right + left) / (right - left);
	self.m[1 * 4 + 3] = -(top + bottom) / (top - bottom);
	self.m[2 * 4 + 3] = -(far + near) / (far - near);
}
void Matrix4_SetPerspectiveProjection(Matrix4& self, float left, float right, float top, float bottom, float far, float near)
{
	const auto w = 2.0f * near / (right - left);
	const auto h = 2.0f * near / (top - bottom);
	const auto q = far / (far - near);

	for(auto& e : self.m) e = 0.0f;
	self.m[0 * 4 + 0] = w;
	self.m[1 * 4 + 1] = h;
	self.m[2 * 4 + 2] = q;
	self.m[2 * 4 + 3] = 1.0f;
	self.m[3 * 4 + 2] = far * near / (near - far);
}
void Matrix4_TLRotY(Matrix4& self, float x, float y, float z, float th_deg)
{
	const auto th_rad = 4.0f * atanf(1.0f) * th_deg / 180.0f;

	for(auto& e : self.m) e = 0.0f;
	self.m[0 * 4 + 0] = cosf(th_rad);
	self.m[0 * 4 + 2] = sinf(th_rad);
	self.m[1 * 4 + 1] = 1.0f;
	self.m[2 * 4 + 0] = -sinf(th_rad);
	self.m[2 * 4 + 2] = cosf(th_rad);
	self.m[3 * 4 + 3] = 1.0f;
	self.m[0 * 4 + 3] = x;
	self.m[1 * 4 + 3] = y;
	self.m[2 * 4 + 3] = z;
}
void Matrix4_Translation(Matrix4& self, float x, float y, float z)
{
	for(auto& e : self.m) e = 0.0f;
	self.m[0 * 4 + 0] = 1.0f;
	self.m[1 * 4 + 1] = 1.0f;
	self.m[2 * 4 + 2] = 1.0f;
	self.m[3 * 4 + 3] = 1.0f;
	self.m[0 * 4 + 3] = x;
	self.m[1 * 4 + 3] = y;
	self.m[2 * 4 + 3] = z;
}
void Matrix4_TLScale(Matrix4& self, float x, float y, float z, float scale)
{
	for(auto& e : self.m) e = 0.0f;
	self.m[0 * 4 + 0] = scale;
	self.m[1 * 4 + 1] = scale;
	self.m[2 * 4 + 2] = 1.0f;
	self.m[3 * 4 + 3] = 1.0f;
	self.m[0 * 4 + 3] = x;
	self.m[1 * 4 + 3] = y;
	self.m[2 * 4 + 3] = z;
}
