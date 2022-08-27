/*
* Descriptor pool abstraction class
*
* Copyright (C) by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include "vulkan/vulkan.h"
#include "VulkanInitializers.hpp"
#include "VulkanTools.h"

class DescriptorPool {
private:
	VkDevice device;
	std::vector<VkDescriptorPoolSize> poolSizes;
	uint32_t maxSets;
public:
	VkDescriptorPool handle;
	DescriptorPool(VkDevice device) {
		this->device = device;
	}
	~DescriptorPool() {
		// @todo
	}
	void create() {
		assert(poolSizes.size() > 0);
		assert(maxSets > 0);
		VkDescriptorPoolCreateInfo CI{};// = vks::initializers::descriptorPoolCreateInfo(poolSizes.size(), poolSizes.data(), 5 * 10);
		CI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		CI.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		CI.pPoolSizes = poolSizes.data();
		CI.maxSets = maxSets;
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &CI, nullptr, &handle));
	}
	void setMaxSets(uint32_t maxSets) {
		this->maxSets = maxSets;
	}
	void addPoolSize(VkDescriptorType type, uint32_t descriptorCount) {
		VkDescriptorPoolSize poolSize{};
		poolSize.type = type;
		poolSize.descriptorCount = descriptorCount;
		poolSizes.push_back(poolSize);
	}
};