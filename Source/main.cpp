#include "VulkanRenderer.h"
#include <windows.h>

int main()
{
    SetCurrentDirectory(L"D:\\dev\\SolarSystem");
    VulkanRenderer renderer;

    //renderer.LoadModel("./Assert/Model/teapot.gltf");
    renderer.LoadModel("./Assert/Model/sphere.obj");
    //renderer.LoadModel("./Assert/Model/sphere.gltf");
    //renderer.LoadModel("./Assert/Model/triangle.gltf");
    //renderer.LoadModel("./Assert/Model/vulkanscenemodels.gltf");
    //renderer.LoadModel("./Assert/Model/viking_room.obj");

    renderer.Init();

    renderer.Loop();

    return 0;
}