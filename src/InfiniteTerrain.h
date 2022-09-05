/*
 * Vulkan infinite procedurally generated terrain renderer
 *
 * Copyright (C) 2022 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */
#pragma once

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include "HeightMapSettings.h"
#include "TerrainChunk.h"
#include "frustum.hpp"

class InfiniteTerrain {
public:
	glm::vec2 viewerPosition;
	int chunkSize;
	int chunksVisibleInViewDistance;

	std::vector<TerrainChunk*> terrainChunks{};
	std::vector<TerrainChunk*> terrainChunkgsUpdateList{};

	InfiniteTerrain();
	void updateViewDistance(float viewDistance);
	bool chunkPresent(glm::ivec2 coords);
	TerrainChunk* getChunk(glm::ivec2 coords);
	TerrainChunk* getChunkFromWorldPos(glm::vec3 coords);
	bool getHeight(const glm::vec3 worldPos, float &height);
	int getVisibleChunkCount();
	int getVisibleTreeCount();
	bool updateVisibleChunks(vks::Frustum& frustum);
	void updateChunks();
	void clear();
	void update(float deltaTime);
};