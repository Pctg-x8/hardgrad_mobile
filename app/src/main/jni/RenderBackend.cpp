//
// Created by S on 2016/06/20.
//

#include "RenderBackend.h"
#include "utils.h"
#include "VkHelper.h"

void RenderBackend::init()
{
    uint32_t queue_family_index;

    RenderBackend::initInstance(&this->instance).assertFailure();
    auto hardware_count = RenderBackend::getHardwareCount(this->instance);
    Error::failIf(hardware_count <= 0, "Physical devices not found").assertFailure();
    auto hardwares = std::make_unique<VkPhysicalDevice[]>(hardware_count);
    RenderBackend::enumerateDevices(this->instance, hardwares).assertFailure();
    this->hardware = hardwares[0];
    this->diagnosisHardware();
    RenderBackend::initDevice(hardwares[0], &queue_family_index, &this->device).assertFailure();
    RenderBackend::initCommandPool(this->device, queue_family_index, &this->command_pool).assertFailure();
    vkGetDeviceQueue(this->device, queue_family_index, 0, &this->command_queue);

    return;
}

RenderBackend::~RenderBackend()
{
    vkDestroyCommandPool(this->device, this->command_pool, nullptr);
    vkDestroyDevice(this->device, nullptr);
    vkDestroyInstance(this->instance, nullptr);
}

void Swapchain::initForWindow(const RenderBackend& backend, ANativeWindow* window)
{
    VkResult res;

    this->backend_ref = &backend;

    const auto device_width = ANativeWindow_getWidth(window);
    const auto device_height = ANativeWindow_getHeight(window);

    const auto fpCreateAndroidSurfaceKHR = reinterpret_cast<PFN_vkCreateAndroidSurfaceKHR>(
            vkGetInstanceProcAddr(backend.getInstance(), "vkCreateAndroidSurfaceKHR")
    );
    Error::failIf(fpCreateAndroidSurfaceKHR == nullptr, "Cannot find function vkCreateAndroidSurfaceKHR").assertFailure();

    VkAndroidSurfaceCreateInfoKHR surface_info{};
    surface_info.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    surface_info.window = window;
    res = fpCreateAndroidSurfaceKHR(backend.getInstance(), &surface_info, nullptr, &this->surface);
    Error::failIf(res != VK_SUCCESS, "Unable to create surface for ANativeWindow").assertFailure();

    // Format check
    uint32_t format_count;
    res = vkGetPhysicalDeviceSurfaceFormatsKHR(backend.getHardware(), this->surface, &format_count, nullptr);
    Error::failIf(res != VK_SUCCESS, "Formats not found").assertFailure();
    auto formats = std::make_unique<VkSurfaceFormatKHR[]>(format_count);
    res = vkGetPhysicalDeviceSurfaceFormatsKHR(backend.getHardware(), this->surface, &format_count, formats.get());
    Error::failIf(res != VK_SUCCESS, "Formats enumeration error").assertFailure();
    this->format = (format_count == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
                    ? VK_FORMAT_R8G8B8A8_UNORM : formats[0].format;

    // Check Capabilities
    VkSurfaceCapabilitiesKHR surface_caps;
    res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(backend.getHardware(), this->surface, &surface_caps);
    Error::failIf(res != VK_SUCCESS, "Couldn't get surface capabilities").assertFailure();
    uint32_t present_mode_count;
    res = vkGetPhysicalDeviceSurfacePresentModesKHR(backend.getHardware(), this->surface, &present_mode_count, nullptr);
    Error::failIf(res != VK_SUCCESS, "Present Mode not found").assertFailure();
    auto present_modes = std::make_unique<VkPresentModeKHR[]>(present_mode_count);
    res = vkGetPhysicalDeviceSurfacePresentModesKHR(backend.getHardware(), this->surface, &present_mode_count, present_modes.get());
    Error::failIf(res != VK_SUCCESS, "Enumeration error: Present Modes").assertFailure();

    // Create swap chain
    VkExtent2D extent;
    if(surface_caps.currentExtent.width == static_cast<uint32_t>(-1))
    {
        // Undefined surface size -> Desired size
        extent.width = static_cast<uint32_t>(device_width);
        extent.height = static_cast<uint32_t>(device_height);
    }
    else
    {
        extent = surface_caps.currentExtent;
    }
    this->width = extent.width;
    this->height = extent.height;

    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    /*for(size_t i = 0; i < present_mode_count; i++)
    {
        // unsupported code for Android
        if(present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
        if((present_mode != VK_PRESENT_MODE_MAILBOX_KHR) && (present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR))
        {
            present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        }
    }*/

    uint32_t desired_swap_chain_images = surface_caps.minImageCount + 1;
    if((surface_caps.minImageCount > 0) && (desired_swap_chain_images > surface_caps.maxImageCount))
    {
        desired_swap_chain_images = surface_caps.maxImageCount;
    }

    const auto pre_transform = (surface_caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) != 0
                               ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : surface_caps.currentTransform;

    VkSwapchainCreateInfoKHR sc_info{};
    sc_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sc_info.surface = this->surface;
    sc_info.minImageCount = desired_swap_chain_images;
    sc_info.imageFormat = this->format;
    sc_info.imageExtent = extent;
    sc_info.preTransform = pre_transform;
    sc_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sc_info.imageArrayLayers = 1;
    sc_info.presentMode = present_mode;
    sc_info.oldSwapchain = VK_NULL_HANDLE;
    sc_info.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    sc_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sc_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    res = vkCreateSwapchainKHR(backend.getDevice(), &sc_info, nullptr, &this->swapchain);
    Error::failIf(res != VK_SUCCESS, "Unable to create swapchain").assertFailure();

    // Common Render Pass
    VkAttachmentDescription attachment{};
    attachment.format = this->format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference attachment_ref{};
    attachment_ref.attachment = 0;
    attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &attachment_ref;
    VkRenderPassCreateInfo rp_info{};
    rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp_info.attachmentCount = 1;
    rp_info.pAttachments = &attachment;
    rp_info.subpassCount = 1;
    rp_info.pSubpasses = &subpass;
    res = vkCreateRenderPass(backend.getDevice(), &rp_info, nullptr, &this->render_pass);
    Error::failIf(res != VK_SUCCESS, "Unable to create render pass").assertFailure();

    // ImageView/Framebuffer
    uint32_t bb_count;
    res = vkGetSwapchainImagesKHR(backend.getDevice(), this->swapchain, &bb_count, nullptr);
    Error::failIf(res != VK_SUCCESS, "Unable to get backbuffer count").assertFailure();
    this->backbuffer_count = bb_count;
    auto backbuffer_objects = std::make_unique<VkImage[]>(this->backbuffer_count);
    res = vkGetSwapchainImagesKHR(backend.getDevice(), this->swapchain, &bb_count, backbuffer_objects.get());
    this->backbuffer_frames = std::make_unique<BackBufferFrame[]>(this->backbuffer_count);
    for(size_t i = 0; i < this->backbuffer_count; i++)
    {
        // Image View
        VkImageViewCreateInfo iv_info{};
        iv_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        iv_info.format = this->format;
        iv_info.components.r = VK_COMPONENT_SWIZZLE_R;
        iv_info.components.g = VK_COMPONENT_SWIZZLE_G;
        iv_info.components.b = VK_COMPONENT_SWIZZLE_B;
        iv_info.components.a = VK_COMPONENT_SWIZZLE_A;
        iv_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        iv_info.subresourceRange.levelCount = 1;
        iv_info.subresourceRange.layerCount = 1;
        iv_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        iv_info.image = backbuffer_objects[i];
        this->backbuffer_frames[i].backend_ref = &backend;
        this->backbuffer_frames[i].resource = backbuffer_objects[i];
        res = vkCreateImageView(backend.getDevice(), &iv_info, nullptr, &this->backbuffer_frames[i].resource_view);
        Error::failIf(res != VK_SUCCESS, "Unable to create image view").assertFailure();

        // Framebuffer
        VkFramebufferCreateInfo fb_info{};
        fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb_info.renderPass = this->render_pass;
        fb_info.attachmentCount = 1;
        fb_info.pAttachments = &this->backbuffer_frames[i].resource_view;
        fb_info.width = sc_info.imageExtent.width;
        fb_info.height = sc_info.imageExtent.height;
        fb_info.layers = 1;
        res = vkCreateFramebuffer(backend.getDevice(), &fb_info, nullptr, &this->backbuffer_frames[i].framebuffer);
        Error::failIf(res != VK_SUCCESS, "Unable to create framebuffer").assertFailure();
    }
}
Swapchain::~Swapchain()
{
    vkDestroyRenderPass(this->backend_ref->getDevice(), this->render_pass, nullptr);
    vkDestroySwapchainKHR(this->backend_ref->getDevice(), this->swapchain, nullptr);
    vkDestroySurfaceKHR(this->backend_ref->getInstance(), this->surface, nullptr);
}
Swapchain::BackBufferFrame::~BackBufferFrame()
{
    vkDestroyFramebuffer(this->backend_ref->getDevice(), this->framebuffer, nullptr);
    vkDestroyImageView(this->backend_ref->getDevice(), this->resource_view, nullptr);
}

void RenderBackend::diagnosisHardware()
{
    vkGetPhysicalDeviceMemoryProperties(this->hardware, &this->memory_properties);
}

// Factory Methods //
Error RenderBackend::initInstance(VkInstance* instance_ref)
{
    static const char* layers[] = { /*"VK_LAYER_LUNARG_standard_validation"*/ };
    static const char* extensions[] = { "VK_KHR_surface", "VK_KHR_android_surface"/*, "VK_EXT_debug_report"*/ };
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "HardGrad_Mobile";
    app_info.applicationVersion = 1;
    app_info.pEngineName = "HardGrad-RenderBackend";
    app_info.engineVersion = 1;
    app_info.apiVersion = VK_API_VERSION_1_0;
    VkInstanceCreateInfo instance_info{};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app_info;
    instance_info.enabledLayerCount = /*array_size(layers)*/0;
    instance_info.ppEnabledLayerNames = /*layers*/nullptr;
    instance_info.enabledExtensionCount = array_size(extensions);
    instance_info.ppEnabledExtensionNames = extensions;

    auto res = vkCreateInstance(&instance_info, nullptr, instance_ref);
    return Error::failIf(res != VK_SUCCESS, "Unable to create device");
}
uint32_t RenderBackend::getHardwareCount(const VkInstance& instance)
{
    uint32_t phys_device_num;
    auto res = vkEnumeratePhysicalDevices(instance, &phys_device_num, nullptr);
    return phys_device_num;
}
Error RenderBackend::enumerateDevices(const VkInstance& instance, std::unique_ptr<VkPhysicalDevice[]>& device_array)
{
    uint32_t phys_device_num;
    auto res = vkEnumeratePhysicalDevices(instance, &phys_device_num, device_array.get());
    return Error::failIf(res != VK_SUCCESS, "Enumeration error");
}
Error RenderBackend::initDevice(const VkPhysicalDevice& hardware, uint32_t* queue_family_index_ref, VkDevice* device_ref)
{
    uint32_t queue_count, queue_family_index = UINT32_MAX;
    vkGetPhysicalDeviceQueueFamilyProperties(hardware, &queue_count, nullptr);
    if(queue_count <= 0) return Error::fail("Queue not found");
    auto queue_properties = std::make_unique<VkQueueFamilyProperties[]>(queue_count);
    vkGetPhysicalDeviceQueueFamilyProperties(hardware, &queue_count, queue_properties.get());
    for(uint32_t i = 0; i < queue_count; i++)
    {
        if((queue_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
        {
            queue_family_index = i;
            break;
        }
    }
    if(queue_family_index >= queue_count) return Error::fail("Matching queue not found");
    *queue_family_index_ref = queue_family_index;

    const char* dev_extensions[] = { "VK_KHR_swapchain" };
    static float queue_priorities[] = { 0.0f };
    VkDeviceQueueCreateInfo queue_info{};
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueCount = 1;
    queue_info.queueFamilyIndex = queue_family_index;
    queue_info.pQueuePriorities = queue_priorities;
    VkDeviceCreateInfo device_info{};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue_info;
    device_info.enabledExtensionCount = array_size(dev_extensions);
    device_info.ppEnabledExtensionNames = dev_extensions;
    auto res = vkCreateDevice(hardware, &device_info, nullptr, device_ref);
    return Error::failIf(res != VK_SUCCESS, "Unable to create device");
}
Error RenderBackend::initCommandPool(const VkDevice& device, uint32_t queue_family_index, VkCommandPool* pool_ref)
{
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = queue_family_index;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    auto res = vkCreateCommandPool(device, &pool_info, nullptr, pool_ref);
    return Error::failIf(res != VK_SUCCESS, "Unable to create command pool");
}
Result<VkDeviceMemory> RenderBackend::allocateMemoryForBuffer(const VkBuffer& buffer) const
{
	const auto memory_req = this->getMemoryRequirementsForBuffer(buffer);
    const auto alloc_info = MemoryAllocate_Mappable(*this, memory_req).unwrap();
    LogWrite("Memory Allocation: %u bytes", static_cast<uint32_t>(alloc_info.allocationSize));
	auto mem = this->allocateMemory(alloc_info).unwrap();
	auto res = vkBindBufferMemory(this->device, buffer, mem, 0);
	return res != VK_SUCCESS ? Result<VkDeviceMemory>::err("Unable to bind buffer memory") : Result<VkDeviceMemory>::ok(mem);
}
