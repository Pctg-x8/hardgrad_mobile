//
// Created by S on 2016/06/23.
//

#include "Enemy.h"
#include "RenderEngine.h"
#include "VkHelper.h"
#include "utils.h"

void EnemyBufferBlocks::init(const RenderBackend& backend)
{
	this->backend_ref = &backend;

	const auto buffer_info_t = BufferInfo(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, Enemy::CenterTFMatrixMemoryRequirements);
	auto buffer_temp = backend.createBuffer(buffer_info_t).unwrap();
	const auto memory_req = backend.getMemoryRequirementsForBuffer(buffer_temp);
	vkDestroyBuffer(backend.getDevice(), buffer_temp, nullptr);

	// Allocate actual memory/buffer
	const auto CubeRotQMemoryRequirementsPadded = Alignment(Enemy::CubeRotQMemoryRequirements, memory_req.alignment);
	const auto CenterTFMatrixMemoryRequirementsPadded = Alignment(Enemy::CenterTFMatrixMemoryRequirements, memory_req.alignment);
	const auto ChildTFMatrixMemoryRequirementsPadded = Alignment(Enemy::ChildTFMatrixMemoryRequirements, memory_req.alignment);
	this->bytes_per_block = CubeRotQMemoryRequirementsPadded + CenterTFMatrixMemoryRequirementsPadded + ChildTFMatrixMemoryRequirementsPadded;
	this->CenterTF_offs = CubeRotQMemoryRequirementsPadded;
	this->ChildTF_offs = this->CenterTF_offs + CenterTFMatrixMemoryRequirementsPadded;
	const auto buffer_info = BufferInfo(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, this->bytes_per_block * EnemyMaxCount);
	this->buffer = backend.createBuffer(buffer_info).unwrap();
	this->memory = backend.allocateMemoryForBuffer(this->buffer).unwrap();

	this->free_range.emplace_back(FreeBlockRange { 0, EnemyMaxCount });
	this->total_size = this->bytes_per_block * EnemyMaxCount;
}
EnemyBufferBlocks::~EnemyBufferBlocks()
{
	vkFreeMemory(this->backend_ref->getDevice(), this->memory, nullptr);
	vkDestroyBuffer(this->backend_ref->getDevice(), this->buffer, nullptr);
}
EnemyParameterOffsets EnemyBufferBlocks::allocate()
{
	auto& first_range = this->free_range.front();
	if(first_range.begin + 1 >= first_range.end)
	{
		__android_log_print(ANDROID_LOG_ERROR, "HardGrad", "!Limit!!");
		assert(false);
	}
	const auto alloc_begin = first_range.begin * this->bytes_per_block;
	first_range.begin++;
	if(first_range.begin == first_range.end) this->free_range.pop_front();
	return EnemyParameterOffsets { alloc_begin, alloc_begin + this->CenterTF_offs, alloc_begin + this->ChildTF_offs, this->bytes_per_block };
}
Error EnemyBufferBlocks::map(VkDeviceSize offset, VkDeviceSize size, void** data)
{
	auto res = vkMapMemory(this->backend_ref->getDevice(), this->memory, offset, size, 0, data);
	return Error::failIf(res != VK_SUCCESS, "Unable to map memory");
}
void EnemyBufferBlocks::unmap()
{
	vkUnmapMemory(this->backend_ref->getDevice(), this->memory);
}

Enemy::Enemy(const RenderEngine* engine, double time, float init_x) : engine_ref(engine), appear_time(time), current_x(init_x)
{
	// LogWrite("Enemy: Appear Time: %lf", time);
	this->buffer_offsets = engine->getEnemyBufferBlocks()->allocate();
	this->desc_set = engine->getEnemyRendererCommonLayout().createDescriptorSetWith(engine->getProjectionMatrixes(), engine->getEnemyBufferBlocks()->getResource(), this->buffer_offsets.center_tf).unwrap();

	const auto cmdbuffer_info = CommandBuffer_BundledFromPool(engine->backend.getCommandPool());
	auto res = vkAllocateCommandBuffers(engine->backend.getDevice(), &cmdbuffer_info, &this->bundled_buffer);
	Error::failIf(res != VK_SUCCESS, "Unable to allocate command buffers").assertFailure();
	// Populate
	VkCommandBufferBeginInfo beginfo{};
	beginfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	res = vkBeginCommandBuffer(this->bundled_buffer, &beginfo);
	Error::failIf(res != VK_SUCCESS, "Unable to begin command recording").assertFailure();

	/// BODY ///
	this->engine_ref->ebody_render.cmdBeginPipeline(this->bundled_buffer);
	this->populateBodyRenderCommands(this->bundled_buffer);
	/// CHILD ///
	this->engine_ref->echild_render.cmdBeginPipeline(this->bundled_buffer);
	this->populateChildRenderCommands(this->bundled_buffer);

	res = vkEndCommandBuffer(this->bundled_buffer);
	Error::failIf(res != VK_SUCCESS, "Unable to record to command buffer").assertFailure();
}
Enemy::~Enemy()
{
	// delocation
	vkFreeCommandBuffers(this->engine_ref->backend.getDevice(), this->engine_ref->backend.getCommandPool(), 1, &this->bundled_buffer);
	vkFreeDescriptorSets(this->engine_ref->backend.getDevice(), this->engine_ref->getEnemyRendererCommonLayout().getDescriptorPool(), 1, &this->desc_set);
}
void Enemy::update(double time_ms, uint8_t* buffer_ptr)
{
	const auto living_s = static_cast<float>((time_ms - this->appear_time) / 1000.0);
	const auto current_y = living_s < 0.875f ? -15.0f * (1.0f - powf(1.0f - living_s, 2.0f)) + 3.0f : -(15.0f + (living_s - 0.875f) * 2.5f) + 3.0f;

	const auto v4s = reinterpret_cast<Vector4*>(buffer_ptr + this->buffer_offsets.cube_rot);
	Vector4_UnitQuaternion(v4s[0], -1.0f, 0.0f, 0.75f, 260.0f * living_s);
	Vector4_UnitQuaternion(v4s[1], 1.0f, -1.0f, 0.5f, -260.0f * living_s + 13.0f);
	const auto m4_c = reinterpret_cast<Matrix4*>(buffer_ptr + this->buffer_offsets.center_tf);
	Matrix4_Translation(*m4_c, this->current_x, current_y, 0.0f);
	const auto m4 = reinterpret_cast<Matrix4*>(buffer_ptr + this->buffer_offsets.child_tf);
	Vector4 va;
	Vector4_ZRotated(va, 2.5f, 0.0f, 0.0f, 1.0f, -130.0f * living_s);
	Matrix4_TLRotY(m4[0], va.x, va.y, va.z, 220.0f * living_s);
	Vector4_ZRotated(va, 2.5f, 0.0f, 0.0f, 1.0f, -130.0f * living_s + 120.0f);
	Matrix4_TLRotY(m4[1], va.x, va.y, va.z, 220.0f * living_s + 20.0f);
	Vector4_ZRotated(va, 2.5f, 0.0f, 0.0f, 1.0f, -130.0f * living_s + 240.0f);
	Matrix4_TLRotY(m4[2], va.x, va.y, va.z, 220.0f * living_s + 40.0f);
}
void Enemy::populateBodyRenderCommands(const VkCommandBuffer& list)
{
	vkCmdBindDescriptorSets(list, VK_PIPELINE_BIND_POINT_GRAPHICS, this->engine_ref->getEnemyRendererCommonLayout().getPipelineLayout(), 0, 1, &this->desc_set, 0, nullptr);
	const VkBuffer buffers[] = { this->engine_ref->resource.getMeshstoreBuffer(), this->engine_ref->getEnemyBufferBlocks()->getResource() };
	const VkDeviceSize offsets[] = { this->engine_ref->resource.getUnitCubeVerticesStart(), this->buffer_offsets.cube_rot };
	this->engine_ref->getEnemyRendererCommonLayout().pushWireColor(list, { 0.25f, 1.25f, 1.5f, 1.0f });
	vkCmdBindVertexBuffers(list, 0, array_size(buffers), buffers, offsets);
	vkCmdBindIndexBuffer(list, this->engine_ref->resource.getMeshstoreBuffer(), this->engine_ref->resource.getUnitCubeIndicesStart(), VK_INDEX_TYPE_UINT16);
	vkCmdDrawIndexed(list, 24, 2, 0, 0, 0);
}
void Enemy::populateChildRenderCommands(const VkCommandBuffer& list)
{
	vkCmdBindDescriptorSets(list, VK_PIPELINE_BIND_POINT_GRAPHICS, this->engine_ref->getEnemyRendererCommonLayout().getPipelineLayout(), 0, 1, &this->desc_set, 0, nullptr);
	const VkBuffer buffers[] = { this->engine_ref->resource.getMeshstoreBuffer(), this->engine_ref->getEnemyBufferBlocks()->getResource() };
	const VkDeviceSize offsets[] = { this->engine_ref->resource.getEnemyGuardVerticesStart(), this->buffer_offsets.child_tf };
	this->engine_ref->getEnemyRendererCommonLayout().pushWireColor(list, { 1.5f, 0.625f, 0.75f, 1.0f });
	vkCmdBindVertexBuffers(list, 0, array_size(buffers), buffers, offsets);
	vkCmdBindIndexBuffer(list, this->engine_ref->resource.getMeshstoreBuffer(), this->engine_ref->resource.getEnemyGuardIndicesStart(), VK_INDEX_TYPE_UINT16);
	vkCmdDrawIndexed(list, 16, 3, 0, 0, 0);
}
