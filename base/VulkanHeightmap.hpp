/*
* Heightmap terrain generator
*
* Copyright (C) by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <glm/glm.hpp>

#include "vulkan/vulkan.h"
#include "VulkanDevice.hpp"
#include "VulkanBuffer.hpp"
#include "VulkanTexture.hpp"
#include "Noise.h"
#include <random>
#include <ktx.h>
#include <ktxvulkan.h>

namespace vks 
{
	struct TerrainType
	{
		std::string name;
		float height;
		glm::vec3 color;
	};

	class HeightMap
	{
	private:
		uint32_t meshDim;
		uint32_t scale;

		vks::VulkanDevice *device = nullptr;
		VkQueue copyQueue = VK_NULL_HANDLE;

	public:
		static constexpr const int chunkSize = 241;
		// Height data also contains info on neighbouring borders to properly calculate normals
		float heights[chunkSize + 2][chunkSize + 2];
		// Store random values for each heightmap position that can be used at runtime for dynamic randomization (like grass rendering)
		float randomValues[chunkSize + 2][chunkSize + 2];
		// @todo: store random values per coordinate for doing random stuff, e.g. grass rendering
		enum Topology { topologyTriangles, topologyQuads };

		float minHeight = std::numeric_limits<float>::max();
		float maxHeight = std::numeric_limits<float>::min();

		float heightScale = 4.0f;
		float uvScale = 1.0f;

		vks::Buffer vertexBuffer;
		vks::Buffer indexBuffer;

		struct Vertex {
			glm::vec3 pos;
			glm::vec3 normal;
			glm::vec2 uv;
			glm::vec4 color;
			glm::vec4 pad1;
			float terrainHeight;
		};

		size_t vertexBufferSize = 0;
		size_t indexBufferSize = 0;
		uint32_t indexCount = 0;

		std::vector<TerrainType> regions;

		glm::vec3 rgb(int r, int g, int b) {
			return glm::vec3((float)r, (float)g, (float)b) / 255.f;
		}

		HeightMap(vks::VulkanDevice *device, VkQueue copyQueue)
		{
			this->device = device;
			this->copyQueue = copyQueue;
			// @todo
			regions.resize(8);

			regions[0] = { "Water Deep",	0.3f,  rgb(25, 50, 191) };
			regions[1] = { "Water Shallow", 0.4f,  rgb(54, 100, 191) };
			regions[2] = { "Sand",			0.45f, rgb(207, 207, 124) };
			regions[3] = { "Grass",			0.55f, rgb(85, 151, 25) };
			regions[4] = { "Grass 2",		0.6f,  rgb(62, 105, 20) };
			regions[5] = { "Rock",			0.7f,  rgb(88, 64, 59) };
			regions[6] = { "Rock 2",		0.9f,  rgb(66, 53, 50) };
			regions[7] = { "snow",			1.0f,  rgb(212, 212, 212) };
		};

		~HeightMap()
		{
			vertexBuffer.destroy();
			indexBuffer.destroy();
		}


		float getHeight(uint32_t x, uint32_t y)
		{
			if (x < 0) { x = 0; }
			if (y < 0) { y = 0; }
			if (x > chunkSize + 1) { x = chunkSize + 1; }
			if (y > chunkSize + 1) { y = chunkSize + 1; }
			float height = heights[x][y] * abs(heightScale);
			if (height < 0.0f) {
				height = 0.0f;
			}
			return height;
		}

		float getRandomValue(uint32_t x, uint32_t y)
		{
			if (x < 0) { x = 0; }
			if (y < 0) { y = 0; }
			if (x > chunkSize + 1) { x = chunkSize + 1; }
			if (y > chunkSize + 1) { y = chunkSize + 1; }
			return randomValues[x][y];
		}

		float inverseLerp(float xx, float yy, float value)
		{
			return (value - xx) / (yy - xx);
		}

		float gold_noise(glm::vec2 xy, float seed) {
			const float PHI = 1.61803398874989484820459;
			float ip;
			return modf(tan(glm::distance(xy * PHI, xy) * seed) * xy.x, &ip);
		}

		void generate(int seed, float noiseScale, int octaves, float persistence, float lacunarity, glm::vec2 offset)
		{
			float maxPossibleNoiseHeight = 0;
			float amplitude = 1;
			float frequency = 1;

			std::default_random_engine prng(seed);
			std::uniform_real_distribution<float> distribution(-100000, +100000);
			std::vector<glm::vec2> octaveOffsets(octaves);
			for (int32_t i = 0; i < octaves; i++) {
				float offsetX = distribution(prng) + offset.x;
				float offsetY = distribution(prng) - offset.y;
				octaveOffsets[i] = glm::vec2(offsetX, offsetY);
				maxPossibleNoiseHeight += amplitude;
				amplitude *= persistence;
			}

			PerlinNoise perlinNoise;

			float maxNoiseHeight = std::numeric_limits<float>::min();
			float minNoiseHeight = std::numeric_limits<float>::max();

			float halfWidth = (chunkSize + 2) / 2.0f;
			float halfHeight = (chunkSize + 2) / 2.0f;

			std::normal_distribution<float> rndDist(0.0f, 1.0f);

			for (int32_t y = 0; y < chunkSize + 2; y++) {
				for (int32_t x = 0; x < chunkSize + 2; x++) {

					amplitude = 1;
					frequency = 1;

					float noiseHeight = 0;

					for (int i = 0; i < octaves; i++) {
						float sampleX = ((float)x - halfWidth + octaveOffsets[i].x) / noiseScale * frequency;
						float sampleY = ((float)y - halfHeight + octaveOffsets[i].y) / noiseScale * frequency;

						float perlinValue = perlinNoise.noise(sampleX, sampleY) * 2.0f - 1.0f;
						noiseHeight += perlinValue * amplitude;

						amplitude *= persistence;
						frequency *= lacunarity;
					}

					if (noiseHeight > maxNoiseHeight) {
						maxNoiseHeight = noiseHeight;
					}
					else if (noiseHeight < minNoiseHeight) {
						minNoiseHeight = noiseHeight;
					}

					heights[x][y] = noiseHeight;
					//randomValues[x][y] = gold_noise(glm::vec2((float)x, (float)y) + offset, (float)(x + offset.x) + (float)(y + offset.y) * (float)chunkSize);
					randomValues[x][y] = gold_noise(glm::vec2((float)x + 0.5f, (float)y + 0.5f), (float)(x) + (float)(y) * (float)chunkSize * (float)seed);
				}
			}

			// Normalize
			for (size_t y = 0; y < chunkSize + 2; y++) {
				for (size_t x = 0; x < chunkSize + 2; x++) {
					// Local
					//data[x + y * texture.width] = inverseLerp(minNoiseHeight, maxNoiseHeight, data[x + y * texture.width]);
//					data[x + y * texture.width] = (data[x + y * texture.width] + 1.0f) / (2.0f * maxPossibleNoiseHeight / 1.5f);
					//data[x + y * texture.width] = data[x + y * texture.width] / 0.6;
					//data[x + y * texture.width] = glm::clamp(data[x + y * texture.width], 0.0f, std::numeric_limits<float>::max());
					heights[x][y] = inverseLerp(-3.0f, 0.6f, heights[x][y]);

					if (heights[x][y] < 0.0f) {
						heights[x][y] = 0.0f;
					}
				}
			}
		}

		void generateMesh(glm::vec3 scale, Topology topology, int levelOfDetail)
		{
			int meshDim = chunkSize;
			this->meshDim = meshDim;
			this->heightScale = -scale.y;
			// @todo: heightcurve (see E06:LOD)
			// @todo: two buffers, current and update, switch in cb once done (signal via flag)?

			float topLeftX = (float)(meshDim - 1) / -2.0f;
			float topLeftZ = (float)(meshDim - 1) / 2.0f;

			int meshSimplificationIncrement = std::max(levelOfDetail, 1) * 2;
			int verticesPerLine = (meshDim - 1) / meshSimplificationIncrement + 1;

			Vertex* vertices = new Vertex[verticesPerLine * verticesPerLine];
			uint32_t* triangles = new uint32_t[(verticesPerLine - 1) * (verticesPerLine - 1) * 6];
			uint32_t triangleIndex = 0;
			uint32_t vertexIndex = 0;
			indexCount = (verticesPerLine - 1) * (verticesPerLine - 1) * 6;

			auto addTriangle = [&triangleIndex, triangles](int a, int b, int c) {
				triangles[triangleIndex] = a;
				triangles[triangleIndex+1] = b;
				triangles[triangleIndex+2] = c;
				triangleIndex += 3;
			};

			auto getHeight = [this, scale](int x, int y) {
				if (x < 0) { x = 0; }
				if (y < 0) { y = 0; }
				if (x > chunkSize + 1) { x = chunkSize + 1; }
				if (y > chunkSize + 1) { y = chunkSize + 1; }
				float height = heights[x][y] * abs(scale.y);
				if (height < 0.0f) {
					height = 0.0f;
				}
				return height;
			};

			for (int32_t y = 0; y < meshDim; y += meshSimplificationIncrement) {
				for (int32_t x = 0; x < meshDim; x += meshSimplificationIncrement) {
					int xOff = x + 1;
					int yOff = y + 1;
					float currentHeight = heights[xOff][yOff];
					if (currentHeight < 0.0f) {
						currentHeight = 0.0f;
					}
					vertices[vertexIndex].pos.x = topLeftX + (float)x;
					vertices[vertexIndex].pos.y = currentHeight;
					vertices[vertexIndex].pos.z = topLeftZ - (float)y;
					vertices[vertexIndex].pos *= scale;
					vertices[vertexIndex].uv = glm::vec2((float)x / (float)meshDim, (float)y / (float)meshDim);
					vertices[vertexIndex].terrainHeight = currentHeight;

					if (abs(vertices[vertexIndex].pos.y) > maxHeight) {
						maxHeight = abs(vertices[vertexIndex].pos.y);
					}

					if (abs(vertices[vertexIndex].pos.y) < minHeight) {
						minHeight = abs(vertices[vertexIndex].pos.y);
					}

					float hL = getHeight(xOff - 1, yOff);
					float hR = getHeight(xOff + 1, yOff);
					float hD = getHeight(xOff, yOff + 1);
					float hU = getHeight(xOff, yOff - 1);
					glm::vec3 normalVector = glm::normalize(glm::vec3(hL - hR, -2.0f, hD - hU));
					vertices[vertexIndex].normal = normalVector;

					if ((x < meshDim - 1) && (y < meshDim - 1)) {
						addTriangle(vertexIndex, vertexIndex + verticesPerLine + 1, vertexIndex + verticesPerLine);
						addTriangle(vertexIndex + verticesPerLine + 1, vertexIndex, vertexIndex + 1);
					}
					vertexIndex++;
				}
			}

			// @todo: slighlty alter to take e.g. added trees into account
			maxHeight += 20.0f;
			minHeight -= 20.0f;

			VkDeviceSize vertexBufferSize = verticesPerLine * verticesPerLine * sizeof(Vertex);
			VkDeviceSize indexBufferSize = (verticesPerLine - 1) * (verticesPerLine - 1) * 6 * sizeof(uint32_t);

			// Create staging buffers
			vks::Buffer vertexStaging, indexStaging;
			device->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &vertexStaging, vertexBufferSize, vertices);
			device->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &indexStaging, indexBufferSize, triangles);
			// Device local (target) buffer
			device->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vertexBuffer, vertexBufferSize);
			device->createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &indexBuffer, indexBufferSize);
			// Copy from staging buffers
			VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true, VK_QUEUE_TRANSFER_BIT);
			VkBufferCopy copyRegion = {};
			copyRegion.size = vertexBufferSize;
			vkCmdCopyBuffer(copyCmd, vertexStaging.buffer, vertexBuffer.buffer, 1, &copyRegion);
			copyRegion.size = indexBufferSize;
			vkCmdCopyBuffer(copyCmd, indexStaging.buffer, indexBuffer.buffer, 1, &copyRegion);
			device->flushCommandBuffer(copyCmd, copyQueue, true, VK_QUEUE_TRANSFER_BIT);

			vkDestroyBuffer(device->logicalDevice, vertexStaging.buffer, nullptr);
			vkFreeMemory(device->logicalDevice, vertexStaging.memory, nullptr);
			vkDestroyBuffer(device->logicalDevice, indexStaging.buffer, nullptr);
			vkFreeMemory(device->logicalDevice, indexStaging.memory, nullptr);
		}

		void draw(VkCommandBuffer cb) {
			const VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(cb, 0, 1, &vertexBuffer.buffer, offsets);
			vkCmdBindIndexBuffer(cb, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(cb, indexCount, 1, 0, 0, 0);
		}
	};
}
