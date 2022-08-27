/*
* Vulkan descriptor set layout abstraction class
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

class DescriptorSetLayout {
private:
	VkDevice device;
	std::vector<VkDescriptorSetLayoutBinding> bindings;
public:
	VkDescriptorSetLayout handle = VK_NULL_HANDLE;
	DescriptorSetLayout(VkDevice device) {
		this->device = device;
	}
	~DescriptorSetLayout() {
		// @todo
	}
	void create() {
		VkDescriptorSetLayoutCreateInfo CI = vks::initializers::descriptorSetLayoutCreateInfo(bindings.data(), static_cast<uint32_t>(bindings.size()));
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &CI, nullptr, &handle));
	}
	void addBinding(VkDescriptorSetLayoutBinding binding) {
		bindings.push_back(binding);
	}
	void addBinding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t descriptorCount = 1) {
		VkDescriptorSetLayoutBinding setLayoutBinding{};
		setLayoutBinding.descriptorType = type;
		setLayoutBinding.stageFlags = stageFlags;
		setLayoutBinding.binding = binding;
		setLayoutBinding.descriptorCount = descriptorCount;
		bindings.push_back(setLayoutBinding);
	}
};