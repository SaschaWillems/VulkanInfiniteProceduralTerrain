/*
 * Vulkan infinite procedurally generated terrain renderer
 *
 * Copyright (C) 2022 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#include "TerrainChunk.h"

TerrainChunk::TerrainChunk(glm::ivec2 coords, int size) : size(size) {
		position = coords;
		worldPosition = glm::vec2(position.x * (float)(vks::HeightMap::chunkSize - 1) - (float)(vks::HeightMap::chunkSize - 1) / 2.0f, position.y* (float)(vks::HeightMap::chunkSize - 1) - (float)(vks::HeightMap::chunkSize - 1) / -2.0f);
		center = glm::vec3(0.0f);
		center.x = (float)coords.x * (float)size;
		center.z = (float)coords.y * (float)size;
		min = glm::vec3(center) - glm::vec3((float)size / 2.0f);
		max = glm::vec3(center) + glm::vec3((float)size / 2.0f);
		heightMap = new vks::HeightMap(VulkanContext::device, VulkanContext::copyQueue);
}
TerrainChunk::~TerrainChunk()
{
	if (state == TerrainChunk::State::generated) {
		heightMap->vertexBuffer.destroy();
		heightMap->indexBuffer.destroy();
	}
}

void TerrainChunk::update() {

}

void TerrainChunk::updateHeightMap() {
	std::cout << "Updating chunk at " << this->position.x << " / " << this->position.y << "\n";
	assert(heightMap);
	if (heightMap->vertexBuffer.buffer != VK_NULL_HANDLE) {
		heightMap->vertexBuffer.destroy();
		heightMap->indexBuffer.destroy();
	}
	heightMap->generate(
		heightMapSettings.seed,
		heightMapSettings.noiseScale,
		heightMapSettings.octaves,
		heightMapSettings.persistence,
		heightMapSettings.lacunarity,
		// @todo: base on offset instead of changing it
		heightMapSettings.offset);
	glm::vec3 scale = glm::vec3(1.0f, -heightMapSettings.heightScale, 1.0f); // @todo
	heightMap->generateMesh(
		scale,
		vks::HeightMap::topologyTriangles,
		heightMapSettings.levelOfDetail
	);
}

float TerrainChunk::getHeight(int x, int y) {
	assert(heightMap);
	return heightMap->getHeight(x, y);
}

float TerrainChunk::getRandomValue(int x, int y)
{
	assert(heightMap);
	return heightMap->getRandomValue(x, y);
}

void TerrainChunk::updateTrees() {
	assert(heightMap);

	float topLeftX = (float)(vks::HeightMap::chunkSize - 1) / -2.0f;
	float topLeftZ = (float)(vks::HeightMap::chunkSize - 1) / 2.0f;

	// Random distribution

	const int dim = 30; // 24 241
	treeInstanceCount = heightMapSettings.treeDensity * heightMapSettings.treeDensity;
	std::vector<InstanceData> instanceData(treeInstanceCount);
	trees.resize(treeInstanceCount);
	std::default_random_engine prng(heightMapSettings.seed);
	std::uniform_real_distribution<float> distribution(0.0f, (float)(vks::HeightMap::chunkSize - 1));
	std::uniform_real_distribution<float> scaleDist(heightMapSettings.minTreeSize, heightMapSettings.maxTreeSize);
	std::uniform_real_distribution<float> rotDist(0.0f, 1.0f);

	for (int i = 0; i < treeInstanceCount; i++) {
		float xPos = distribution(prng);
		float yPos = distribution(prng);
		int terrainX = round(xPos + 0.5f);
		int terrainY = round(yPos + 0.5f);
		float h1 = getHeight(terrainX - 1, terrainY);
		float h2 = getHeight(terrainX + 1, terrainY);
		float h3 = getHeight(terrainX, terrainY - 1);
		float h4 = getHeight(terrainX, terrainY + 1);
		float h = (h1 + h2 + h3 + h4) / 4.0f;
		if ((h <= heightMapSettings.waterPosition) || (h > 15.0f)) {
			continue;
		}
		InstanceData inst{};
		inst.pos = glm::vec3((float)topLeftX + xPos, -h, (float)topLeftZ - yPos);
		inst.scale = glm::vec3(scaleDist(prng));
		inst.rotation = glm::vec3(M_PI * rotDist(prng) * 0.035f, M_PI * rotDist(prng), M_PI * rotDist(prng) * 0.035f);
		instanceData[i] = inst;
		trees[i].worldpos = glm::vec3((float)position.x, 0.0f, (float)position.y) * glm::vec3(vks::HeightMap::chunkSize - 1.0f, 0.0f, vks::HeightMap::chunkSize - 1.0f) + inst.pos;
		trees[i].rotation = inst.rotation;
		trees[i].scale = inst.scale;
		trees[i].color = glm::vec4(0.6f + rotDist(prng) * 0.4f);
		trees[i].color.a = 1.0f;
	}
	// Even distribution
	/*

	std::vector<InstanceData> instanceData;
	const int dim = 24; // 241
	const int maxTreeCount = dim * dim; // @todo
	std::uniform_real_distribution<float> uniformDist(0.0f, 1.0f);
	for (int x = 0; x < dim; x++) {
		for (int y = 0; y < dim; y++) {
			const float f = 10.1f;
			float xPos = (float)x * f + 5.0f;
			float yPos = (float)y * f + 5.0f;
			int terrainX = round(xPos + 0.5f);
			int terrainY = round(yPos + 0.5f);
			float h1 = getHeight(terrainX - 1, terrainY);
			float h2 = getHeight(terrainX + 1, terrainY);
			float h3 = getHeight(terrainX, terrainY - 1);
			float h4 = getHeight(terrainX, terrainY + 1);
			float h = (h1 + h2 + h3 + h4) / 4.0f;
			if ((h <= waterPosition) || (h > 15.0f)) {
				continue;
			}
			InstanceData inst{};
			inst.pos = glm::vec3((float)topLeftX + xPos, -h, (float)topLeftZ - yPos);
			instanceData.push_back(inst);
		}
	}
	*/

	// @todo: remove
	//if (treeInstanceCount > 0) {
	//	vks::Buffer stagingBuffer;
	//	VK_CHECK_RESULT(VulkanContext::device->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, instanceData.size() * sizeof(InstanceData), instanceData.data()));
	//	VK_CHECK_RESULT(VulkanContext::device->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &instanceBuffer, stagingBuffer.size));
	//	VkCommandBuffer copyCmd = VulkanContext::device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true, VK_QUEUE_TRANSFER_BIT);
	//	VkBufferCopy bufferCopy{};
	//	bufferCopy.size = stagingBuffer.size;
	//	vkCmdCopyBuffer(copyCmd, stagingBuffer.buffer, instanceBuffer.buffer, 1, &bufferCopy);
	//	VulkanContext::device->flushCommandBuffer(copyCmd, VulkanContext::copyQueue, true, VK_QUEUE_TRANSFER_BIT);
	//	stagingBuffer.destroy();
	//}
}

void TerrainChunk::updateGrass() {
	// @todo: remove

	// Random distribution
	//grassInstanceCount = heightMapSettings.grassDensity * heightMapSettings.grassDensity;
	//InstanceData* instanceData = new InstanceData[grassInstanceCount];
	//VkDrawIndexedIndirectCommand* indirectData = new VkDrawIndexedIndirectCommand[grassInstanceCount];
	//std::default_random_engine prng(heightMapSettings.seed);
	//std::uniform_real_distribution<float> distribution(0.0f, (float)(vks::HeightMap::chunkSize - 1));
	//std::uniform_real_distribution<float> scaleDist(0.75f, 1.25f);
	//std::uniform_real_distribution<float> rotDist(0.0f, 1.0f);
	//std::uniform_int_distribution<int> uvDist(0, 3);

	//for (int i = 0; i < grassInstanceCount; i++) {
	//	float xPos = distribution(prng);
	//	float yPos = distribution(prng);
	//	int terrainX = round(xPos + 0.5f);
	//	int terrainY = round(yPos + 0.5f);
	//	float h1 = getHeight(terrainX - 1, terrainY);
	//	float h2 = getHeight(terrainX + 1, terrainY);
	//	float h3 = getHeight(terrainX, terrainY - 1);
	//	float h4 = getHeight(terrainX, terrainY + 1);
	//	float h = (h1 + h2 + h3 + h4) / 4.0f;
	//	if ((h <= heightMapSettings.waterPosition) || (h > 12.0f)) {
	//		continue;
	//	}
	//	InstanceData inst{};
	//	inst.pos = glm::vec3((float)topLeftX + xPos, -h, (float)topLeftZ - yPos);
	//	inst.pos.y -= 0.25f;
	//	inst.scale = glm::vec3(scaleDist(prng));
	//	inst.scale.y *= 0.75f;
	//	inst.rotation = glm::vec3(M_PI * rotDist(prng) * 0.035f, M_PI * rotDist(prng) * 2.0f, M_PI * rotDist(prng) * 0.035f);
	//	inst.uv.s = 0.25f * uvDist(prng);
	//	instanceData[i] = inst;
	//}
}

void TerrainChunk::uploadBuffers()
{
}

void TerrainChunk::draw(CommandBuffer* cb) {
	if (state == TerrainChunk::State::generated) {
		heightMap->draw(cb->handle);
	}
}
