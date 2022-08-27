/*
* Vulkan descriptor set abstraction class
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
#include "DescriptorPool.hpp"

class DescriptorSet {
private:
	VkDevice device = VK_NULL_HANDLE;
	DescriptorPool *pool = nullptr;
	std::vector<VkDescriptorSetLayout> layouts;
	std::vector<VkWriteDescriptorSet> descriptors;
public:
	VkDescriptorSet handle;
	DescriptorSet(VkDevice device) {
		this->device = device;
	}
	~DescriptorSet() {
		// @todo
	}
	bool empty() {
		return (descriptors.size() == 0);
	}
	void create() {
		VkDescriptorSetAllocateInfo descriptorSetAI = vks::initializers::descriptorSetAllocateInfo(pool->handle, layouts.data(), static_cast<uint32_t>(layouts.size()));
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAI, &handle));
		for (auto& descriptor : descriptors) {
			descriptor.dstSet = handle;
		}
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptors.size()), descriptors.data(), 0, nullptr);
	}
	operator VkDescriptorSet() const { 
		return handle; 
	}
	void setPool(DescriptorPool *pool) {
		this->pool = pool;
	}
	void addLayout(VkDescriptorSetLayout layout) {
		layouts.push_back(layout);
	}
	void addLayout(DescriptorSetLayout *layout) {
		layouts.push_back(layout->handle);
	}
	void addDescriptor(VkWriteDescriptorSet descriptor) {
		descriptors.push_back(descriptor);
	}
	void addDescriptor(uint32_t binding, VkDescriptorType type, VkDescriptorBufferInfo* bufferInfo, uint32_t descriptorCount = 1) {
		VkWriteDescriptorSet writeDescriptorSet{};
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.descriptorType = type;
		writeDescriptorSet.dstBinding = binding;
		writeDescriptorSet.pBufferInfo = bufferInfo;
		writeDescriptorSet.descriptorCount = descriptorCount;
		descriptors.push_back(writeDescriptorSet);
	}
	void addDescriptor(uint32_t binding, VkDescriptorType type, VkDescriptorImageInfo* imageInfo, uint32_t descriptorCount = 1) {
		VkWriteDescriptorSet writeDescriptorSet{};
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.descriptorType = type;
		writeDescriptorSet.dstBinding = binding;
		writeDescriptorSet.pImageInfo = imageInfo;
		writeDescriptorSet.descriptorCount = descriptorCount;
		descriptors.push_back(writeDescriptorSet);
	}
	void updateDescriptor(uint32_t binding, VkDescriptorType type, VkDescriptorImageInfo* imageInfo, uint32_t descriptorCount = 1) {
		VkWriteDescriptorSet writeDescriptorSet{};
		for (auto &descriptor : descriptors) {
			if (descriptor.dstBinding == binding) {
				descriptor.descriptorType = type;
				descriptor.pImageInfo = imageInfo;
				descriptor.descriptorCount = descriptorCount;
				vkUpdateDescriptorSets(device, 1, &descriptor, 0, nullptr);
				break;
			}
		}
	}
};
