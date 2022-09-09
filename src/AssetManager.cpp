/*
 * Vulkan infinite procedurally generated terrain renderer
 *
 * Copyright (C) 2022 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#include "AssetManager.h"

void AssetManager::addModel(const std::string& filePath, const std::string& name)
{
	auto it = models.find(name);
	if (it != models.end()) {
		// Asset already loaded
		return;
	}
	std::shared_ptr<ModelAsset> asset = std::make_shared<ModelAsset>();
	const int fileLoadingFlags = vkglTF::FileLoadingFlags::FlipY | vkglTF::FileLoadingFlags::PreTransformVertices;
	asset->model.loadFromFile(filePath, VulkanContext::device, VulkanContext::graphicsQueue, fileLoadingFlags);
	asset->filePath = filePath;
	asset->name = name;
	models.insert(std::make_pair(name, asset));
}

std::shared_ptr<ModelAsset> AssetManager::getAsset(const std::string& name)
{
	auto it = models.find(name);
	if (it != models.end()) {
		return it->second;
	}
	return nullptr;
}