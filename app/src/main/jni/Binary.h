//
// Created by S on 2016/06/22.
//

#ifndef HARDGRAD_MOBILE_BINARY_H
#define HARDGRAD_MOBILE_BINARY_H

#include "android_native_app_glue.h"
#include <stddef.h>
#include <memory>

struct Binary final
{
	std::unique_ptr<uint8_t[]> bytes;
	size_t size;
public:
	static Binary fromAsset(android_app* app, const char* path);
};


#endif //HARDGRAD_MOBILE_BINARY_H
