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
        "./Source/**.c",
    }

    libdirs --附加库目录
    {
        "D:/VulkanSDK/Lib",
    }

    links --附加依赖项
    {
        "vulkan-1.lib",
        "glfw3.lib",
    }

    includedirs --外部包含目录
    {
        "D:/VulkanSDK/Include",
        "./Submodule/GLFW/include",
        "./Submodule/glm",
        "./Submodule/spdlog/include",
        "./Submodule/ImGui",
        "./Submodule/tinygltf",
        "./Submodule/tinyobjloader",
        "./Submodule/KTX/include",
        "./Submodule/KTX/other_include",
        "./Submodule/KTX/lib",
    }

    filter "configurations:Debug"
        defines "DEBUG"
        symbols "On"

    filter "configurations:Release"
        defines "NDEBUG"
        optimize "On"