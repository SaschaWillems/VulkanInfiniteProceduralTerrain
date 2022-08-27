/*
 * Vulkan infinite procedurally generated terrain renderer
 *
 * Copyright (C) 2022 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#include "HeightMapSettings.h"

HeightMapSettings heightMapSettings{};

void HeightMapSettings::loadFromFile(const std::string filename)
{
	std::ifstream file;
	file.open(filename);
	assert(file.is_open());
	std::string line;
	std::string key;
	std::string value;
	std::map<std::string, std::string> settings{};
	while (file.good()) {
		getline(file, line);
		std::istringstream ss(line);
		ss >> key >> value;
		settings[key] = value;
	}
	file.close();
	if (settings.find("noiseScale") != settings.end()) {
		noiseScale = std::stof(settings["noiseScale"]);
	}
	if (settings.find("seed") != settings.end()) {
		seed = std::stoi(settings["seed"]);
	}
	if (settings.find("heightScale") != settings.end()) {
		heightScale = std::stof(settings["heightScale"]);
	}
	if (settings.find("persistence") != settings.end()) {
		persistence = std::stof(settings["persistence"]);
	}
	if (settings.find("lacunarity") != settings.end()) {
		lacunarity = std::stof(settings["lacunarity"]);
	}
	if (settings.find("treeDensity") != settings.end()) {
		treeDensity = std::stoi(settings["treeDensity"]);
	}
	if (settings.find("grassDensity") != settings.end()) {
		grassDensity = std::stoi(settings["grassDensity"]);
	}
	if (settings.find("treeModelIndex") != settings.end()) {
		treeModelIndex = std::stoi(settings["treeModelIndex"]);
	}
	if (settings.find("treeType") != settings.end()) {
		treeType = settings["treeType"];
	}
	if (settings.find("minTreeSize") != settings.end()) {
		minTreeSize = std::stof(settings["minTreeSize"]);
	}
	if (settings.find("maxTreeSize") != settings.end()) {
		maxTreeSize = std::stof(settings["maxTreeSize"]);
	}
	if (settings.find("waterColor.r") != settings.end()) {
		waterColor[0] = std::stof(settings["waterColor.r"]) / 255.0f;
	}
	if (settings.find("waterColor.g") != settings.end()) {
		waterColor[1] = std::stof(settings["waterColor.g"]) / 255.0f;
	}
	if (settings.find("waterColor.b") != settings.end()) {
		waterColor[2] = std::stof(settings["waterColor.b"]) / 255.0f;
	}
	if (settings.find("fogColor.r") != settings.end()) {
		fogColor[0] = std::stof(settings["fogColor.r"]) / 255.0f;
	}
	if (settings.find("fogColor.g") != settings.end()) {
		fogColor[1] = std::stof(settings["fogColor.g"]) / 255.0f;
	}
	if (settings.find("fogColor.b") != settings.end()) {
		fogColor[2] = std::stof(settings["fogColor.b"]) / 255.0f;
	}
	if (settings.find("grassColor.r") != settings.end()) {
		grassColor[0] = std::stof(settings["grassColor.r"]) / 255.0f;
	}
	if (settings.find("grassColor.g") != settings.end()) {
		grassColor[1] = std::stof(settings["grassColor.g"]) / 255.0f;
	}
	if (settings.find("grassColor.b") != settings.end()) {
		grassColor[2] = std::stof(settings["grassColor.b"]) / 255.0f;
	}
	if (settings.find("skySphere") != settings.end()) {
		const int idx = std::stoi(settings["skySphere"]);
		skySphere = "skysphere" + std::to_string(idx) + ".ktx";
	}
	if (settings.find("grassDim") != settings.end()) {
		grassDim = std::stoi(settings["grassDim"]);
	}
	if (settings.find("grassScale") != settings.end()) {
		grassScale = std::stof(settings["grassScale"]);
	}
	if (settings.find("terrainSet") != settings.end()) {
		terrainSet = settings["terrainSet"];
	}
	if (settings.find("grassType") != settings.end()) {
		grassType = settings["grassType"];
	}
	for (int i = 0; i < TERRAIN_LAYER_COUNT; i++) {
		const std::string id = "textureLayers[" + std::to_string(i) + "]";
		if (settings.find(id + ".start") != settings.end()) {
			textureLayers[i].x = std::stof(settings[id + ".start"]);
		}
		if (settings.find(id + ".range") != settings.end()) {
			textureLayers[i].y = std::stof(settings[id + ".range"]);
		}
	}
}