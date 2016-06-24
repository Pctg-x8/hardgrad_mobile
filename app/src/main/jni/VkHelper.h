//
// Created by S on 2016/06/22.
//

#ifndef HARDGRAD_MOBILE_VKHELPER_H
#define HARDGRAD_MOBILE_VKHELPER_H

// Helper / Default Data for Vulkan Structures
#include "RenderBackend.h"

inline auto DescriptorSetLayoutBinding_UniformBuffer(size_t index, size_t descriptor_count, VkShaderStageFlags shader_visibility)
{
	VkDescriptorSetLayoutBinding binding;
	binding.binding = static_cast<uint32_t>(index);
	binding.descriptorCount = static_cast<uint32_t>(descriptor_count);
	binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	binding.stageFlags = shader_visibility;
	binding.pImmutableSamplers = nullptr;
	return binding;
}
template<size_t BindingsCount>
auto DescriptorSetLayout_WithBindings(const VkDescriptorSetLayoutBinding(&bindings)[BindingsCount])
{
	VkDescriptorSetLayoutCreateInfo dslinfo{};
	dslinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dslinfo.bindingCount = static_cast<uint32_t>(BindingsCount);
	dslinfo.pBindings = bindings;
	return dslinfo;
}
template<size_t LayoutCount>
auto PipelineLayout_WithDescriptorSetLayouts(const VkDescriptorSetLayout(&layouts)[LayoutCount])
{
	VkPipelineLayoutCreateInfo layoutinfo{};
	layoutinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutinfo.setLayoutCount = static_cast<uint32_t>(LayoutCount);
	layoutinfo.pSetLayouts = layouts;
	return layoutinfo;
}
template<typename ResourceStructure>
auto BufferInfo_UniformBuffer()
{
	VkBufferCreateInfo buffer_info{};

	buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	buffer_info.size = sizeof(ResourceStructure);
	buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	return buffer_info;
}
inline Result<VkMemoryAllocateInfo> MemoryAllocate_Mappable(const RenderBackend& backend, const VkMemoryRequirements& memreq)
{
	VkMemoryAllocateInfo alloc_info{};

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = memreq.size;
	alloc_info.memoryTypeIndex = UINT32_MAX;
	for(size_t i = 0; i < backend.getMemoryProperties().memoryTypeCount; i++)
	{
		auto& memtype = backend.getMemoryProperties().memoryTypes[i];
		if((memtype.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0)
		{
			alloc_info.memoryTypeIndex = static_cast<uint32_t>(i);
			break;
		}
	}
	if(alloc_info.memoryTypeIndex == UINT32_MAX) return Result<VkMemoryAllocateInfo>::err("mappable memory not found");
	return Result<VkMemoryAllocateInfo>::ok(alloc_info);
}
inline auto ShaderStage_VertexFromModule(const VkShaderModule& mod, const char* entry_point)
{
	VkPipelineShaderStageCreateInfo stage_info{};

	stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage_info.module = mod;
	stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
	stage_info.pName = entry_point;
	return stage_info;
}
inline auto ShaderStage_FragmentFromModule(const VkShaderModule& mod, const char* entry_point)
{
	VkPipelineShaderStageCreateInfo stage_info{};

	stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage_info.module = mod;
	stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stage_info.pName = entry_point;
	return stage_info;
}
template<typename Stride>
auto VertexInputBinding_PerVertex(size_t index)
{
	VkVertexInputBindingDescription vi_bind_desc{};
	vi_bind_desc.binding = static_cast<uint32_t>(index);
	vi_bind_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	vi_bind_desc.stride = static_cast<uint32_t>(sizeof(Stride));
	return vi_bind_desc;
}
template<typename Stride>
auto VertexInputBinding_PerInstance(size_t index)
{
	VkVertexInputBindingDescription vi_bind_desc{};
	vi_bind_desc.binding = static_cast<uint32_t>(index);
	vi_bind_desc.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
	vi_bind_desc.stride = static_cast<uint32_t>(sizeof(Stride));
	return vi_bind_desc;
}
inline auto DescriptorPoolSize_UniformBuffer(size_t size)
{
	VkDescriptorPoolSize ps{};
	ps.descriptorCount = static_cast<uint32_t>(size);
	ps.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	return ps;
}
template<typename BufferStructure>
auto DescriptorBufferInfo(const VkBuffer& buffer, VkDeviceSize offset = 0)
{
	VkDescriptorBufferInfo db_info;

	db_info.buffer = buffer;
	db_info.offset = offset;
	db_info.range = sizeof(BufferStructure);
	return db_info;
}
inline auto VertexInputAttribute(size_t bind_index, size_t location, VkFormat format, VkDeviceSize offset)
{
	VkVertexInputAttributeDescription iadesc;

	iadesc.binding = static_cast<uint32_t>(bind_index);
	iadesc.location = static_cast<uint32_t>(location);
	iadesc.format = format;
	iadesc.offset = static_cast<uint32_t>(offset);
	return iadesc;
}
template<size_t BindElementCount, size_t InputElementCount>
auto Pipeline_VertexInputState(const VkVertexInputBindingDescription(&bindings)[BindElementCount], const VkVertexInputAttributeDescription(&attributes)[InputElementCount])
{
	VkPipelineVertexInputStateCreateInfo state_info{};

	state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	state_info.vertexBindingDescriptionCount = static_cast<uint32_t>(BindElementCount);
	state_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(InputElementCount);
	state_info.pVertexBindingDescriptions = bindings;
	state_info.pVertexAttributeDescriptions = attributes;
	return state_info;
}
inline auto Pipeline_InputAssemblyState(VkPrimitiveTopology topo)
{
	VkPipelineInputAssemblyStateCreateInfo iainfo{};

	iainfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	iainfo.primitiveRestartEnable = VK_FALSE;
	iainfo.topology = topo;
	return iainfo;
}
inline auto ClearValue_Color(float r, float g, float b, float a = 1.0f)
{
	VkClearValue clear_value;
	clear_value.color.float32[0] = r;
	clear_value.color.float32[1] = g;
	clear_value.color.float32[2] = b;
	clear_value.color.float32[3] = a;
	return clear_value;
}
inline auto BufferInfo(VkBufferUsageFlags usage, VkDeviceSize size)
{
	VkBufferCreateInfo bufinfo{};

	bufinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufinfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufinfo.usage = usage;
	bufinfo.size = size;
	return bufinfo;
}
inline auto Viewport(float x, float y, float width, float height, float max_depth, float min_depth)
{
	VkViewport vport;

	vport.maxDepth = max_depth; vport.minDepth = min_depth;
	vport.x = x; vport.y = y;
	vport.width = width;
	vport.height = height;
	return vport;
}
inline auto Rect2D(int32_t x, int32_t y, uint32_t w, uint32_t h)
{
	VkRect2D r;

	r.offset.x = x; r.offset.y = y;
	r.extent.width = w; r.extent.height = h;
	return r;
}
template<size_t ViewportCount, size_t ScissorCount>
auto Pipeline_ViewportState(const VkViewport(&vps)[ViewportCount], const VkRect2D(&rect)[ScissorCount])
{
	VkPipelineViewportStateCreateInfo vpinfo{};
	vpinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	vpinfo.viewportCount = static_cast<uint32_t>(ViewportCount);
	vpinfo.pViewports = vps;
	vpinfo.scissorCount = static_cast<uint32_t>(ScissorCount);
	vpinfo.pScissors = rect;
	return vpinfo;
}
inline auto Pipeline_MultisampleDisable()
{
	VkPipelineMultisampleStateCreateInfo msinfo{};
	msinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	msinfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	msinfo.sampleShadingEnable = VK_FALSE;
	msinfo.alphaToCoverageEnable = VK_FALSE;
	msinfo.alphaToOneEnable = VK_FALSE;
	return msinfo;
}
inline auto BlendAttachment_Disabled()
{
	VkPipelineColorBlendAttachmentState blendinfo{};
	blendinfo.blendEnable = VK_FALSE;
	blendinfo.colorWriteMask = VK_COLOR_COMPONENT_A_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_R_BIT;
	return blendinfo;
}
template<size_t AttachmentCount>
auto Pipeline_BlendState(const VkPipelineColorBlendAttachmentState(&attachments)[AttachmentCount])
{
	VkPipelineColorBlendStateCreateInfo blendstate{};
	blendstate.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blendstate.logicOpEnable = VK_FALSE;
	blendstate.attachmentCount = static_cast<uint32_t>(AttachmentCount);
	blendstate.pAttachments = attachments;
	return blendstate;
}
inline auto Pipeline_NoDynamicStates()
{
	VkPipelineDynamicStateCreateInfo dynamicstate{};
	dynamicstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	return dynamicstate;
}
inline auto CommandBuffers_PrimaryFromPool(const VkCommandPool& pool, size_t count)
{
	VkCommandBufferAllocateInfo alloc_info{};

	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.commandPool = pool;
	alloc_info.commandBufferCount = static_cast<uint32_t>(count);
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	return alloc_info;
}
inline auto CommandBuffer_BundledFromPool(const VkCommandPool& pool)
{
	VkCommandBufferAllocateInfo info{};

	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	info.commandPool = pool;
	info.commandBufferCount = 1;
	info.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
	return info;
}
template<size_t ElementCount>
auto DescriptorPoolInfo(uint32_t max_sets, const VkDescriptorPoolSize(&sizes)[ElementCount])
{
	VkDescriptorPoolCreateInfo pool_info{};

	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.maxSets = max_sets;
	pool_info.poolSizeCount = static_cast<uint32_t>(ElementCount);
	pool_info.pPoolSizes = sizes;
	return pool_info;
}
template<size_t ElementCount>
auto DescriptorSetAllocateInfo(const VkDescriptorPool& pool, const VkDescriptorSetLayout(&layouts)[ElementCount])
{
	VkDescriptorSetAllocateInfo set_alloc_info{};
	set_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	set_alloc_info.descriptorPool = pool;
	set_alloc_info.descriptorSetCount = static_cast<uint32_t>(ElementCount);
	set_alloc_info.pSetLayouts = layouts;
	return set_alloc_info;
}
template<size_t ElementCount>
auto DescriptorSetWrite_UniformBuffers(const VkDescriptorBufferInfo(&buffer_infos)[ElementCount], const VkDescriptorSet& set, uint32_t dest_binding)
{
	VkWriteDescriptorSet write_data{};
	write_data.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write_data.dstSet = set;
	write_data.descriptorCount = static_cast<uint32_t>(ElementCount);
	write_data.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	write_data.pBufferInfo = buffer_infos;
	write_data.dstArrayElement = 0;
	write_data.dstBinding = dest_binding;
	return write_data;
}
template<typename ElementT>
auto PushConstantRange(uint32_t offset, VkShaderStageFlags visibility)
{
	VkPushConstantRange range;
	range.offset = offset;
	range.size = sizeof(ElementT);
	range.stageFlags = visibility;
	return range;
}
template<size_t LayoutCount, size_t ConstantCount>
auto PipelineLayout(const VkDescriptorSetLayout(&layouts)[LayoutCount], const VkPushConstantRange(&ranges)[ConstantCount])
{
	VkPipelineLayoutCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	info.setLayoutCount = static_cast<uint32_t>(LayoutCount);
	info.pushConstantRangeCount = static_cast<uint32_t>(ConstantCount);
	info.pSetLayouts = layouts;
	info.pPushConstantRanges = ranges;
	return info;
};

#endif //HARDGRAD_MOBILE_VKHELPER_H
