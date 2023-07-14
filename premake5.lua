workspace "SolarSystem"
    architecture "x64"
    configurations { "Debug", "Release" }

project "SolarSystem"
    kind "ConsoleApp"
    language "C++"
    cppdialect "c++latest"

    targetdir "bin/%{cfg.buildcfg}"

    files
    {
        "./Source/**.h",
        "./Source/**.cpp",
        "./Submodule/ImGui/*.h",
        "./Submodule/ImGui/*.cpp",
    }

    libdirs --附加库目录
    {
        "D:/VulkanSDK/Lib",
        "./Submodule/ImGui/bin/Debug"
    }

    links --附加依赖项
    {
        "vulkan-1.lib",
        "glfw3.lib",
    }

    includedirs --外部包含目录
    {
        "./Submodule/GLFW/include",
        "./Submodule/glm",
        "./Submodule/spdlog/include",
        "./Submodule/ImGui",
        "./Submodule/tinygltf",
        "./Submodule/tinyobjloader",
        "D:/VulkanSDK/Include",
    }

    filter "configurations:Debug"
        defines "DEBUG"
        symbols "On"

    filter "configurations:Release"
        defines "NDEBUG"
        optimize "On"