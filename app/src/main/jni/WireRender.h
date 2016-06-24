//
// Created by S on 2016/06/22.
//

#ifndef HARDGRAD_MOBILE_WIRERENDER_H
#define HARDGRAD_MOBILE_WIRERENDER_H

#include "RenderBackend.h"

class EnemyBodyRender; class EnemyChildRender; class BackgroundRender;
class WireRender final
{
	friend class EnemyBodyRender;
	friend class EnemyChildRender;
	friend class BackgroundRender;

	const RenderBackend* backend_ref;
	VkShaderModule common_ps_module;
public:
	~WireRender();
	void init(android_app* app, const RenderBackend& backend);
};
class ProjectionMatrixes;
class EnemyRendererDescriptorSetLayouts
{
	const RenderBackend* backend_ref;

	VkDescriptorSetLayout set_layout;
	VkDescriptorPool desc_pool;
	VkPipelineLayout pipe_layout;
public:
	EnemyRendererDescriptorSetLayouts() = default;
	~EnemyRendererDescriptorSetLayouts();
	void init(const RenderBackend& backend);
	void pushWireColor(const VkCommandBuffer& list, const float(&color)[4]) const;
	Result<VkDescriptorSet> createDescriptorSetWith(const ProjectionMatrixes& matrixes, const VkBuffer& enemy_instance_buffer, VkDeviceSize offset_in_buffer) const;

	auto getPipelineLayout() const { return this->pipe_layout; }
	auto getDescriptorPool() const { return this->desc_pool; }
};
class ProjectionMatrixes final
{
	friend class EnemyRendererDescriptorSetLayouts;
	friend class BackgroundRender;

	const RenderBackend* backend_ref;
	VkDeviceMemory memory;
	VkBuffer buffer;
	VkDeviceSize ortho_offset, persp_offset;
public:
	ProjectionMatrixes() = default;
	~ProjectionMatrixes();
	void init(const RenderBackend& backend, int32_t width, int32_t height);

	auto getOrthoOffset() const { return this->ortho_offset; }
	auto getPerspOffset() const { return this->persp_offset; }
};

class EnemyBodyRender final
{
	const RenderBackend* backend_ref;
	const EnemyRendererDescriptorSetLayouts* common_layout_ref;

	VkPipelineCache cache_data;
	VkPipeline pipe;
public:
	~EnemyBodyRender();
	void init(android_app* app, const WireRender& wire_render, const EnemyRendererDescriptorSetLayouts& er_common, int32_t vp_width, int32_t vp_height, const VkRenderPass& render_pass, const RenderBackend& backend);

	void cmdBeginPipeline(const VkCommandBuffer& list) const;
};
class EnemyChildRender final
{
	const RenderBackend* backend_ref;
	const EnemyRendererDescriptorSetLayouts* common_layout_ref;

	VkPipelineCache cache_data;
	VkPipeline pipe;
public:
	~EnemyChildRender();
	void init(android_app* app, const WireRender& wire_render, const EnemyRendererDescriptorSetLayouts& er_common, uint32_t vp_width, uint32_t vp_height, const VkRenderPass& render_pass, const RenderBackend& backend);

	void cmdBeginPipeline(const VkCommandBuffer& list) const;
};
class BackgroundRender final
{
	const RenderBackend* backend_ref;

	VkDescriptorSetLayout desc_layout;
	VkDescriptorPool desc_pool;
	VkPipelineLayout pipe_layout;
	VkPipelineCache cache_data;
	VkPipeline pipe;
public:
	~BackgroundRender();
	void init(android_app* app, const WireRender& wire_render, uint32_t vp_width, uint32_t vp_height, const VkRenderPass& render_pass, const RenderBackend& backend);

	void cmdBeginPipeline(const VkCommandBuffer& list) const;
	auto& getPipelineLayout() const { return this->pipe_layout; }
	void pushWireColor(const VkCommandBuffer& list, const float(&color)[4]) const;
	Result<VkDescriptorSet> createDescriptorSetWith(const ProjectionMatrixes& matrixes, const VkBuffer& enemy_instance_buffer, VkDeviceSize offset_in_buffer) const;
};

#endif //HARDGRAD_MOBILE_WIRERENDER_H
