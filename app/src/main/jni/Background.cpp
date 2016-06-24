//
// Created by S on 2016/06/24.
//

#include "Background.h"
#include "RenderEngine.h"
#include "utils.h"
#include "VkHelper.h"

void BackgroundBufferBlocks::init(const RenderBackend& backend)
{
	this->backend_ref = &backend;

	auto buffer_temp = backend.createBuffer(BufferInfo(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(Matrix4))).unwrap();
	const auto memory_req = backend.getMemoryRequirementsForBuffer(buffer_temp);
	vkDestroyBuffer(backend.getDevice(), buffer_temp, nullptr);

	// Allocate actual memory/buffer
	const auto matr4_aligned = Alignment(sizeof(Matrix4), memory_req.alignment);
	this->bytes_per_block = matr4_aligned;
	const auto buffer_info = BufferInfo(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, this->bytes_per_block * MaxBackgroundCount);
	this->buffer = backend.createBuffer(buffer_info).unwrap();
	this->memory = backend.allocateMemoryForBuffer(this->buffer).unwrap();

	this->free_range.emplace_back(FreeBlockRange { 0, MaxBackgroundCount });
	this->total_size = this->bytes_per_block * MaxBackgroundCount;
}
BackgroundBufferBlocks::~BackgroundBufferBlocks()
{
	vkFreeMemory(this->backend_ref->getDevice(), this->memory, nullptr);
	vkDestroyBuffer(this->backend_ref->getDevice(), this->buffer, nullptr);
}
VkDeviceSize BackgroundBufferBlocks::allocate()
{
	auto& first_range = this->free_range.front();
	if(first_range.begin + 1 >= first_range.end)
	{
		__android_log_print(ANDROID_LOG_ERROR, "HardGrad::Background", "!Limit!!");
		assert(false);
	}
	const auto alloc_begin = first_range.begin * this->bytes_per_block;
	first_range.begin++;
	if(first_range.begin == first_range.end) this->free_range.pop_front();
	return alloc_begin;
}
Error BackgroundBufferBlocks::map(VkDeviceSize offset, VkDeviceSize size, void** data)
{
	auto res = vkMapMemory(this->backend_ref->getDevice(), this->memory, offset, size, 0, data);
	return Error::failIf(res != VK_SUCCESS, "Unable to map memory");
}
void BackgroundBufferBlocks::unmap()
{
	vkUnmapMemory(this->backend_ref->getDevice(), this->memory);
}
Background::Background(const RenderEngine* engine, double time, float init_x, float init_scale, uint8_t instance_count)
	: engine_ref(engine), appear_time(time), current_x(init_x), scale(init_scale)
{
	// LogWrite("Background: Appear Time: %lf", time);
	this->matrix_offset = engine->getBackgroundBufferBlocks()->allocate();
	this->desc_set = engine->background_render.createDescriptorSetWith(engine->getProjectionMatrixes(), engine->getBackgroundBufferBlocks()->getResource(), this->matrix_offset).unwrap();

	const auto cmdbuffer_info = CommandBuffer_BundledFromPool(engine->backend.getCommandPool());
	auto res = vkAllocateCommandBuffers(engine->backend.getDevice(), &cmdbuffer_info, &this->bundled_buffer);
	Error::failIf(res != VK_SUCCESS, "Unable to allocate command buffers").assertFailure();
	// Populate
	VkCommandBufferBeginInfo beginfo{};
	beginfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	res = vkBeginCommandBuffer(this->bundled_buffer, &beginfo);
	Error::failIf(res != VK_SUCCESS, "Unable to begin command recording").assertFailure();

	this->engine_ref->background_render.cmdBeginPipeline(this->bundled_buffer);
	vkCmdBindDescriptorSets(this->bundled_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, this->engine_ref->background_render.getPipelineLayout(), 0, 1, &this->desc_set, 0, nullptr);
	const VkBuffer buffers[] = { this->engine_ref->resource.getMeshstoreBuffer() };
	const VkDeviceSize offsets[] = { this->engine_ref->resource.getBackgroundVerticesStart() };
	this->engine_ref->background_render.pushWireColor(this->bundled_buffer, { 0.125f, 0.625f, 0.125f, 0.75f });
	vkCmdBindVertexBuffers(this->bundled_buffer, 0, array_size(buffers), buffers, offsets);
	vkCmdBindIndexBuffer(this->bundled_buffer, this->engine_ref->resource.getMeshstoreBuffer(), this->engine_ref->resource.getBackgroundIndicesStart(), VK_INDEX_TYPE_UINT16);
	vkCmdDrawIndexed(this->bundled_buffer, 8, instance_count, 0, 0, 0);

	res = vkEndCommandBuffer(this->bundled_buffer);
	Error::failIf(res != VK_SUCCESS, "Unable to record to command buffer").assertFailure();
}
Background::~Background()
{
	// delocation
	vkFreeCommandBuffers(this->engine_ref->backend.getDevice(), this->engine_ref->backend.getCommandPool(), 1, &this->bundled_buffer);
	vkFreeDescriptorSets(this->engine_ref->backend.getDevice(), this->engine_ref->getEnemyRendererCommonLayout().getDescriptorPool(), 1, &this->desc_set);
}
void Background::update(double time_ms, uint8_t* buffer_ptr)
{
	const auto living_s = static_cast<float>((time_ms - this->appear_time) / 1000.0);
	const auto current_y = living_s * 16.25f - 20.0f;
	const auto mapped_ptr = reinterpret_cast<Matrix4*>(buffer_ptr + this->matrix_offset);
	Matrix4_TLScale(*mapped_ptr, this->current_x, current_y, 10.0f, this->scale);
}
