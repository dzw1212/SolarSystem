#pragma once
#include <tuple>
#include <string>
#include <memory>
#include <vector>
#include <set>
#include <unordered_map>
#include <algorithm>
#include <format>
#include <optional>
#include <filesystem>
#include <fstream>

#include "Log.h"

using UCHAR = unsigned char;
using UINT = uint32_t;

#define ASSERT(res, ...)\
{\
	if (!(res))\
	{\
		Log::Error(std::format("Assertion Failed:{}", __VA_ARGS__));\
		__debugbreak();\
	}\
}

#define VULKAN_ASSERT(res, ...)\
{\
	if (res != VK_SUCCESS)\
	{\
		Log::Error(std::format("Vulkan Assertion Failed:{}", __VA_ARGS__));\
		__debugbreak();\
	}\
}