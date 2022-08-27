/*
* Vulkan pipeline layout abstraction class
*
* Copyright (C) by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <vector>
#include "vulkan/vulkan.h"
#include "VulkanInitializers.hpp"
#include "VulkanTools.h"
#include "DescriptorSetLayout.hpp"

class PipelineLayout {
private:
	VkDevice device = VK_NULL_HANDLE;
	std::vector<VkDescriptorSetLayout> layouts;
	std::vector<VkPushConstantRange> pushConstantRanges;
public:
	VkPipelineLayout handle;
	PipelineLayout(VkDevice device) {
		this->device = device;
	}
	~PipelineLayout() {
		// @todo
	}
	void create() {
		VkPipelineLayoutCreateInfo CI = vks::initializers::pipelineLayoutCreateInfo(layouts.data(), static_cast<uint32_t>(layouts.size()));
		CI.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
		CI.pPushConstantRanges = pushConstantRanges.data();
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &CI, nullptr, &handle));
	}
	void addLayout(VkDescriptorSetLayout layout) {
		layouts.push_back(layout);
	}
	void addLayout(DescriptorSetLayout* layout) {
		layouts.push_back(layout->handle);
	}
	void addPushConstantRange(uint32_t size, uint32_t offset, VkShaderStageFlags stageFlags) {
		VkPushConstantRange pushConstantRange{};
		pushConstantRange.stageFlags = stageFlags;
		pushConstantRange.offset = offset;
		pushConstantRange.size = size;
		pushConstantRanges.push_back(pushConstantRange);
	}
	VkPushConstantRange getPushConstantRange(uint32_t index) {
		assert(index < pushConstantRanges.size());
		return pushConstantRanges[index];
	}
};