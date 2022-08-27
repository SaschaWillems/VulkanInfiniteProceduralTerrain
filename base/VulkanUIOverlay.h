/*
* UI overlay class using ImGui
*
* Copyright (C) 2017 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>
#include <sstream>
#include <iomanip>

#include <vulkan/vulkan.h>
#include "VulkanTools.h"
#include "VulkanDebug.h"
#include "VulkanBuffer.hpp"
#include "VulkanDevice.hpp"

#include "../external/imgui/imgui.h"

#if defined(__ANDROID__)
#include "VulkanAndroid.h"
#endif

namespace vks 
{
	class UIOverlay 
	{
	public:
		vks::VulkanDevice *device;
		VkQueue queue;

		VkSampleCountFlagBits rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		struct FrameObjects {
			vks::Buffer vertexBuffer;
			vks::Buffer indexBuffer;
			int32_t vertexCount = 0;
			int32_t indexCount = 0;
		};
		std::vector<FrameObjects> frameObjects;

		std::vector<VkPipelineShaderStageCreateInfo> shaders;

		VkDescriptorPool descriptorPool;
		VkDescriptorSetLayout descriptorSetLayout;
		VkDescriptorSet descriptorSet;
		VkPipelineLayout pipelineLayout;
		VkPipeline pipeline;

		VkDeviceMemory fontMemory = VK_NULL_HANDLE;
		VkImage fontImage = VK_NULL_HANDLE;
		VkImageView fontView = VK_NULL_HANDLE;
		VkSampler sampler;

		struct PushConstBlock {
			glm::vec2 scale;
			glm::vec2 translate;
		} pushConstBlock;

		bool visible = true;
		bool updated = false;
		float scale = 1.0f;

		UIOverlay();
		~UIOverlay();

		void setFrameCount(uint32_t frameCount);

		void preparePipeline(const VkPipelineCache pipelineCache, VkFormat colorFormat, VkFormat depthFormat);
		void prepareResources();

		bool update();
		void draw(const VkCommandBuffer commandBuffer, uint32_t frameIndex);
		void resize(uint32_t width, uint32_t height);

		void freeResources();

		bool header(const char* caption);
		bool checkBox(const char* caption, bool* value);
		bool checkBox(const char* caption, int32_t* value);
		bool checkBox(const char* caption, uint32_t* value);
		bool inputFloat(const char* caption, float* value, float step, uint32_t precision);
		bool sliderFloat(const char* caption, float* value, float min, float max);
		bool sliderFloat2(const char* caption, float& value0, float& value1, float min, float max);
		bool sliderInt(const char* caption, int32_t* value, int32_t min, int32_t max);
		bool comboBox(const char* caption, int32_t* itemindex, std::vector<std::string> items);
		bool button(const char* caption);
		void text(const char* formatstr, ...);

		// @todo: for new sync

		// Checks if the vertex and/or index buffers need to be recreated
		bool bufferUpdateRequired(uint32_t frameIndex);
		// (Re)allocate vertex and index buffers
		void allocateBuffers(uint32_t frameIndex);
		// Updates the vertex and index buffers with ImGui's current frame data
		void updateBuffers(uint32_t frameIndex);

		// @todo
		void setSampleCount(VkSampleCountFlagBits sampleCount);
	};
}