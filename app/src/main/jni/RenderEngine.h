//
// Created by S on 2016/06/21.
//

#ifndef HARDGRAD_MOBILE_RENDERENGINE_H
#define HARDGRAD_MOBILE_RENDERENGINE_H

#include "RenderBackend.h"
#include "WireRender.h"
#include "Enemy.h"
#include <list>
#include <random>
#include "Background.h"

class RenderEngine;

class RenderResources final
{
    const RenderBackend* backend_ref;
    VkDeviceMemory resource_mem;
    VkBuffer meshstore_buffer;
    VkDeviceSize unit_cube_vertices_start, unit_cube_indices_start;
	VkDeviceSize enemy_guard_vertices_start, enemy_guard_indices_start;
	VkDeviceSize background_vertices_start, background_indices_start;
public:
    ~RenderResources();
    void init(const RenderBackend& backend);

    auto& getMeshstoreBuffer() const { return this->meshstore_buffer; }
    auto& getUnitCubeVerticesStart() const { return this->unit_cube_vertices_start; }
    auto& getUnitCubeIndicesStart() const { return this->unit_cube_indices_start; }
	auto& getEnemyGuardVerticesStart() const { return this->enemy_guard_vertices_start; }
	auto& getEnemyGuardIndicesStart() const { return this->enemy_guard_indices_start; }
	auto& getBackgroundVerticesStart() const { return this->background_vertices_start; }
	auto& getBackgroundIndicesStart() const { return this->background_indices_start; }
};

class RenderEngine final
{
	ProjectionMatrixes p_matrixes;
	EnemyRendererDescriptorSetLayouts er_common_layout;
	std::unique_ptr<EnemyBufferBlocks> enemy_buffer_blocks;
	std::list<std::unique_ptr<Enemy>> enemy_objects;
	size_t current_enemy_count;
	std::unique_ptr<BackgroundBufferBlocks> background_buffer_blocks;
	std::list<std::unique_ptr<Background>> background_objects;
	size_t current_background_count;
	std::mt19937 randomizer;
    RenderEngine() = default;

	int64_t start_ns;
	void populatePrimaryCommands(bool reset);
public:
    static RenderEngine instance;
    bool readyForRender = false;
    VkSemaphore present_completion_semaphore;
    VkFence device_fence;
    Swapchain swapchain;
    RenderBackend backend;
    RenderResources resource;
	WireRender wire_render_common;
	EnemyBodyRender ebody_render;
	EnemyChildRender echild_render;
	BackgroundRender background_render;
    std::unique_ptr<VkCommandBuffer[]> command_buffers;

    void init(android_app* app);
    ~RenderEngine();
	void update();
    void render_frame();

	auto getEnemyBufferBlocks() const { return this->enemy_buffer_blocks.get(); }
	auto getBackgroundBufferBlocks() const { return this->background_buffer_blocks.get(); }
	auto& getEnemyRendererCommonLayout() const { return this->er_common_layout; }
	auto& getProjectionMatrixes() const { return this->p_matrixes; }
};


#endif //HARDGRAD_MOBILE_RENDERENGINE_H
