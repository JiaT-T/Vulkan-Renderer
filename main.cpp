#include "GLfwGeneral.hpp"
using namespace vulkan;

// Forward declaration to resolve undefined identifier in this translation unit
void TitleFps();

int main()
{   
    if (!InitializeWindow({ 1280, 720 }))
        return -1; 

    // 创建栅栏与信号量
    fence fence(VK_FENCE_CREATE_SIGNALED_BIT);
    semaphore semaphore_imageIsAvailable;
    semaphore semaphore_renderingIsOver;

    while (!glfwWindowShouldClose(pWindow))
    {

        /*渲染过程，待填充*/

        glfwPollEvents();
        TitleFps();
    }
    TerminateWindow();
    return 0;
}
