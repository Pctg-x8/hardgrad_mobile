//
// Created by S on 2016/06/23.
//

#ifndef HARDGRAD_MOBILE_ENEMY_H
#define HARDGRAD_MOBILE_ENEMY_H

#include "RenderBackend.h"
#include "Vector.h"
#include <list>
#include "free_block_range.h"

constexpr auto EnemyMaxCount = 1000;

struct EnemyParameterOffsets
{
	VkDeviceSize cube_rot, center_tf, child_tf, total_size;
};

class EnemyBufferBlocks final
{
	const RenderBackend* backend_ref;

	VkDeviceMemory memory;
	VkBuffer buffer;
	size_t bytes_per_block;
	VkDeviceSize CenterTF_offs, ChildTF_offs;
	std::list<FreeBlockRange> free_range;
	VkDeviceSize total_size;
public:
	EnemyBufferBlocks() = default;
	~EnemyBufferBlocks();
	void init(const RenderBackend& backend);

	auto getResource() const { return this->buffer; }
	EnemyParameterOffsets allocate();
	Error map(VkDeviceSize offset, VkDeviceSize size, void** data);
	void unmap();
	auto getTotalSize() const { return this->total_size; }
};

class RenderEngine;
class Enemy final
{
	const RenderEngine* engine_ref;

	EnemyParameterOffsets buffer_offsets;
	VkDescriptorSet desc_set;
	VkCommandBuffer bundled_buffer;
	double appear_time;
	float current_x;

	void populateBodyRenderCommands(const VkCommandBuffer& list);
	void populateChildRenderCommands(const VkCommandBuffer& list);
public:
	static constexpr auto CubeRotQMemoryRequirements = 2 * sizeof(Vector4);
	static constexpr auto CenterTFMatrixMemoryRequirements = sizeof(Matrix4);
	static constexpr auto ChildTFMatrixMemoryRequirements = 3 * sizeof(Matrix4);

	Enemy(const RenderEngine* engine, double time, float init_x);
	~Enemy();
	void update(double time_ms, uint8_t* buffer_ptr);
	auto& getBundledCommands() const { return this->bundled_buffer; }
};


#endif //HARDGRAD_MOBILE_ENEMY_H
