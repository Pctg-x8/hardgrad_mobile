//
// Created by S on 2016/06/22.
//

#include "WireRender.h"
#include "VkHelper.h"
#include "utils.h"
#include "Vector.h"
#include "Enemy.h"
#include "Background.h"

void ProjectionMatrixes::init(const RenderBackend& backend, int32_t width, int32_t height)
{
	this->backend_ref = &backend;

	const auto buffer_t = backend.createBuffer(BufferInfo(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(Matrix4) * 2)).unwrap();
	const auto memory_req = backend.getMemoryRequirementsForBuffer(buffer_t);
	vkDestroyBuffer(backend.getDevice(), buffer_t, nullptr);

	const auto mat_size_aligned = Alignment(sizeof(Matrix4), memory_req.alignment);
	this->buffer = backend.createBuffer(BufferInfo(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, mat_size_aligned * 2)).unwrap();
	this->memory = backend.allocateMemoryForBuffer(this->buffer).unwrap();
	this->ortho_offset = 0;
	this->persp_offset = mat_size_aligned;

	uint8_t* mapped_ptr;
	auto res = vkMapMemory(backend.getDevice(), this->memory, 0, mat_size_aligned * 2, 0, reinterpret_cast<void**>(&mapped_ptr));
	Error::failIf(res != VK_SUCCESS, "Unable to map memory").assertFailure();

	const auto ortho = reinterpret_cast<Matrix4*>(mapped_ptr);
	const auto device_aspect = static_cast<float>(height) / width;
	const auto descale = 16.0f;
	Matrix4_SetOrthoProjection(*ortho, -descale, descale, -descale * device_aspect * 2.0f, 0.0f, 100.0f, -100.0f);
	const auto persp = reinterpret_cast<Matrix4*>(mapped_ptr + this->persp_offset);
	Matrix4_SetPerspectiveProjection(*persp, -descale / 16.0f, descale / 16.0f, -descale * device_aspect * 2.0f / 16.0f, 0.0f, 75.0f, -10.0f);

	vkUnmapMemory(backend.getDevice(), this->memory);
}
ProjectionMatrixes::~ProjectionMatrixes()
{
	vkFreeMemory(this->backend_ref->getDevice(), this->memory, nullptr);
	vkDestroyBuffer(this->backend_ref->getDevice(), this->buffer, nullptr);
}

void WireRender::init(android_app* app, const RenderBackend& backend)
{
	this->backend_ref = &backend;
	this->common_ps_module = backend.createShaderModule(Binary::fromAsset(app, "Colorize.spv")).unwrap();
}
WireRender::~WireRender()
{
	vkDestroyShaderModule(this->backend_ref->getDevice(), this->common_ps_module, nullptr);
}

void EnemyRendererDescriptorSetLayouts::init(const RenderBackend& backend)
{
	this->backend_ref = &backend;

	// Create DescriptorSetLayout
	const VkDescriptorSetLayoutBinding bindings[] = {
		DescriptorSetLayoutBinding_UniformBuffer(0, 1, VK_SHADER_STAGE_VERTEX_BIT),
		DescriptorSetLayoutBinding_UniformBuffer(1, 1, VK_SHADER_STAGE_VERTEX_BIT)
	};
	const auto descriptor_set_layout_info = DescriptorSetLayout_WithBindings(bindings);
	auto res = vkCreateDescriptorSetLayout(backend.getDevice(), &descriptor_set_layout_info, nullptr, &this->set_layout);
	Error::failIf(res != VK_SUCCESS, "Unable to create descriptor set layout").assertFailure();

	// Descriptor Pool
	const VkDescriptorPoolSize pools[] = {
		DescriptorPoolSize_UniformBuffer(1),
		DescriptorPoolSize_UniformBuffer(1)
	};
	const auto pool_info = DescriptorPoolInfo(EnemyMaxCount, pools);
	res = vkCreateDescriptorPool(backend.getDevice(), &pool_info, nullptr, &this->desc_pool);
	Error::failIf(res != VK_SUCCESS, "Unable to create descriptor pool").assertFailure();

	// Create Pipeline Layout
	const auto pipe_layout = PipelineLayout({ this->set_layout }, { PushConstantRange<Vector4>(0, VK_SHADER_STAGE_FRAGMENT_BIT) });
	res = vkCreatePipelineLayout(backend.getDevice(), &pipe_layout, nullptr, &this->pipe_layout);
	Error::failIf(res != VK_SUCCESS, "Unable to create pipeline layout").assertFailure();
}
EnemyRendererDescriptorSetLayouts::~EnemyRendererDescriptorSetLayouts()
{
	vkDestroyPipelineLayout(this->backend_ref->getDevice(), this->pipe_layout, nullptr);
	vkDestroyDescriptorPool(this->backend_ref->getDevice(), this->desc_pool, nullptr);
	vkDestroyDescriptorSetLayout(this->backend_ref->getDevice(), this->set_layout, nullptr);
}
void EnemyRendererDescriptorSetLayouts::pushWireColor(const VkCommandBuffer& list, const float(&color)[4]) const
{
	vkCmdPushConstants(list, this->pipe_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float[4]), color);
}
Result<VkDescriptorSet> EnemyRendererDescriptorSetLayouts::createDescriptorSetWith(const ProjectionMatrixes& matrixes, const VkBuffer& enemy_instance_buffer, VkDeviceSize offset_in_buffer) const
{
	VkDescriptorSet o;

	// Descriptor Set
	const auto set_alloc_info = DescriptorSetAllocateInfo(this->desc_pool, { this->set_layout });
	auto res = vkAllocateDescriptorSets(this->backend_ref->getDevice(), &set_alloc_info, &o);
	if(res != VK_SUCCESS) return Result<VkDescriptorSet>::err("Unable to allocate descriptor set");
	// Update
	const auto ortho_projection_ub_info = DescriptorBufferInfo<Matrix4>(matrixes.buffer, matrixes.ortho_offset);
	const auto buffer_info = DescriptorBufferInfo<Matrix4>(enemy_instance_buffer, offset_in_buffer);
	const VkWriteDescriptorSet write_data[] = { DescriptorSetWrite_UniformBuffers({ ortho_projection_ub_info, buffer_info }, o, 0) };
	vkUpdateDescriptorSets(this->backend_ref->getDevice(), array_size(write_data), write_data, 0, nullptr);

	return Result<VkDescriptorSet>::ok(o);
}

void EnemyBodyRender::init(android_app* app, const WireRender& wire_render, const EnemyRendererDescriptorSetLayouts& er_common, int32_t vp_width, int32_t vp_height, const VkRenderPass& render_pass, const RenderBackend& backend)
{
	VkResult res;
	this->backend_ref = &backend;
	this->common_layout_ref = &er_common;

	// Shader Modules
	auto vert_mod = backend.createShaderModule(Binary::fromAsset(app, "ApplyQuaternionInstanced.spv")).unwrap();

	// Pipeline Cache
	VkPipelineCacheCreateInfo cacheinfo{};
	cacheinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	res = vkCreatePipelineCache(backend.getDevice(), &cacheinfo, nullptr, &this->cache_data);
	Error::failIf(res != VK_SUCCESS, "Unable to create pipeline cache").assertFailure();

	// Pipeline
	const VkPipelineShaderStageCreateInfo stageinfo[] = {
		ShaderStage_VertexFromModule(vert_mod, "main"),
		ShaderStage_FragmentFromModule(wire_render.common_ps_module, "main")
	};
	const VkVertexInputBindingDescription bind_descs[] = {
		VertexInputBinding_PerVertex<Position>(0),
		VertexInputBinding_PerInstance<Vector4>(1)
	};
	const VkVertexInputAttributeDescription input_attributes[] = {
		VertexInputAttribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0),
		VertexInputAttribute(1, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 0)
	};
	const auto input_state = Pipeline_VertexInputState(bind_descs, input_attributes);
	const auto ia_state = Pipeline_InputAssemblyState(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
	const auto vport = Viewport(0, 0, vp_width, vp_height, 1.0f, -1.0f);
	const auto scissor = Rect2D(0, 0, static_cast<uint32_t>(vp_width), static_cast<uint32_t>(vp_height));
	const auto viewport_state = Pipeline_ViewportState({ vport }, { scissor });
	VkPipelineRasterizationStateCreateInfo rasterizerinfo{};
	rasterizerinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizerinfo.depthClampEnable = VK_FALSE;
	rasterizerinfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizerinfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizerinfo.cullMode = VK_CULL_MODE_NONE;
	rasterizerinfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizerinfo.depthBiasEnable = VK_FALSE;
	rasterizerinfo.lineWidth = 1.0f;
	const auto sample_info = Pipeline_MultisampleDisable();
	const auto blend_attachment = BlendAttachment_Disabled();
	const auto blend_state = Pipeline_BlendState({ blend_attachment });
	const auto dynamic_state = Pipeline_NoDynamicStates();
	VkGraphicsPipelineCreateInfo pipeinfo{};
	pipeinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeinfo.stageCount = array_size(stageinfo);
	pipeinfo.pStages = stageinfo;
	pipeinfo.pVertexInputState = &input_state;
	pipeinfo.pInputAssemblyState = &ia_state;
	pipeinfo.pViewportState = &viewport_state;
	pipeinfo.pRasterizationState = &rasterizerinfo;
	pipeinfo.pMultisampleState = &sample_info;
	pipeinfo.pColorBlendState = &blend_state;
	pipeinfo.pDynamicState = &dynamic_state;
	pipeinfo.layout = er_common.getPipelineLayout();
	pipeinfo.renderPass = render_pass;
	pipeinfo.subpass = 0;
	res = vkCreateGraphicsPipelines(backend.getDevice(), this->cache_data, 1, &pipeinfo, nullptr, &this->pipe);
	Error::failIf(res != VK_SUCCESS, "Unable to create pipeline").assertFailure();

	vkDestroyShaderModule(backend.getDevice(), vert_mod, nullptr);
}
EnemyBodyRender::~EnemyBodyRender()
{
	vkDestroyPipeline(this->backend_ref->getDevice(), this->pipe, nullptr);
	vkDestroyPipelineCache(this->backend_ref->getDevice(), this->cache_data, nullptr);
}

void EnemyBodyRender::cmdBeginPipeline(const VkCommandBuffer& list) const
{
	vkCmdBindPipeline(list, VK_PIPELINE_BIND_POINT_GRAPHICS, this->pipe);
}

void EnemyChildRender::init(android_app* app, const WireRender& wire_render, const EnemyRendererDescriptorSetLayouts& er_common, uint32_t vp_width, uint32_t vp_height, const VkRenderPass& render_pass, const RenderBackend& backend)
{
	VkResult res;
	this->backend_ref = &backend;
	this->common_layout_ref = &er_common;

	// Shader Module
	auto vert_mod = backend.createShaderModule(Binary::fromAsset(app, "EnemyGuardRot.spv")).unwrap();

	// Pipeline Cache
	VkPipelineCacheCreateInfo cacheinfo{};
	cacheinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	res = vkCreatePipelineCache(backend.getDevice(), &cacheinfo, nullptr, &this->cache_data);
	Error::failIf(res != VK_SUCCESS, "Unable to create pipeline cache").assertFailure();

	// Pipeline
	const VkPipelineShaderStageCreateInfo stageinfo[] = {
		ShaderStage_VertexFromModule(vert_mod, "main"),
		ShaderStage_FragmentFromModule(wire_render.common_ps_module, "main")
	};
	const VkVertexInputBindingDescription bind_descs[] = {
		VertexInputBinding_PerVertex<Position>(0),
	    VertexInputBinding_PerInstance<Matrix4>(1)
	};
	const VkVertexInputAttributeDescription input_attributes[] = {
		VertexInputAttribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0),
		VertexInputAttribute(1, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 0),
		VertexInputAttribute(1, 2, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(Vector4) * 1),
		VertexInputAttribute(1, 3, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(Vector4) * 2),
		VertexInputAttribute(1, 4, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(Vector4) * 3)
	};
	const auto input_state = Pipeline_VertexInputState(bind_descs, input_attributes);
	const auto ia_state = Pipeline_InputAssemblyState(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
	const auto vport = Viewport(0, 0, vp_width, vp_height, 1.0f, -1.0f);
	const auto scissor = Rect2D(0, 0, static_cast<uint32_t>(vp_width), static_cast<uint32_t>(vp_height));
	const auto viewport_state = Pipeline_ViewportState({ vport }, { scissor });
	VkPipelineRasterizationStateCreateInfo rasterizerinfo{};
	rasterizerinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizerinfo.depthClampEnable = VK_FALSE;
	rasterizerinfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizerinfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizerinfo.cullMode = VK_CULL_MODE_NONE;
	rasterizerinfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizerinfo.depthBiasEnable = VK_FALSE;
	rasterizerinfo.lineWidth = 1.0f;
	const auto sample_info = Pipeline_MultisampleDisable();
	const auto blend_attachment = BlendAttachment_Disabled();
	const auto blend_state = Pipeline_BlendState({ blend_attachment });
	const auto dynamic_state = Pipeline_NoDynamicStates();
	VkGraphicsPipelineCreateInfo pipeinfo{};
	pipeinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeinfo.stageCount = array_size(stageinfo);
	pipeinfo.pStages = stageinfo;
	pipeinfo.pVertexInputState = &input_state;
	pipeinfo.pInputAssemblyState = &ia_state;
	pipeinfo.pViewportState = &viewport_state;
	pipeinfo.pRasterizationState = &rasterizerinfo;
	pipeinfo.pMultisampleState = &sample_info;
	pipeinfo.pColorBlendState = &blend_state;
	pipeinfo.pDynamicState = &dynamic_state;
	pipeinfo.layout = er_common.getPipelineLayout();
	pipeinfo.renderPass = render_pass;
	pipeinfo.subpass = 0;
	res = vkCreateGraphicsPipelines(backend.getDevice(), this->cache_data, 1, &pipeinfo, nullptr, &this->pipe);
	Error::failIf(res != VK_SUCCESS, "Unable to create pipeline").assertFailure();

	vkDestroyShaderModule(backend.getDevice(), vert_mod, nullptr);
}
EnemyChildRender::~EnemyChildRender()
{
	vkDestroyPipeline(this->backend_ref->getDevice(), this->pipe, nullptr);
	vkDestroyPipelineCache(this->backend_ref->getDevice(), this->cache_data, nullptr);
}
void EnemyChildRender::cmdBeginPipeline(const VkCommandBuffer& list) const
{
	vkCmdBindPipeline(list, VK_PIPELINE_BIND_POINT_GRAPHICS, this->pipe);
}

void BackgroundRender::init(android_app* app, const WireRender& wire_render, uint32_t vp_width, uint32_t vp_height, const VkRenderPass& render_pass, const RenderBackend& backend)
{
	VkResult res;
	this->backend_ref = &backend;

	// Shader Module
	auto vert_mod = backend.createShaderModule(Binary::fromAsset(app, "PerspectiveBack.spv")).unwrap();

	// Descriptor Sets
	const VkDescriptorSetLayoutBinding binding_elements[] = {
	    DescriptorSetLayoutBinding_UniformBuffer(0, 1, VK_SHADER_STAGE_VERTEX_BIT),
	    DescriptorSetLayoutBinding_UniformBuffer(1, 1, VK_SHADER_STAGE_VERTEX_BIT)
	};
	const auto dsl_info = DescriptorSetLayout_WithBindings(binding_elements);
	res = vkCreateDescriptorSetLayout(backend.getDevice(), &dsl_info, nullptr, &this->desc_layout);
	Error::failIf(res != VK_SUCCESS, "Unable to create descriptor set layout");
	const VkDescriptorPoolSize dsl_sizes[] = {
		DescriptorPoolSize_UniformBuffer(1), DescriptorPoolSize_UniformBuffer(1)
	};
	const auto dsp_info = DescriptorPoolInfo(MaxBackgroundCount, dsl_sizes);
	res = vkCreateDescriptorPool(backend.getDevice(), &dsp_info, nullptr, &this->desc_pool);
	Error::failIf(res != VK_SUCCESS, "Unable to create descriptor pool");

	// Pipeline Layout
	const auto pl_info = PipelineLayout({ this->desc_layout }, { PushConstantRange<Vector4>(0, VK_SHADER_STAGE_FRAGMENT_BIT) });
	res = vkCreatePipelineLayout(backend.getDevice(), &pl_info, nullptr, &this->pipe_layout);
	Error::failIf(res != VK_SUCCESS, "Unable to create pipeline layout");

	// Pipeline Cache
	VkPipelineCacheCreateInfo cacheinfo{};
	cacheinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	res = vkCreatePipelineCache(backend.getDevice(), &cacheinfo, nullptr, &this->cache_data);
	Error::failIf(res != VK_SUCCESS, "Unable to create pipeline cache").assertFailure();

	// Pipeline
	const VkPipelineShaderStageCreateInfo stageinfo[] = {
		ShaderStage_VertexFromModule(vert_mod, "main"),
		ShaderStage_FragmentFromModule(wire_render.common_ps_module, "main")
	};
	const VkVertexInputBindingDescription bind_descs[] = {
		VertexInputBinding_PerVertex<Position>(0)
	};
	const VkVertexInputAttributeDescription input_attributes[] = {
		VertexInputAttribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0)
	};
	const auto input_state = Pipeline_VertexInputState(bind_descs, input_attributes);
	const auto ia_state = Pipeline_InputAssemblyState(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
	const auto vport = Viewport(0, 0, vp_width, vp_height, 1.0f, -1.0f);
	const auto scissor = Rect2D(0, 0, static_cast<uint32_t>(vp_width), static_cast<uint32_t>(vp_height));
	const auto viewport_state = Pipeline_ViewportState({ vport }, { scissor });
	VkPipelineRasterizationStateCreateInfo rasterizerinfo{};
	rasterizerinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizerinfo.depthClampEnable = VK_FALSE;
	rasterizerinfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizerinfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizerinfo.cullMode = VK_CULL_MODE_NONE;
	rasterizerinfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizerinfo.depthBiasEnable = VK_FALSE;
	rasterizerinfo.lineWidth = 1.0f;
	const auto sample_info = Pipeline_MultisampleDisable();
	const auto blend_attachment = BlendAttachment_Disabled();
	const auto blend_state = Pipeline_BlendState({ blend_attachment });
	const auto dynamic_state = Pipeline_NoDynamicStates();
	VkGraphicsPipelineCreateInfo pipeinfo{};
	pipeinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeinfo.stageCount = array_size(stageinfo);
	pipeinfo.pStages = stageinfo;
	pipeinfo.pVertexInputState = &input_state;
	pipeinfo.pInputAssemblyState = &ia_state;
	pipeinfo.pViewportState = &viewport_state;
	pipeinfo.pRasterizationState = &rasterizerinfo;
	pipeinfo.pMultisampleState = &sample_info;
	pipeinfo.pColorBlendState = &blend_state;
	pipeinfo.pDynamicState = &dynamic_state;
	pipeinfo.layout = this->pipe_layout;
	pipeinfo.renderPass = render_pass;
	pipeinfo.subpass = 0;
	res = vkCreateGraphicsPipelines(backend.getDevice(), this->cache_data, 1, &pipeinfo, nullptr, &this->pipe);
	Error::failIf(res != VK_SUCCESS, "Unable to create pipeline").assertFailure();

	vkDestroyShaderModule(backend.getDevice(), vert_mod, nullptr);
}
BackgroundRender::~BackgroundRender()
{
	vkDestroyPipeline(this->backend_ref->getDevice(), this->pipe, nullptr);
	vkDestroyPipelineCache(this->backend_ref->getDevice(), this->cache_data, nullptr);
}
void BackgroundRender::cmdBeginPipeline(const VkCommandBuffer& list) const
{
	vkCmdBindPipeline(list, VK_PIPELINE_BIND_POINT_GRAPHICS, this->pipe);
}
void BackgroundRender::pushWireColor(const VkCommandBuffer& list, const float(&color)[4]) const
{
	vkCmdPushConstants(list, this->pipe_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Vector4), color);
}
Result<VkDescriptorSet> BackgroundRender::createDescriptorSetWith(const ProjectionMatrixes& matrixes, const VkBuffer& enemy_instance_buffer, VkDeviceSize offset_in_buffer) const
{
	VkDescriptorSet o;

	// Descriptor Set
	const auto set_alloc_info = DescriptorSetAllocateInfo(this->desc_pool, { this->desc_layout });
	auto res = vkAllocateDescriptorSets(this->backend_ref->getDevice(), &set_alloc_info, &o);
	if(res != VK_SUCCESS) return Result<VkDescriptorSet>::err("Unable to allocate descriptor set");
	// Update
	const auto projection_ub_info = DescriptorBufferInfo<Matrix4>(matrixes.buffer, matrixes.persp_offset);
	const auto buffer_info = DescriptorBufferInfo<Matrix4>(enemy_instance_buffer, offset_in_buffer);
	const VkWriteDescriptorSet write_data[] = { DescriptorSetWrite_UniformBuffers({ projection_ub_info, buffer_info }, o, 0) };
	vkUpdateDescriptorSets(this->backend_ref->getDevice(), array_size(write_data), write_data, 0, nullptr);

	return Result<VkDescriptorSet>::ok(o);
}
