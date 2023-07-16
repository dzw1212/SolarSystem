#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>

#include "Core.h"

struct PlanetInfo
{
	std::string strDesc;
	std::filesystem::path texturePath;
	UINT uiDiameter;
	UINT uiDistance;
};