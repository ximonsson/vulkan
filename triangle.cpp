#include <vulkan/vulkan.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <cstring>
#include <array>

#define GLFW_INCLUDE_VULKAM
#include <GLFW/glfw3.h>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const std::vector<const char*> validation_layers =
{
	"VK_LAYER_KHRONOS_validation",
};

#ifndef NDEBUG
const bool enable_validation_layers = false;
#else
const bool enable_validation_layers = true;
#endif

bool check_validation_layer_support ()
{
	uint32_t layer_count;
	vkEnumerateInstanceLayerProperties (&layer_count, nullptr);
	std::vector<VkLayerProperties> available_layers (layer_count);
	vkEnumerateInstanceLayerProperties (&layer_count, available_layers.data ());

	for (const char* layer_name : validation_layers)
	{
		bool layer_found = false;
		for (const auto& layer_properties : available_layers)
		{
			if (strcmp (layer_name, layer_properties.layerName) == 0)
			{
				layer_found = true;
				break;
			}
		}
		if (!layer_found) return false;
	}

	return true;
}

std::vector<const char*> get_required_extensions ()
{
	uint32_t count = 0;
	const char** extensions_ = glfwGetRequiredInstanceExtensions (&count);
	std::vector<const char*> extensions (extensions_, extensions_ + count);
	if (enable_validation_layers)
		extensions.push_back (VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	return extensions;
}

std::vector<VkExtensionProperties> get_supported_extensions ()
{
	uint32_t count;
	vkEnumerateInstanceExtensionProperties (nullptr, &count, nullptr);
	std::vector<VkExtensionProperties> extensions (count);
	vkEnumerateInstanceExtensionProperties (nullptr, &count, extensions.data ());
	return extensions;
}

// apparently `vkCreateDebugUtilsMessengerEXT` is not loaded automatically because it is an extension,
// so we need to create this function as a wrapper that looks it up.
VkResult CreateDebugUtilsMessengerEXT
(
	VkInstance instance,
	const VkDebugUtilsMessengerCreateInfoEXT* info,
	const VkAllocationCallbacks* allocator,
	VkDebugUtilsMessengerEXT* messenger
)
{
	auto fn = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr (instance, "vkCreateDebugUtilsMessengerEXT");
	if (fn != nullptr)
		return fn (instance, info, allocator, messenger);
	else
		return VK_ERROR_EXTENSION_NOT_PRESENT;
}

// same as with `CreateDebugUtilsMessengerEXT`
void DestroyDebugUtilsMessengerEXT
(
	VkInstance instance,
	VkDebugUtilsMessengerEXT messenger,
	const VkAllocationCallbacks* allocator
)
{
	auto fn = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr (instance, "vkDestroyDebugUtilsMessengerEXT");
	if (fn != nullptr)
		fn (instance, messenger, allocator);
}


class HelloTriangleApplication
{
public:
	void run ()
	{
		init_window ();
		init_vulkan ();
		main ();
		cleanup ();
	}

private:
	GLFWwindow* win;
	VkInstance instance;
	VkDebugUtilsMessengerEXT debug_messenger;

	static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback
	(
		VkDebugUtilsMessageSeverityFlagBitsEXT severity,
		VkDebugUtilsMessageTypeFlagsEXT type,
		const VkDebugUtilsMessengerCallbackDataEXT* data,
		void* userd
	)
	{
		std::cerr << "validation layer: " << data->pMessage << std::endl;
		return VK_FALSE;
	}

	void init_window ()
	{
		glfwInit ();
		glfwWindowHint (GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
		win = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
	}

	void populate_debug_messenger_info (VkDebugUtilsMessengerCreateInfoEXT& info)
	{
		info = {};
		info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		info.messageSeverity =
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		info.messageType =
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		info.pfnUserCallback = debug_callback;
		info.pUserData = nullptr;
	}

	void setup_debugger ()
	{
		if (!enable_validation_layers) return;

		VkDebugUtilsMessengerCreateInfoEXT info;
		populate_debug_messenger_info (info);

		if (CreateDebugUtilsMessengerEXT (instance, &info, nullptr, &debug_messenger) != VK_SUCCESS)
			throw std::runtime_error ("failed to set up debug messenger!");
	}

	void create_instance ()
	{
		// application info

		VkApplicationInfo app_info {};
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pApplicationName = "hello triangle";
		app_info.applicationVersion = VK_MAKE_VERSION (1, 0, 0);
		app_info.pEngineName = "No Engine";
		app_info.engineVersion = VK_MAKE_VERSION (1, 0, 0);
		app_info.apiVersion = VK_API_VERSION_1_0;

		// instance creation info

		VkInstanceCreateInfo create_info {};
		create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		create_info.pApplicationInfo = &app_info;

		// extensions
		// get required extensions and then supported ones. make sure all required are supported.
		auto extensions = get_required_extensions ();
		auto vk_extensions = get_supported_extensions ();
		std::cout << extensions.size () << " extensions required:" << std::endl;
		for (auto& ex : extensions)
		{
			std::cout << "\t" << ex << " => ";
			bool extension_found = false;
			for (const auto& extension : vk_extensions)
			{
				if (strcmp (ex, extension.extensionName) == 0)
				{
					extension_found = true;
					break;
				}
			}
			if (!extension_found)
			{
				std::cout << "not found!" << std::endl;
				throw std::runtime_error ("not all extensions supported!");
			}
			else
				std::cout << "found" << std::endl;
		}
		create_info.enabledExtensionCount = extensions.size ();
		create_info.ppEnabledExtensionNames = extensions.data ();

		// validation layers
		VkDebugUtilsMessengerCreateInfoEXT debug_info;
		if (enable_validation_layers)
		{
			if (!check_validation_layer_support ())
				throw std::runtime_error ("validation_layers requested, but not available!");
			create_info.enabledLayerCount = static_cast<uint32_t> (validation_layers.size());
			create_info.ppEnabledLayerNames = validation_layers.data ();

			// debug info for create events
			populate_debug_messenger_info (debug_info);
			create_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debug_info;
		}
		else
		{
			create_info.enabledLayerCount = 0;
			create_info.pNext = nullptr;
		}

		// create the instance
		VkResult res = vkCreateInstance (&create_info, nullptr, &instance);
		if (res != VK_SUCCESS)
		{
			std::cerr << "recieved error code " << res << std::endl;
			if (res == VK_ERROR_LAYER_NOT_PRESENT)
				std::cerr << "layer not present: this should not happen!!" << std::endl;
			throw std::runtime_error ("failed to create instance!");
		}
	}

	void init_vulkan ()
	{
		create_instance ();
		setup_debugger ();
	}

	void main ()
	{
		while (!glfwWindowShouldClose (win))
		{
			glfwPollEvents ();
		}
	}

	void cleanup ()
	{
		if (enable_validation_layers)
			DestroyDebugUtilsMessengerEXT (instance, debug_messenger, nullptr);

		vkDestroyInstance (instance, nullptr);
		glfwDestroyWindow (win);
		glfwTerminate ();
	}
};

int main ()
{
	HelloTriangleApplication app;

	try
	{
		app.run ();
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what () << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
