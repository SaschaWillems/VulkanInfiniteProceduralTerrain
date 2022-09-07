/*
 * Vulkan infinite procedurally generated terrain renderer
 *
 * Copyright (C) 2022 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>
#include <thread>
#include <mutex>
#include <math.h>
#include <filesystem>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkan/vulkan.h>
#include "vulkanexamplebase.h"
#include "VulkanTexture.hpp"
#include "VulkanglTFModel.h"
#include "VulkanBuffer.hpp"
#include "VulkanHeightmap.hpp"

#include "Pipeline.hpp"
#include "PipelineLayout.hpp"
#include "DescriptorSet.hpp"
#include "DescriptorSetLayout.hpp"
#include "DescriptorPool.hpp"
#include "Image.hpp"
#include "ImageView.hpp"
#include "frustum.hpp"
#include "TerrainChunk.h"
#include "HeightMapSettings.h"
#include "InfiniteTerrain.h"

#define ENABLE_VALIDATION false
#define FB_DIM 768
#define SHADOWMAP_DIM 2048
#define SHADOW_MAP_CASCADE_COUNT 4

vks::Frustum frustum;
const float chunkDim = 241.0f;

class VulkanExample : public VulkanExampleBase
{
public:
	bool debugDisplayReflection = false;
	bool debugDisplayRefraction = false;
	bool displayWaterPlane = true;
	bool renderShadows = true;
	bool renderTrees = true;
	bool renderGrass = true;
	bool renderTerrain = true;
	bool fixFrustum = false;
	bool hasExtMemoryBudget = false;
	bool stickToTerrain = false;
	bool waterBlending = true;

	struct MemoryBudget {
		int heapCount;
		VkDeviceSize heapBudget[VK_MAX_MEMORY_HEAPS];
		VkDeviceSize heapUsage[VK_MAX_MEMORY_HEAPS];
		std::chrono::time_point<std::chrono::high_resolution_clock> lastUpdate;
	} memoryBudget{};

	InfiniteTerrain infiniteTerrain;

	glm::vec4 lightPos;

	enum class SceneDrawType { sceneDrawTypeRefract, sceneDrawTypeReflect, sceneDrawTypeDisplay };
	enum class ImageType { Color, DepthStencil };

	struct TreeModelInfo {
		std::string name;
		struct Models {
			vkglTF::Model model;
			vkglTF::Model imposter;
		} models;
	};
	int selectedTreeType = 0;
	int selectedGrassType = 0;

	const std::vector<std::string> treeTypes = {
		"spruce", "fir", "birch", "pine", "tropical", "tropical2", "palm", "coconut_palm"
	};
	const std::vector<std::string> grassTypes = {
		"grasspatch", "grasspatch_medium", "grasspatch_large"
	};

	std::vector<TreeModelInfo> treeModelInfo;
	std::vector<vkglTF::Model> grassModels;

	struct CascadeDebug {
		bool enabled = false;
		int32_t cascadeIndex = 0;
		Pipeline* pipeline;
		PipelineLayout* pipelineLayout;
		DescriptorSet* descriptorSet;
		DescriptorSetLayout* descriptorSetLayout;
	} cascadeDebug;

	struct {
		Pipeline* debug;
		Pipeline* water;
		Pipeline* waterBlend;
		Pipeline* waterOffscreen;
		Pipeline* terrain;
		Pipeline* terrainBlend;
		Pipeline* terrainOffscreen;
		Pipeline* sky;
		Pipeline* skyOffscreen;
		Pipeline* depthpass;
		Pipeline* depthpassTree;
		Pipeline* tree;
		Pipeline* treeOffscreen;
		Pipeline* grass;
		Pipeline* grassOffscreen;
	} pipelines;

	struct Textures {
		vks::Texture2D skySphere;
		vks::Texture2D waterNormalMap;
		vks::Texture2DArray terrainArray;
	} textures;

	std::vector<vks::Texture2D> skyspheres;
	int32_t skysphereIndex;

	// @todo: add some kind of basic asset manager
	struct Models {
		vkglTF::Model skysphere;
		vkglTF::Model plane;
	} models;

	struct UBO {
		glm::mat4 projection;
		glm::mat4 model;
		glm::vec4 lightDir = glm::vec4(10.0f, 10.0f, 10.0f, 1.0f);
		glm::vec4 cameraPos;
		float time;
	} uboShared;

	struct UBOCSM {
		float cascadeSplits[SHADOW_MAP_CASCADE_COUNT];
		glm::mat4 cascadeViewProjMat[SHADOW_MAP_CASCADE_COUNT];
		glm::mat4 inverseViewMat;
		glm::vec4 lightDir;
		glm::mat4 biasMat = glm::mat4(
			0.5f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.5f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.5f, 0.5f, 0.0f, 1.0f
		);
	} uboCSM;

	struct UniformDataParams {
		uint32_t shadows = 0;
		uint32_t smoothCoastLine = 1;
		float waterAlpha = 512.0f;
		uint32_t shadowPCF = 1;
		glm::vec4 fogColor;
		glm::vec4 waterColor;
		glm::vec4 grassColor = glm::vec4(69.0f, 98.0f, 31.0f, 1.0f) / 255.0f;
		glm::vec4 layers[TERRAIN_LAYER_COUNT];
	} uniformDataParams;

	struct FrameObjects : public VulkanFrameObjects {
		struct UniformBuffers {
			vks::Buffer shared;
			vks::Buffer CSM;
			vks::Buffer params;
			vks::Buffer depthPass;
		} uniformBuffers;
	};
	std::vector<FrameObjects> frameObjects;

	struct {
		PipelineLayout* debug;
		PipelineLayout* textured;
		PipelineLayout* terrain;
		PipelineLayout* sky;
		PipelineLayout* tree;
		PipelineLayout* water;
	} pipelineLayouts;

	DescriptorPool* descriptorPool;

	struct DescriptorSets {
		DescriptorSet* waterplane;
		DescriptorSet* debugquad;
		DescriptorSet* terrain = nullptr;
		DescriptorSet* skysphere = nullptr;
		DescriptorSet* shadowCascades = nullptr;
	} descriptorSets;

	struct {
		DescriptorSetLayout* textured;
		DescriptorSetLayout* terrain;
		DescriptorSetLayout* skysphere;
		DescriptorSetLayout* water;
		// @todo
		DescriptorSetLayout* ubo;
		DescriptorSetLayout* images;
		DescriptorSetLayout* shadowCascades;
	} descriptorSetLayouts;

	struct OffscreenImage {
		ImageView* view;
		Image* image;
		VkDescriptorImageInfo descriptor;
	};
	struct OffscreenPass {
		int32_t width, height;
		OffscreenImage reflection, refraction, depthReflection, depthRefraction;
		VkSampler sampler;
	} offscreenPass;

	VkSampler terrainSampler = VK_NULL_HANDLE;

	/* CSM */

	float cascadeSplitLambda = 0.95f;

	float zNear = 0.5f;
	float zFar = 1024.0f;

	// Resources of the depth map generation pass
	struct DepthPass {
		PipelineLayout* pipelineLayout;
		VkPipeline pipeline;
		DescriptorSetLayout* descriptorSetLayout;
		struct UniformBlock {
			std::array<glm::mat4, SHADOW_MAP_CASCADE_COUNT> cascadeViewProjMat;
		} ubo;
	} depthPass;
	// Layered depth image containing the shadow cascade depths
	struct DepthImage {
		Image* image;
		ImageView* view;
		VkSampler sampler;
		void destroy(VkDevice device) {
			vkDestroySampler(device, sampler, nullptr);
		}
	} depth;

	// Contains all resources required for a single shadow map cascade
	struct Cascade {
		float splitDepth;
		glm::mat4 viewProjMatrix;
	};
	std::array<Cascade, SHADOW_MAP_CASCADE_COUNT> cascades;
	VkImageView cascadesView;

	std::mutex lock_guard;
	bool transferQueueBlocked = false;
	std::atomic<int> activeThreadCount = 0;

	// Dynamic buffers
	struct DrawBatchBuffer : vks::Buffer {
		int32_t elements = 0;
	};
	struct DrawBatch {
		vkglTF::Model* model = nullptr;
		std::vector<DrawBatchBuffer> instanceBuffers;
	};
	struct DrawBatches {
		DrawBatch trees;
		DrawBatch treeImpostors;
		DrawBatch grass;
	} drawBatches;

	// @todo: move
	struct Timing {
		std::chrono::steady_clock::time_point tStart;
		std::chrono::steady_clock::time_point tEnd;
		double tDelta;

		void start() {
			tStart = std::chrono::high_resolution_clock::now();
		}
		void stop() {
			tEnd = std::chrono::high_resolution_clock::now();
			tDelta = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
		}
	};
	struct Profiling {
		Timing drawBatchUpdate;
		Timing drawBatchCpu;
		Timing drawBatchUpload;
		Timing cbBuild;
		Timing uniformUpdate;
	} profiling;

	struct FileList {
		std::vector<std::string> terrainSets;
		std::vector<std::string> presets;
	} fileList;
	int32_t presetIndex = 0;
	int32_t terrainSetIndex = 0;

	inline float goldNoise(glm::vec2 xy, float seed) {
		const float PHI = 1.61803398874989484820459f;
		float ip;
		return modf(tan(glm::distance(xy * PHI, xy) * seed) * xy.x, &ip);
	}

	void updateDrawBatches() {

		// @todo: store time when object was first displayed for smooth fade in / transition

		profiling.drawBatchUpdate.start();

		profiling.drawBatchCpu.start();

		uint32_t countFull = 0;
		uint32_t countImpostor = 0;
		uint32_t countGrass = 0;

		// Gather chunks
		std::vector<TerrainChunk*> chunks;
		for (auto& terrainChunk : infiniteTerrain.terrainChunks) {
			if (terrainChunk->visible && (terrainChunk->state == TerrainChunk::State::generated)) {
				chunks.push_back(terrainChunk);
			}
		}

		// Determine number of visible trees
		for (auto& terrainChunk : chunks) {
			if (terrainChunk->treeInstanceCount > 0) {
				for (auto& object : terrainChunk->trees) {
					if (!frustum.checkSphere(object.worldpos, 10.0f)) {
						object.visible = false;
						continue;
					}
					object.visible = true;
					float d = glm::distance(object.worldpos, camera.position);
					object.distance = d;
					if (d < heightMapSettings.maxDrawDistanceTreesFull) {
						countFull++;
					}
					else {
						if (d < heightMapSettings.maxDrawDistanceTreesImposter) {
							countImpostor++;
						}
					}
				}
			}
		}

		if (chunks.empty()) {
			return;
		}

		InstanceData* idTrees = nullptr; 
		if (countFull > 0) {
			idTrees = new InstanceData[countFull];
		}
		InstanceData* idImpostors = nullptr;
		if (countImpostor > 0) {
			idImpostors = new InstanceData[countImpostor];
		}
		uint32_t idxFull = 0;
		uint32_t idxImpostor = 0;
		for (auto& terrainChunk : chunks) {
			if (terrainChunk->treeInstanceCount > 0) {
				for (auto& object : terrainChunk->trees) {
					if (object.visible) {
						if (object.distance < heightMapSettings.maxDrawDistanceTreesFull) {
							if (idxFull > countFull) {
								continue;
							}
							idTrees[idxFull].pos = object.worldpos;
							idTrees[idxFull].rotation = object.rotation;
							idTrees[idxFull].scale = object.scale;
							idTrees[idxFull].color = object.color;
							// Fade in with terrain chunk
							idTrees[idxFull].color.a = terrainChunk->alpha;
							idxFull++;
						}
						else {
							if (object.distance < heightMapSettings.maxDrawDistanceTreesImposter) {
								if (idxFull > countImpostor) {
									continue;
								}
								idImpostors[idxImpostor].pos = object.worldpos;
								idImpostors[idxImpostor].rotation = object.rotation;
								idImpostors[idxImpostor].scale = object.scale;
								idImpostors[idxImpostor].color = object.color;
								// Fade in with terrain chunk
								idImpostors[idxImpostor].color.a = terrainChunk->alpha;
								idxImpostor++;
							}
						}
					}
				}
			}
		}

		// Generate grass layer around player

		int dim = heightMapSettings.grassDim;
		float scale = heightMapSettings.grassScale;
		float hdim = (float)dim * scale / 2.0f;
		float adim = (float)dim * scale;
		float fdim = adim * 0.75f;
		uint32_t idx = 0;
		int32_t countGrassActual = 0;
		countGrass = dim * dim;
		InstanceData* idGrass = nullptr;
		if (countGrass > 0) {
			idGrass = new InstanceData[countGrass];
		}
		glm::vec3 camFront = camera.frontVector();
		glm::vec3 center = camera.position + camFront * hdim;
		for (int x = -dim / 2; x < dim / 2; x++) {
			for (int y = -dim / 2; y < dim / 2; y++) {
				glm::vec3 worldPos = glm::vec3(round(center.x) + x * scale, 0.0f, round(center.z) + y * scale);
				// @todo: store random number for each terrain chunk pos at chunk generation and use that instead of calculating
				float rndVal = goldNoise(glm::vec2(worldPos.x, worldPos.z), worldPos.x + worldPos.z * (float)dim);
				float rndValB = goldNoise(glm::vec2(worldPos.z, worldPos.x), worldPos.x * worldPos.z / (float)dim);
				float h = 0.0f;
				float r = 0.0f;
				worldPos.x += rndVal;// *2.0f - rndValB * 2.0f;
				worldPos.z -= rndVal;// *2.0f - rndValB * 2.0f;
				infiniteTerrain.getHeightAndRandomValue(worldPos, h, r);
				if ((abs(h) <= heightMapSettings.waterPosition) || (abs(h) > 12.0f)) {
					continue;
				}
				idGrass[idx].pos = worldPos;
				idGrass[idx].pos.y = h;
				if (!frustum.checkSphere(idGrass[idx].pos, 10.0f)) {
					continue;
				}
				idGrass[idx].scale = glm::vec3(1.0f + rndVal * 0.15f, 0.5f + rndVal * 0.25f, 1.0f + rndVal * 0.15f);
				idGrass[idx].rotation = glm::vec3(M_PI * rndVal * 0.035f, M_PI * rndVal * 360.0f, M_PI * rndVal * -0.035f);
				idGrass[idx].uv = glm::vec2((float)((int)(round(rndVal * 5.0f)) % 4) * 0.25f, 0.0f);
				//idGrass[idx].uv.s = 0.75f; // @todo: looks nicer in certain scenarios (e.g. default)
				idGrass[idx].color = glm::vec4(0.6f + rndVal * 0.4f);
				float d = glm::distance(worldPos, camera.position);
				idGrass[idx].color.a = 1.0f;
				if (d > fdim) {
					const float farea = adim - fdim;
					float alpha = ((adim - d) / farea);
					idGrass[idx].color.a = alpha;
				}
				idx++;
				countGrassActual++;
			}
		}

		// @todo: may crash if no grass is visible
		countGrassActual -= 1;

		// @todo: lambda or fn for draw batch setup instead of duplicating code, simply pass vector with object data

		profiling.drawBatchCpu.stop();

		// Uploads

		profiling.drawBatchUpload.start();

		const uint32_t currentFrameIndex = getCurrentFrameIndex();

		if (idTrees) {
			// Trees at full detail
			if ((countFull > 0) && ((countFull > drawBatches.trees.instanceBuffers[currentFrameIndex].elements) || (drawBatches.trees.instanceBuffers[currentFrameIndex].buffer == VK_NULL_HANDLE))) {
				VkDeviceSize bufferSize = countFull * sizeof(InstanceData);
				drawBatches.trees.instanceBuffers[currentFrameIndex].destroy();
				// Create device local / host accessible buffer (@todo: check mem consumption and if it works elsewhere)
				VK_CHECK_RESULT(VulkanContext::device->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &drawBatches.trees.instanceBuffers[currentFrameIndex], bufferSize));
				drawBatches.trees.instanceBuffers[currentFrameIndex].map();
			}
			drawBatches.trees.model = &treeModelInfo[selectedTreeType].models.model;
			drawBatches.trees.instanceBuffers[currentFrameIndex].elements = countFull;
			if ((countFull > 0) && (drawBatches.trees.instanceBuffers[currentFrameIndex].buffer != VK_NULL_HANDLE)) {
				VkDeviceSize bufferSize = countFull * sizeof(InstanceData);
				memcpy(drawBatches.trees.instanceBuffers[currentFrameIndex].mapped, idTrees, bufferSize);
				VkMappedMemoryRange memRange = vks::initializers::mappedMemoryRange();
				memRange.memory = drawBatches.trees.instanceBuffers[currentFrameIndex].memory;
				memRange.size = VK_WHOLE_SIZE;
				vkFlushMappedMemoryRanges(device, 1, &memRange);
			}
			delete[] idTrees;
		}

		if (idImpostors) {
			// Tree impostors
			DrawBatch* drawBatch = &drawBatches.treeImpostors;
			if ((countImpostor > 0) && ((countImpostor > drawBatch->instanceBuffers[currentFrameIndex].elements) || (drawBatch->instanceBuffers[currentFrameIndex].buffer == VK_NULL_HANDLE))) {
				VkDeviceSize bufferSize = countImpostor * sizeof(InstanceData);
				drawBatch->instanceBuffers[currentFrameIndex].destroy();
				// Create device local / host accessible buffer (@todo: check mem consumption and if it works elsewhere)
				VK_CHECK_RESULT(VulkanContext::device->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &drawBatch->instanceBuffers[currentFrameIndex], bufferSize));
				drawBatch->instanceBuffers[currentFrameIndex].map();
			}
			drawBatch->model = &treeModelInfo[selectedTreeType].models.imposter;
			drawBatch->instanceBuffers[currentFrameIndex].elements = countImpostor;
			if ((countImpostor > 0) && (drawBatch->instanceBuffers[currentFrameIndex].buffer != VK_NULL_HANDLE)) {
				VkDeviceSize bufferSize = countImpostor * sizeof(InstanceData);
				memcpy(drawBatch->instanceBuffers[currentFrameIndex].mapped, idImpostors, bufferSize);
				VkMappedMemoryRange memRange = vks::initializers::mappedMemoryRange();
				memRange.memory = drawBatch->instanceBuffers[currentFrameIndex].memory;
				memRange.size = VK_WHOLE_SIZE;
				vkFlushMappedMemoryRanges(device, 1, &memRange);
			}
			delete[] idImpostors;
		}

		// Dynamic grass layer
		// Tree impostors
		if (idGrass) {
			DrawBatch*  drawBatch = &drawBatches.grass;
			if ((countGrassActual > 0) && ((countGrassActual > drawBatch->instanceBuffers[currentFrameIndex].elements) || (drawBatch->instanceBuffers[currentFrameIndex].buffer == VK_NULL_HANDLE))) {
				VkDeviceSize bufferSize = countGrassActual * sizeof(InstanceData);
				drawBatch->instanceBuffers[currentFrameIndex].destroy();
				// Create device local / host accessible buffer (@todo: check mem consumption and if it works elsewhere)
				VK_CHECK_RESULT(VulkanContext::device->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &drawBatch->instanceBuffers[currentFrameIndex], bufferSize));
				drawBatch->instanceBuffers[currentFrameIndex].map();
			}
			drawBatch->model = &grassModels[selectedGrassType];
			drawBatch->instanceBuffers[currentFrameIndex].elements = countGrassActual;
			if ((countGrassActual > 0) && (drawBatch->instanceBuffers[currentFrameIndex].buffer != VK_NULL_HANDLE)) {
				VkDeviceSize bufferSize = countGrassActual * sizeof(InstanceData);
				memcpy(drawBatch->instanceBuffers[currentFrameIndex].mapped, idGrass, bufferSize);
				VkMappedMemoryRange memRange = vks::initializers::mappedMemoryRange();
				memRange.memory = drawBatch->instanceBuffers[currentFrameIndex].memory;
				memRange.size = VK_WHOLE_SIZE;
				vkFlushMappedMemoryRanges(device, 1, &memRange);
			}
			delete[] idGrass;
		}

		profiling.drawBatchUpload.stop();

		profiling.drawBatchUpdate.stop();
	}

	void updateTerrainChunkThreadFn(TerrainChunk* chunk) {
		activeThreadCount++;
		std::lock_guard<std::mutex> guard(lock_guard);
		heightMapSettings.offset.x = (float)chunk->position.x * (float)(chunk->size);
		heightMapSettings.offset.y = (float)chunk->position.y * (float)(chunk->size);
		while (transferQueueBlocked) {};
		transferQueueBlocked = true;
		chunk->state = TerrainChunk::State::generating;
		chunk->updateHeightMap();
		chunk->updateTrees();
		chunk->min.y = chunk->heightMap->minHeight;
		chunk->max.y = chunk->heightMap->maxHeight;
		//chunk->hasValidMesh = true;
		chunk->state = TerrainChunk::State::generated;
		transferQueueBlocked = false;
		std::cout << "Chunk generated\n";
		activeThreadCount--;
		//std::terminate();
	}

	void readFileLists()
	{
		fileList.terrainSets.clear();
		for (const auto& fileName : std::filesystem::directory_iterator(getAssetPath() + "textures/terrainsets")) {
			fileList.terrainSets.push_back(fileName.path().filename().string());
		}
		fileList.presets.clear();
		for (const auto& fileName : std::filesystem::directory_iterator(getAssetPath() + "presets")) {
			if (fileName.path().extension() == ".txt") {
				fileList.presets.push_back(fileName.path().stem().string());
			}
		}
	}


	VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		title = "Vulkan infinite terrain";
		camera.type = Camera::CameraType::firstperson;
		camera.setPerspective(45.0f, (float)width / (float)height, zNear, zFar);
		camera.movementSpeed = 7.5f * 5.0f;
		camera.rotationSpeed = 0.1f;
		settings.overlay = true;
		timerSpeed *= 0.05f;

		camera.setPosition(glm::vec3(0.0f, -25.0f, 0.0f));

		camera.update(0.0f);
		frustum.update(camera.matrices.perspective * camera.matrices.view);

		vks::VulkanDevice::enabledFeatures.shaderClipDistance = VK_TRUE;
		vks::VulkanDevice::enabledFeatures.samplerAnisotropy = VK_TRUE;
		vks::VulkanDevice::enabledFeatures.depthClamp = VK_TRUE;
		vks::VulkanDevice::enabledFeatures.fillModeNonSolid = VK_TRUE;

		vks::VulkanDevice::enabledFeatures11.multiview = VK_TRUE;
		vks::VulkanDevice::enabledFeatures13.dynamicRendering = VK_TRUE;

		apiVersion = VK_API_VERSION_1_3;
		enabledDeviceExtensions.push_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);

#if defined(_WIN32)
		//ShowCursor(false);
#endif

		readFileLists();
	}

	void loadHeightMapSettings(std::string name) 
	{
		heightMapSettings.loadFromFile(getAssetPath() + "presets/" + name + ".txt");
		for (size_t i = 0; i < treeTypes.size(); i++) {
			if (heightMapSettings.treeType == treeTypes[i]) {
				selectedTreeType = i;
				break;
			}
		}
		for (size_t i = 0; i < grassTypes.size(); i++) {
			if (heightMapSettings.grassType == grassTypes[i]) {
				selectedGrassType = i;
				break;
			}
		}

		loadSkySphere(heightMapSettings.skySphere);
		loadTerrainSet(heightMapSettings.terrainSet);
		memcpy(uniformDataParams.layers, heightMapSettings.textureLayers, sizeof(glm::vec4) * TERRAIN_LAYER_COUNT);
		infiniteTerrain.clear();
		updateHeightmap();
		viewChanged();
	}

	~VulkanExample()
	{
		vkDestroySampler(device, offscreenPass.sampler, nullptr);
		// @todo: wait for detachted threads to finish (maybe use atomic active thread counter)
	}

	void createImage(OffscreenImage& target, ImageType type)
	{
		VkFormat format = VK_FORMAT_UNDEFINED;
		VkImageAspectFlags aspectMask;
		VkImageUsageFlags usageFlags;
		switch (type) {
		case ImageType::Color:
			format = swapChain.colorFormat;
			usageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			break;
		case ImageType::DepthStencil:
			VkBool32 validDepthFormat = vks::tools::getSupportedDepthFormat(physicalDevice, &format);
			usageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;// | VK_IMAGE_ASPECT_STENCIL_BIT;
			break;
		}
		assert(format != VK_FORMAT_UNDEFINED);

		target.image = new Image(vulkanDevice);
		target.image->setType(VK_IMAGE_TYPE_2D);
		target.image->setFormat(format);
		target.image->setExtent({ (uint32_t)offscreenPass.width, (uint32_t)offscreenPass.height, 1 });
		target.image->setTiling(VK_IMAGE_TILING_OPTIMAL);
		target.image->setUsage(usageFlags);
		target.image->create();

		target.view = new ImageView(vulkanDevice);
		target.view->setType(VK_IMAGE_VIEW_TYPE_2D);
		target.view->setFormat(format);
		target.view->setSubResourceRange({ aspectMask, 0, 1, 0, 1});
		target.view->setImage(target.image);
		target.view->create();

		target.descriptor = { offscreenPass.sampler, target.view->handle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

		if (type == ImageType::DepthStencil) {
			target.descriptor.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		}
	}

	void setObjectName(VkObjectType object_type, uint64_t object_handle, const char* object_name)
	{
		if (vkSetDebugUtilsObjectNameEXT == nullptr) {
			vkSetDebugUtilsObjectNameEXT = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT"));
		}

		VkDebugUtilsObjectNameInfoEXT name_info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
		name_info.objectType = object_type;
		name_info.objectHandle = object_handle;
		name_info.pObjectName = object_name;
		vkSetDebugUtilsObjectNameEXT(device, &name_info);
	}

	// Setup the offscreen images for rendering reflection and refractions
	void prepareOffscreen()
	{
		offscreenPass.width = FB_DIM;
		offscreenPass.height = FB_DIM;

		// Find a suitable depth format
		VkFormat fbDepthFormat;
		VkBool32 validDepthFormat = vks::tools::getSupportedDepthFormat(physicalDevice, &fbDepthFormat);
		assert(validDepthFormat);

		VkSamplerCreateInfo samplerInfo = vks::initializers::samplerCreateInfo();
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeV = samplerInfo.addressModeU;
		samplerInfo.addressModeW = samplerInfo.addressModeU;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.maxAnisotropy = 1.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 1.0f;
		samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(device, &samplerInfo, nullptr, &offscreenPass.sampler));

		createImage(offscreenPass.refraction, ImageType::Color);
		createImage(offscreenPass.reflection, ImageType::Color);
		createImage(offscreenPass.depthRefraction, ImageType::DepthStencil);
		createImage(offscreenPass.depthReflection, ImageType::DepthStencil);

		VkCommandBuffer cb = VulkanContext::device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		vks::tools::setImageLayout(cb, offscreenPass.reflection.image->handle, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
		vks::tools::setImageLayout(cb, offscreenPass.refraction.image->handle, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
		vks::tools::setImageLayout(cb, offscreenPass.depthReflection.image->handle, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, { VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1 });
		vks::tools::setImageLayout(cb, offscreenPass.depthRefraction.image->handle, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, { VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1 });
		VulkanContext::device->flushCommandBuffer(cb, queue);
	}		

	void drawScene(CommandBuffer* cb, SceneDrawType drawType)
	{
		// @todo: lower draw distance for reflection and refraction

		// @todo: rename to localMat
		struct PushConst {
			glm::mat4 scale = glm::mat4(1.0f);
			glm::vec4 clipPlane = glm::vec4(0.0f);
			uint32_t shadows = 0;
			float alpha = 1.0f;
		} pushConst;
		pushConst.shadows = renderShadows ? 1 : 0;

		if (drawType == SceneDrawType::sceneDrawTypeReflect) {
			pushConst.scale = glm::scale(pushConst.scale, glm::vec3(1.0f, -1.0f, 1.0f));
		}

		switch (drawType) {
		case SceneDrawType::sceneDrawTypeRefract:
			pushConst.clipPlane = glm::vec4(0.0f, 1.0f, 0.0f, heightMapSettings.waterPosition + 0.1f);
			pushConst.shadows = 0;
			break;
		case SceneDrawType::sceneDrawTypeReflect:
			pushConst.clipPlane = glm::vec4(0.0f, 1.0f, 0.0f, heightMapSettings.waterPosition + 0.1f);
			pushConst.shadows = 0;
			break;
		}

		bool offscreen = drawType != SceneDrawType::sceneDrawTypeDisplay;

		const uint32_t currentFrameIndex = getCurrentFrameIndex();

		FrameObjects currentFrame = frameObjects[getCurrentFrameIndex()];

		// Skysphere
		if (drawType != SceneDrawType::sceneDrawTypeRefract) {
			vkCmdSetCullMode(cb->handle, drawType == SceneDrawType::sceneDrawTypeReflect ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_FRONT_BIT);
			vkCmdSetCullMode(cb->handle, VK_CULL_MODE_NONE);
			cb->bindPipeline(offscreen ? pipelines.skyOffscreen : pipelines.sky);
			cb->bindDescriptorSets(pipelineLayouts.sky, {
				descriptorSets.skysphere,
				frameObjects[currentFrameIndex].uniformBuffers.shared.descriptorSet },
				0);
			cb->updatePushConstant(pipelineLayouts.sky, 0, &pushConst);
			models.skysphere.draw(cb->handle);
		}

		// Terrain
		// @todo: rework pipeline binding
		if (renderTerrain) {
			cb->bindPipeline(offscreen ? pipelines.terrainOffscreen : pipelines.terrain);
			cb->bindDescriptorSets(pipelineLayouts.terrain,
				{ descriptorSets.terrain,
					frameObjects[currentFrameIndex].uniformBuffers.shared.descriptorSet,
					frameObjects[currentFrameIndex].uniformBuffers.params.descriptorSet,
					frameObjects[currentFrameIndex].uniformBuffers.CSM.descriptorSet },
				0);
			for (auto& terrainChunk : infiniteTerrain.terrainChunks) {
				if (terrainChunk->visible && (terrainChunk->state == TerrainChunk::State::generated)) {
					pushConst.alpha = terrainChunk->alpha;
					if (terrainChunk->alpha < 1.0f) {
						cb->bindPipeline(offscreen ? pipelines.terrainOffscreen : pipelines.terrainBlend);
					}
					else {
						cb->bindPipeline(offscreen ? pipelines.terrainOffscreen : pipelines.terrain);
					}
					cb->updatePushConstant(pipelineLayouts.terrain, 0, &pushConst);
					glm::vec3 pos = glm::vec3((float)terrainChunk->position.x, 0.0f, (float)terrainChunk->position.y) * glm::vec3(chunkDim - 1.0f, 0.0f, chunkDim - 1.0f);
					if (drawType == SceneDrawType::sceneDrawTypeReflect) {
						pos.y += heightMapSettings.waterPosition * 2.0f;
						vkCmdSetCullMode(cb->handle, VK_CULL_MODE_BACK_BIT);
					}
					else {
						vkCmdSetCullMode(cb->handle, VK_CULL_MODE_FRONT_BIT);
					}
					vkCmdPushConstants(cb->handle, pipelineLayouts.terrain->handle, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 96, sizeof(glm::vec3), &pos);
					terrainChunk->draw(cb);
				}
			}
		}

		// Water
		vkCmdSetCullMode(cb->handle, VK_CULL_MODE_BACK_BIT);
		if ((drawType == SceneDrawType::sceneDrawTypeDisplay) && (displayWaterPlane)) {
			cb->bindDescriptorSets(pipelineLayouts.water, { 
				descriptorSets.waterplane,
				frameObjects[currentFrameIndex].uniformBuffers.shared.descriptorSet,
				frameObjects[currentFrameIndex].uniformBuffers.params.descriptorSet,
				frameObjects[currentFrameIndex].uniformBuffers.CSM.descriptorSet }, 
			0);
			cb->bindPipeline(offscreen ? pipelines.waterOffscreen : (waterBlending ? pipelines.waterBlend : pipelines.water));
			for (auto& terrainChunk : infiniteTerrain.terrainChunks) {
				if (terrainChunk->visible && (terrainChunk->state == TerrainChunk::State::generated)) {
					pushConst.alpha = terrainChunk->alpha;
					cb->updatePushConstant(pipelineLayouts.terrain, 0, &pushConst);
					glm::vec3 pos = glm::vec3((float)terrainChunk->position.x, -heightMapSettings.waterPosition, (float)terrainChunk->position.y) * glm::vec3(chunkDim - 1.0f, 1.0f, chunkDim - 1.0f);
					vkCmdPushConstants(cb->handle, pipelineLayouts.terrain->handle, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 96, sizeof(glm::vec3), &pos);
					models.plane.draw(cb->handle);
				}
			}
		}

		vkCmdSetCullMode(cb->handle, VK_CULL_MODE_NONE);
		const VkDeviceSize offsets[1] = { 0 };

		// Trees
		if ((renderTrees) && (drawType != SceneDrawType::sceneDrawTypeRefract) && (drawBatches.trees.instanceBuffers[currentFrameIndex].buffer != VK_NULL_HANDLE) && (drawBatches.trees.instanceBuffers[currentFrameIndex].elements > 0)) {
			cb->bindPipeline(offscreen ? pipelines.treeOffscreen : pipelines.tree);
			cb->bindDescriptorSets(pipelineLayouts.tree, { currentFrame.uniformBuffers.shared.descriptorSet }, 0);
			cb->bindDescriptorSets(pipelineLayouts.tree, { currentFrame.uniformBuffers.params.descriptorSet, descriptorSets.shadowCascades, currentFrame.uniformBuffers.CSM.descriptorSet }, 2);

			pushConst.alpha = 1.0f;
			cb->updatePushConstant(pipelineLayouts.tree, 0, &pushConst);

			glm::vec3 pos = glm::vec3(0.0f);// glm::vec3((float)terrainChunk->position.x, 0.0f, (float)terrainChunk->position.y)* glm::vec3(chunkDim - 1.0f, 0.0f, chunkDim - 1.0f);
			if (drawType == SceneDrawType::sceneDrawTypeReflect) {
				pos.y += heightMapSettings.waterPosition * 2.0f;
			}
			//cb->updatePushConstant(pipelineLayouts.tree, 0, &pushConst);
			vkCmdPushConstants(cb->handle, pipelineLayouts.terrain->handle, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 96, sizeof(glm::vec3), &pos);

			std::vector<DrawBatch*> batches = { &drawBatches.trees, &drawBatches.treeImpostors };
			for (auto& drawBatch : batches) {
				if (drawBatch->instanceBuffers[currentFrameIndex].elements <= 0) {
					continue;
				}
				vkCmdBindVertexBuffers(cb->handle, 0, 1, &drawBatch->model->vertices.buffer, offsets);
				vkCmdBindIndexBuffer(cb->handle, drawBatch->model->indices.buffer, 0, VK_INDEX_TYPE_UINT32);
				vkCmdBindVertexBuffers(cb->handle, 1, 1, &drawBatch->instanceBuffers[currentFrameIndex].buffer, offsets);
				for (auto& node : drawBatch->model->linearNodes) {
					if (node->mesh) {
						vkglTF::Primitive* primitive = node->mesh->primitives[0];
						vkCmdBindDescriptorSets(cb->handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.tree->handle, 1, 1, &primitive->material.descriptorSet, 0, nullptr);
						vkCmdDrawIndexed(cb->handle, primitive->indexCount, drawBatch->instanceBuffers[currentFrameIndex].elements, primitive->firstIndex, 0, 0);
					}
				}
			}
		}

		// Grass
		if (renderGrass && (drawType != SceneDrawType::sceneDrawTypeRefract) && (drawBatches.grass.instanceBuffers[currentFrameIndex].buffer != VK_NULL_HANDLE) && (drawBatches.grass.instanceBuffers[currentFrameIndex].elements > 0)) {

			std::vector<DrawBatch*> batches = { &drawBatches.grass };
			for (auto& drawBatch : batches) {
				vkCmdBindVertexBuffers(cb->handle, 0, 1, &drawBatch->model->vertices.buffer, offsets);
				vkCmdBindIndexBuffer(cb->handle, drawBatch->model->indices.buffer, 0, VK_INDEX_TYPE_UINT32);
				vkCmdBindVertexBuffers(cb->handle, 1, 1, &drawBatch->instanceBuffers[currentFrameIndex].buffer, offsets);

				pushConst.alpha = 1.0f;
				cb->updatePushConstant(pipelineLayouts.tree, 0, &pushConst);

				cb->bindPipeline(offscreen ? pipelines.grassOffscreen : pipelines.grass);
				cb->bindDescriptorSets(pipelineLayouts.tree,  { currentFrame.uniformBuffers.shared.descriptorSet}, 0);
				cb->bindDescriptorSets(pipelineLayouts.tree, { currentFrame.uniformBuffers.params.descriptorSet, descriptorSets.shadowCascades, currentFrame.uniformBuffers.CSM.descriptorSet }, 2);

				glm::vec3 pos = glm::vec3(0.0f);// glm::vec3((float)terrainChunk->position.x, 0.0f, (float)terrainChunk->position.y)* glm::vec3(chunkDim - 1.0f, 0.0f, chunkDim - 1.0f);
				if (drawType == SceneDrawType::sceneDrawTypeReflect) {
					pos.y += heightMapSettings.waterPosition * 2.0f;
				}
				//cb->updatePushConstant(pipelineLayouts.tree, 0, &pushConst);
				vkCmdPushConstants(cb->handle, pipelineLayouts.terrain->handle, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 96, sizeof(glm::vec3), &pos);

				for (auto& node : drawBatch->model->linearNodes) {
					if (node->mesh) {
						vkglTF::Primitive* primitive = node->mesh->primitives[0];
						vkCmdBindDescriptorSets(cb->handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.tree->handle, 1, 1, &primitive->material.descriptorSet, 0, nullptr);
						vkCmdDrawIndexed(cb->handle, primitive->indexCount, drawBatch->instanceBuffers[currentFrameIndex].elements, primitive->firstIndex, 0, 0);
					}
				}
			}
		}

		vkCmdSetCullMode(cb->handle, VK_CULL_MODE_NONE);
	}

	void drawShadowCasters(CommandBuffer* cb) {
		const uint32_t currentFrameIndex = getCurrentFrameIndex();

		FrameObjects currentFrame = frameObjects[getCurrentFrameIndex()];

		glm::vec4 pushConstPos = glm::vec4(0.0f);
		cb->bindPipeline(pipelines.depthpass);
		cb->bindDescriptorSets(depthPass.pipelineLayout, { currentFrame.uniformBuffers.depthPass.descriptorSet }, 0);

		// Terrain
		// @todo: limit distance
		for (auto& terrainChunk : infiniteTerrain.terrainChunks) {
			if (terrainChunk->visible && (terrainChunk->state == TerrainChunk::State::generated)) {
				pushConstPos = glm::vec4((float)terrainChunk->position.x, 0.0f, (float)terrainChunk->position.y, 0.0f) * glm::vec4(chunkDim - 1.0f, 0.0f, chunkDim - 1.0f, 0.0f);
				cb->updatePushConstant(depthPass.pipelineLayout, 0, &pushConstPos);
				terrainChunk->draw(cb);
			}
		}
		// Trees
		if (renderTrees) {
			std::vector<DrawBatch*> batches = { &drawBatches.trees, &drawBatches.treeImpostors };
			for (auto drawBatch : batches) {
				if (drawBatches.trees.instanceBuffers[currentFrameIndex].buffer != VK_NULL_HANDLE) {
					vkCmdSetCullMode(cb->handle, VK_CULL_MODE_NONE);
					const VkDeviceSize offsets[1] = { 0 };
					cb->bindPipeline(pipelines.depthpassTree);
					vkCmdBindVertexBuffers(cb->handle, 1, 1, &drawBatch->instanceBuffers[currentFrameIndex].buffer, offsets);
					pushConstPos = glm::vec4(0.0f);
					cb->updatePushConstant(depthPass.pipelineLayout, 0, &pushConstPos);
					drawBatch->model->draw(cb->handle, vkglTF::RenderFlags::BindImages, depthPass.pipelineLayout->handle, 1, drawBatch->instanceBuffers[currentFrameIndex].elements);
				}
			}
		}
	}

	/*
		CSM
	*/

	void prepareCSM()
	{
		VkFormat depthFormat;
		vks::tools::getSupportedDepthFormat(physicalDevice, &depthFormat);

		/*
			Layered depth image and views
		*/
		depth.image = new Image(vulkanDevice);
		depth.image->setType(VK_IMAGE_TYPE_2D);
		depth.image->setFormat(depthFormat);
		depth.image->setExtent({ SHADOWMAP_DIM, SHADOWMAP_DIM, 1 });
		depth.image->setNumArrayLayers(SHADOW_MAP_CASCADE_COUNT);
		depth.image->setUsage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
		depth.image->setTiling(VK_IMAGE_TILING_OPTIMAL);
		depth.image->create();

		// Full depth map view (all layers)
		depth.view = new ImageView(vulkanDevice);
		depth.view->setImage(depth.image);
		depth.view->setType(VK_IMAGE_VIEW_TYPE_2D_ARRAY);
		depth.view->setFormat(depthFormat);
		depth.view->setSubResourceRange({ VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, SHADOW_MAP_CASCADE_COUNT });
		depth.view->create();

		VkCommandBuffer cb = VulkanContext::device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		vks::tools::setImageLayout(cb, depth.image->handle, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, { VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, SHADOW_MAP_CASCADE_COUNT });
		// @todo: VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		VulkanContext::device->flushCommandBuffer(cb, queue);

		// Image view for this cascade's layer (inside the depth map) this view is used to render to that specific depth image layer
		VkImageViewCreateInfo imageViewCI = vks::initializers::imageViewCreateInfo();
		imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		imageViewCI.format = depthFormat;
		imageViewCI.subresourceRange = {};
		imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		imageViewCI.subresourceRange.levelCount = 1;
		imageViewCI.subresourceRange.layerCount = SHADOW_MAP_CASCADE_COUNT;
		imageViewCI.image = depth.image->handle;
		VK_CHECK_RESULT(vkCreateImageView(device, &imageViewCI, nullptr, &cascadesView));

		// Shared sampler for cascade depth reads
		VkSamplerCreateInfo sampler = vks::initializers::samplerCreateInfo();
		sampler.magFilter = VK_FILTER_LINEAR;
		sampler.minFilter = VK_FILTER_LINEAR;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.addressModeV = sampler.addressModeU;
		sampler.addressModeW = sampler.addressModeU;
		sampler.mipLodBias = 0.0f;
		sampler.maxAnisotropy = 1.0f;
		sampler.minLod = 0.0f;
		sampler.maxLod = 1.0f;
		sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(device, &sampler, nullptr, &depth.sampler));
	}

	/*
		Calculate frustum split depths and matrices for the shadow map cascades
		Based on https://johanmedestrom.wordpress.com/2016/03/18/opengl-cascaded-shadow-maps/
	*/
	void updateCascades()
	{
		float cascadeSplits[SHADOW_MAP_CASCADE_COUNT];

		float nearClip = camera.getNearClip();
		float farClip = camera.getFarClip();
		float clipRange = farClip - nearClip;

		float minZ = nearClip;
		float maxZ = nearClip + clipRange;

		float range = maxZ - minZ;
		float ratio = maxZ / minZ;

		// Calculate split depths based on view camera furstum
		// Based on method presentd in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
		for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
			float p = (i + 1) / static_cast<float>(SHADOW_MAP_CASCADE_COUNT);
			float log = minZ * std::pow(ratio, p);
			float uniform = minZ + range * p;
			float d = cascadeSplitLambda * (log - uniform) + uniform;
			cascadeSplits[i] = (d - nearClip) / clipRange;
		}

		// Calculate orthographic projection matrix for each cascade
		float lastSplitDist = 0.0;
		for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
			float splitDist = cascadeSplits[i];

			glm::vec3 frustumCorners[8] = {
				glm::vec3(-1.0f,  1.0f, -1.0f),
				glm::vec3( 1.0f,  1.0f, -1.0f),
				glm::vec3( 1.0f, -1.0f, -1.0f),
				glm::vec3(-1.0f, -1.0f, -1.0f),
				glm::vec3(-1.0f,  1.0f,  1.0f),
				glm::vec3( 1.0f,  1.0f,  1.0f),
				glm::vec3( 1.0f, -1.0f,  1.0f),
				glm::vec3(-1.0f, -1.0f,  1.0f),
			};

			// Project frustum corners into world space
			glm::mat4 invCam = glm::inverse(camera.matrices.perspective * camera.matrices.view);
			for (uint32_t i = 0; i < 8; i++) {
				glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[i], 1.0f);
				frustumCorners[i] = invCorner / invCorner.w;
			}

			for (uint32_t i = 0; i < 4; i++) {
				glm::vec3 dist = frustumCorners[i + 4] - frustumCorners[i];
				frustumCorners[i + 4] = frustumCorners[i] + (dist * splitDist);
				frustumCorners[i] = frustumCorners[i] + (dist * lastSplitDist);
			}

			// Get frustum center
			glm::vec3 frustumCenter = glm::vec3(0.0f);
			for (uint32_t i = 0; i < 8; i++) {
				frustumCenter += frustumCorners[i];
			}
			frustumCenter /= 8.0f;

			float radius = 0.0f;
			for (uint32_t i = 0; i < 8; i++) {
				float distance = glm::length(frustumCorners[i] - frustumCenter);
				radius = glm::max(radius, distance);
			}
			radius = std::ceil(radius * 16.0f) / 16.0f;

			glm::vec3 maxExtents = glm::vec3(radius);
			glm::vec3 minExtents = -maxExtents;

			glm::vec3 lightDir = glm::normalize(-lightPos);
			glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter - lightDir * -minExtents.z, frustumCenter, glm::vec3(0.0f, 1.0f, 0.0f));
			glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f, maxExtents.z - minExtents.z);

			// Store split distance and matrix in cascade
			cascades[i].splitDepth = (camera.getNearClip() + splitDist * clipRange) * -1.0f;
			cascades[i].viewProjMatrix = lightOrthoMatrix * lightViewMatrix;

			lastSplitDist = cascadeSplits[i];
		}
	}

	void drawCSM(CommandBuffer *cb) {
		// Generate depth map cascades
		// All cascades are rendered in one pass using a layered depth image and multiview

		cb->setViewport(0, 0, (float)SHADOWMAP_DIM, (float)SHADOWMAP_DIM, 0.0f, 1.0f);
		cb->setScissor(0, 0, SHADOWMAP_DIM, SHADOWMAP_DIM);
		drawShadowCasters(cb);
	}

	void loadSkySphere(const std::string filename)
	{
		if (textures.skySphere.image != VK_NULL_HANDLE) {
			vkQueueWaitIdle(queue);
			textures.skySphere.destroy();
		}
		textures.skySphere.loadFromFile(getAssetPath() + "textures/" + filename, VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
		if ((descriptorSets.skysphere) && (!descriptorSets.skysphere->empty())) {
			descriptorSets.skysphere->updateDescriptor(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &textures.skySphere.descriptor);
		}
	}
	
	void loadTerrainSet(const std::string name)
	{
		const std::string path = getAssetPath() + "textures/terrainsets/" + name + "/";
		std::vector<std::string> filenames;
		for (int i = 0; i < 6; i++) {
			filenames.push_back(path + std::to_string(i) + ".ktx");
		}
		if (textures.terrainArray.image != VK_NULL_HANDLE) {
			vkQueueWaitIdle(queue);
			textures.terrainArray.destroy();
		}
		textures.terrainArray.loadFromFiles(filenames, VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
		if (descriptorSets.terrain != VK_NULL_HANDLE) {
			textures.terrainArray.descriptor.sampler = terrainSampler;
			descriptorSets.terrain->updateDescriptor(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &textures.terrainArray.descriptor);
		}
	}

	void loadAssets()
	{
		models.skysphere.loadFromFile(getAssetPath() + "scenes/geosphere.gltf", vulkanDevice, queue);
		models.plane.loadFromFile(getAssetPath() + "scenes/plane.gltf", vulkanDevice, queue);

		const int fileLoadingFlags = vkglTF::FileLoadingFlags::FlipY | vkglTF::FileLoadingFlags::PreTransformVertices;

		treeModelInfo.resize(treeTypes.size());
		for (size_t i = 0; i < treeTypes.size(); i++) {
			treeModelInfo[i].name = treeTypes[i];
			treeModelInfo[i].models.model.loadFromFile(getAssetPath() + "scenes/trees/" + treeTypes[i] + "/" + treeTypes[i] + ".gltf", vulkanDevice, queue, fileLoadingFlags);
			treeModelInfo[i].models.imposter.loadFromFile(getAssetPath() + "scenes/trees/" + treeTypes[i] + "_imposter/" + treeTypes[i] + "_imposter.gltf", vulkanDevice, queue, fileLoadingFlags);
		}

		grassModels.resize(grassTypes.size());
		for (size_t i = 0; i < grassTypes.size(); i++) {
			grassModels[i].loadFromFile(getAssetPath() + "scenes/" + grassTypes[i] + ".gltf", vulkanDevice, queue, fileLoadingFlags);
		}

		loadSkySphere(heightMapSettings.skySphere);
		textures.waterNormalMap.loadFromFile(getAssetPath() + "textures/water_normal_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
		loadTerrainSet(heightMapSettings.terrainSet);

		VkSamplerCreateInfo samplerInfo = vks::initializers::samplerCreateInfo();

		// Setup a repeating sampler for the terrain texture layers
		// @todo: Sampler class, that can e.g. check against device if anisotropy > max and then lower it
		// also enable aniso if max is > 1.0
		samplerInfo = vks::initializers::samplerCreateInfo();
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = samplerInfo.addressModeU;
		samplerInfo.addressModeW = samplerInfo.addressModeU;
		samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = (float)textures.terrainArray.mipLevels;
		samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		if (deviceFeatures.samplerAnisotropy)
		{
			samplerInfo.maxAnisotropy = 4.0f;
			samplerInfo.anisotropyEnable = VK_TRUE;
		}
		VK_CHECK_RESULT(vkCreateSampler(device, &samplerInfo, nullptr, &terrainSampler));
		textures.terrainArray.descriptor.sampler = terrainSampler;
	}

	void updateHeightmap()
	{
		//infiniteTerrain.updateChunks();
		infiniteTerrain.viewerPosition = glm::vec2(camera.position.x, camera.position.z);
		infiniteTerrain.updateVisibleChunks(frustum);
		infiniteTerrain.update(frameTimer);
		//infiniteTerrain.updateChunks(); @todo
		if (infiniteTerrain.terrainChunkgsUpdateList.size() > 0) {
			//vks::ThreadPool threadPool;
			//threadPool.setThreadCount(numThreads);			
			for (size_t i = 0; i < infiniteTerrain.terrainChunkgsUpdateList.size(); i++) {

				TerrainChunk* chunk = infiniteTerrain.terrainChunkgsUpdateList[i];
				if (chunk->state == TerrainChunk::State::_new) {
					chunk->state == TerrainChunk::State::generating;
					std::thread chunkThread(&VulkanExample::updateTerrainChunkThreadFn, this, infiniteTerrain.terrainChunkgsUpdateList[i]);
					chunkThread.detach();
				}
			}
			infiniteTerrain.terrainChunkgsUpdateList.clear();
		}
		// @todo
		// terrainChunk->updateHeightMap();
	}

	void setupDescriptorPool()
	{
		// @todo: proper sizes
		descriptorPool = new DescriptorPool(device);
		descriptorPool->setMaxSets(16);
		descriptorPool->addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 32);
		descriptorPool->addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 32);
		descriptorPool->create();
	}

	void setupDescriptorSetLayout()
	{
		// @todo
		descriptorSetLayouts.ubo = new DescriptorSetLayout(device);
		descriptorSetLayouts.ubo->addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSetLayouts.ubo->create();

		// @todo
		descriptorSetLayouts.images = new DescriptorSetLayout(device);
		descriptorSetLayouts.images->addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSetLayouts.images->create();

		// @todo
		descriptorSetLayouts.shadowCascades = new DescriptorSetLayout(device);
		descriptorSetLayouts.shadowCascades->addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSetLayouts.shadowCascades->addBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSetLayouts.shadowCascades->create();

		// Shared (use all layout bindings)
		descriptorSetLayouts.textured = new DescriptorSetLayout(device);
		descriptorSetLayouts.textured->addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSetLayouts.textured->addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSetLayouts.textured->addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSetLayouts.textured->addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSetLayouts.textured->addBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSetLayouts.textured->addBinding(5, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSetLayouts.textured->create();

		pipelineLayouts.textured = new PipelineLayout(device);
		pipelineLayouts.textured->addLayout(descriptorSetLayouts.textured);
		pipelineLayouts.textured->addLayout(descriptorSetLayouts.ubo);
		pipelineLayouts.textured->addPushConstantRange(108, 0, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		pipelineLayouts.textured->create();

		// Debug
		pipelineLayouts.debug = new PipelineLayout(device);
		pipelineLayouts.debug->addLayout(descriptorSetLayouts.textured);
		pipelineLayouts.debug->addPushConstantRange(sizeof(uint32_t), 0, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		pipelineLayouts.debug->create();

		// Water
		descriptorSetLayouts.water = new DescriptorSetLayout(device);
		descriptorSetLayouts.water->addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSetLayouts.water->addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSetLayouts.water->addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSetLayouts.water->addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSetLayouts.water->addBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSetLayouts.water->create();

		pipelineLayouts.water = new PipelineLayout(device);
		pipelineLayouts.water->addLayout(descriptorSetLayouts.water);
		pipelineLayouts.water->addLayout(descriptorSetLayouts.ubo);
		pipelineLayouts.water->addLayout(descriptorSetLayouts.ubo);
		pipelineLayouts.water->addLayout(descriptorSetLayouts.ubo);
		pipelineLayouts.water->addPushConstantRange(108, 0, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		pipelineLayouts.water->create();

		// Terrain
		descriptorSetLayouts.terrain = new DescriptorSetLayout(device);
		descriptorSetLayouts.terrain->addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSetLayouts.terrain->addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSetLayouts.terrain->create();

		pipelineLayouts.terrain = new PipelineLayout(device);
		pipelineLayouts.terrain->addLayout(descriptorSetLayouts.terrain);
		pipelineLayouts.terrain->addLayout(descriptorSetLayouts.ubo);
		pipelineLayouts.terrain->addLayout(descriptorSetLayouts.ubo);
		pipelineLayouts.terrain->addLayout(descriptorSetLayouts.ubo);
		pipelineLayouts.terrain->addPushConstantRange(108, 0, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		pipelineLayouts.terrain->create();

		// Trees
		pipelineLayouts.tree = new PipelineLayout(device);
		pipelineLayouts.tree->addLayout(descriptorSetLayouts.ubo);
		pipelineLayouts.tree->addLayout(vkglTF::descriptorSetLayoutImage);
		pipelineLayouts.tree->addLayout(descriptorSetLayouts.ubo);
		pipelineLayouts.tree->addLayout(descriptorSetLayouts.shadowCascades);
		pipelineLayouts.tree->addLayout(descriptorSetLayouts.ubo);
		pipelineLayouts.tree->addPushConstantRange(108, 0, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		pipelineLayouts.tree->create();

		// Skysphere
		descriptorSetLayouts.skysphere = new DescriptorSetLayout(device);
		descriptorSetLayouts.skysphere->addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSetLayouts.skysphere->create();

		pipelineLayouts.sky = new PipelineLayout(device);
		pipelineLayouts.sky->addLayout(descriptorSetLayouts.skysphere);
		pipelineLayouts.sky->addLayout(descriptorSetLayouts.ubo);
		pipelineLayouts.sky->addPushConstantRange(sizeof(glm::mat4) + sizeof(glm::vec4) + sizeof(uint32_t), 0, VK_SHADER_STAGE_VERTEX_BIT);
		pipelineLayouts.sky->create();

		// Depth pass
		depthPass.descriptorSetLayout = new DescriptorSetLayout(device);
		depthPass.descriptorSetLayout->addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		depthPass.descriptorSetLayout->create();

		depthPass.pipelineLayout = new PipelineLayout(device);
		depthPass.pipelineLayout->addLayout(depthPass.descriptorSetLayout);
		depthPass.pipelineLayout->addLayout(vkglTF::descriptorSetLayoutImage);
		depthPass.pipelineLayout->addPushConstantRange(sizeof(glm::vec4), 0, VK_SHADER_STAGE_VERTEX_BIT);
		depthPass.pipelineLayout->create();

		// Cascade debug
		cascadeDebug.descriptorSetLayout = new DescriptorSetLayout(device);
		cascadeDebug.descriptorSetLayout->addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		cascadeDebug.descriptorSetLayout->create();

		cascadeDebug.pipelineLayout = new PipelineLayout(device);
		cascadeDebug.pipelineLayout->addLayout(cascadeDebug.descriptorSetLayout);
		cascadeDebug.pipelineLayout->addPushConstantRange(sizeof(uint32_t), 0, VK_SHADER_STAGE_VERTEX_BIT);
		cascadeDebug.pipelineLayout->create();

	}

	void setupDescriptorSet()
	{
		VkDescriptorImageInfo shadowMapDescriptor = vks::initializers::descriptorImageInfo(depth.sampler, depth.view->handle, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
		VkDescriptorImageInfo depthRefractionDescriptor = vks::initializers::descriptorImageInfo(offscreenPass.sampler, offscreenPass.depthRefraction.view->handle, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);

		// Water plane
		descriptorSets.waterplane = new DescriptorSet(device);
		descriptorSets.waterplane->setPool(descriptorPool);
		descriptorSets.waterplane->addLayout(descriptorSetLayouts.water);
		descriptorSets.waterplane->addDescriptor(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &offscreenPass.refraction.descriptor);
		descriptorSets.waterplane->addDescriptor(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &offscreenPass.reflection.descriptor);
		descriptorSets.waterplane->addDescriptor(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &offscreenPass.depthRefraction.descriptor);
		descriptorSets.waterplane->addDescriptor(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &textures.waterNormalMap.descriptor);
		descriptorSets.waterplane->addDescriptor(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &shadowMapDescriptor);
		//@todo
		//descriptorSets.waterplane->addDescriptor(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uniformBuffers.vsShared.descriptor);
		//descriptorSets.waterplane->addDescriptor(5, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uniformBuffers.CSM.descriptor);
		descriptorSets.waterplane->create();
			   			   		
		// Debug quad
		descriptorSets.debugquad = new DescriptorSet(device);
		descriptorSets.debugquad->setPool(descriptorPool);
		descriptorSets.debugquad->addLayout(descriptorSetLayouts.textured);
		descriptorSets.debugquad->addDescriptor(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &offscreenPass.depthReflection.descriptor);
		descriptorSets.debugquad->addDescriptor(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &offscreenPass.depthRefraction.descriptor); // @todo
		descriptorSets.debugquad->create();

		// Terrain
		descriptorSets.terrain = new DescriptorSet(device);
		descriptorSets.terrain->setPool(descriptorPool);
		descriptorSets.terrain->addLayout(descriptorSetLayouts.terrain);
		descriptorSets.terrain->addDescriptor(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &textures.terrainArray.descriptor);
		descriptorSets.terrain->addDescriptor(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &shadowMapDescriptor);
		descriptorSets.terrain->create();

		// Skysphere
		descriptorSets.skysphere = new DescriptorSet(device);
		descriptorSets.skysphere->setPool(descriptorPool);
		descriptorSets.skysphere->addLayout(descriptorSetLayouts.skysphere);
		descriptorSets.skysphere->addDescriptor(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &textures.skySphere.descriptor);
		descriptorSets.skysphere->create();

		// Cascade debug
		cascadeDebug.descriptorSet = new DescriptorSet(device);
		cascadeDebug.descriptorSet->setPool(descriptorPool);
		cascadeDebug.descriptorSet->addLayout(cascadeDebug.descriptorSetLayout);
		cascadeDebug.descriptorSet->addDescriptor(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &shadowMapDescriptor);
		cascadeDebug.descriptorSet->create();

		// Shadow cascades
		descriptorSets.shadowCascades = new DescriptorSet(device);
		descriptorSets.shadowCascades->setPool(descriptorPool);
		descriptorSets.shadowCascades->addLayout(descriptorSetLayouts.shadowCascades);
		descriptorSets.shadowCascades->addDescriptor(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &shadowMapDescriptor);
		descriptorSets.shadowCascades->create();
	}

	void createPipelines()
	{
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE,VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_CULL_MODE, VK_DYNAMIC_STATE_BLEND_CONSTANTS };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);

		if (settings.multiSampling) {
			multisampleState.rasterizationSamples = settings.sampleCount;
		}

		// Vertex bindings and attributes
		// Terrain / shared
		const VkVertexInputBindingDescription vertexInputBinding = vks::initializers::vertexInputBindingDescription(0, sizeof(vks::HeightMap::Vertex), VK_VERTEX_INPUT_RATE_VERTEX);
		const std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
			vks::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vks::HeightMap::Vertex, pos)),
			vks::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vks::HeightMap::Vertex, normal)),
			vks::initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(vks::HeightMap::Vertex, uv)),
			vks::initializers::vertexInputAttributeDescription(0, 3, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vks::HeightMap::Vertex, color)),
			vks::initializers::vertexInputAttributeDescription(0, 4, VK_FORMAT_R32_SFLOAT, offsetof(vks::HeightMap::Vertex, terrainHeight)),
		};
		VkPipelineVertexInputStateCreateInfo vertexInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
		vertexInputState.vertexBindingDescriptionCount = 1;
		vertexInputState.pVertexBindingDescriptions = &vertexInputBinding;
		vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
		vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();

		// glTF models
		VkPipelineVertexInputStateCreateInfo* vertexInputStateModel = vkglTF::Vertex::getPipelineVertexInputState({
			vkglTF::VertexComponent::Position, 
			vkglTF::VertexComponent::Normal, 
			vkglTF::VertexComponent::UV 
		});

		// Instanced
		VkPipelineVertexInputStateCreateInfo vertexInputStateModelInstanced = vks::initializers::pipelineVertexInputStateCreateInfo();
		std::vector<VkVertexInputBindingDescription> bindingDescriptions;
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions;

		bindingDescriptions = {
			vks::initializers::vertexInputBindingDescription(0, sizeof(vkglTF::Vertex), VK_VERTEX_INPUT_RATE_VERTEX),
			vks::initializers::vertexInputBindingDescription(1, sizeof(InstanceData), VK_VERTEX_INPUT_RATE_INSTANCE)
		};

		attributeDescriptions = {
			vks::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0),
			vks::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3),
			vks::initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 6),
			vks::initializers::vertexInputAttributeDescription(1, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(InstanceData, pos)),
			vks::initializers::vertexInputAttributeDescription(1, 4, VK_FORMAT_R32G32B32_SFLOAT, offsetof(InstanceData, scale)),
			vks::initializers::vertexInputAttributeDescription(1, 5, VK_FORMAT_R32G32B32_SFLOAT, offsetof(InstanceData, rotation)),
			vks::initializers::vertexInputAttributeDescription(1, 6, VK_FORMAT_R32G32_SFLOAT, offsetof(InstanceData, uv)),
			vks::initializers::vertexInputAttributeDescription(1, 7, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(InstanceData, color)),
		};
		vertexInputStateModelInstanced.pVertexBindingDescriptions = bindingDescriptions.data();
		vertexInputStateModelInstanced.pVertexAttributeDescriptions = attributeDescriptions.data();
		vertexInputStateModelInstanced.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
		vertexInputStateModelInstanced.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());

		// Empty state (no input)
		VkPipelineVertexInputStateCreateInfo vertexInputStateEmpty = vks::initializers::pipelineVertexInputStateCreateInfo();

		VkGraphicsPipelineCreateInfo pipelineCI{};
		pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineCI.pVertexInputState = &vertexInputState;
		pipelineCI.pInputAssemblyState = &inputAssemblyState;
		pipelineCI.pRasterizationState = &rasterizationState;
		pipelineCI.pColorBlendState = &colorBlendState;
		pipelineCI.pMultisampleState = &multisampleState;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pDynamicState = &dynamicState;

		rasterizationState.cullMode = VK_CULL_MODE_NONE;
		depthStencilState.depthTestEnable = VK_FALSE;

		// New create info to define color, depth and stencil attachments at pipeline create time
		VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{};
		pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
		pipelineRenderingCreateInfo.colorAttachmentCount = 1;
		pipelineRenderingCreateInfo.pColorAttachmentFormats = &swapChain.colorFormat;
		pipelineRenderingCreateInfo.depthAttachmentFormat = depthFormat;
		pipelineRenderingCreateInfo.stencilAttachmentFormat = depthFormat;

		// @todo: not required
		VkPipelineRenderingCreateInfo pipelineRenderingCreateInfoOffscreen{};
		pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
		pipelineRenderingCreateInfo.colorAttachmentCount = 1;
		pipelineRenderingCreateInfo.pColorAttachmentFormats = &swapChain.colorFormat;
		pipelineRenderingCreateInfo.depthAttachmentFormat = depthFormat;
		pipelineRenderingCreateInfo.stencilAttachmentFormat = depthFormat;

		// Debug
		pipelines.debug = new Pipeline(device);
		pipelines.debug->setCreateInfo(pipelineCI);
		pipelines.debug->setVertexInputState(&vertexInputStateEmpty);
		pipelines.debug->setCache(pipelineCache);
		pipelines.debug->setLayout(pipelineLayouts.debug);
		pipelines.debug->addShader(getAssetPath() + "shaders/quad.vert.spv");
		pipelines.debug->addShader(getAssetPath() + "shaders/quad.frag.spv");
		pipelines.debug->setpNext(&pipelineRenderingCreateInfo);
		pipelines.debug->create();
		// Debug cascades
		cascadeDebug.pipeline = new Pipeline(device);
		cascadeDebug.pipeline->setCreateInfo(pipelineCI);
		cascadeDebug.pipeline->setVertexInputState(&vertexInputStateEmpty);
		cascadeDebug.pipeline->setCache(pipelineCache);
		cascadeDebug.pipeline->setLayout(cascadeDebug.pipelineLayout);
		cascadeDebug.pipeline->addShader(getAssetPath() + "shaders/debug_csm.vert.spv");
		cascadeDebug.pipeline->addShader(getAssetPath() + "shaders/debug_csm.frag.spv");
		cascadeDebug.pipeline->setpNext(&pipelineRenderingCreateInfo);
		cascadeDebug.pipeline->create();

		depthStencilState.depthTestEnable = VK_TRUE;

		// Water
		rasterizationState.cullMode = VK_CULL_MODE_NONE;
		pipelines.water = new Pipeline(device);
		pipelines.water->setCreateInfo(pipelineCI);
		pipelines.water->setSampleCount(settings.multiSampling ? settings.sampleCount : VK_SAMPLE_COUNT_1_BIT);
		pipelines.water->setVertexInputState(vertexInputStateModel);
		pipelines.water->setCache(pipelineCache);
		pipelines.water->setLayout(pipelineLayouts.water);
		pipelines.water->addShader(getAssetPath() + "shaders/water.vert.spv");
		pipelines.water->addShader(getAssetPath() + "shaders/water.frag.spv");
		pipelines.water->setpNext(&pipelineRenderingCreateInfo);
		pipelines.water->create();

		blendAttachmentState.blendEnable = VK_TRUE;
		blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
		blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;

		pipelines.waterBlend = new Pipeline(device);
		pipelines.waterBlend->setCreateInfo(pipelineCI);
		pipelines.waterBlend->setSampleCount(settings.multiSampling ? settings.sampleCount : VK_SAMPLE_COUNT_1_BIT);
		pipelines.waterBlend->setVertexInputState(vertexInputStateModel);
		pipelines.waterBlend->setCache(pipelineCache);
		pipelines.waterBlend->setLayout(pipelineLayouts.water);
		pipelines.waterBlend->addShader(getAssetPath() + "shaders/water.vert.spv");
		pipelines.waterBlend->addShader(getAssetPath() + "shaders/water.frag.spv");
		pipelines.waterBlend->setpNext(&pipelineRenderingCreateInfo);
		pipelines.waterBlend->create();

		// Offscreen
		pipelines.waterOffscreen = new Pipeline(device);
		pipelines.waterOffscreen->setCreateInfo(pipelineCI);
		pipelines.waterOffscreen->setSampleCount(VK_SAMPLE_COUNT_1_BIT);
		pipelines.waterOffscreen->setVertexInputState(vertexInputStateModel);
		pipelines.waterOffscreen->setCache(pipelineCache);
		pipelines.waterOffscreen->setLayout(pipelineLayouts.water);
		pipelines.waterOffscreen->addShader(getAssetPath() + "shaders/water.vert.spv");
		pipelines.waterOffscreen->addShader(getAssetPath() + "shaders/water.frag.spv");
		pipelines.waterOffscreen->setpNext(&pipelineRenderingCreateInfo);
		pipelines.waterOffscreen->create();

		// Terrain
		//rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
		rasterizationState.cullMode = VK_CULL_MODE_NONE;
		pipelines.terrain = new Pipeline(device);
		pipelines.terrain->setCreateInfo(pipelineCI);
		pipelines.terrain->setSampleCount(settings.multiSampling ? settings.sampleCount : VK_SAMPLE_COUNT_1_BIT);
		pipelines.terrain->setVertexInputState(&vertexInputState);
		pipelines.terrain->setCache(pipelineCache);
		pipelines.terrain->setLayout(pipelineLayouts.terrain);
		pipelines.terrain->addShader(getAssetPath() + "shaders/terrain.vert.spv");
		pipelines.terrain->addShader(getAssetPath() + "shaders/terrain.frag.spv");
		pipelines.terrain->setpNext(&pipelineRenderingCreateInfo);
		pipelines.terrain->create();

		blendAttachmentState.blendEnable = VK_TRUE;
		blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
		blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;

		pipelines.terrainBlend = new Pipeline(device);
		pipelines.terrainBlend->setCreateInfo(pipelineCI);
		pipelines.terrainBlend->setVertexInputState(&vertexInputState);
		pipelines.terrainBlend->setCache(pipelineCache);
		pipelines.terrainBlend->setLayout(pipelineLayouts.terrain);
		pipelines.terrainBlend->addShader(getAssetPath() + "shaders/terrain.vert.spv");
		pipelines.terrainBlend->addShader(getAssetPath() + "shaders/terrain.frag.spv");
		pipelines.terrainBlend->setpNext(&pipelineRenderingCreateInfo);
		pipelines.terrainBlend->create();

		blendAttachmentState.blendEnable = VK_FALSE;

		// Offscreen
		multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		//rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
		pipelines.terrainOffscreen = new Pipeline(device);
		pipelines.terrainOffscreen->setCreateInfo(pipelineCI);
		pipelines.terrainOffscreen->setSampleCount(VK_SAMPLE_COUNT_1_BIT);
		pipelines.terrainOffscreen->setVertexInputState(&vertexInputState);
		pipelines.terrainOffscreen->setCache(pipelineCache);
		pipelines.terrainOffscreen->setLayout(pipelineLayouts.terrain);
		pipelines.terrainOffscreen->addShader(getAssetPath() + "shaders/terrain.vert.spv");
		pipelines.terrainOffscreen->addShader(getAssetPath() + "shaders/terrain.frag.spv");
		pipelines.terrainOffscreen->setpNext(&pipelineRenderingCreateInfo);
		pipelines.terrainOffscreen->create();

		// Sky
		rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
		depthStencilState.depthWriteEnable = VK_FALSE;
		pipelines.sky = new Pipeline(device);
		pipelines.sky->setCreateInfo(pipelineCI);
		pipelines.sky->setSampleCount(settings.multiSampling ? settings.sampleCount : VK_SAMPLE_COUNT_1_BIT);
		pipelines.sky->setVertexInputState(vertexInputStateModel);
		pipelines.sky->setCache(pipelineCache);
		pipelines.sky->setLayout(pipelineLayouts.sky);
		pipelines.sky->addShader(getAssetPath() + "shaders/skysphere.vert.spv");
		pipelines.sky->addShader(getAssetPath() + "shaders/skysphere.frag.spv");
		pipelines.sky->setpNext(&pipelineRenderingCreateInfo);
		pipelines.sky->create();
		// Offscreen
		multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		pipelines.skyOffscreen = new Pipeline(device);
		pipelines.skyOffscreen->setCreateInfo(pipelineCI);
		pipelines.skyOffscreen->setSampleCount(VK_SAMPLE_COUNT_1_BIT);
		pipelines.skyOffscreen->setVertexInputState(vertexInputStateModel);
		pipelines.skyOffscreen->setCache(pipelineCache);
		pipelines.skyOffscreen->setLayout(pipelineLayouts.sky);
		pipelines.skyOffscreen->addShader(getAssetPath() + "shaders/skysphere.vert.spv");
		pipelines.skyOffscreen->addShader(getAssetPath() + "shaders/skysphere.frag.spv");
		pipelines.skyOffscreen->setpNext(&pipelineRenderingCreateInfo);
		pipelines.skyOffscreen->create();

		// Trees
		rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizationState.cullMode = VK_CULL_MODE_NONE;
		depthStencilState.depthWriteEnable = VK_TRUE;

		pipelines.tree = new Pipeline(device);
		pipelines.tree->setCreateInfo(pipelineCI);
		pipelines.tree->setSampleCount(settings.multiSampling ? settings.sampleCount : VK_SAMPLE_COUNT_1_BIT);
		pipelines.tree->setVertexInputState(&vertexInputStateModelInstanced);
		pipelines.tree->setCache(pipelineCache);
		pipelines.tree->setLayout(pipelineLayouts.tree);
		pipelines.tree->addShader(getAssetPath() + "shaders/tree.vert.spv");
		pipelines.tree->addShader(getAssetPath() + "shaders/tree.frag.spv");
		pipelines.tree->setpNext(&pipelineRenderingCreateInfo);
		pipelines.tree->create();
		// Offscreen
		pipelines.treeOffscreen = new Pipeline(device);
		pipelines.treeOffscreen->setCreateInfo(pipelineCI);
		pipelines.treeOffscreen->setSampleCount(VK_SAMPLE_COUNT_1_BIT);
		pipelines.treeOffscreen->setVertexInputState(&vertexInputStateModelInstanced);
		pipelines.treeOffscreen->setCache(pipelineCache);
		pipelines.treeOffscreen->setLayout(pipelineLayouts.tree);
		pipelines.treeOffscreen->addShader(getAssetPath() + "shaders/tree.vert.spv");
		pipelines.treeOffscreen->addShader(getAssetPath() + "shaders/tree.frag.spv");
		pipelines.treeOffscreen->setpNext(&pipelineRenderingCreateInfo);
		pipelines.treeOffscreen->create();

		// Grass
		pipelines.grass = new Pipeline(device);
		pipelines.grass->setCreateInfo(pipelineCI);
		pipelines.grass->setSampleCount(settings.multiSampling ? settings.sampleCount : VK_SAMPLE_COUNT_1_BIT);
		pipelines.grass->setVertexInputState(&vertexInputStateModelInstanced);
		pipelines.grass->setCache(pipelineCache);
		pipelines.grass->setLayout(pipelineLayouts.tree);
		pipelines.grass->addShader(getAssetPath() + "shaders/grass.vert.spv");
		pipelines.grass->addShader(getAssetPath() + "shaders/grass.frag.spv");
		pipelines.grass->setpNext(&pipelineRenderingCreateInfo);
		pipelines.grass->create();
		// Offscreen
		pipelines.grassOffscreen = new Pipeline(device);
		pipelines.grassOffscreen->setCreateInfo(pipelineCI);
		pipelines.grassOffscreen->setSampleCount(VK_SAMPLE_COUNT_1_BIT);
		pipelines.grassOffscreen->setVertexInputState(&vertexInputStateModelInstanced);
		pipelines.grassOffscreen->setCache(pipelineCache);
		pipelines.grassOffscreen->setLayout(pipelineLayouts.tree);
		pipelines.grassOffscreen->addShader(getAssetPath() + "shaders/grass.vert.spv");
		pipelines.grassOffscreen->addShader(getAssetPath() + "shaders/grass.frag.spv");
		pipelines.grassOffscreen->setpNext(&pipelineRenderingCreateInfo);
		pipelines.grassOffscreen->create();

		// Shadow map depth pass
		depthStencilState.depthWriteEnable = VK_TRUE;
		blendAttachmentState.blendEnable = VK_FALSE;
		multisampleState.alphaToCoverageEnable = VK_FALSE;

		VkPipelineRenderingCreateInfo pipelineRenderingCreateInfoDepthPass{};
		pipelineRenderingCreateInfoDepthPass.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
		pipelineRenderingCreateInfoDepthPass.depthAttachmentFormat = depthFormat;
		pipelineRenderingCreateInfoDepthPass.stencilAttachmentFormat = depthFormat;
		// Use multi view to generate all shadow map cascades in a single pass
		pipelineRenderingCreateInfoDepthPass.viewMask = 0b00001111;

		colorBlendState.attachmentCount = 0;
		depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		// Enable depth clamp (if available)
		rasterizationState.depthClampEnable = deviceFeatures.depthClamp;
		pipelines.depthpass = new Pipeline(device);
		pipelines.depthpass->setCreateInfo(pipelineCI);
		pipelines.depthpass->setVertexInputState(&vertexInputState);
		pipelines.depthpass->setCache(pipelineCache);
		pipelines.depthpass->setLayout(depthPass.pipelineLayout);
		pipelines.depthpass->addShader(getAssetPath() + "shaders/depthpass.vert.spv");
		pipelines.depthpass->addShader(getAssetPath() + "shaders/terrain_depthpass.frag.spv");
		pipelines.depthpass->setpNext(&pipelineRenderingCreateInfoDepthPass);
		pipelines.depthpass->create();
		// Depth pres pass pipeline for glTF models
		pipelines.depthpassTree = new Pipeline(device);
		pipelines.depthpassTree->setCreateInfo(pipelineCI);
		pipelines.depthpassTree->setVertexInputState(&vertexInputStateModelInstanced);
		pipelines.depthpassTree->setCache(pipelineCache);
		pipelines.depthpassTree->setLayout(depthPass.pipelineLayout);
		pipelines.depthpassTree->addShader(getAssetPath() + "shaders/tree_depthpass.vert.spv");
		pipelines.depthpassTree->addShader(getAssetPath() + "shaders/tree_depthpass.frag.spv");
		pipelines.depthpassTree->setpNext(&pipelineRenderingCreateInfoDepthPass);
		pipelines.depthpassTree->create();
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers()
	{		
		frameObjects.resize(getFrameCount());
		for (FrameObjects& frame : frameObjects) {
			createBaseFrameObjects(frame);
			// Uniform buffers
			VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &frame.uniformBuffers.shared, sizeof(uboShared)));
			VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &frame.uniformBuffers.depthPass, sizeof(depthPass.ubo)));
			VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &frame.uniformBuffers.CSM, sizeof(uboCSM)));
			VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &frame.uniformBuffers.params, sizeof(UniformDataParams)));

			// Map persistent
			VK_CHECK_RESULT(frame.uniformBuffers.shared.map());
			VK_CHECK_RESULT(frame.uniformBuffers.depthPass.map());
			VK_CHECK_RESULT(frame.uniformBuffers.CSM.map());
			VK_CHECK_RESULT(frame.uniformBuffers.params.map());

			// Descriptor sets
			frame.uniformBuffers.shared.createDescriptorSet(descriptorPool, descriptorSetLayouts.ubo);
			frame.uniformBuffers.CSM.createDescriptorSet(descriptorPool, descriptorSetLayouts.ubo);
			frame.uniformBuffers.params.createDescriptorSet(descriptorPool, descriptorSetLayouts.ubo);
			frame.uniformBuffers.depthPass.createDescriptorSet(descriptorPool, descriptorSetLayouts.ubo);
		}

		std::vector<DrawBatch*> batches = { &drawBatches.trees, &drawBatches.treeImpostors, &drawBatches.grass };
		for (auto batch : batches) {
			batch->instanceBuffers.resize(getFrameCount());
			for (auto& buffer : batch->instanceBuffers) {
				// @todo: clear required?
			}
		}
	}

	void updateUniformBuffers()
	{
		profiling.uniformUpdate.start();

		const uint32_t currentFrameIndex = getCurrentFrameIndex();

		// Shared UBO
		lightPos = glm::vec4(-48.0f, -80.0f, 46.0f, 0.0f);
		uboShared.lightDir = glm::normalize(-lightPos);
		uboShared.projection = camera.matrices.perspective;
		uboShared.model = camera.matrices.view;
		uboShared.time = sin(glm::radians(timer * 360.0f));
		uboShared.cameraPos = glm::vec4(camera.position, 0.0f);
		memcpy(frameObjects[currentFrameIndex].uniformBuffers.shared.mapped, &uboShared, sizeof(uboShared));

		// Scene parameters
		uniformDataParams.shadows = renderShadows;
		uniformDataParams.fogColor = glm::vec4(heightMapSettings.fogColor[0], heightMapSettings.fogColor[1], heightMapSettings.fogColor[2], 1.0f);
		uniformDataParams.waterColor = glm::vec4(heightMapSettings.waterColor[0], heightMapSettings.waterColor[1], heightMapSettings.waterColor[2], 1.0f);
		uniformDataParams.grassColor = glm::vec4(heightMapSettings.grassColor[0], heightMapSettings.grassColor[1], heightMapSettings.grassColor[2], 1.0f);
		memcpy(frameObjects[currentFrameIndex].uniformBuffers.params.mapped, &uniformDataParams, sizeof(UniformDataParams));

		// Shadow cascades
		for (auto i = 0; i < cascades.size(); i++) {
			depthPass.ubo.cascadeViewProjMat[i] = cascades[i].viewProjMatrix;
		}
		memcpy(frameObjects[currentFrameIndex].uniformBuffers.depthPass.mapped, &depthPass.ubo, sizeof(depthPass.ubo));
		for (auto i = 0; i < cascades.size(); i++) {
			uboCSM.cascadeSplits[i] = cascades[i].splitDepth;
			uboCSM.cascadeViewProjMat[i] = cascades[i].viewProjMatrix;
		}
		uboCSM.inverseViewMat = glm::inverse(camera.matrices.view);
		uboCSM.lightDir = normalize(-lightPos);
		memcpy(frameObjects[currentFrameIndex].uniformBuffers.CSM.mapped, &uboCSM, sizeof(uboCSM));

		profiling.uniformUpdate.stop();
	}

	void prepare()
	{
		VulkanExampleBase::prepare();

		VulkanContext::graphicsQueue = queue;
		VulkanContext::device = vulkanDevice;
		// We try to get a transfer queue for background uploads
		if (vulkanDevice->queueFamilyIndices.graphics != vulkanDevice->queueFamilyIndices.transfer) {
			std::cout << "Using dedicated transfer queue for background uploads\n";
			vkGetDeviceQueue(device, vulkanDevice->queueFamilyIndices.transfer, 0, &VulkanContext::copyQueue);
		} else {
			VulkanContext::copyQueue = queue;
		}

		hasExtMemoryBudget = vulkanDevice->extensionSupported("VK_EXT_memory_budget");

		loadAssets();
		prepareOffscreen();
		prepareCSM();
		setupDescriptorSetLayout();
		setupDescriptorPool();
		prepareUniformBuffers();
		createPipelines();
		setupDescriptorSet();
		loadHeightMapSettings("coastline");

		prepared = true;
	}

	void buildCommandBuffer(CommandBuffer* commandBuffer)
	{
		profiling.cbBuild.start();

		CommandBuffer* cb = commandBuffer;
		cb->begin();

		// CSM
		if (renderShadows) {
			// A single depth stencil attachment info can be used, but they can also be specified separately.
			// When both are specified separately, the only requirement is that the image view is identical.			
			VkRenderingAttachmentInfo depthStencilAttachment{};
			depthStencilAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
			depthStencilAttachment.imageView = depth.view->handle;
			depthStencilAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL_KHR;
			depthStencilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			depthStencilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			depthStencilAttachment.clearValue.depthStencil = { 1.0f,  0 };

			VkRenderingInfo renderingInfo{};
			renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
			renderingInfo.renderArea = { 0, 0, (uint32_t)SHADOWMAP_DIM, (uint32_t)SHADOWMAP_DIM };
			renderingInfo.layerCount = SHADOW_MAP_CASCADE_COUNT;
			renderingInfo.pDepthAttachment = &depthStencilAttachment;
			renderingInfo.pStencilAttachment = &depthStencilAttachment;
			renderingInfo.viewMask = 0b00001111;

			vks::tools::setImageLayout(cb->handle, depth.image->handle, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, { VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, SHADOW_MAP_CASCADE_COUNT });
			vkCmdBeginRendering(cb->handle, &renderingInfo);
			drawCSM(cb);
			vkCmdEndRendering(cb->handle);
			vks::tools::setImageLayout(cb->handle, depth.image->handle, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, { VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, SHADOW_MAP_CASCADE_COUNT });
		}

		// Offscreen

		// New structures are used to define the attachments used in dynamic rendering
		VkRenderingAttachmentInfo colorAttachment{};
		colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
		colorAttachment.imageView = offscreenPass.refraction.view->handle;
		colorAttachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.clearValue.color = { 0.0f,0.0f,0.0f,0.0f };

		// A single depth stencil attachment info can be used, but they can also be specified separately.
		// When both are specified separately, the only requirement is that the image view is identical.			
		VkRenderingAttachmentInfo depthStencilAttachment{};
		depthStencilAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
		depthStencilAttachment.imageView = offscreenPass.depthRefraction.view->handle;
		depthStencilAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL_KHR;
		depthStencilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthStencilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depthStencilAttachment.clearValue.depthStencil = { 1.0f,  0 };

		VkRenderingInfo renderingInfo{};
		renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
		renderingInfo.renderArea = { 0, 0, (uint32_t)offscreenPass.width, (uint32_t)offscreenPass.height };
		renderingInfo.layerCount = 1;
		renderingInfo.colorAttachmentCount = 1;
		renderingInfo.pColorAttachments = &colorAttachment;
		renderingInfo.pDepthAttachment = &depthStencilAttachment;
		renderingInfo.pStencilAttachment = &depthStencilAttachment;

		// Begin dynamic rendering
		vks::tools::insertImageMemoryBarrier(
			cb->handle,
			offscreenPass.reflection.image->handle,
			0,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
		vks::tools::insertImageMemoryBarrier(
			cb->handle,
			offscreenPass.refraction.image->handle,
			0,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
		//vks::tools::insertImageMemoryBarrier(
		//	cb->handle,
		//	offscreenPass.depthRefraction.image->handle,
		//	0,
		//	VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		//	VK_IMAGE_LAYOUT_UNDEFINED,
		//	VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
		//	VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		//	VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		//	VkImageSubresourceRange{ VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1 });
		//vks::tools::insertImageMemoryBarrier(
		//	cb->handle,
		//	offscreenPass.depthReflection.image->handle,
		//	0,
		//	VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		//	VK_IMAGE_LAYOUT_UNDEFINED,
		//	VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
		//	VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		//	VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		//	VkImageSubresourceRange{ VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1 });

		//vks::tools::setImageLayout(cb->handle, offscreenPass.depthRefraction.image->handle, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, { VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1 });


		// Refraction
		vks::tools::setImageLayout(cb->handle, offscreenPass.depthRefraction.image->handle, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, { VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1 });
		vkCmdBeginRendering(cb->handle, &renderingInfo);
		cb->setViewport(0.0f, 0.0f, (float)offscreenPass.width, (float)offscreenPass.height, 0.0f, 1.0f);
		cb->setScissor(0, 0, offscreenPass.width, offscreenPass.height);
		drawScene(cb, SceneDrawType::sceneDrawTypeRefract);
		vkCmdEndRendering(cb->handle);
		vks::tools::setImageLayout(cb->handle, offscreenPass.depthRefraction.image->handle, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, { VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1 });

		// New structures are used to define the attachments used in dynamic rendering
		colorAttachment = {};
		colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
		colorAttachment.imageView = offscreenPass.reflection.view->handle;
		colorAttachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.clearValue.color = { 0.0f,0.0f,0.0f,0.0f };

		// A single depth stencil attachment info can be used, but they can also be specified separately.
		// When both are specified separately, the only requirement is that the image view is identical.			
		depthStencilAttachment = {};
		depthStencilAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
		depthStencilAttachment.imageView = offscreenPass.depthReflection.view->handle;
		depthStencilAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL_KHR;
		depthStencilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthStencilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depthStencilAttachment.clearValue.depthStencil = { 1.0f,  0 };

		renderingInfo = {};
		renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
		renderingInfo.renderArea = { 0, 0, (uint32_t)offscreenPass.width, (uint32_t)offscreenPass.height };
		renderingInfo.layerCount = 1;
		renderingInfo.colorAttachmentCount = 1;
		renderingInfo.pColorAttachments = &colorAttachment;
		renderingInfo.pDepthAttachment = &depthStencilAttachment;
		renderingInfo.pStencilAttachment = &depthStencilAttachment;

		// Reflection
		vks::tools::setImageLayout(cb->handle, offscreenPass.depthReflection.image->handle, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, { VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1 });
		vkCmdBeginRendering(cb->handle, &renderingInfo);
		cb->setViewport(0.0f, 0.0f, (float)offscreenPass.width, (float)offscreenPass.height, 0.0f, 1.0f);
		cb->setScissor(0, 0, offscreenPass.width, offscreenPass.height);
		drawScene(cb, SceneDrawType::sceneDrawTypeReflect);
		vkCmdEndRendering(cb->handle);
		vks::tools::setImageLayout(cb->handle, offscreenPass.depthReflection.image->handle, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, { VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1 });

		vks::tools::setImageLayout(cb->handle, offscreenPass.reflection.image->handle, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
		vks::tools::setImageLayout(cb->handle, offscreenPass.refraction.image->handle, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

		//vks::tools::insertImageMemoryBarrier(
		//	cb->handle,
		//	offscreenPass.reflection.image->handle,
		//	0,
		//	VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		//	VK_IMAGE_LAYOUT_UNDEFINED,
		//	VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		//	VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		//	VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		//	VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
		//vks::tools::insertImageMemoryBarrier(
		//	cb->handle,
		//	offscreenPass.refraction.image->handle,
		//	0,
		//	VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		//	VK_IMAGE_LAYOUT_UNDEFINED,
		//	VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		//	VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		//	VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		//	VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

		// Scene

		// Transition color and depth images for drawing
		vks::tools::insertImageMemoryBarrier(
			cb->handle,
			swapChain.buffers[swapChain.currentImageIndex].image,
			0,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
		vks::tools::insertImageMemoryBarrier(
			cb->handle,
			depthStencil.image,
			0,
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
			VkImageSubresourceRange{ VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1 });

		// New structures are used to define the attachments used in dynamic rendering
		colorAttachment = {};
		colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
		colorAttachment.imageView = multisampleTarget.color.view;
		colorAttachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.clearValue.color = { 0.0f,0.0f,0.0f,0.0f };
		colorAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
		colorAttachment.resolveImageView = swapChain.buffers[swapChain.currentImageIndex].view;
		colorAttachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;

		// A single depth stencil attachment info can be used, but they can also be specified separately.
		// When both are specified separately, the only requirement is that the image view is identical.			
		depthStencilAttachment = {};
		depthStencilAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
		depthStencilAttachment.imageView = multisampleTarget.depth.view;
		depthStencilAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL_KHR;
		depthStencilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthStencilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depthStencilAttachment.clearValue.depthStencil = { 1.0f,  0 };
		depthStencilAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
		depthStencilAttachment.resolveImageView = depthStencil.view;
		depthStencilAttachment.resolveMode = VK_RESOLVE_MODE_NONE;

		renderingInfo = {};
		renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
		renderingInfo.renderArea = { 0, 0, width, height };
		renderingInfo.layerCount = 1;
		renderingInfo.colorAttachmentCount = 1;
		renderingInfo.pColorAttachments = &colorAttachment;
		renderingInfo.pDepthAttachment = &depthStencilAttachment;
		renderingInfo.pStencilAttachment = &depthStencilAttachment;

		// Begin dynamic rendering
		vkCmdBeginRendering(cb->handle, &renderingInfo);

		cb->setViewport(0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f);
		cb->setScissor(0, 0, width, height);
		drawScene(cb, SceneDrawType::sceneDrawTypeDisplay);

		if (debugDisplayReflection) {
			uint32_t val0 = 0;
			cb->bindDescriptorSets(pipelineLayouts.debug, { descriptorSets.debugquad }, 0);
			cb->bindPipeline(pipelines.debug);
			cb->updatePushConstant(pipelineLayouts.debug, 0, &val0);
			cb->draw(6, 1, 0, 0);
		}

		if (debugDisplayRefraction) {
			uint32_t val1 = 1;
			cb->bindDescriptorSets(pipelineLayouts.debug, { descriptorSets.debugquad }, 0);
			cb->bindPipeline(pipelines.debug);
			cb->updatePushConstant(pipelineLayouts.debug, 0, &val1);
			cb->draw(6, 1, 0, 0);
		}

		if (cascadeDebug.enabled) {
			cb->bindDescriptorSets(cascadeDebug.pipelineLayout, { cascadeDebug.descriptorSet }, 0);
			cb->bindPipeline(cascadeDebug.pipeline);
			cb->updatePushConstant(cascadeDebug.pipelineLayout, 0, &cascadeDebug.cascadeIndex);
			cb->draw(6, 1, 0, 0);
		}

		if (UIOverlay.visible) {
			UIOverlay.draw(cb->handle, getCurrentFrameIndex());
		}

		vkCmdEndRendering(cb->handle);

		// Transition color image for presentation
		vks::tools::insertImageMemoryBarrier(
			cb->handle,
			swapChain.buffers[swapChain.currentImageIndex].image,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			0,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

		cb->end();

		profiling.cbBuild.stop();
	}

	void updateMemoryBudgets() {
		if (std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - memoryBudget.lastUpdate).count() > 1000) {
			VkPhysicalDeviceMemoryBudgetPropertiesEXT physicalDeviceMemoryBudgetPropertiesEXT{};
			physicalDeviceMemoryBudgetPropertiesEXT.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
			VkPhysicalDeviceMemoryProperties2 physicalDeviceMemoryProperties2{};
			physicalDeviceMemoryProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
			physicalDeviceMemoryProperties2.pNext = &physicalDeviceMemoryBudgetPropertiesEXT;
			vkGetPhysicalDeviceMemoryProperties2(vulkanDevice->physicalDevice, &physicalDeviceMemoryProperties2);
			memoryBudget.heapCount = physicalDeviceMemoryProperties2.memoryProperties.memoryHeapCount;
			memcpy(memoryBudget.heapBudget, physicalDeviceMemoryBudgetPropertiesEXT.heapBudget, sizeof(VkDeviceSize) * VK_MAX_MEMORY_HEAPS);
			memcpy(memoryBudget.heapUsage, physicalDeviceMemoryBudgetPropertiesEXT.heapUsage, sizeof(VkDeviceSize) * VK_MAX_MEMORY_HEAPS);
		}
	}

	virtual void render()
	{
		FrameObjects currentFrame = frameObjects[getCurrentFrameIndex()];

		VulkanExampleBase::prepareFrame(currentFrame);

		if (stickToTerrain) {
			float h = 0.0f;
			infiniteTerrain.getHeight(camera.position, h);
			camera.position.y = h - 3.0f;
		}

		updateCascades();
		updateUniformBuffers();
		updateDrawBatches();

		updateOverlay(getCurrentFrameIndex());

		buildCommandBuffer(currentFrame.commandBuffer);

		//if (vulkanDevice->queueFamilyIndices.graphics == vulkanDevice->queueFamilyIndices.transfer) {
		//	// If we don't have a dedicated transfer queue, we need to make sure that the main and background threads don't use the (graphics) pipeline simultaneously
		//	std::lock_guard<std::mutex> guard(lock_guard);
		//}
		VulkanExampleBase::submitFrame(currentFrame);

		updateMemoryBudgets();

		updateHeightmap();
	}

	virtual void viewChanged()
	{
		if (!fixFrustum) {
			frustum.update(camera.matrices.perspective * camera.matrices.view);
		}
		// @todo
		infiniteTerrain.viewerPosition = glm::vec2(camera.position.x, camera.position.z);
		infiniteTerrain.updateVisibleChunks(frustum);
	}

	virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay)
	{
		ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiSetCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiSetCond_FirstUseEver);
		ImGui::Begin("Info", nullptr, ImGuiWindowFlags_None);
		ImGui::TextUnformatted("Vulkan infinite terrain");
		ImGui::TextUnformatted("2022 by Sascha Willems");
		ImGui::TextUnformatted(deviceProperties.deviceName);
		ImGui::End();

		ImGui::SetNextWindowPos(ImVec2(15, 15), ImGuiSetCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiSetCond_FirstUseEver);
		ImGui::Begin("Performance", nullptr, ImGuiWindowFlags_None);
		ImGui::Text("%.2f ms/frame (%.1d fps)", (1000.0f / lastFPS), lastFPS);
		if (overlay->header("Memory")) {
			for (int i = 0; i < memoryBudget.heapCount; i++) {
				const float divisor = 1024.0f * 1024.0f;
				ImGui::Text("Heap %i: %.2f / %.2f", i, (float)memoryBudget.heapUsage[i] / divisor, (float)memoryBudget.heapBudget[i] / divisor);
			}
		}
		if (overlay->header("Timings")) {
			ImGui::Text("Draw batch CPU: %.2f ms", profiling.drawBatchCpu.tDelta);
			ImGui::Text("Draw batch upload: %.2f ms", profiling.drawBatchUpload.tDelta);
			ImGui::Text("Draw batch total: %.2f ms", profiling.drawBatchUpdate.tDelta);
			ImGui::Text("Uniform update: %.2f ms", profiling.uniformUpdate.tDelta);
			ImGui::Text("Command buffer building: %.2f ms", profiling.cbBuild.tDelta);
		}
		ImGui::Text("Active threads: %d", activeThreadCount.load());
		ImGui::End();

		ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiSetCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiSetCond_FirstUseEver);
		ImGui::Begin("Debugging", nullptr, ImGuiWindowFlags_None);
		overlay->checkBox("Fix frustum", &fixFrustum);
		overlay->checkBox("Waterplane", &displayWaterPlane);
		overlay->checkBox("Display reflection", &debugDisplayReflection);
		overlay->checkBox("Display refraction", &debugDisplayRefraction);
		overlay->checkBox("Display cascades", &cascadeDebug.enabled);
		if (cascadeDebug.enabled) {
			overlay->sliderInt("Cascade", &cascadeDebug.cascadeIndex, 0, SHADOW_MAP_CASCADE_COUNT - 1);
		}
		if (overlay->sliderFloat("Split lambda", &cascadeSplitLambda, 0.1f, 1.0f)) {
			updateCascades();
			updateUniformBuffers();
		}
		ImGui::End();

		uint32_t currentFrameIndex = getCurrentFrameIndex();

		ImGui::SetNextWindowPos(ImVec2(30, 30), ImGuiSetCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiSetCond_FirstUseEver);
		ImGui::Begin("Terrain", nullptr, ImGuiWindowFlags_None);
		overlay->text("%d chunks in memory", infiniteTerrain.terrainChunks.size());
		overlay->text("%d chunks visible", infiniteTerrain.getVisibleChunkCount());
		//overlay->text("%d trees visible", infiniteTerrain.getVisibleTreeCount());
		overlay->text("%d trees visible (full)", drawBatches.trees.instanceBuffers[currentFrameIndex].elements);
		overlay->text("%d trees visible (impostor)", drawBatches.treeImpostors.instanceBuffers[currentFrameIndex].elements);
		overlay->text("%d grass patches visible", drawBatches.grass.instanceBuffers[currentFrameIndex].elements);
		int currentChunkCoordX = round((float)infiniteTerrain.viewerPosition.x / (float)(heightMapSettings.mapChunkSize - 1));
		int currentChunkCoordY = round((float)infiniteTerrain.viewerPosition.y / (float)(heightMapSettings.mapChunkSize - 1));
		overlay->text("chunk coord x = %d / y =%d", currentChunkCoordX, currentChunkCoordY);
		overlay->text("cam x = %.2f / z =%.2f", camera.position.x, camera.position.z);
		overlay->text("cam yaw = %.2f / pitch =%.2f", camera.yaw, camera.pitch);
		ImGui::End();

		ImGui::SetNextWindowPos(ImVec2(40, 40), ImGuiSetCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiSetCond_FirstUseEver);
		ImGui::Begin("Render options", nullptr, ImGuiWindowFlags_None);
		overlay->checkBox("Shadows", &renderShadows);
		overlay->checkBox("Trees", &renderTrees);
		overlay->checkBox("Grass", &renderGrass);
		overlay->checkBox("Smooth coast line", &uniformDataParams.smoothCoastLine);
		overlay->sliderFloat("Water alpha", &uniformDataParams.waterAlpha, 1.0f, 4096.0f);
		if (overlay->sliderFloat("Chunk draw distance", &heightMapSettings.maxChunkDrawDistance, 0.0f, 1024.0f)) {
			infiniteTerrain.updateViewDistance(heightMapSettings.maxChunkDrawDistance);
		}
		ImGui::End();

		ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiSetCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiSetCond_FirstUseEver);
		ImGui::Begin("Terrain layers", nullptr, ImGuiWindowFlags_None);
		for (uint32_t i = 0; i < TERRAIN_LAYER_COUNT; i++) {
			overlay->sliderFloat2(("##layer_x" + std::to_string(i)).c_str(), uniformDataParams.layers[i].x, uniformDataParams.layers[i].y, 0.0f, 1.0f);
		}
		ImGui::End();

		ImGui::SetNextWindowPos(ImVec2(60, 60), ImGuiSetCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiSetCond_FirstUseEver);
		ImGui::Begin("Terrain settings", nullptr, ImGuiWindowFlags_None);
		overlay->sliderInt("Seed", &heightMapSettings.seed, 0, 128);
		overlay->sliderFloat("Noise scale", &heightMapSettings.noiseScale, 0.0f, 128.0f);
		overlay->sliderFloat("Height scale", &heightMapSettings.heightScale, 0.1f, 64.0f);
		overlay->sliderFloat("Persistence", &heightMapSettings.persistence, 0.0f, 10.0f);
		overlay->sliderFloat("Lacunarity", &heightMapSettings.lacunarity, 0.0f, 10.0f);
		
		ImGui::ColorEdit4("Water color", heightMapSettings.waterColor);
		ImGui::ColorEdit4("Fog color", heightMapSettings.fogColor);
		ImGui::ColorEdit4("Grass color", heightMapSettings.grassColor);

		// @todo
		//overlay->comboBox("Tree type", &heightMapSettings.treeModelIndex, treeModels);
		overlay->sliderInt("Tree density", &heightMapSettings.treeDensity, 1, 64);
		//overlay->sliderInt("Grass density", &heightMapSettings.grassDensity, 1, 512);
		overlay->sliderFloat("Min. tree size", &heightMapSettings.minTreeSize, 0.1f, heightMapSettings.maxTreeSize);
		overlay->sliderFloat("Max. tree size", &heightMapSettings.maxTreeSize, heightMapSettings.minTreeSize, 5.0f);
		overlay->comboBox("Tree type", &selectedTreeType, treeTypes);
		overlay->comboBox("Grass type", &selectedGrassType, grassTypes);
		//overlay->sliderInt("LOD", &heightMapSettings.levelOfDetail, 1, 6);
		if (overlay->button("Update heightmap")) {
			infiniteTerrain.clear();
			updateHeightmap();
		}
		if (overlay->comboBox("Load preset", &presetIndex, fileList.presets)) {
			loadHeightMapSettings(fileList.presets[presetIndex]);
		}
		if (overlay->comboBox("Terrain set", &terrainSetIndex, fileList.terrainSets)) {
			loadTerrainSet(fileList.terrainSets[terrainSetIndex]);
		}
		ImGui::End();

		ImGui::SetNextWindowPos(ImVec2(70, 70), ImGuiSetCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiSetCond_FirstUseEver);
		ImGui::Begin("Grass layer settings", nullptr, ImGuiWindowFlags_None);
		overlay->sliderInt("Patch dimension", &heightMapSettings.grassDim, 1, 512);
		overlay->sliderFloat("Patch scale", &heightMapSettings.grassScale, 0.25f, 2.5f);
		ImGui::End();
	}

	virtual void mouseMoved(double x, double y, bool& handled)
	{
		ImGuiIO& io = ImGui::GetIO();
		handled = io.WantCaptureMouse;
	}

	virtual void keyPressed(uint32_t key)
	{
		if (key == KEY_F) {
			fixFrustum = !fixFrustum;
		}
		if (key == KEY_F2) {
			selectedTreeType++;
			if (selectedTreeType >= treeModelInfo.size()) {
				selectedTreeType = 0;
			}
		}
		if (key == KEY_F3) {
			renderShadows = !renderShadows;
		}
		if (key == KEY_F4) {
			renderGrass = !renderGrass;
		}
		if (key == KEY_F5) {
			renderTerrain = !renderTerrain;
		}
		if (key == KEY_F6) {
			displayWaterPlane = !displayWaterPlane;
		}
		if (key == KEY_F7) {
			stickToTerrain = !stickToTerrain;
		}
		if (key == KEY_F8) {
			std::cout << camera.position.x << " " << camera.position.y << " " << camera.position.z << "\n";
			std::cout << camera.pitch << " " << camera.yaw <<  "\n";
		}
	}

};

VULKAN_EXAMPLE_MAIN()
