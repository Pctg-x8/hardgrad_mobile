//
// Created by S on 2016/06/24.
//

#ifndef HARDGRAD_MOBILE_BACKGROUND_H
#define HARDGRAD_MOBILE_BACKGROUND_H

#include "RenderBackend.h"
#include <list>
#include "free_block_range.h"

constexpr auto MaxBackgroundCount = 6000;

class BackgroundBufferBlocks final
{
	const RenderBackend* backend_ref;

	VkDeviceMemory memory;
	VkBuffer buffer;
	size_t bytes_per_block;
	std::list<FreeBlockRange> free_range;
	VkDeviceSize total_size;
public:
	BackgroundBufferBlocks() = default;
	~BackgroundBufferBlocks();
	void init(const RenderBackend& backend);

	auto getResource() const { return this->buffer; }
	VkDeviceSize allocate();
	Error map(VkDeviceSize offset, VkDeviceSize size, void** data);
	void unmap();
	auto getTotalSize() const { return this->total_size; }
};

class RenderEngine;
class Background final
{
	const RenderEngine* engine_ref;

	VkDeviceSize matrix_offset;
	VkDescriptorSet desc_set;
	VkCommandBuffer bundled_buffer;
	double appear_time;
	float current_x, scale;
public:
	Background(const RenderEngine* engine, double time, float init_x, float init_scale, uint8_t instance_count);
	~Background();
	void update(double time_ms, uint8_t* buffer_ptr);

	auto& getBundledCommands() const { return this->bundled_buffer; }
};


#endif //HARDGRAD_MOBILE_BACKGROUND_H
