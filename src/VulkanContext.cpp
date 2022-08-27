/*
 * Vulkan infinite procedurally generated terrain renderer
 *
 * Copyright (C) 2022 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#include "VulkanContext.h"

VulkanContext vulkanContext{};

VkQueue VulkanContext::copyQueue = VK_NULL_HANDLE;
VkQueue VulkanContext::graphicsQueue = VK_NULL_HANDLE;
vks::VulkanDevice* VulkanContext::device = nullptr;
