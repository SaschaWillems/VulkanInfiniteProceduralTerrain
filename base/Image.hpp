/*
* Vulkan image abstraction class
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
#include "VulkanDevice.hpp"

class Image {
private:
	vks::VulkanDevice* device;
	VkDeviceMemory memory;
	VkImageType type;
	VkFormat format;
	VkExtent3D extent;
	uint32_t mipLevels = 1;
	uint32_t arrayLayers = 1;
	VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
	VkImageTiling tiling;
	VkImageUsageFlags usage;
	VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE;
public:
	VkImage handle;
	Image(vks::VulkanDevice* device) {
		this->device = device;
	}
	~Image() {
		// @todo
	}
	void create() {
		VkImageCreateInfo CI = vks::initializers::imageCreateInfo();
		CI.imageType = type;
		CI.format = format;
		CI.extent = extent;
		CI.mipLevels = mipLevels;
		CI.arrayLayers = arrayLayers;
		CI.samples = samples;
		CI.tiling = tiling;
		CI.usage = usage;
		VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &CI, nullptr, &handle));
		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(device->logicalDevice, handle, &memReqs);
		VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAlloc, nullptr, &memory));
		VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, handle, memory, 0));
	}
	void setType(VkImageType type) {
		this->type = type;
	}
	void setFormat(VkFormat format) {
		this->format = format;
	}
	void setExtent(VkExtent3D extent) {
		this->extent = extent;
	}
	void setNumMipLevels(uint32_t mipLevels) {
		this->mipLevels = mipLevels;
	}
	void setNumArrayLayers(uint32_t arrayLayers) {
		this->arrayLayers = arrayLayers;
	}
	void setSampleCount(VkSampleCountFlagBits samples) {
		this->samples = samples;
	}
	void setTiling(VkImageTiling tiling) {
		this->tiling = tiling;
	}
	void setUsage(VkImageUsageFlags usage) {
		this->usage = usage;
	}
	void setSharingMode(VkSharingMode sharingMode) {
		this->sharingMode = sharingMode;
	}
};