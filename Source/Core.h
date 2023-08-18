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
using UINT64 = uint64_t;

#define ASSERT(res, ...)\
{\
	if (!(res))\
	{\
		Log::Error("Assertion Failed:{}", __VA_ARGS__);\
		__debugbreak();\
	}\
}

#define VULKAN_ASSERT(res, ...)\
{\
	if (res != VK_SUCCESS)\
	{\
		Log::Error("Vulkan Assertion Failed:{}, res = {}", __VA_ARGS__, res);\
		__debugbreak();\
	}\
}