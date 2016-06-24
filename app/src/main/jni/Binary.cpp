//
// Created by S on 2016/06/22.
//

#include "Binary.h"
#include <assert.h>
#include "utils.h"

Binary Binary::fromAsset(android_app* app, const char* path)
{
	LogWrite("Loading Asset %s...", path);
	auto asset = AAssetManager_open(app->activity->assetManager, path, AASSET_MODE_UNKNOWN);
	assert(asset != nullptr);
	const auto size = static_cast<uint32_t>(AAsset_getLength(asset));
	auto bytes = std::make_unique<uint8_t[]>(size);
	uint8_t read_buffer[1024], *write_ptr = bytes.get();
	int read_size;
	while((read_size = AAsset_read(asset, read_buffer, 1024)) > 0)
	{
		memcpy(write_ptr, read_buffer, static_cast<size_t>(read_size));
		write_ptr += read_size;
	}
	return Binary { std::move(bytes), static_cast<size_t>(size) };
}
