#include "VulkanRenderer.h"
#include <windows.h>

int main()
{
    SetCurrentDirectory(L"D:\\dev\\SolarSystem");
    VulkanRenderer renderer;

    renderer.Init();

    renderer.Loop();

    return 0;
}