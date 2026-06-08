#include "GLfwGeneral.hpp"

// Forward declaration to resolve undefined identifier in this translation unit
void TitleFps();

int main()
{   
    if (!InitializeWindow({ 1280, 720 }))
        return -1; 
    while (!glfwWindowShouldClose(pWindow))
    {

        /*渲染过程，待填充*/

        glfwPollEvents();
        TitleFps();
    }
    TerminateWindow();
    return 0;
}
