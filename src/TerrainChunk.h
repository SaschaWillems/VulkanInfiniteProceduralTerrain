/*
 * Vulkan infinite procedurally generated terrain renderer
 *
 * Copyright (C) 2022 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#pragma once

#include "HeightMapSettings.h"
#include "VulkanHeightmap.hpp"
#include "VulkanBuffer.hpp"
#include "CommandBuffer.hpp"
#include "VulkanContext.h"
#include <glm/glm.hpp>

struct InstanceData {
	glm::vec3 pos;
	glm::vec3 scale;
	glm::vec3 rotation;
	glm::vec2 uv;
	glm::vec4 color;
};

struct ObjectData {
	glm::vec3 worldpos;
	glm::vec3 scale;
	glm::vec3 rotation;
	glm::vec4 color;
	glm::vec2 uv;
	float distance;
	int visibilityInfo = 0;
	bool visible = true;
};

class TerrainChunk {
public:
	enum class State { _new, generating, generated, deleting, deleted };

	State state = State::_new;
	vks::HeightMap* heightMap = nullptr;
	glm::ivec2 position;
	glm::vec2 worldPosition;
	glm::vec3 center;
	glm::vec3 min;
	glm::vec3 max;
	std::vector<ObjectData> trees;
	int size;
	//bool hasValidMesh = false;
	bool visible = false;
	int treeInstanceCount = 0;
	int grassInstanceCount = 0;
	float alpha = 0.0f;

	TerrainChunk(glm::ivec2 coords, int size);
	~TerrainChunk();
	void update();
	void updateHeightMap();
	float getHeight(int x, int y);
	float getRandomValue(int x, int y);
	void updateTrees();
	void updateGrass();
	void uploadBuffers();
	void draw(CommandBuffer* cb);
};