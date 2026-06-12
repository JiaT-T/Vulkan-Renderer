#pragma once
#include "VKStart.h"
#include "helper.h"

namespace vulkan
{
// 指定窗口大小
// 因为是全局变量，所以需要在类外定义，使用constexpr来确保它在编译时就被初始化，并且在程序运行期间保持不变
constexpr VkExtent2D defaultWindowSize = { 1280, 720 };
// 一个单例类
// 用于管理 Vulkan 中那些最基础的对象和行为
class graphicsBase
{
private:
	graphicsBase() = default;
	~graphicsBase()
	{

	}
	graphicsBase operator=(const graphicsBase&) = delete;
	graphicsBase(graphicsBase&&) = delete;

	static graphicsBase singleton;

public :
	static graphicsBase& Base()
	{
		return singleton;
	}

// -----Vulkan 实例-----
private :
	// 实例是 Vulkan 应用程序与 Vulkan 库之间的连接点，代表了应用程序与 Vulkan 的交互环境
	VkInstance instance;
	std::vector<const char*> instanceLayers;
	std::vector<const char*> instanceExtensions;

	//该函数用于向instanceLayers或instanceExtensions容器中添加字符串指针，并确保不重复
	static void AddLayerOrExtension(std::vector<const char*>& container, const char* name)
	{
		for (auto& i : container)
			if (!strcmp(name, i))   //strcmp(...)在字符串匹配时返回0
				return;             //如果层/扩展的名称已在容器中，直接返回
		container.push_back(name);
	}

public :
	// Getters
	VkInstance Instance() const
	{
		return instance;
	}
	const std::vector<const char*>& InstanceLayers() const
	{
		return instanceLayers;
	}
	const std::vector<const char*>& InstanceExtensions() const
	{
		return instanceExtensions;
	}

	//以下函数用于创建Vulkan实例前
	void AddInstanceLayer(const char* layerName)
	{
		AddLayerOrExtension(instanceLayers, layerName);
	}
	void AddInstanceExtension(const char* extensionName)
	{
		AddLayerOrExtension(instanceExtensions, extensionName);
	}
	//该函数用于创建Vulkan实例
	result_t CreateInstance(VkInstanceCreateFlags flags = 0)
	{
		if constexpr (ENABLE_DEBUG_MESSENGER)
		{
			AddInstanceLayer("VK_LAYER_KHRONOS_validation"),
			AddInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}
		// 填充信息结构体并创建 Vulkan 实例
		VkApplicationInfo appInfo =
		{
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pApplicationName = "Hello Triangle",
			.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
			.pEngineName = "No Engine",
			.engineVersion = VK_MAKE_VERSION(1, 0, 0),
			.apiVersion = apiVersion
		};
		VkInstanceCreateInfo createInfo =
		{
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.flags = flags,
			.pApplicationInfo = &appInfo,
			.enabledLayerCount = uint32_t(instanceLayers.size()),
			.ppEnabledLayerNames = instanceLayers.data(),
			.enabledExtensionCount = uint32_t(instanceExtensions.size()),
			.ppEnabledExtensionNames = instanceExtensions.data()
		};

		// 当 Vukan 实例创建失败时，vkCreateInstance(...)会返回一个错误码，可以根据这个错误码来判断失败的原因
		if (VkResult result = vkCreateInstance(&createInfo, nullptr, &instance); result != VK_SUCCESS)
		{
			outStream << std::format("[ graphicsBase ] ERROR\nFailed to create a vulkan instance!\nError code: {}\n", int32_t(result));
			return result;
		}
		//成功创建 Vulkan 实例后，输出Vulkan版本
		outStream << std::format(
			"Vulkan API Version: {}.{}.{}\n",
			VK_API_VERSION_MAJOR(apiVersion),
			VK_API_VERSION_MINOR(apiVersion),
			VK_API_VERSION_PATCH(apiVersion));
		// 如果启用了验证层和调试工具扩展，创建 Debug Messenger
		if constexpr (ENABLE_DEBUG_MESSENGER)
			CreateDebugMessenger();
		return VK_SUCCESS;
	}
	//以下函数用于创建Vulkan实例失败后
	result_t CheckInstanceLayers(std::span<const char*> layersToCheck)
	{
		// 获取可用层的总数
		uint32_t layerCount = 0;
		std::vector<VkLayerProperties> availableLayers;
		// 如果可用层的总数为0，说明当前环境不支持任何 Vulkan 层，输出错误信息并返回
		// VkResult VKAPI_CALL vkEnumerateInstanceLayerProperties(...) 的参数:
		// uint32_t* pPropertyCount				|			若pProperties为nullptr，则将可用层的数量返回到*pPropertyCount，否则由*pPropertyCount指定所需获取的VkLayerProperties的数量
		// VkLayerProperties* pProperties		|			若pProperties非nullptr，则将*pPropertyCount个可用层的VkLayerProperties返回到*pProperties
		if (VkResult result = vkEnumerateInstanceLayerProperties(&layerCount, nullptr); result != VK_SUCCESS)
		{
			outStream << std::format("[ graphicsBase ] ERROR\nFailed to get the count of instance layers!\n");
			return result;
		}

		// 如果可用层的总数不为0，说明当前环境支持至少一个 Vulkan 层，接着获取所有可用层的属性并将它们返回到availableLayers
		if (layerCount)
		{
			availableLayers.resize(layerCount);
			if (VkResult result = vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data()))
			{
				outStream << std::format("[ graphicsBase ] ERROR\nFailed to enumerate instance layer properties!\nError code: {}\n", string_VkResult(result));
				return result;
			}

			for (auto& i : layersToCheck)
			{
				bool found = false;
				for (auto& j : availableLayers)
				{
					// strcmp(...)在字符串匹配时返回0
					// 这里的i是要检查的层的名称，j.layerName是当前可用层的名称，如果两者匹配则说明当前环境支持这个层
					if (!strcmp(i, j.layerName))
					{
						found = true;
						break;
					}
				}
				// 否则，说明当前环境不支持这个层，将其置空以便后续输出错误信息
				if (!found)
					i = nullptr;
			}
		}
		// 如果可用层的总数为0，说明当前环境不支持任何 Vulkan 层，将所有要检查的层都置空以便后续输出错误信息
		else
		{
			for (auto& i : layersToCheck)
			{
				i = nullptr;
			}
		}
		//一切顺利则返回VK_SUCCESS
		return VK_SUCCESS;
	}
	void InstanceLayers(const std::vector<const char*>& layerNames)
	{
		instanceLayers = layerNames;
	}
	// 写法与 CheckInstanceLayers（）基本一样
	result_t CheckInstanceExtensions(std::span<const char*> extensionsToCheck, const char* layerName = nullptr) const 
	{
		uint32_t extensionCount;
		std::vector<VkExtensionProperties> availableExtensions;
		if (VkResult result = vkEnumerateInstanceExtensionProperties(layerName, &extensionCount, nullptr))
		{
			layerName ?
				outStream << std::format("[ graphicsBase ] ERROR\nFailed to get the count of instance extensions!\nLayer name: {}\n", layerName) :
				outStream << std::format("[ graphicsBase ] ERROR\nFailed to get the count of instance extensions!\n");
			return result;
		}
		if (extensionCount)
		{
			availableExtensions.resize(extensionCount);
			if (VkResult result = vkEnumerateInstanceExtensionProperties(layerName, &extensionCount, availableExtensions.data()))
			{
				outStream << std::format("[ graphicsBase ] ERROR\nFailed to enumerate instance extension properties!\nError code: {}\n", string_VkResult(result));
				return result;
			}
			for (auto& i : extensionsToCheck)
			{
				bool found = false;
				for (auto& j : availableExtensions)
				{
					if (!strcmp(i, j.extensionName))
					{
						found = true;
						break;
					}
				}
				if (!found)
					i = nullptr;
			}
		}
		else
		{
			for (auto& i : extensionsToCheck)
			{
				i = nullptr;
			}
		}
		return VK_SUCCESS;
	}
	void InstanceExtensions(const std::vector<const char*>& extensionNames)
	{
		instanceExtensions = extensionNames;
	}

// -----Debug 相关-----
private :
	VkDebugUtilsMessengerEXT debugMessenger;
	result_t CreateDebugMessenger()
	{
		// 返回值为 VkBool32 的回调函数
		// 当 Vulkan 代码触发了验证层（Validation Layers）的警告或错误时，这个函数就会被隐式回调
		// static 是为了确保这个函数只在 graphicsBase 类中存在一份，并且在整个程序运行期间都保持有效
		static PFN_vkDebugUtilsMessengerCallbackEXT DebugUtilsMessengerCallback = [](
			VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
			VkDebugUtilsMessageTypeFlagsEXT messageTypes,
			const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
			void* pUserData)->VkBool32
			{
				// 在控制台输出验证层的警告或错误信息，pMessage成员包含了具体的消息内容
				// 如：重要程度、消息类型、触发消息的 Vulkan API 调用等
				outStream << std::format("{}\n\n", pCallbackData->pMessage);
				return VK_FALSE;
			};

		// 这个结构体用于指定调试消息的过滤条件和回调函数
		// 需要在结构体中写清楚希望监控哪些级别的错误（比如只看错误和警告，不看流水账）、想监控哪些类型的事件、以及当警报触发时，应该通知哪一个回调函数来处理
		VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo =
		{
			// 结构体的类型，必须设置为 VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			// 错误的重要程度，越高的重要程度会触发越多的警报
			.messageSeverity =
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
			// 需要获取的消息类型，越多的消息类型会触发越多的警报
			.messageType =
				VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
			// 产生 debug 信息后，需要调用的回调函数的指针
			.pfnUserCallback = DebugUtilsMessengerCallback
		};

		// PFN_vkCreateDebugUtilsMessengerEXT 是一个扩展函数的函数指针类型，代表了 vkCreateDebugUtilsMessengerEXT(...) 函数的函数指针
		// 因此需要先使用 vkGetInstanceProcAddr 来获取 vkCreateDebugUtilsMessengerEXT(...) 的函数指针，并将其转换为 PFN_vkCreateDebugUtilsMessengerEXT 类型
		PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessenger =
			reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
		if (vkCreateDebugUtilsMessenger)
		{
			VkResult result = vkCreateDebugUtilsMessenger(instance, &debugUtilsMessengerCreateInfo, nullptr, &debugMessenger);
			if (result != VK_SUCCESS)
			{
				outStream << std::format("[ graphicsBase ] ERROR\nFailed to create a debug messenger!\nError code: {}\n", string_VkResult(result));
			}
			return result;
		}
		outStream << std::format("[ graphicsBase ] ERROR\nFailed to get the function pointer of vkCreateDebugUtilsMessengerEXT!\n");
		// VK_RESULT_MAX_ENUM 的值为 INT32_MAX
		// 这个值不会与任何实际的 VkResult 错误码以及 Vulkan 函数的返回值冲突
		return VK_RESULT_MAX_ENUM;
	}

// -----Vulkan 接口(Surface)相关-----
private :
	VkSurfaceKHR surface;

public :
	// Getter
	VkSurfaceKHR Surface() const
	{
		return surface;
	}
	void Surface(VkSurfaceKHR surface)
	{
		this->surface = surface;
	}
	//该函数用于选择物理设备前
	void CreateSurface(VkSurfaceKHR surface)
	{
		if(!this->surface)
			this->surface = surface;
	}

// -----设备与队列相关-----
private :
	// 物理设备
	VkPhysicalDevice physicalDevice;
	VkPhysicalDeviceProperties physicalDeviceProperties;
	VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
	std::vector<VkPhysicalDevice> availablePhysicalDevices;

	// 逻辑设备
	VkDevice device;
	// 队列族索引和队列句柄
	// 有效的索引从0开始，因此使用特殊值VK_QUEUE_FAMILY_IGNORED（为UINT32_MAX）为队列族索引的默认值
	uint32_t queueFamilyIndex_graphics = VK_QUEUE_FAMILY_IGNORED;
	uint32_t queueFamilyIndex_presentation = VK_QUEUE_FAMILY_IGNORED;
	uint32_t queueFamilyIndex_compute = VK_QUEUE_FAMILY_IGNORED;
	VkQueue queue_graphics;
	VkQueue queue_presentation;
	VkQueue queue_compute;

	std::vector<const char*> deviceExtensions;

	//该函数被 DeterminePhysicalDevice(...)调用，用于检查物理设备是否满足所需的队列族类型，并将对应的队列族索引返回到queueFamilyIndices，执行成功时直接将索引写入相应成员变量
	result_t GetQueueFamilyIndices(VkPhysicalDevice physicalDevice, bool enableGraphicsQueue, bool enableComputeQueue)
	{
		// 用 vkGetPhysicalDeviceQueueFamilyProperties(...)取得物理设备所具有的队列族的列表
		// void vkGetPhysicalDeviceQueueFamilyProperties(...) 的参数:
		// VkPhysicalDevice physicalDevice					|		要查询的物理设备的 handle
		// uint32_t* pQueueFamilyPropertyCount				|		若 pQueueFamilyProperties为nullptr，则将物理设备的队列族数量返回到 *pQueueFamilyPropertyCount，否则由 *pQueueFamilyPropertyCount 指定所需获取的 VkQueueFamilyProperties 的数量
		// VkQueueFamilyProperties* pQueueFamilyProperties			|		若 pQueueFamilyProperties非nullptr，则将 *pQueueFamilyPropertyCount 个物理设备的队列族属性返回到 *pQueueFamilyProperties

		// 所有可用的队列族的数量
		uint32_t queueFamilyCount = 0;
		// 第一次调用；动态获取数量，用来决定下一步需要分配多大的内存空间（因此要使用 nullptr 来占位）
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
		if (!queueFamilyCount)
			return VK_RESULT_MAX_ENUM;
		std::vector<VkQueueFamilyProperties> queueFamilyPropertieses(queueFamilyCount);
		// 第二次调用;获取数据（真正填充数据）
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyPropertieses.data());
		// 遍历检测每一个队列族是否满足需求，找到第一个满足需求的队列族后就直接返回成功
		for (uint32_t i = 0; i < queueFamilyCount; i++) 
		{
			// 通过检查队列族的 queueFlags 来判断它是否支持图形、计算等功能
			VkBool32
				supportGraphics = queueFamilyPropertieses[i].queueFlags & VK_QUEUE_GRAPHICS_BIT,
				supportPresentation = false,
				supportCompute = queueFamilyPropertieses[i].queueFlags & VK_QUEUE_COMPUTE_BIT;
			// 此处检测的是呈现支持，只有当存在 surface 时才需要检测
			if (surface)
			{
				if (VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &supportPresentation))
				{
					outStream << std::format("[ graphicsBase ] ERROR\nFailed to determine if the queue family supports presentation!\nError code: {}\n", string_VkResult(result));
					return result;
				}
			}
			// 如果这个队列族同时满足了图形、计算和呈现（如果需要）的支持要求，就直接返回成功，并将这个队列族的索引写入相应成员变量
			if (supportGraphics && supportCompute &&
				(!surface || supportPresentation))
			{ //短路执行，需要呈现的时候才需要判断supportPresentation
				queueFamilyIndex_graphics = queueFamilyIndex_compute = i;
				if (surface)
					queueFamilyIndex_presentation = i;
				return VK_SUCCESS;
			}
		}
		// 如果循环结束都没找到符合条件的，返回错误
		return VK_RESULT_MAX_ENUM;
	}

public :
	// Getters
	VkPhysicalDevice PhysicalDevice() const
	{
		return physicalDevice;
	}
	const VkPhysicalDeviceProperties& PhysicalDeviceProperties() const
	{
		return physicalDeviceProperties;
	}
	const VkPhysicalDeviceMemoryProperties& PhysicalDeviceMemoryProperties() const
	{
		return physicalDeviceMemoryProperties;
	}
	VkPhysicalDevice AvailablePhysicalDevice(uint32_t index) const
	{
		return availablePhysicalDevices[index];
	}
	uint32_t AvailablePhysicalDeviceCount() const
	{
		return uint32_t(availablePhysicalDevices.size());
	}

	VkDevice Device() const
	{
		return device;
	}
	uint32_t QueueFamilyIndex_Graphics() const
	{
		return queueFamilyIndex_graphics;
	}
	uint32_t QueueFamilyIndex_Presentation() const
	{
		return queueFamilyIndex_presentation;
	}
	uint32_t QueueFamilyIndex_Compute() const
	{
		return queueFamilyIndex_compute;
	}
	VkQueue Queue_Graphics() const
	{
		return queue_graphics;
	}
	VkQueue Queue_Presentation() const
	{
		return queue_presentation;
	}
	VkQueue Queue_Compute() const
	{
		return queue_compute;
	}

	const std::vector<const char*>& DeviceExtensions() const
	{
		return deviceExtensions;
	}

	// 用于创建逻辑设备前
	void AddDeviceExtension(const char* extensionName)
	{
		AddLayerOrExtension(deviceExtensions, extensionName);
	}
	// 用于获取物理设备
	result_t GetPhysicalDevices()
	{
		// 物理设备的数量
		uint32_t deviceCount;
		// VKAPI_CALL vkEnumeratePhysicalDevices(...) 的参数：
		// VkInstance instance						|			Vulkan 实例的 handle
		// uint32_t* pPhysicalDeviceCount			|			若 pPhysicalDevices 为 nullptr，则将可用物理设备的数量返回到 *pPhysicalDeviceCount，否则由 *pPhysicalDeviceCount 指定所需获取的 VkPhysicalDevice 的数量
		// VkPhysicalDevice* pPhysicalDevices		|			若 pPhysicalDevices 非 nullptr，则将 *pPhysicalDeviceCount 个可用物理设备的 VkPhysicalDevice 返回到 *pPhysicalDevices
		if (VkResult result = vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr)) {
			outStream << std::format("[ graphicsBase ] ERROR\nFailed to get the count of physical devices!\nError code: {}\n", string_VkResult(result));
			return result;
		}
		// 如果可用物理设备的数量为0，说明当前环境不支持 Vulkan，输出错误信息并终止程序
		if (!deviceCount)
			outStream << std::format("[ graphicsBase ] ERROR\nFailed to find any physical device supports vulkan!\n"),
			// abort(): C++标准库中的一个函数，用于立即终止程序的执行
			abort();
		// 调整 availablePhysicalDevices 的大小以容纳所有可用物理设备，并将它们返回到 availablePhysicalDevices
		availablePhysicalDevices.resize(deviceCount);
		VkResult result = vkEnumeratePhysicalDevices(instance, &deviceCount, availablePhysicalDevices.data());
		if (result)
			outStream << std::format("[ graphicsBase ] ERROR\nFailed to enumerate physical devices!\nError code: {}\n", string_VkResult(result));
		return result;
	}
	// 该函数用于指定所用物理设备并调用GetQueueFamilyIndices(...)取得队列族索引
	// 更明确的说法是：为每个物理设备保存一份所需队列族索引的组合
	result_t DeterminePhysicalDevice(uint32_t deviceIndex = 0, bool enableGraphicsQueue = true, bool enableComputeQueue = true)
	{
		// 定义一个特殊值用于标记一个队列族索引已被找过但未找到
		static constexpr uint32_t notFound = INT32_MAX; // == VK_QUEUE_FAMILY_IGNORED & INT32_MAX
		// queueFamilyIndices 用于为各个物理设备保存一份队列族索引
		static std::vector<uint32_t> queueFamilyIndices(availablePhysicalDevices.size());

		// 分支 A：已知失败，直接拦截（已找过，但没找到）
		// 如果缓存记录显示这个显卡之前已经查过了，并且结果是不符合要求（notFound），那就不用再费劲查一遍了，直接返回错误代码。
		if (queueFamilyIndices[deviceIndex] == notFound)
			return VK_RESULT_MAX_ENUM;

		// 分支 B：首次查询，执行具体查找（未找过）
		// 如果是第一次遇到这个显卡，就调用 GetQueueFamilyIndices 去驱动里真实查询
		if (queueFamilyIndices[deviceIndex] == VK_QUEUE_FAMILY_IGNORED)
		{
			VkResult result = GetQueueFamilyIndices(availablePhysicalDevices[deviceIndex], enableGraphicsQueue, enableComputeQueue);
			//若GetQueueFamilyIndices(...)返回VK_SUCCESS或VK_RESULT_MAX_ENUM（vkGetPhysicalDeviceSurfaceSupportKHR(...)执行成功但没找齐所需队列族），
			//说明对所需队列族索引已有结论，保存结果到queueFamilyIndexCombinations[deviceIndex]中相应变量
			if (result) {
				if (result == VK_RESULT_MAX_ENUM)
					queueFamilyIndices[deviceIndex] = notFound;
				return result;
			}
			else
				queueFamilyIndices[deviceIndex] = queueFamilyIndex_graphics;
		}

		// 分支 C：命中缓存，直接读取（已找过，且成功）
		// 如果前两个 if 都不满足，说明这个显卡之前查过且成功了
		// 代码直接从缓存数组中取出之前存好的索引，迅速赋值给全局/成员变量（graphics、compute、presentation），省去了所有 Vulkan API 的查询开销
		else
		{
			queueFamilyIndex_graphics = queueFamilyIndex_compute = queueFamilyIndices[deviceIndex];
			queueFamilyIndex_presentation = surface ? queueFamilyIndices[deviceIndex] : VK_QUEUE_FAMILY_IGNORED;
		}
		// 结尾：锁定设备
		// 无论是通过分支 B 刚刚查到的，还是通过分支 C 从缓存里直接读取的
		// 只要成功找到了合法的队列族，代码就会把当前选中的物理设备句柄赋值给 physicalDevice，并返回 VK_SUCCESS
		physicalDevice = availablePhysicalDevices[deviceIndex];
		return VK_SUCCESS;
	}
	// 该函数用于创建逻辑设备，并取得队列
	result_t CreateDevice(VkDeviceCreateFlags flags = 0)
	{
		// 队列优先级，范围是0.0到1.0，数值越大优先级越高
		float queuePriority = 1.f;
		// 此处创建了三个 VkDeviceQueueCreateInfo 结构体的数组，分别用于图形、呈现和计算队列的创建信息
		VkDeviceQueueCreateInfo queueCreateInfos[3] =
		{
			{
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.queueCount = 1,
				.pQueuePriorities = &queuePriority
			},
			{
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.queueCount = 1,
				.pQueuePriorities = &queuePriority
			},
			{
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.queueCount = 1,
				.pQueuePriorities = &queuePriority
			} 
		};
		// queueCreateInfoCount 指的是实际要用到的 VkDeviceQueueCreateInfo 结构体的数量，初始值为0
		uint32_t queueCreateInfoCount = 0;
		// 根据之前确定的队列族索引，填充 VkDeviceQueueCreateInfo 结构体的 queueFamilyIndex 成员，并统计实际需要用到的结构体数量
		if (queueFamilyIndex_graphics != VK_QUEUE_FAMILY_IGNORED)
			queueCreateInfos[queueCreateInfoCount++].queueFamilyIndex = queueFamilyIndex_graphics;
		if (queueFamilyIndex_presentation != VK_QUEUE_FAMILY_IGNORED &&
			queueFamilyIndex_presentation != queueFamilyIndex_graphics)
			queueCreateInfos[queueCreateInfoCount++].queueFamilyIndex = queueFamilyIndex_presentation;
		if (queueFamilyIndex_compute != VK_QUEUE_FAMILY_IGNORED &&
			queueFamilyIndex_compute != queueFamilyIndex_graphics &&
			queueFamilyIndex_compute != queueFamilyIndex_presentation)
			queueCreateInfos[queueCreateInfoCount++].queueFamilyIndex = queueFamilyIndex_compute;

		// 填充 VkPhysicalDeviceFeatures 结构体，用于保存物理设备所支持的硬件特性
		VkPhysicalDeviceFeatures physicalDeviceFeatures;
		// vkGetPhysicalDeviceFeatures 的作用：查询特定物理设备（显卡）所支持的硬件特性（Features）
		vkGetPhysicalDeviceFeatures(physicalDevice, &physicalDeviceFeatures);

		// 接下来填充 VkDeviceCreateInfo 结构体，指定创建逻辑设备时的各种参数
		VkDeviceCreateInfo deviceCreateInfo =
		{
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,                   // 结构体的类型，本处必须是 VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO
			.flags = flags,
			.queueCreateInfoCount = queueCreateInfoCount,                    // 实际要用到的 VkDeviceQueueCreateInfo 结构体的数量
			.pQueueCreateInfos = queueCreateInfos,							 // 指向由队列的创建信息构成的数组
			.enabledExtensionCount = uint32_t(deviceExtensions.size()),      // 启用的设备扩展数量
			.ppEnabledExtensionNames = deviceExtensions.data(),              // 指向由所需开启的扩展的名称构成的数组
			.pEnabledFeatures = &physicalDeviceFeatures                      // 指向一个 VkPhysicalDeviceFeatures 结构体，指明需要开启哪些特性
		};
		// 调用 vkCreateDevice(...) 来创建逻辑设备
		if (VkResult result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device))
		{
			outStream << std::format("[ graphicsBase ] ERROR\nFailed to create a vulkan logical device!\nError code: {}\n", string_VkResult(result));
			return result;
		}
		// 调用 vkGetDeviceQueue(...) 来获取之前指定的图形、呈现和计算队列的句柄
		if (queueFamilyIndex_graphics != VK_QUEUE_FAMILY_IGNORED)
			vkGetDeviceQueue(device, queueFamilyIndex_graphics, 0, &queue_graphics);
		if (queueFamilyIndex_presentation != VK_QUEUE_FAMILY_IGNORED)
			vkGetDeviceQueue(device, queueFamilyIndex_presentation, 0, &queue_presentation);
		if (queueFamilyIndex_compute != VK_QUEUE_FAMILY_IGNORED)
			vkGetDeviceQueue(device, queueFamilyIndex_compute, 0, &queue_compute);

		// 获取物理设备属性
		vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
		// 获取物理设备内存属性
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceMemoryProperties);
		//输出所用的物理设备名称
		outStream << std::format("Renderer: {}\n", physicalDeviceProperties.deviceName);
		// 调用回调函数
		ExecuteCallbacks(callbacks_createDevice);
		return VK_SUCCESS;
	}
	// 以下函数用于创建逻辑设备失败后
	result_t CheckDeviceExtensions(std::span<const char*> extensionsToCheck, const char* layerName = nullptr) const
	{
		/*待Ch1-3填充*/
	}
	void DeviceExtensions(const std::vector<const char*>& extensionNames)
	{
		deviceExtensions = extensionNames;
	}
	// 等待设备空闲
	result_t WaitIdle() const
	{
		VkResult result = vkDeviceWaitIdle(device);
		if (result)
			outStream << std::format("[ graphicsBase ] ERROR\nFailed to wait for the device to be idle!\nError code: {}\n", string_VkResult(result));
		return result;
	}

// ----- 设备相关的回调函数列表 -----
private:
	std::vector<void(*)()> callbacks_createDevice;
	std::vector<void(*)()> callbacks_destroyDevice;

public:
	void AddCallback_CreateDevice(void(*function)())
	{
		callbacks_createDevice.push_back(function);
	}
	void AddCallback_DestroyDevice(void(*function)())
	{
		callbacks_destroyDevice.push_back(function);
	}

// -----交换链（Swap Chain）相关-----
private :
	/*
	typedef struct VkSurfaceFormatKHR
	{
		VkFormat           format;        // 像素颜色格式
		VkColorSpaceKHR    colorSpace;    // 色彩空间
	} VkSurfaceFormatKHR;
	*/
	// availableSurfaceFormats 代表“当前物理显卡与你创建的窗口屏幕（Surface）共同支持的、所有可用的『像素颜色格式』与『色彩空间』的组合列表”
	// 在初始化 Vulkan 交换链（Swapchain）时，必须先获取这个列表
	// 然后从中挑选出一个最适合渲染需求的组合（比如最常见的 8 - bit BGRA 格式配 sRGB 色彩空间）
	std::vector< VkSurfaceFormatKHR> availableSurfaceFormats;

	VkSwapchainKHR swapchain;
	// VkImage: Vulkan 中的图像对象，代表一块内存区域，可以用来存储纹理、渲染目标等数据
	std::vector <VkImage> swapchainImages;
	// VkImageView: Vulkan 中的图像视图对象，指定 VkImage 的使用方式
	std::vector <VkImageView> swapchainImageViews;
	//保存交换链的创建信息以便重建交换链
	VkSwapchainCreateInfoKHR swapchainCreateInfo = {};

	//该函数被CreateSwapchain(...)和RecreateSwapchain()调用
	result_t CreateSwapchain_Internal()
	{
		// 创建交换链
		if (VkResult result = vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain))
		{
			outStream << std::format("[ graphicsBase ] ERROR\nFailed to create a swapchain!\nError code: {}\n", string_VkResult(result));
			return result;
		}

		// 创建交换链图像
		uint32_t swapchainImageCount;
		if (VkResult result = vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr))
		{
			outStream << std::format("[ graphicsBase ] ERROR\nFailed to get the count of swapchain images!\nError code: {}\n", string_VkResult(result));
			return result;
		}
		swapchainImages.resize(swapchainImageCount);
		if (VkResult result = vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data()))
		{
			outStream << std::format("[ graphicsBase ] ERROR\nFailed to get swapchain images!\nError code: {}\n", string_VkResult(result));
			return result;
		}

		// 为每个交换链图像创建一个对应的图像视图
		// VkImageView 定义了图像的使用方式
		swapchainImageViews.resize(swapchainImageCount);
		VkImageViewCreateInfo imageViewCreateInfo =
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = swapchainCreateInfo.imageFormat,
			//.components = {}, //四个成员皆为VK_COMPONENT_SWIZZLE_IDENTITY
			.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
		};
		// 遍历每一个交换链图像
		for (size_t i = 0; i < swapchainImageCount; i++)
		{
			imageViewCreateInfo.image = swapchainImages[i];
			if (VkResult result = vkCreateImageView(device, &imageViewCreateInfo, nullptr, &swapchainImageViews[i]))
			{
				outStream << std::format("[ graphicsBase ] ERROR\nFailed to create a swapchain image view!\nError code: {}\n", string_VkResult(result));
				return result;
			}
		}
		return VK_SUCCESS;
	}

public :
	// Getters
	// 返回可用的表面格式列表中指定索引处的像素颜色格式
	const VkFormat& AvailableSurfaceFormat_Format(uint32_t index) const
	{
		return availableSurfaceFormats[index].format;
	}
	// 返回可用的表面格式列表中指定索引处的色彩空间
	const VkColorSpaceKHR& AvailableSurfaceColorSpace(uint32_t index) const
	{
		return availableSurfaceFormats[index].colorSpace;
	}
	// 返回所有可用的表面格式组合数量
	uint32_t AvailableSurfaceFormatCount() const
	{
		return uint32_t(availableSurfaceFormats.size());
	}

	VkSwapchainKHR Swapchain() const
	{
		return swapchain;
	}
	VkImage SwapchainImage(uint32_t index) const
	{
		return swapchainImages[index];
	}
	VkImageView SwapchainImageView(uint32_t index) const
	{
		return swapchainImageViews[index];
	}
	uint32_t SwapchainImageCount() const
	{
		return uint32_t(swapchainImages.size());
	}
	const VkSwapchainCreateInfoKHR& SwapchainCreateInfo() const
	{
		return swapchainCreateInfo;
	}
	
	// 取得 surface 的可用格式到 availableSurfaceFormats
	result_t GetSurfaceFormats()
	{
		// surface 可用的图像格式及色彩空间的数量
		uint32_t surfaceFormatCount;
		if (VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, nullptr))
		{
			outStream << std::format("[ graphicsBase ] ERROR\nFailed to get the count of surface formats!\nError code: {}\n", string_VkResult(result));
			return result;
		}
		// 如果没有可用的表面格式，说明当前环境不支持 Vulkan 的交换链功能，输出错误信息并终止程序
		if (!surfaceFormatCount)
		{
			outStream << std::format("[ graphicsBase ] ERROR\nFailed to find any supported surface format!\n"),
			abort();
		}
		// 为 availableSurfaceFormats 分配足够的空间以容纳所有可用的表面格式组合，并将它们返回到 availableSurfaceFormats
		availableSurfaceFormats.resize(surfaceFormatCount);
		VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, availableSurfaceFormats.data());
		if (result)
			outStream << std::format("[ graphicsBase ] ERROR\nFailed to get surface formats!\nError code: {}\n", string_VkResult(result));
		return result;
	}
	// 验证并指定图像格式和色彩空间
	result_t SetSurfaceFormat(VkSurfaceFormatKHR surfaceFormat)
	{
		// VkSurfaceFormatKHR 的成员：
		// format				|			图像的像素颜色格式
		// colorSpace			|			图像的色彩空间
		bool formatIsAvailable = false;
		// 如果格式未指定，只匹配色彩空间，图像格式有啥就用啥
		if (!surfaceFormat.format)
		{
			// 遍历 availableSurfaceFormats 中的每一个表面格式组合，检查它们的 colorSpace 是否与 surfaceFormat.colorSpace 匹配
			for (auto& i : availableSurfaceFormats)
			{
				// 如果找到一个匹配的色彩空间，
				// 就将这个组合的 format 和 colorSpace 分别赋值给 swapchainCreateInfo.imageFormat 和 swapchainCreateInfo.imageColorSpace，
				// 并将 formatIsAvailable 置为 true，跳出循环
				if (i.colorSpace == surfaceFormat.colorSpace)
				{
					swapchainCreateInfo.imageFormat = i.format;
					swapchainCreateInfo.imageColorSpace = i.colorSpace;
					formatIsAvailable = true;
					break;
				}
			}
		}
		// 否则匹配格式和色彩空间
		else
		{
			for (auto& i : availableSurfaceFormats)
			{
				// 如果找到一个同时匹配的格式和色彩空间，
				if (i.format == surfaceFormat.format &&
					i.colorSpace == surfaceFormat.colorSpace)
				{
					swapchainCreateInfo.imageFormat = i.format;
					swapchainCreateInfo.imageColorSpace = i.colorSpace;
					formatIsAvailable = true;
					break;
				}
			}
		}

		// 如果没有符合的格式，恰好有个语义相符的错误代码
		if (!formatIsAvailable)
			return VK_ERROR_FORMAT_NOT_SUPPORTED;
		// 如果交换链已存在，调用RecreateSwapchain()重建交换链
		if (swapchain)
			return RecreateSwapchain();
		return VK_SUCCESS;
	}
	// 该函数用于创建交换链
	result_t CreateSwapchain(bool limitFrameRate = true, VkSwapchainCreateFlagsKHR flags = 0)
	{
		// 填充创建交换链所需的信息
		// 与 VkSurfaceCapabilitiesKHR 相关的参数有：交换链图像的数量、尺寸、视点数、变换、处理透明通道的方式、图像的用途
		VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
		if (VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities))
		{
			outStream << std::format("[ graphicsBase ] ERROR\nFailed to get physical device surface capabilities!\nError code: {}\n", string_VkResult(result));
			return result;
		}
		// 交换链图像的数量，必须至少为 surfaceCapabilities.minImageCount，并且不能超过 surfaceCapabilities.maxImageCount（如果 maxImageCount 不为0）
		// 一般设置为 surfaceCapabilities.minImageCount + 1，这样可以在渲染时有更多的图像可用，从而提高帧率，但也会占用更多的内存
		swapchainCreateInfo.minImageCount = surfaceCapabilities.minImageCount + (surfaceCapabilities.maxImageCount > surfaceCapabilities.minImageCount);
		// 图像的尺寸，必须在 surfaceCapabilities.minImageExtent 和 surfaceCapabilities.maxImageExtent 之间
		swapchainCreateInfo.imageExtent =
			surfaceCapabilities.currentExtent.width == -1 
			? VkExtent2D
			  {
				 glm::clamp(defaultWindowSize.width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width),
				 glm::clamp(defaultWindowSize.height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height)
			  } 
			: surfaceCapabilities.currentExtent;
		// imageArrayLayers：对于多视点（multiview）或立体显示设备，需要提供一个视点数
		// 对于普通的2D显示设备，该值为1
		swapchainCreateInfo.imageArrayLayers = 1;
		// preTransform：对交换链图像的变换，比如旋转90°、镜像等
		swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
		// 设置交换链处理透明通道的方式，优先使用 VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR（如果支持），否则使用 VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
		// 可选参数：
		//	vk_composite_alpha_opaque_bit_khr：         表示不透明，每个像素的a通道值皆被视作1.f

		//	vk_composite_alpha_pre_multiplied_bit_khr： 表示将颜色值视作预乘透明度（premultiplied alpha）形式

		//	vk_composite_alpha_post_multiplied_bit_khr：表示将颜色值视作后乘透明度形式，或称直接透明度（straight alpha）形式

		//	vk_composite_alpha_inherit_bit_khr：		    表示透明度的处理方式由应用程序的其他部分（vulkan以外的部分）指定
		if (surfaceCapabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
			swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
		else
		{
			for (size_t i = 0; i < 4; i++)
			{
				// 按优先级（不透明 -> 预乘 -> 后乘 -> 继承）自动挑出一个当前显卡支持的、最稳妥的 Alpha 混合模式，用来初始化 Vulkan 交换链
				if (surfaceCapabilities.supportedCompositeAlpha & 1 << i)
				{
					// 一旦找到一个支持的模式，就直接设置到 swapchainCreateInfo.compositeAlpha 中，并跳出循环
					swapchainCreateInfo.compositeAlpha = VkCompositeAlphaFlagBitsKHR(surfaceCapabilities.supportedCompositeAlpha & 1 << i);
					break;
				}
			}

		}
		// 设置图像的用途，至少要包含 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT（表示图像将被用作渲染目标），还可以根据需要添加其他用途（比如 VK_IMAGE_USAGE_TRANSFER_SRC_BIT 表示图像将被用作数据传输的源）
		swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		// 根据 surfaceCapabilities.supportedUsageFlags 的值，自动添加交换链图像的用途标志，以便在后续的渲染过程中能够使用这些用途
		// |= 是按位或赋值运算符，用于将新的用途标志追加到 swapchainCreateInfo.imageUsage 中，而不会覆盖之前已经设置的用途
		if (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
			swapchainCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;                // VK_IMAGE_USAGE_TRANSFER_SRC_BIT 表示图像将被用作数据传输的源
		if (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
			swapchainCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;                // VK_IMAGE_USAGE_TRANSFER_DST_BIT 表示图像将被用作数据传输的目的地
		else
			outStream << std::format("[ graphicsBase ] WARNING\nVK_IMAGE_USAGE_TRANSFER_DST_BIT isn't supported!\n");

		// 获取 surface 格式
		if (availableSurfaceFormats.empty())
		{
			if (VkResult result = GetSurfaceFormats())
				return result;
		}

		// 如果 swapchainCreateInfo.imageFormat 还没有被指定（即没有调用 SetSurfaceFormat(...) 来设置过）
		// 就自动选择一个合适的图像格式和色彩空间的组合来初始化交换链
		if (!swapchainCreateInfo.imageFormat)
		{
			// 用 && 操作符来短路执行
			if (SetSurfaceFormat({ VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR }) &&
				SetSurfaceFormat({ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR }))
			{
				// 如果找不到上述图像格式和色彩空间的组合，那只能有什么用什么，采用availableSurfaceFormats中的第一组
				swapchainCreateInfo.imageFormat = availableSurfaceFormats[0].format;
				swapchainCreateInfo.imageColorSpace = availableSurfaceFormats[0].colorSpace;
				outStream << std::format("[ graphicsBase ] WARNING\nFailed to select a four-component UNORM surface format!\n");
			}
		}

		// 指定呈现模式
		
		// VK_PRESENT_MODE_IMMEDIATE_KHR 表示立即模式，该模式下不限制帧率且帧率在所有模式中是最高的。该模式不等待垂直同步信号，一旦图片渲染完，用于呈现的图像就会被立刻替换掉，这可能导致画面撕裂

		// VK_PRESENT_MODE_FIFO_KHR 表示先入先出模式，该模式限制帧率与屏幕刷新率一致，这种模式是必定支持的。在该模式下，图像被推送进一个用于待呈现图像的队列，然后等待垂直同步信号，按顺序被推出队列并输出到屏幕，因此叫先入先出

		// VK_PRESENT_MODE_FIFO_RELAXED_KHR 同 VK_PRESENT_MODE_FIFO_KHR 的差别在于，若屏幕上图像的停留时间长于一个刷新间隔，呈现引擎可能在下一个垂直同步信号到来前便试图将呈现队列中的图像输出到屏幕，该模式相比 VK_PRESENT_MODE_FIFO_KHR 更不容易引起阻塞或迟滞，但在帧率较低时可能会导致画面撕裂

		// VK_PRESENT_MODE_MAILBOX_KHR 是一种类似于三重缓冲的模式。它的待呈现图像队列中只容纳一个元素，在等待垂直同步信号期间若有新的图像入队，那么旧的图像会直接出队而不被输出到屏幕（即出队不需要等待垂直同步信号，因此不限制帧率），出现在屏幕上的总会是最新的图像
		uint32_t surfacePresentModeCount;
		if (VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &surfacePresentModeCount, nullptr))
		{
			outStream << std::format("[ graphicsBase ] ERROR\nFailed to get the count of surface present modes!\nError code: {}\n", string_VkResult(result));
			return result;
		}
		if (!surfacePresentModeCount)
		{
			outStream << std::format("[ graphicsBase ] ERROR\nFailed to find any surface present mode!\n");
			abort();
		}
		std::vector<VkPresentModeKHR> surfacePresentModes(surfacePresentModeCount);
		if (VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &surfacePresentModeCount, surfacePresentModes.data()))
		{
			outStream << std::format("[ graphicsBase ] ERROR\nFailed to get surface present modes!\nError code: {}\n", string_VkResult(result));
			return result;
		}
		swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
		// 在不需要限制帧率时应当选择 VK_PRESENT_MODE_MAILBOX_KHR
		if (!limitFrameRate)
		{
			for (size_t i = 0; i < surfacePresentModeCount; i++)
			{
				if (surfacePresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
				{
					swapchainCreateInfo.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
					break;
				}
			}
		}
		// 填充 swapchainCreateInfo 中其他必要的成员
		swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapchainCreateInfo.flags = flags;
		swapchainCreateInfo.surface = surface;
		swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchainCreateInfo.clipped = VK_TRUE;

		if (VkResult result = CreateSwapchain_Internal())
			return result;
		// 调用回调函数
		ExecuteCallbacks(callbacks_createSwapchain);
		return VK_SUCCESS;
	}
	// 该函数用于重建交换链
	result_t RecreateSwapchain()
	{
		// 创立窗口大小改变时的情况
		VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
		if (VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities))
		{
			outStream << std::format("[ graphicsBase ] ERROR\nFailed to get physical device surface capabilities!\nError code: {}\n", string_VkResult(result));
			return result;
		}
		if (surfaceCapabilities.currentExtent.width == 0 ||
			surfaceCapabilities.currentExtent.height == 0)
			return VK_SUBOPTIMAL_KHR;
		swapchainCreateInfo.imageExtent = surfaceCapabilities.currentExtent;
		// 把旧交换链的 handle 传给 swapchainCreateInfo.oldSwapchain
		// 从而实现资源复用，提高重建交换链的效率
		swapchainCreateInfo.oldSwapchain = swapchain;

		VkResult result = vkQueueWaitIdle(queue_graphics);
		// 确保程序没有在使用旧交换链的资源
		// 仅在等待图形队列成功，且图形与呈现所用队列不同时等待呈现队列
		if (!result && queue_graphics != queue_presentation)
			result = vkQueueWaitIdle(queue_presentation);
		if (result)
		{
			outStream << std::format("[ graphicsBase ] ERROR\nFailed to wait for the queue to be idle!\nError code: {}\n", string_VkResult(result));
			return result;
		}
		// 销毁旧交换链相关对象
		ExecuteCallbacks(callbacks_destroySwapchain);

		for (auto& i : swapchainImageViews)
		{
			// 使用 vkDestroyImageView(...) 销毁旧交换链图像视图
			if (i) vkDestroyImageView(device, i, nullptr);
		}
		swapchainImageViews.clear();
		// 调用 CreateSwapchain_Internal(...) 来重建旧交换链
		if (result = CreateSwapchain_Internal())
			return result;
		// 调用回调函数
		ExecuteCallbacks(callbacks_createSwapchain);
		return VK_SUCCESS;
	}

// ----- 交换链相关的回调函数列表 -----
private:
	std::vector<void(*)()> callbacks_createSwapchain;
	std::vector<void(*)()> callbacks_destroySwapchain;

	// 用于执行回调函数列表中的所有函数
	static void ExecuteCallbacks(std::vector<void(*)()> callbacks)
	{
		for (size_t size = callbacks.size(), i = 0; i < size; i++)
		{
			callbacks[i]();
		}
	}

public:
	void AddCallback_CreateSwapchain(void(*function)())
	{
		callbacks_createSwapchain.push_back(function);
	}
	void AddCallback_DestroySwapchain(void(*function)())
	{
		callbacks_destroySwapchain.push_back(function);
	}

// -----使用 Vulkan 的最新版本-----
private:
	uint32_t apiVersion = VK_API_VERSION_1_0;

public:
	//Getter
	uint32_t ApiVersion() const 
	{
		return apiVersion;
	}
	result_t UseLatestApiVersion()
	{
		// PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(...) 的参数说明：
		// VkInstance instance：      |       Vulkan实例的handle，若要取得的函数指针不依赖Vulkan实例，可为VK_NULL_HANDLE
		// const char* pName：        |       所要取得的函数的名称
		if (vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"))
		{
			// vkEnumerateInstanceVersion(...)用于取得当前运行环境所支持的最新Vulkan版本
			return vkEnumerateInstanceVersion(&apiVersion);
		}
		return VK_SUCCESS;   //如果vkEnumerateInstanceVersion(...)不存在，说明当前环境只支持Vulkan 1.0，直接返回成功
	}
};

inline graphicsBase graphicsBase::singleton;
}
