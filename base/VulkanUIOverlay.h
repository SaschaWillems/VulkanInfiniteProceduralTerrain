/*
* UI overlay class using ImGui
*
* Copyright (C) 2017-2025 by Sascha Willems - www.saschawillems.de
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

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace vks
{
	class UIOverlay
	{
	public:
		vks::VulkanDevice* device{ nullptr };
		VkQueue queue{ VK_NULL_HANDLE };
		std::string fontFile;

		VkSampleCountFlagBits rasterizationSamples{ VK_SAMPLE_COUNT_1_BIT };
		uint32_t subpass{ 0 };

		struct Buffers {
			vks::Buffer vertexBuffer;
			vks::Buffer indexBuffer;
			int32_t vertexCount{ 0 };
			int32_t indexCount{ 0 };
		};
		std::vector<Buffers> buffers;
		uint32_t maxConcurrentFrames{ 0 };
		uint32_t currentBuffer{ 0 };

		std::vector<VkPipelineShaderStageCreateInfo> shaders;

		VkDescriptorPool descriptorPool{ VK_NULL_HANDLE };
		VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };
		VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
		VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
		VkPipeline pipeline{ VK_NULL_HANDLE };

		VkDeviceMemory fontMemory{ VK_NULL_HANDLE };
		VkImage fontImage{ VK_NULL_HANDLE };
		VkImageView fontView{ VK_NULL_HANDLE };
		VkSampler sampler{ VK_NULL_HANDLE };

		struct PushConstBlock {
			glm::vec2 scale;
			glm::vec2 translate;
		} pushConstBlock;

		bool visible{ true };
		float scale{ 1.0f };

		UIOverlay();
		~UIOverlay();

		void preparePipeline(const VkPipelineCache pipelineCache, const VkFormat colorFormat, const VkFormat depthFormat);
		void prepareResources();

		void update(uint32_t currentBuffer);
		void draw(const VkCommandBuffer commandBuffer, uint32_t currentBuffer);
		void resize(uint32_t width, uint32_t height);

		void freeResources();

		bool header(const char* caption);
		bool checkBox(const char* caption, bool* value);
		bool checkBox(const char* caption, int32_t* value);
		bool checkBox(const char* caption, uint32_t* value);
		bool radioButton(const char* caption, bool value);
		bool inputFloat(const char* caption, float* value, float step, uint32_t precision);
		bool sliderFloat(const char* caption, float* value, float min, float max);
		bool sliderFloat2(const char* caption, float& value0, float& value1, float min, float max);
		bool sliderInt(const char* caption, int32_t* value, int32_t min, int32_t max);
		bool comboBox(const char* caption, int32_t* itemindex, std::vector<std::string> items);
		bool button(const char* caption);
		bool colorPicker(const char* caption, float* color);
		void text(const char* formatstr, ...);
	};
}
