//
// Created by S on 2016/06/20.
//

#ifndef HARDGRAD_MOBILE_RENDERBACKEND_H
#define HARDGRAD_MOBILE_RENDERBACKEND_H

#include <memory>
#include "functions.h"
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <assert.h>
#include "android_native_app_glue.h"
#include <android/log.h>
#include "Errors.h"
#include "Binary.h"

struct VertexData
{
    float pos[3];
    float col[4];
};
struct Position { float x, y, z; };

class RenderBackend final
{
    VkInstance instance;
    VkPhysicalDevice hardware;
    VkDevice device;
    VkCommandPool command_pool;
    VkQueue command_queue;

    VkPhysicalDeviceMemoryProperties memory_properties;

    static Error initInstance(VkInstance* instance_ref);
    static uint32_t getHardwareCount(const VkInstance& instance);
    static Error enumerateDevices(const VkInstance& instance, std::unique_ptr<VkPhysicalDevice[]>& device_array);
    static Error initDevice(const VkPhysicalDevice& hardware, uint32_t* queue_family_index_ref, VkDevice* device_ref);
    static Error initCommandPool(const VkDevice& device, uint32_t queue_family_index, VkCommandPool* pool_ref);
    void diagnosisHardware();
public:
    RenderBackend() = default;
    ~RenderBackend();
    void init();

    auto getInstance() const { return this->instance; }
    auto getHardware() const { return this->hardware; }
    auto getDevice() const { return this->device; }
    auto getCommandPool() const { return this->command_pool; }
    auto getQueue() const { return this->command_queue; }
    auto& getMemoryProperties() const { return this->memory_properties; }

    auto getMemoryRequirementsForBuffer(const VkBuffer& buffer) const
    {
        VkMemoryRequirements memreq;
        vkGetBufferMemoryRequirements(this->device, buffer, &memreq);
        return memreq;
    }
	auto createBuffer(const VkBufferCreateInfo& info) const
	{
		VkBuffer buf;
		auto res = vkCreateBuffer(this->device, &info, nullptr, &buf);
		return res != VK_SUCCESS ? Result<VkBuffer>::err("Unable to create buffer object") : Result<VkBuffer>::ok(buf);
	}
	auto allocateMemory(const VkMemoryAllocateInfo& info) const
	{
		VkDeviceMemory mem;
		auto res = vkAllocateMemory(this->device, &info, nullptr, &mem);
		return res != VK_SUCCESS ? Result<VkDeviceMemory>::err("Unable to allocate device memory") : Result<VkDeviceMemory>::ok(mem);
	}
	auto createShaderModule(const Binary& bin) const
	{
		VkShaderModuleCreateInfo shaderinfo{};
		shaderinfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		shaderinfo.codeSize = bin.size;
		shaderinfo.pCode = reinterpret_cast<uint32_t*>(bin.bytes.get());
		VkShaderModule mod;
		auto res = vkCreateShaderModule(this->device, &shaderinfo, nullptr, &mod);
		return res != VK_SUCCESS ? Result<VkShaderModule>::err("Unable to create shader module") : Result<VkShaderModule>::ok(mod);
	}
	Result<VkDeviceMemory> allocateMemoryForBuffer(const VkBuffer& buffer) const;
};

class Swapchain final
{
    struct BackBufferFrame
    {
        const RenderBackend* backend_ref;
        VkImage resource;
        VkImageView resource_view;
        VkFramebuffer framebuffer;

        ~BackBufferFrame();
    };

    const RenderBackend* backend_ref;
    VkSurfaceKHR surface;
    VkFormat format;
    int32_t width, height;
    VkSwapchainKHR swapchain;
    size_t backbuffer_count;
    std::unique_ptr<BackBufferFrame[]> backbuffer_frames;
    VkRenderPass render_pass;
public:
    void initForWindow(const RenderBackend& backend, ANativeWindow* window);
    ~Swapchain();

    auto getFormat() const { return this->format; }
    auto& get() const { return this->swapchain; }
    auto getRenderPass() const { return this->render_pass; }
    auto count() const { return this->backbuffer_count; }
    auto getWidth() const { return this->width; }
    auto getHeight() const { return this->height; }
    auto getBackBufferResource(size_t i) const { return this->backbuffer_frames[i].resource; }
    auto getFramebuffer(size_t i) const { return this->backbuffer_frames[i].framebuffer; }
};

#endif //HARDGRAD_MOBILE_RENDERBACKEND_H
