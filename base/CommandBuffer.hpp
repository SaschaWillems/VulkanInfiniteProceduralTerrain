/*
* Command buffer abstraction class
*
* Copyright (C) by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include "vulkan/vulkan.h"
#include "VulkanInitializers.hpp"
#include "VulkanTools.h"
#include "DescriptorSet.hpp"
#include "Pipeline.hpp"
#include "PipelineLayout.hpp"
#include "CommandPool.hpp"

class CommandBuffer {
private:
	VkDevice device;
	CommandPool *pool = nullptr;
	VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
public:
	VkCommandBuffer handle;
	CommandBuffer(VkDevice device) {
		this->device = device;
	}
	~CommandBuffer() {
		vkFreeCommandBuffers(device, pool->handle, 1, &handle);
	}
	void create() {
		assert(pool);
		VkCommandBufferAllocateInfo AI = vks::initializers::commandBufferAllocateInfo(pool->handle, level, 1);
		VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &AI, &handle));
	}
	void setPool(CommandPool* pool) {
		this->pool = pool;
	}
	void setLevel(VkCommandBufferLevel level) {
		this->level = level;
	}
	void begin() {
		VkCommandBufferBeginInfo beginInfo = vks::initializers::commandBufferBeginInfo();
		VK_CHECK_RESULT(vkBeginCommandBuffer(handle, &beginInfo));
	}
	void end() {
		VK_CHECK_RESULT(vkEndCommandBuffer(handle));
	}
	void setViewport(float x, float y, float width, float height, float minDepth, float maxDepth) {
		VkViewport viewport = { x, y, width, height, minDepth, maxDepth };
		vkCmdSetViewport(handle, 0, 1, &viewport);
	}
	void setScissor(int32_t offsetx, int32_t offsety, uint32_t width, uint32_t height) {
		VkRect2D scissor = { offsetx, offsety, width, height };
		vkCmdSetScissor(handle, 0, 1, &scissor);
	}
	void bindDescriptorSets(PipelineLayout* layout, std::vector<DescriptorSet*> sets, uint32_t firstSet = 0) {
		std::vector<VkDescriptorSet> descSets;
		for (auto set : sets) {
			descSets.push_back(set->handle);
		}
		vkCmdBindDescriptorSets(handle, VK_PIPELINE_BIND_POINT_GRAPHICS, layout->handle, firstSet, static_cast<uint32_t>(descSets.size()), descSets.data(), 0, nullptr);
	}
	void bindPipeline(Pipeline* pipeline) {
		vkCmdBindPipeline(handle, pipeline->getBindPoint(), pipeline->getHandle());
	}
	void draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
		vkCmdDraw(handle, 6, 1, 0, 0);
	}
	void updatePushConstant(PipelineLayout *layout, uint32_t index, const void* values) {
		VkPushConstantRange pushConstantRange = layout->getPushConstantRange(index);
		vkCmdPushConstants(handle, layout->handle, pushConstantRange.stageFlags, pushConstantRange.offset, pushConstantRange.size, values);
	}
};