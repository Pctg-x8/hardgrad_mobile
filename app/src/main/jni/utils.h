//
// Created by S on 2016/06/21.
//

#ifndef HARDGRAD_MOBILE_UTILS_H
#define HARDGRAD_MOBILE_UTILS_H

#include <stddef.h>
#include <math.h>
#include <android/log.h>
#include "functions.h"

#define LogWrite(fmt, ...) __android_log_print(ANDROID_LOG_INFO, "HardGrad", fmt, __VA_ARGS__)

template <typename T, size_t N> constexpr auto array_size(T(&)[N]) { return N; }
inline auto BlockCount(VkDeviceSize value, VkDeviceSize align)
{
	return static_cast<VkDeviceSize>(ceil(value / static_cast<double>(align)));
}
inline auto Alignment(VkDeviceSize value, VkDeviceSize align)
{
	return align * BlockCount(value, align);
}

#endif //HARDGRAD_MOBILE_UTILS_H
