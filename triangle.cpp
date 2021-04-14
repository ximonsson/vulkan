#include <vulkan/vulkan.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <cstring>
#include <array>
#include <optional>
#include <set>

#define GLFW_INCLUDE_VULKAM
#include <GLFW/glfw3.h>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const std::vector<const char*> validation_layers =
{
	"VK_LAYER_KHRONOS_validation",
};

const std::vector<const char*> device_ext =
{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
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

struct QueueFamilyIndices
{
	std::optional<uint32_t> gfx_family;
	std::optional<uint32_t> present_family;

	bool is_complete ()
	{
		return gfx_family.has_value () && present_family.has_value ();
	}
};

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> present_modes;
};

/**
 * Main application to display a triangle. Feels a bit overkill...
 */
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
	VkPhysicalDevice physical_device = VK_NULL_HANDLE;
	VkDevice device; // logical device
	VkQueue gfx_queue;
	VkQueue present_queue;
	VkSurfaceKHR surface;
	VkSwapchainKHR swap_chain;
	std::vector<VkImage> swapchain_images;
	VkFormat swapchain_img_fmt;
	VkExtent2D swapchain_ext;
	std::vector<VkImageView> swapchain_image_views;

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

	QueueFamilyIndices find_queue_families (VkPhysicalDevice dev)
	{
		QueueFamilyIndices idx;

		uint32_t count = 0;

		vkGetPhysicalDeviceQueueFamilyProperties (dev, &count, nullptr);
		std::vector<VkQueueFamilyProperties> families (count);
		vkGetPhysicalDeviceQueueFamilyProperties (dev, &count, families.data ());

		int i = 0;
		for (const auto& family : families)
		{
			if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
				idx.gfx_family = i;

			VkBool32 present_support = false;
			vkGetPhysicalDeviceSurfaceSupportKHR (dev, i, surface, &present_support);

			if (present_support)
				idx.present_family = i;

			if (idx.is_complete ())
				break;

			i ++;
		}

		return idx;
	}

	SwapChainSupportDetails query_swap_chain_support (VkPhysicalDevice dev)
	{
		SwapChainSupportDetails details;

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR (dev, surface, &details.capabilities);

		uint32_t format_count;
		vkGetPhysicalDeviceSurfaceFormatsKHR (dev, surface, &format_count, nullptr);
		if (format_count != 0)
		{
			details.formats.resize (format_count);
			vkGetPhysicalDeviceSurfaceFormatsKHR (dev, surface, &format_count, details.formats.data ());
		}

		uint32_t present_mode_count;
		vkGetPhysicalDeviceSurfacePresentModesKHR (dev, surface, &present_mode_count, nullptr);
		if (present_mode_count != 0)
		{
			details.present_modes.resize (present_mode_count);
			vkGetPhysicalDeviceSurfacePresentModesKHR (dev, surface, &present_mode_count, details.present_modes.data ());
		}

		return details;
	}

	VkSurfaceFormatKHR choose_swap_surface_format (const std::vector<VkSurfaceFormatKHR>& formats)
	{
		for (const auto& format : formats)
		{
			if
			(
				format.format == VK_FORMAT_B8G8R8A8_SRGB &&
				format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
			)
			{
				return format;
			}
		}

		return formats[0];
	}

	VkPresentModeKHR choose_swap_present_mode (const std::vector<VkPresentModeKHR>& modes)
	{
		for (const auto& mode : modes)
		{
			if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
				return mode;
		}

		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D choose_swap_extent (const VkSurfaceCapabilitiesKHR& capabilities)
	{
		if (capabilities.currentExtent.width != UINT32_MAX)
			return capabilities.currentExtent;
		else
		{
			int w, h;
			glfwGetFramebufferSize (win, &w, &h);

			VkExtent2D ext =
			{
				static_cast<uint32_t>(w),
				static_cast<uint32_t>(h),
			};

			ext.width = std::max
			(
				capabilities.minImageExtent.width,
				std::min (capabilities.maxImageExtent.width, ext.width)
			);
			ext.height = std::max
			(
				capabilities.minImageExtent.height,
				std::min (capabilities.maxImageExtent.height, ext.height)
			);

			return ext;
		}
	}

	bool check_device_ext_support (VkPhysicalDevice dev)
	{
		uint32_t count;
		vkEnumerateDeviceExtensionProperties (dev, nullptr, &count, nullptr);
		std::vector<VkExtensionProperties> exts (count);
		vkEnumerateDeviceExtensionProperties (dev, nullptr, &count, exts.data ());

		// make sure all the required extensions are covered
		std::set<std::string> required_ext (device_ext.begin(), device_ext.end ());
		for (const auto& ext : exts)
		{
			required_ext.erase (ext.extensionName);
		}
		return required_ext.empty ();
	}

	bool is_device_suitable (VkPhysicalDevice dev)
	{
		// Apparently this was just an example on how to do it.
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties (dev, &props);

		VkPhysicalDeviceFeatures feats;
		vkGetPhysicalDeviceFeatures (dev, &feats);

		//return props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && feats.geometryShader;
		std::cout << "\t" << props.deviceName << std::endl;
		QueueFamilyIndices i = find_queue_families (dev);
		bool ext_support = check_device_ext_support (dev);

		bool swap_chain_adequate = false;
		if (ext_support)
		{
			SwapChainSupportDetails sup = query_swap_chain_support (dev);
			swap_chain_adequate = !sup.formats.empty() && !sup.present_modes.empty();
		}

		return i.is_complete () && ext_support && swap_chain_adequate;
	}

	void pick_physical_device ()
	{
		uint32_t count;
		vkEnumeratePhysicalDevices (instance, &count, nullptr);
		if (count == 0)
			throw std::runtime_error ("failed to find GPUs with Vulkan support!");

		std::vector<VkPhysicalDevice> devices (count);
		vkEnumeratePhysicalDevices (instance, &count, devices.data ());
		std::cout << "searching suitable device:" << std::endl;
		for (const auto& dev : devices)
		{
			if (is_device_suitable (dev))
			{
				physical_device = dev;
				break;
			}
		}

		if (physical_device == VK_NULL_HANDLE)
			throw std::runtime_error ("failed to find a suitable GPU!");
	}

	void create_logical_device ()
	{
		QueueFamilyIndices idx = find_queue_families (physical_device);

		std::vector<VkDeviceQueueCreateInfo> qinfos;
		std::set<uint32_t> families = { idx.gfx_family.value (), idx.present_family.value () };

		float prio = 1.0f;

		for (uint32_t family : families)
		{
			VkDeviceQueueCreateInfo qinfo {};
			qinfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			qinfo.queueFamilyIndex = idx.gfx_family.value ();
			qinfo.queueCount = 1;
			qinfo.pQueuePriorities = &prio;
			qinfos.push_back (qinfo);
		}

		VkPhysicalDeviceFeatures feats {};
		VkDeviceCreateInfo info {};
		info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		info.pQueueCreateInfos = qinfos.data ();
		info.queueCreateInfoCount = qinfos.size ();
		info.pEnabledFeatures = &feats;
		info.enabledExtensionCount = device_ext.size ();
		info.ppEnabledExtensionNames = device_ext.data ();

		if (enable_validation_layers)
		{
			info.enabledLayerCount = validation_layers.size ();
			info.ppEnabledLayerNames = validation_layers.data ();
		}
		else
			info.enabledLayerCount = 0;

		if (vkCreateDevice (physical_device, &info, nullptr, &device) != VK_SUCCESS)
			throw std::runtime_error ("failed to create logical device!");

		vkGetDeviceQueue (device, idx.gfx_family.value (), 0, &gfx_queue);
		vkGetDeviceQueue (device, idx.present_family.value (), 0, &present_queue);
	}

	void create_surface ()
	{
		if (glfwCreateWindowSurface (instance, win, nullptr, &surface) != VK_SUCCESS)
			throw std::runtime_error ("failed to create window surface!");
	}

	void create_swap_chain ()
	{
		SwapChainSupportDetails sup = query_swap_chain_support (physical_device);

		VkSurfaceFormatKHR fmt = choose_swap_surface_format (sup.formats);
		VkPresentModeKHR mode = choose_swap_present_mode (sup.present_modes);
		VkExtent2D ext = choose_swap_extent (sup.capabilities);

		uint32_t imcount = sup.capabilities.minImageCount + 1;
		if (sup.capabilities.maxImageCount > 0 && imcount > sup.capabilities.maxImageCount)
			imcount = sup.capabilities.maxImageCount;

		VkSwapchainCreateInfoKHR info {};
		info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		info.surface = surface;
		info.minImageCount = imcount;
		info.imageFormat = fmt.format;
		info.imageColorSpace = fmt.colorSpace;
		info.imageExtent = ext;
		info.imageArrayLayers = 1;
		info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		QueueFamilyIndices idx = find_queue_families (physical_device);
		uint32_t qfamilyidx[] = { idx.gfx_family.value (), idx.present_family.value () };

		if (idx.gfx_family != idx.present_family)
		{
			info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			info.queueFamilyIndexCount = 2;
			info.pQueueFamilyIndices = qfamilyidx;
		}
		else
		{
			info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			info.queueFamilyIndexCount = 0; // optional
			info.pQueueFamilyIndices = nullptr; // optional
		}

		info.preTransform = sup.capabilities.currentTransform;
		info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		info.presentMode = mode;
		info.clipped = VK_TRUE;
		info.oldSwapchain = VK_NULL_HANDLE;

		if (vkCreateSwapchainKHR (device, &info, nullptr, &swap_chain) != VK_SUCCESS)
			throw std::runtime_error ("failed to create swap chain!");

		// create swapchain image handles

		vkGetSwapchainImagesKHR (device, swap_chain, &imcount, nullptr);
		swapchain_images.resize (imcount);
		vkGetSwapchainImagesKHR (device, swap_chain, &imcount, swapchain_images.data ());

		swapchain_img_fmt = fmt.format;
		swapchain_ext = ext;
	}

	void create_image_views ()
	{
		swapchain_image_views.resize (swapchain_images.size ());

		for (size_t i = 0; i < swapchain_images.size (); i ++)
		{
			VkImageViewCreateInfo info {};
			info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			info.image = swapchain_images[i];
			info.viewType = VK_IMAGE_VIEW_TYPE_2D;
			info.format = swapchain_img_fmt;
			info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			info.subresourceRange.baseMipLevel = 0;
			info.subresourceRange.levelCount = 1;
			info.subresourceRange.baseArrayLayer = 0;
			info.subresourceRange.layerCount = 1;

			if (vkCreateImageView (device, &info, nullptr, &swapchain_image_views[i]) != VK_SUCCESS)
				throw std::runtime_error ("failed to create image views!");
		}
	}

	void init_vulkan ()
	{
		create_instance ();
		setup_debugger ();
		create_surface ();
		pick_physical_device ();
		create_logical_device ();
		create_swap_chain ();
		create_image_views ();
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
		for (auto v : swapchain_image_views)
			vkDestroyImageView (device, v, nullptr);

		vkDestroySwapchainKHR (device, swap_chain, nullptr);
		vkDestroyDevice (device, nullptr);

		if (enable_validation_layers)
			DestroyDebugUtilsMessengerEXT (instance, debug_messenger, nullptr);

		vkDestroySurfaceKHR (instance, surface, nullptr);
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
