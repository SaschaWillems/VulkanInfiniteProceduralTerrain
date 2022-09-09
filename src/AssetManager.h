/*
 * Vulkan infinite procedurally generated terrain renderer
 *
 * Copyright (C) 2022 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#pragma once

#include <string>
#include <map>
#include "VulkanContext.h"
#include "VulkanglTFModel.h"

class Asset
{
public:
	std::string name;
	std::string filePath;
	virtual ~Asset() {};
};

class TextureAsset : public Asset
{
};

class ModelAsset : public Asset
{
public:
	vkglTF::Model model;
};

class AssetManager
{
private:
	std::map<std::string, std::shared_ptr<ModelAsset>> models;
public:
	void addModel(const std::string& filePath, const std::string& name);
	std::shared_ptr<ModelAsset> getAsset(const std::string& name);
};