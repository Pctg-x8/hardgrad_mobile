//
// Created by S on 2016/06/21.
//

#include "RenderEngine.h"
#include "utils.h"
#include "VkHelper.h"

RenderEngine RenderEngine::instance;

constexpr auto Nanosecs(const timespec& t)
{
	return static_cast<int64_t>(t.tv_sec * 1000LL * 1000LL * 1000LL) + t.tv_nsec;
}

void RenderEngine::init(android_app* app)
{
    this->backend.init();
    this->swapchain.initForWindow(this->backend, app->window);
    this->resource.init(this->backend);

    // Synchronizers
    VkSemaphoreCreateInfo seminfo{};
    seminfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    auto res = vkCreateSemaphore(this->backend.getDevice(), &seminfo, nullptr, &this->present_completion_semaphore);
    Error::failIf(res != VK_SUCCESS, "Unable to create semaphore").assertFailure();
    VkFenceCreateInfo fenceinfo{};
    fenceinfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    res = vkCreateFence(this->backend.getDevice(), &fenceinfo, nullptr, &this->device_fence);
    Error::failIf(res != VK_SUCCESS, "Unable to create fence").assertFailure();

	// Init game timer
	timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	this->start_ns = Nanosecs(t);
	this->randomizer = std::mt19937(std::random_device()());

	// init pipelines
	this->p_matrixes.init(backend, this->swapchain.getWidth(), this->swapchain.getHeight());
	this->er_common_layout.init(backend);
	this->wire_render_common.init(app, backend);
	this->ebody_render.init(app, this->wire_render_common, this->er_common_layout, this->swapchain.getWidth(), this->swapchain.getHeight(), this->swapchain.getRenderPass(), backend);
	this->echild_render.init(app, this->wire_render_common, this->er_common_layout, this->swapchain.getWidth(), this->swapchain.getHeight(), this->swapchain.getRenderPass(), backend);
	this->background_render.init(app, this->wire_render_common, this->swapchain.getWidth(), this->swapchain.getHeight(), this->swapchain.getRenderPass(), backend);

	// Init game object
	this->enemy_buffer_blocks = std::make_unique<EnemyBufferBlocks>();
	this->enemy_buffer_blocks->init(backend);
	this->background_buffer_blocks = std::make_unique<BackgroundBufferBlocks>();
	this->background_buffer_blocks->init(backend);
	this->current_enemy_count = 0;
	this->current_background_count = 0;

    // Create CommandBuffers and Populate commands
    this->command_buffers = std::make_unique<VkCommandBuffer[]>(this->swapchain.count());
	const auto alloc_info = CommandBuffers_PrimaryFromPool(backend.getCommandPool(), this->swapchain.count());
    res = vkAllocateCommandBuffers(this->backend.getDevice(), &alloc_info, this->command_buffers.get());
	Error::failIf(res != VK_SUCCESS, "Unable to allocate command buffers").assertFailure();
	this->populatePrimaryCommands(false);

    this->readyForRender = true;
}
RenderEngine::~RenderEngine()
{
    vkFreeCommandBuffers(this->backend.getDevice(), this->backend.getCommandPool(), this->swapchain.count(), this->command_buffers.get());
    vkDestroyFence(this->backend.getDevice(), this->device_fence, nullptr);
    vkDestroySemaphore(this->backend.getDevice(), this->present_completion_semaphore, nullptr);
}
void RenderEngine::populatePrimaryCommands(bool reset)
{
	std::vector<VkCommandBuffer> buffers;
	buffers.reserve(this->current_enemy_count + this->current_background_count);
	for(const auto& e : this->background_objects) buffers.push_back(e->getBundledCommands());
	for(const auto& e : this->enemy_objects) buffers.push_back(e->getBundledCommands());

#pragma omp parallel for
	for(int i = 0; i < this->swapchain.count(); i++)
	{
		if(reset)
		{
			auto res = vkResetCommandBuffer(this->command_buffers[i], 0);
			Error::failIf(res != VK_SUCCESS, "Unable to reset command buffer").assertFailure();
		}

		VkCommandBufferBeginInfo beginfo{};
		beginfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		auto res = vkBeginCommandBuffer(this->command_buffers[i], &beginfo);
		Error::failIf(res != VK_SUCCESS, "Unable to begin command recording").assertFailure();

		// Resource Barrier
		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.layerCount = 1;
		barrier.image = this->swapchain.getBackBufferResource(i);
		vkCmdPipelineBarrier(this->command_buffers[i], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		                     VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
		const auto clear_value = ClearValue_Color(0.0f, 0.0f, 0.0f, 1.0f);
		VkRenderPassBeginInfo rp_begin{};
		rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		rp_begin.renderPass = this->swapchain.getRenderPass();
		rp_begin.framebuffer = this->swapchain.getFramebuffer(i);
		rp_begin.renderArea.offset.x = 0; rp_begin.renderArea.offset.y = 0;
		rp_begin.renderArea.extent.width = this->swapchain.getWidth();
		rp_begin.renderArea.extent.height = this->swapchain.getHeight();
		rp_begin.clearValueCount = 1;
		rp_begin.pClearValues = &clear_value;
		vkCmdBeginRenderPass(this->command_buffers[i], &rp_begin, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
		vkCmdExecuteCommands(this->command_buffers[i], static_cast<uint32_t>(this->current_enemy_count + this->current_background_count), buffers.data());
		vkCmdEndRenderPass(this->command_buffers[i]);

		res = vkEndCommandBuffer(this->command_buffers[i]);
		Error::failIf(res != VK_SUCCESS, "Unable to end command recording").assertFailure();
	}
}
void RenderEngine::update()
{
	timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	const auto elapsed = static_cast<double>(Nanosecs(t) - this->start_ns) / 1000.0 / 1000.0;

	// Appear object at random //
	bool requireRepopulate = false;
	std::uniform_int_distribution<uint8_t> ud(0, 60);
	std::uniform_int_distribution<uint8_t> ud_b(0, 30);
	std::uniform_real_distribution<float> xud(-8.0f, 8.0f);
	std::uniform_real_distribution<float> xud_b(-10.0f, 10.0f);
	std::uniform_real_distribution<float> sud_b(1.0f, 2.5f);
	std::uniform_int_distribution<uint8_t> iud_b(4, 10);
	if(ud(this->randomizer) == 0)
	{
		this->enemy_objects.emplace_back(std::make_unique<Enemy>(this, elapsed, xud(this->randomizer)));
		this->current_enemy_count++;
		requireRepopulate = true;
	}
	if(ud_b(this->randomizer) == 0)
	{
		this->background_objects.emplace_back(std::make_unique<Background>(this, elapsed, xud_b(this->randomizer), sud_b(this->randomizer), iud_b(this->randomizer)));
		this->current_background_count++;
		requireRepopulate = true;
	}
	if(requireRepopulate) this->populatePrimaryCommands(true);

	uint8_t* buffer_ptr;
	this->enemy_buffer_blocks->map(0, this->enemy_buffer_blocks->getTotalSize(), reinterpret_cast<void**>(&buffer_ptr)).assertFailure();
	#pragma omp parallel for
	for(const auto& e : this->enemy_objects) e->update(elapsed, buffer_ptr);
	this->enemy_buffer_blocks->unmap();
	this->background_buffer_blocks->map(0, this->background_buffer_blocks->getTotalSize(), reinterpret_cast<void**>(&buffer_ptr)).assertFailure();
	#pragma omp parallel for
	for(const auto& e : this->background_objects) e->update(elapsed, buffer_ptr);
	this->background_buffer_blocks->unmap();
}
void RenderEngine::render_frame()
{
    uint32_t current_buffer_index;

    auto res = vkAcquireNextImageKHR(this->backend.getDevice(), this->swapchain.get(), UINT64_MAX,
                                     this->present_completion_semaphore, VK_NULL_HANDLE, &current_buffer_index);
    Error::failIf(res != VK_SUCCESS, "Failed to acquire next image index").assertFailure();
    const VkPipelineStageFlags pipeline_stage_flag = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    VkSubmitInfo subinfo{};
    subinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    subinfo.waitSemaphoreCount = 1;
    subinfo.pWaitSemaphores = &this->present_completion_semaphore;
    subinfo.pWaitDstStageMask = &pipeline_stage_flag;
    subinfo.commandBufferCount = 1;
    subinfo.pCommandBuffers = &this->command_buffers[current_buffer_index];
    res = vkQueueSubmit(this->backend.getQueue(), 1, &subinfo, this->device_fence);
    Error::failIf(res != VK_SUCCESS, "Failed to submit command list").assertFailure();

    // Present
    VkPresentInfoKHR pinfo{};
    pinfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pinfo.swapchainCount = 1;
    pinfo.pSwapchains = &this->swapchain.get();
    pinfo.pImageIndices = &current_buffer_index;

    // Wait for command completion
    do { res = vkWaitForFences(this->backend.getDevice(), 1, &this->device_fence, VK_TRUE, 1000 * 1000 * 1000); } while(res == VK_TIMEOUT);
    res = vkQueuePresentKHR(this->backend.getQueue(), &pinfo);
    Error::failIf(res != VK_SUCCESS, "Error in Presenting buffer").assertFailure();
}

void RenderResources::init(const RenderBackend& backend)
{
    this->backend_ref = &backend;

    // Create Buffer
	const auto buffer_t = backend.createBuffer(BufferInfo(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, sizeof(uint16_t) * 24)).unwrap();
	const auto memory_req = backend.getMemoryRequirementsForBuffer(buffer_t);
	vkDestroyBuffer(backend.getDevice(), buffer_t, nullptr);

	const auto ucv_size_aligned = Alignment(sizeof(Position) * 8, memory_req.alignment);
	const auto uci_size_aligned = Alignment(sizeof(uint16_t) * 24, memory_req.alignment);
	const auto egv_size_aligned = Alignment(sizeof(Position) * 5, memory_req.alignment);
	const auto egi_size_aligned = Alignment(sizeof(uint16_t) * 16, memory_req.alignment);
	const auto bgv_size_aligned = Alignment(sizeof(Position) * 4, memory_req.alignment);
	const auto bgi_size_aligned = Alignment(sizeof(uint16_t) * 8, memory_req.alignment);
	const auto meshstore_size = ucv_size_aligned + uci_size_aligned + egv_size_aligned + egi_size_aligned + bgv_size_aligned + bgi_size_aligned;
	this->meshstore_buffer = backend.createBuffer(BufferInfo(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, meshstore_size)).unwrap();
	this->resource_mem = backend.allocateMemoryForBuffer(this->meshstore_buffer).unwrap();

    // Placement
    this->unit_cube_vertices_start = 0;
    this->unit_cube_indices_start = this->unit_cube_vertices_start + ucv_size_aligned;
	this->enemy_guard_vertices_start = this->unit_cube_indices_start + uci_size_aligned;
	this->enemy_guard_indices_start = this->enemy_guard_vertices_start + egv_size_aligned;
	this->background_vertices_start = this->enemy_guard_indices_start + egi_size_aligned;
	this->background_indices_start = this->background_vertices_start + bgv_size_aligned;

    // Set Buffer Data
	uint8_t* mapped_ptr;
	auto res = vkMapMemory(backend.getDevice(), this->resource_mem, 0, meshstore_size, 0, reinterpret_cast<void**>(&mapped_ptr));
	Error::failIf(res != VK_SUCCESS, "Unable to map device memory").assertFailure();

	const auto ucv_ptr = reinterpret_cast<Position*>(mapped_ptr + this->unit_cube_vertices_start);
	ucv_ptr[0] = Position { -1.0f, -1.0f, -1.0f };
	ucv_ptr[1] = Position {  1.0f, -1.0f, -1.0f };
	ucv_ptr[2] = Position {  1.0f,  1.0f, -1.0f };
	ucv_ptr[3] = Position { -1.0f,  1.0f, -1.0f };
	ucv_ptr[4] = Position { -1.0f, -1.0f,  1.0f };
	ucv_ptr[5] = Position {  1.0f, -1.0f,  1.0f };
	ucv_ptr[6] = Position {  1.0f,  1.0f,  1.0f };
	ucv_ptr[7] = Position { -1.0f,  1.0f,  1.0f };
	const auto uci_ptr = reinterpret_cast<uint16_t*>(mapped_ptr + this->unit_cube_indices_start);
	uci_ptr[2 *  0 + 0] = 0; uci_ptr[2 *  0 + 1] = 1;
	uci_ptr[2 *  1 + 0] = 1; uci_ptr[2 *  1 + 1] = 2;
	uci_ptr[2 *  2 + 0] = 2; uci_ptr[2 *  2 + 1] = 3;
	uci_ptr[2 *  3 + 0] = 3; uci_ptr[2 *  3 + 1] = 0;
	uci_ptr[2 *  4 + 0] = 4; uci_ptr[2 *  4 + 1] = 5;
	uci_ptr[2 *  5 + 0] = 5; uci_ptr[2 *  5 + 1] = 6;
	uci_ptr[2 *  6 + 0] = 6; uci_ptr[2 *  6 + 1] = 7;
	uci_ptr[2 *  7 + 0] = 7; uci_ptr[2 *  7 + 1] = 4;
	uci_ptr[2 *  8 + 0] = 0; uci_ptr[2 *  8 + 1] = 4;
	uci_ptr[2 *  9 + 0] = 1; uci_ptr[2 *  9 + 1] = 5;
	uci_ptr[2 * 10 + 0] = 2; uci_ptr[2 * 10 + 1] = 6;
	uci_ptr[2 * 11 + 0] = 3; uci_ptr[2 * 11 + 1] = 7;
	const auto egv_ptr = reinterpret_cast<Position*>(mapped_ptr + this->enemy_guard_vertices_start);
	egv_ptr[0] = Position { -0.75f,  0.5f,   0.0f };
	egv_ptr[1] = Position {   0.0f,  0.5f, -0.75f };
	egv_ptr[2] = Position {  0.75f,  0.5f,   0.0f };
	egv_ptr[3] = Position {   0.0f,  0.5f,  0.75f };
	egv_ptr[4] = Position {   0.0f, -0.5f,   0.0f };
	const auto egi_ptr = reinterpret_cast<uint16_t*>(mapped_ptr + this->enemy_guard_indices_start);
	egi_ptr[2 * 0 + 0] = 0; egi_ptr[2 * 0 + 1] = 1;
	egi_ptr[2 * 1 + 0] = 1; egi_ptr[2 * 1 + 1] = 2;
	egi_ptr[2 * 2 + 0] = 2; egi_ptr[2 * 2 + 1] = 3;
	egi_ptr[2 * 3 + 0] = 3; egi_ptr[2 * 3 + 1] = 0;
	egi_ptr[2 * 4 + 0] = 4; egi_ptr[2 * 4 + 1] = 0;
	egi_ptr[2 * 5 + 0] = 4; egi_ptr[2 * 5 + 1] = 1;
	egi_ptr[2 * 6 + 0] = 4; egi_ptr[2 * 6 + 1] = 2;
	egi_ptr[2 * 7 + 0] = 4; egi_ptr[2 * 7 + 1] = 3;
	const auto bgv_ptr = reinterpret_cast<Position*>(mapped_ptr + this->background_vertices_start);
	bgv_ptr[0] = Position { -1.0f,  1.0f, 0.0f };
	bgv_ptr[1] = Position {  1.0f,  1.0f, 0.0f };
	bgv_ptr[2] = Position {  1.0f, -1.0f, 0.0f };
	bgv_ptr[3] = Position { -1.0f, -1.0f, 0.0f };
	const auto bgi_ptr = reinterpret_cast<uint16_t*>(mapped_ptr + this->background_indices_start);
	bgi_ptr[0 * 2 + 0] = 0; bgi_ptr[0 * 2 + 1] = 1;
	bgi_ptr[1 * 2 + 0] = 1; bgi_ptr[1 * 2 + 1] = 2;
	bgi_ptr[2 * 2 + 0] = 2; bgi_ptr[2 * 2 + 1] = 3;
	bgi_ptr[3 * 2 + 0] = 3; bgi_ptr[3 * 2 + 1] = 0;

	vkUnmapMemory(backend.getDevice(), this->resource_mem);
}
RenderResources::~RenderResources()
{
    vkDestroyBuffer(this->backend_ref->getDevice(), this->meshstore_buffer, nullptr);
    vkFreeMemory(this->backend_ref->getDevice(), this->resource_mem, nullptr);
}
