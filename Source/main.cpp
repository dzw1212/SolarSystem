#include "VulkanRenderer.h"

int main()
{
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