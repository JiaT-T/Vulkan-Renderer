#pragma once
#include "VKBase.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

//窗口的指针，全局变量自动初始化为NULL
GLFWwindow* pWindow;
//显示器信息的指针
GLFWmonitor* pMonitor;
//窗口标题
const char* windowTitle = "Vk-Renderer";

bool InitializeWindow(VkExtent2D size, bool fullScreen = false, bool isResizable = true, bool limitFrameRate = true)
{
	// 初始化 GLFW
	if (!glfwInit())
	{
		std::cout << std::format("[ InitializeWindow ] ERROR\nFailed to initialize GLFW!\n");
		return false;
	}
	// 向GLFW说明这里不使用 OpenGL 上下文
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	// 设置窗口是否可调整大小
	glfwWindowHint(GLFW_RESIZABLE, isResizable);

	// 获取 GLFW 要求的 Vulkan 实例扩展列表，并将它们添加到 Vulkan 实例的扩展列表中
	uint32_t extensionCount = 0;
	// const char** glfwGetRequiredInstanceExtensions(uint32_t* ...)
	const char** extensionNames = glfwGetRequiredInstanceExtensions(&extensionCount);

	// 如果获取失败，说明此设备不支持 Vulkan
	// 之后输出错误信息并清理 GLFW
	if (!extensionNames)
	{
		std::cout << std::format("[ InitializeWindow ]\nVulkan is not available on this machine!\n");
		glfwTerminate();
		return false;
	}
	for (size_t i = 0; i < extensionCount; i++)
		vulkan::graphicsBase::Base().AddInstanceExtension(extensionNames[i]);

	// 窗口系统相关的 Vulkan 实例扩展
	vulkan::graphicsBase::Base().AddInstanceExtension("VK_KHR_surface");
	vulkan::graphicsBase::Base().AddInstanceExtension("VK_KHR_win32_surface");
	// 设备相关的 Vulkan 实例扩展
	vulkan::graphicsBase::Base().AddDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

	// 创建窗口
	// 第四个参数用于指定全屏模式的显示器，若为nullptr则使用窗口模式
	// 第五个参数可传入一个其他窗口的指针，用于与其他窗口分享内容
	pWindow = glfwCreateWindow(size.width, size.height, windowTitle, nullptr, nullptr);

	// 拿到当前显示器的指针（用于后续的全屏设置）
	pMonitor = glfwGetPrimaryMonitor();
	// 拿到当前显示器的显示模式（用于确保图像分辨率与窗口分辨率一致）
	// 因为视频模式可能因用户设置而改变，所以在每次切换全屏模式时都要重新获取视频模式，而非使用全局变量存储
	const GLFWvidmode* pMode = glfwGetVideoMode(pMonitor);
	// 根据全屏设置调整窗口
	pWindow = fullScreen 
		? glfwCreateWindow(pMode->width, pMode->height, windowTitle, nullptr, nullptr)
		: glfwCreateWindow(size.width, size.height, windowTitle, nullptr, nullptr);
	// 验证窗口是否创建成功
	if (!pWindow)
	{
		std::cout << std::format("[ InitializeWindow ] ERROR\nFailed to create GLFW window!\n");
		// 清理 GLFW 并返回 false
		glfwTerminate();
		return false;
	};

#ifdef _WIN32
	vulkan::graphicsBase::Base().AddInstanceExtension(VK_KHR_SURFACE_EXTENSION_NAME);
	vulkan::graphicsBase::Base().AddInstanceExtension(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#else
	uint32_t extensionCount = 0;
	const char** extensionNames;
	// 获取 GLFW 要求的 Vulkan 实例扩展列表
	extensionNames = glfwGetRequiredInstanceExtensions(&extensionCount);
	// 如果没有获取到任何扩展，说明此设备不支持 Vulkan
	if (!extensionNames)
	{
		std::cout << std::format("[ InitializeWindow ]\nVulkan is not available on this machine!\n");
		glfwTerminate();
		return false;
	}
	// 填充 Vulkan 实例的扩展列表
	for (size_t i = 0; i < extensionCount; i++)
	{
		graphicsBase::Base().AddInstanceExtension(extensionNames[i]);
	}
#endif
	vulkan::graphicsBase::Base().AddDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	// 在创建 window surface 前创建 Vulkan 实例
	vulkan::graphicsBase::Base().UseLatestApiVersion();
	if (vulkan::graphicsBase::Base().CreateInstance())
		return false;

	// 创建 window surface
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	if (VkResult result = glfwCreateWindowSurface(vulkan::graphicsBase::Base().Instance(), pWindow, nullptr, &surface))
	{
		std::cout << std::format("[ InitializeWindow ] ERROR\nFailed to create a window surface!\nError code: {}\n", string_VkResult(result));
		glfwTerminate();
		return false;
	}
	// 将创建的 Surface 传递给 Vulkan 基类以便后续使用
	vulkan::graphicsBase::Base().Surface(surface);

	// 通过用 || 操作符短路执行来省去几行
	if (// 获取物理设备，并使用列表中的第一个物理设备，这里不考虑以下任意函数失败后更换物理设备的情况
		vulkan::graphicsBase::Base().GetPhysicalDevices() ||
		// 一个true一个false，暂时不需要计算用的队列
		vulkan::graphicsBase::Base().DeterminePhysicalDevice(0, true, false) ||
		// 创建逻辑设备
		vulkan::graphicsBase::Base().CreateDevice())
		return false;

	return true;
}
void TerminateWindow()
{
	glfwDestroyWindow(pWindow);
	glfwTerminate();
}

// 在窗口标题上显示帧率
void TitleFps()
{
	static double time0 = glfwGetTime();
	static double time1;
	static double dt;
	static int dframe = -1;
	static std::stringstream info;
	time1 = glfwGetTime();
	dframe++;
	if ((dt = time1 - time0) >= 1)
	{
		info.precision(1);
		info << windowTitle << "    " << std::fixed << dframe / dt << " FPS";
		glfwSetWindowTitle(pWindow, info.str().c_str());
		info.str(""); //别忘了在设置完窗口标题后清空所用的stringstream
		time0 = time1;
		dframe = 0;
	}
}

// 切换全屏和窗口模式
void MakeWindowFullScreen()
{
	const GLFWvidmode* pMode = glfwGetVideoMode(pMonitor);
	glfwSetWindowMonitor(pWindow, pMonitor, 0, 0, pMode->width, pMode->height, pMode->refreshRate);
}
void MakeWindowWindowed(VkOffset2D position, VkExtent2D size)
{
	const GLFWvidmode* pMode = glfwGetVideoMode(pMonitor);
	glfwSetWindowMonitor(pWindow, nullptr, position.x, position.y, size.width, size.height, pMode->refreshRate);
}

