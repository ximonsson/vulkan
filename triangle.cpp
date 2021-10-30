#include <vulkan/vulkan.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <cstring>
#include <array>
#include <optional>
#include <set>
#include <fstream>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <chrono>

#define GLFW_INCLUDE_VULKAM
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>


struct UniformBufferObject
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

struct Vertex
{
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 tex_coord;

	static VkVertexInputBindingDescription get_binding_desc ()
	{
		VkVertexInputBindingDescription desc {};
		desc.binding = 0;
		desc.stride = sizeof (Vertex);
		desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return desc;
	}

	static std::array<VkVertexInputAttributeDescription, 3> get_attrib_desc ()
	{
		std::array<VkVertexInputAttributeDescription, 3> desc;

		desc[0].binding = 0;
		desc[0].location = 0;
		desc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		desc[0].offset = offsetof (Vertex, pos);

		desc[1].binding = 0;
		desc[1].location = 1;
		desc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		desc[1].offset = offsetof (Vertex, color);

		desc[2].binding = 0;
		desc[2].location = 2;
		desc[2].format = VK_FORMAT_R32G32_SFLOAT;
		desc[2].offset = offsetof (Vertex, tex_coord);

		return desc;
	}
};

const std::vector<Vertex> vertices =
{
	{ { -0.5f, -0.5f, 0.0f }, { 1.0f,  0.0f, 0.0f }, { 1.0f, .0f } },
	{ { 0.5f, -0.5f, 0.0f }, { 0.0f,  1.0f, 0.0f }, { .0f, .0f } },
	{ { 0.5f, 0.5f, 0.0f }, { 0.0f,  0.0f, 1.0f }, { .0f, 1.0f } },
	{ { -0.5f, 0.5f, 0.0f }, { 1.0f,  1.0f, 1.0f }, { 1.0f, 1.0f } },

	{ { -0.5f, -0.5f, -0.5f }, { 1.0f,  0.0f, 0.0f }, { 1.0f, .0f } },
	{ { 0.5f, -0.5f, -0.5f }, { 0.0f,  1.0f, 0.0f }, { .0f, .0f } },
	{ { 0.5f, 0.5f, -0.5f }, { 0.0f,  0.0f, 1.0f }, { .0f, 1.0f } },
	{ { -0.5f, 0.5f, -0.5f }, { 1.0f,  1.0f, 1.0f }, { 1.0f, 1.0f } }
};

const std::vector<uint16_t> indices =
{
	0, 1, 2, 2, 3, 0,
	4, 5, 6, 6, 7, 4
};

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;
const int MAX_FRAMES_IN_FLIGHT = 2;

static std::vector<char> read_file (const std::string& filename)
{
	std::ifstream file (filename, std::ios::ate | std::ios::binary);
	if (!file.is_open ())
		throw std::runtime_error ("failed to open file!");

	size_t fsize = (size_t) file.tellg ();
	std::vector<char> buffer (fsize);
	file.seekg (0);
	file.read (buffer.data (), fsize);
	file.close ();

	return buffer;
}

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
	VkRenderPass render_pass;

	/* swapchain */
	VkSwapchainKHR swap_chain;
	std::vector<VkImage> swapchain_images;
	std::vector<VkFramebuffer> swapchain_framebufs;
	VkFormat swapchain_img_fmt;
	VkExtent2D swapchain_ext;
	std::vector<VkImageView> swapchain_image_views;

	/* pipeline */
	VkPipelineLayout pipeline_layout;
	VkPipeline pipeline;

	/* commands */
	VkCommandPool cmdpool;
	std::vector<VkCommandBuffer> cmdbuffers;
	std::vector<VkSemaphore> img_available;
	std::vector<VkSemaphore> render_finished;
	std::vector<VkFence> in_flight_fences;
	std::vector<VkFence> images_in_flight;

	/* vertex buffer */
	VkBuffer vx_buf;
	VkBuffer staging_buf;
	VkDeviceMemory vx_buf_mem;

	/* index buffer */
	VkBuffer idx_buf;
	VkDeviceMemory idx_buf_mem;

	/* uniform buffer */
	std::vector<VkBuffer> unif_buf;
	std::vector<VkDeviceMemory> unif_buf_mem;

	/* descriptor */
	VkDescriptorPool descriptor_pool;
	std::vector<VkDescriptorSet> descriptor_sets;
	VkDescriptorSetLayout descriptor_set_layout;

	/* texture */
	VkImage tex_img;
	VkDeviceMemory tex_img_mem;
	VkImageView tex_img_view;
	VkSampler tex_sampler;

	/* depth buffer */
	VkImage depth_img;
	VkDeviceMemory depth_img_mem;
	VkImageView depth_img_view;

	/* state variables */
	size_t current_frame = 0;
	bool framebuf_resized = false;

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

	static void framebuf_resize_cb (GLFWwindow *win, int w, int h)
	{
		auto app = reinterpret_cast<HelloTriangleApplication*> (glfwGetWindowUserPointer (win));
		app->framebuf_resized = true;
	}

	void init_window ()
	{
		glfwInit ();
		glfwWindowHint (GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
		win = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
		glfwSetWindowUserPointer (win, this);
		glfwSetFramebufferSizeCallback (win, framebuf_resize_cb);
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

		return i.is_complete () && ext_support && swap_chain_adequate && feats.samplerAnisotropy;
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
		feats.samplerAnisotropy = VK_TRUE;

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

	void create_swapchain ()
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
		swapchain_images.resize (imcount); vkGetSwapchainImagesKHR (device, swap_chain, &imcount, swapchain_images.data ());

		swapchain_img_fmt = fmt.format;
		swapchain_ext = ext;
	}

	void cleanup_swapchain ()
	{
		// destroy depth image resources
		vkDestroyImageView (device, depth_img_view, nullptr);
		vkDestroyImage (device, depth_img, nullptr);
		vkFreeMemory (device, depth_img_mem, nullptr);

		for (auto buf : swapchain_framebufs)
			vkDestroyFramebuffer (device, buf, nullptr);

		vkFreeCommandBuffers (device, cmdpool, static_cast<uint32_t> (cmdbuffers.size ()), cmdbuffers.data ());

		vkDestroyPipeline (device, pipeline, nullptr);
		vkDestroyPipelineLayout (device, pipeline_layout, nullptr);
		vkDestroyRenderPass (device, render_pass, nullptr);

		for (auto v : swapchain_image_views)
			vkDestroyImageView (device, v, nullptr);

		vkDestroySwapchainKHR (device, swap_chain, nullptr);

		for (size_t i = 0; i < swapchain_images.size (); i ++)
		{
			vkDestroyBuffer (device, unif_buf[i], nullptr);
			vkFreeMemory (device, unif_buf_mem[i], nullptr);
		}

		vkDestroyDescriptorPool (device, descriptor_pool, nullptr);
	}

	VkImageView create_img_view (VkImage img, VkFormat fmt, VkImageAspectFlags aspect_flags)
	{
		VkImageViewCreateInfo info {};
		info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		info.image = img;
		info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		info.format = fmt;
		info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		info.subresourceRange.baseMipLevel = 0;
		info.subresourceRange.levelCount = 1;
		info.subresourceRange.baseArrayLayer = 0;
		info.subresourceRange.layerCount = 1;
		info.subresourceRange.aspectMask = aspect_flags;

		VkImageView view;
		if (vkCreateImageView (device, &info, nullptr, &view) != VK_SUCCESS)
		{
			throw std::runtime_error ("failed to create image views!");
		}

		return view;
	}

	void create_img_views ()
	{
		swapchain_image_views.resize (swapchain_images.size ());

		for (size_t i = 0; i < swapchain_images.size (); i ++)
		{
			swapchain_image_views[i] = create_img_view
			(
				swapchain_images[i],
				swapchain_img_fmt,
				VK_IMAGE_ASPECT_COLOR_BIT
			);
		}
	}

	void recreate_swapchain ()
	{
		// wait until the window is a size other than 0

		int w = 0, h = 0;
		glfwGetFramebufferSize (win, &w, &h);
		while (w == 0 || h == 0)
		{
			glfwGetFramebufferSize (win, &w, &h);
			glfwWaitEvents ();
		}

		vkDeviceWaitIdle (device);

		cleanup_swapchain ();

		create_swapchain ();
		create_img_views ();
		create_render_pass ();
		create_gfx_pipeline ();
		create_depth_buffer ();
		create_framebuffers ();
		create_uniform_buf ();
		create_descriptor_pool ();
		create_descriptor_sets ();
		create_cmd_buffers ();
	}

	uint32_t find_mem_type (uint32_t type_filter, VkMemoryPropertyFlags props)
	{
		VkPhysicalDeviceMemoryProperties mem_props;
		vkGetPhysicalDeviceMemoryProperties (physical_device, &mem_props);

		for (uint32_t i = 0; i < mem_props.memoryTypeCount; i ++)
		{
			if
			(
				(type_filter & (1 << i)) &&
				(mem_props.memoryTypes[i].propertyFlags & props) == props
			)
				return i;
		}

		throw std::runtime_error ("failed to find suitable memory type!");
	}

	void create_buffer
	(
		VkDeviceSize size,
		VkBufferUsageFlags usage,
		VkMemoryPropertyFlags props,
		VkBuffer &buf,
		VkDeviceMemory &mem
	)
	{
		VkBufferCreateInfo info {};
		info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		info.size = size;
		info.usage = usage;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateBuffer (device, &info, nullptr, &buf) != VK_SUCCESS)
			throw std::runtime_error ("failed to create vertex buffer!");

		VkMemoryRequirements mem_req;
		vkGetBufferMemoryRequirements (device, buf, &mem_req);

		VkMemoryAllocateInfo alloc_info {};
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize = mem_req.size;
		alloc_info.memoryTypeIndex = find_mem_type
		(
			mem_req.memoryTypeBits,
			props
		);

		if (vkAllocateMemory (device, &alloc_info, nullptr, &mem) != VK_SUCCESS)
			throw std::runtime_error ("failed to allocate vertex buffer memory!");

		vkBindBufferMemory (device, buf, mem, 0);
	}

	void create_vx_buf ()
	{
		VkDeviceSize size = sizeof (vertices[0]) * vertices.size ();

		VkBuffer staging_buf;
		VkDeviceMemory staging_buf_mem;
		create_buffer
		(
			size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			staging_buf,
			staging_buf_mem
		);

		void *data;
		vkMapMemory (device, staging_buf_mem, 0, size, 0, &data);
		memcpy (data, vertices.data (), (size_t) size);
		vkUnmapMemory (device, staging_buf_mem);

		create_buffer
		(
			size,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			vx_buf,
			vx_buf_mem
		);

		copy_buf (staging_buf, vx_buf, size);

		vkDestroyBuffer (device, staging_buf, nullptr);
		vkFreeMemory (device, staging_buf_mem, nullptr);
	}

	void create_idx_buf ()
	{
		VkDeviceSize size = sizeof (indices[0]) * indices.size ();

		VkBuffer staging_buf;
		VkDeviceMemory staging_buf_mem;
		create_buffer
		(
			size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			staging_buf,
			staging_buf_mem
		);

		void *data;
		vkMapMemory (device, staging_buf_mem, 0, size, 0, &data);
		memcpy (data, indices.data (), (size_t) size);
		vkUnmapMemory (device, staging_buf_mem);

		create_buffer
		(
			size,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			idx_buf,
			idx_buf_mem
		);

		copy_buf (staging_buf, idx_buf, size);

		vkDestroyBuffer (device, staging_buf, nullptr);
		vkFreeMemory (device, staging_buf_mem, nullptr);
	}

	VkCommandBuffer begin_single_time_cmds ()
	{
		VkCommandBufferAllocateInfo alloc_info {};
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandPool = cmdpool;
		alloc_info.commandBufferCount = 1;

		VkCommandBuffer cmd_buf;
		vkAllocateCommandBuffers (device, &alloc_info, &cmd_buf);

		VkCommandBufferBeginInfo begin_info {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer (cmd_buf, &begin_info);

		return cmd_buf;
	}

	void end_single_time_cmds (VkCommandBuffer cmdbuf)
	{
		vkEndCommandBuffer (cmdbuf);

		VkSubmitInfo submit_info {};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &cmdbuf;

		vkQueueSubmit (gfx_queue, 1, &submit_info, VK_NULL_HANDLE);
		vkQueueWaitIdle (gfx_queue);

		vkFreeCommandBuffers (device, cmdpool, 1, &cmdbuf);
	}

	void copy_buf (VkBuffer src, VkBuffer dst, VkDeviceSize size)
	{
		VkCommandBuffer cmdbuf = begin_single_time_cmds ();

		VkBufferCopy copy {};
		copy.srcOffset = 0; // optional
		copy.dstOffset = 0; // optional
		copy.size = size;
		vkCmdCopyBuffer (cmdbuf, src, dst, 1, &copy);

		end_single_time_cmds (cmdbuf);
	}

	VkShaderModule create_shader_module (const std::vector<char>& code)
	{
		VkShaderModuleCreateInfo info {};
		info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		info.codeSize = code.size ();
		info.pCode = reinterpret_cast<const uint32_t*> (code.data ());

		VkShaderModule mod;
		if (vkCreateShaderModule (device, &info, nullptr, &mod) != VK_SUCCESS)
			throw std::runtime_error ("failed to create shader module!");

		return mod;
	}

	void create_gfx_pipeline ()
	{
		auto vert_shader_code = read_file ("shaders/vert.spv");
		auto frag_shader_code = read_file ("shaders/frag.spv");

		VkShaderModule vert_shader_mod = create_shader_module (vert_shader_code);
		VkShaderModule frag_shader_mod = create_shader_module (frag_shader_code);

		VkPipelineShaderStageCreateInfo vxinfo {};
		vxinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vxinfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vxinfo.module = vert_shader_mod;
		vxinfo.pName = "main";

		VkPipelineShaderStageCreateInfo fginfo {};
		fginfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fginfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fginfo.module = frag_shader_mod;
		fginfo.pName = "main";

		VkPipelineShaderStageCreateInfo stages[] = { vxinfo, fginfo };

		auto binding_desc = Vertex::get_binding_desc ();
		auto attrib_desc = Vertex::get_attrib_desc ();

		VkPipelineVertexInputStateCreateInfo vxinput {};
		vxinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vxinput.vertexBindingDescriptionCount = 1;
		vxinput.pVertexBindingDescriptions = &binding_desc; // optional
		vxinput.vertexAttributeDescriptionCount = static_cast<uint32_t> (attrib_desc.size());
		vxinput.pVertexAttributeDescriptions = attrib_desc.data (); // optional

		VkPipelineInputAssemblyStateCreateInfo inasm {};
		inasm.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inasm.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inasm.primitiveRestartEnable = VK_FALSE;

		VkViewport view {};
		view.x = 0.0f;
		view.y = 0.0f;
		view.width = (float) swapchain_ext.width;
		view.height = (float) swapchain_ext.height;
		view.minDepth = 0.0f;
		view.maxDepth = 1.0f;

		VkRect2D scissor {};
		scissor.offset = {0, 0};
		scissor.extent = swapchain_ext;

		VkPipelineViewportStateCreateInfo viewstate {};
		viewstate.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewstate.viewportCount = 1;
		viewstate.pViewports = &view;
		viewstate.scissorCount = 1;
		viewstate.pScissors = &scissor;

		VkPipelineRasterizationStateCreateInfo rasterizer {};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizer.depthBiasEnable = VK_FALSE;
		rasterizer.depthBiasConstantFactor = 0.0f; // optional
		rasterizer.depthBiasClamp = 0.0f; // optional
		rasterizer.depthBiasSlopeFactor = 0.0f; // optional

		VkPipelineMultisampleStateCreateInfo multisampling {};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampling.minSampleShading = 1.0f; // optional
		multisampling.pSampleMask = nullptr; // optional
		multisampling.alphaToCoverageEnable = VK_FALSE; // optional
		multisampling.alphaToOneEnable = VK_FALSE; // optional

		VkPipelineColorBlendAttachmentState blend_att {};
		blend_att.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT |
			VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT |
			VK_COLOR_COMPONENT_A_BIT;
		blend_att.blendEnable = VK_FALSE;

		blend_att.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		blend_att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blend_att.colorBlendOp = VK_BLEND_OP_ADD;
		blend_att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		blend_att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		blend_att.alphaBlendOp = VK_BLEND_OP_ADD;

		VkPipelineColorBlendStateCreateInfo blend {};
		blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		blend.logicOpEnable = VK_FALSE;
		blend.logicOp = VK_LOGIC_OP_COPY; // optional
		blend.attachmentCount = 1;
		blend.pAttachments = &blend_att;
		blend.blendConstants[0] = 0.0f; // optional
		blend.blendConstants[1] = 0.0f; // optional
		blend.blendConstants[2] = 0.0f; // optional
		blend.blendConstants[3] = 0.0f; // optional

		VkDynamicState dynstates[] =
		{
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_LINE_WIDTH
		};

		VkPipelineDynamicStateCreateInfo dynstate {};
		dynstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynstate.dynamicStateCount = 2;
		dynstate.pDynamicStates = dynstates;

		VkPipelineLayoutCreateInfo pipeline_cinfo {};
		pipeline_cinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipeline_cinfo.setLayoutCount = 1; // optional
		pipeline_cinfo.pSetLayouts = &descriptor_set_layout; // optional
		//pipeline_cinfo.pushConstantRangeCount = 0; // optional
		//pipeline_cinfo.pPushConstantRanges = nullptr; // optional

		if (vkCreatePipelineLayout (device, &pipeline_cinfo, nullptr, &pipeline_layout) != VK_SUCCESS)
			throw std::runtime_error ("failed to create pipeline layout!");

		VkPipelineDepthStencilStateCreateInfo depth_stencil {};
		depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depth_stencil.depthTestEnable = VK_TRUE;
		depth_stencil.depthWriteEnable = VK_TRUE;
		depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
		depth_stencil.depthBoundsTestEnable = VK_FALSE;
		depth_stencil.minDepthBounds = 0.0f; // optional
		depth_stencil.maxDepthBounds = 1.0f; // optional
		depth_stencil.stencilTestEnable = VK_FALSE;
		depth_stencil.front = {}; // optional
		depth_stencil.back = {}; // optional

		VkGraphicsPipelineCreateInfo pipeinfo {};
		pipeinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeinfo.stageCount = 2;
		pipeinfo.pStages = stages;
		pipeinfo.pVertexInputState = &vxinput;
		pipeinfo.pInputAssemblyState = &inasm;
		pipeinfo.pViewportState = &viewstate;
		pipeinfo.pRasterizationState = &rasterizer;
		pipeinfo.pMultisampleState = &multisampling;
		pipeinfo.pDepthStencilState = &depth_stencil;
		pipeinfo.pColorBlendState = &blend;
		pipeinfo.pDynamicState = nullptr; // optional
		pipeinfo.layout = pipeline_layout;
		pipeinfo.renderPass = render_pass;
		pipeinfo.subpass = 0;
		pipeinfo.basePipelineHandle = VK_NULL_HANDLE; // optional
		pipeinfo.basePipelineIndex = -1; // optional

		if (vkCreateGraphicsPipelines (device, VK_NULL_HANDLE, 1, &pipeinfo, nullptr, &pipeline) != VK_SUCCESS)
			throw std::runtime_error ("failed to create graphics pipeline!");

		// cleanup

		vkDestroyShaderModule (device, vert_shader_mod, nullptr);
		vkDestroyShaderModule (device, frag_shader_mod, nullptr);
	}

	void create_render_pass ()
	{
		VkSubpassDependency dep {};
		dep.srcSubpass = VK_SUBPASS_EXTERNAL;
		dep.dstSubpass = 0;
		dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dep.srcAccessMask = 0;
		dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		VkAttachmentDescription color {};
		color.format = swapchain_img_fmt;
		color.samples = VK_SAMPLE_COUNT_1_BIT;
		color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference color_ref {};
		color_ref.attachment = 0;
		color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription depth {};
		depth.format = find_depth_fmt ();
		depth.samples = VK_SAMPLE_COUNT_1_BIT;
		depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depth_ref {};
		depth_ref.attachment = 1;
		depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &color_ref;
		subpass.pDepthStencilAttachment = &depth_ref;

		std::array<VkAttachmentDescription, 2> attachments = { color, depth };
		VkRenderPassCreateInfo info {};
		info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		info.attachmentCount = static_cast<uint32_t> (attachments.size ());
		info.pAttachments = attachments.data ();
		info.subpassCount = 1;
		info.pSubpasses = &subpass;
		info.dependencyCount = 1;
		info.pDependencies = &dep;

		if (vkCreateRenderPass (device, &info, nullptr, &render_pass) != VK_SUCCESS)
			throw std::runtime_error ("failed to create render pass!");
	}

	void create_framebuffers ()
	{
		swapchain_framebufs.resize (swapchain_image_views.size ());

		for (size_t i = 0; i < swapchain_image_views.size (); i ++)
		{
			std::array<VkImageView,2> attachments = { swapchain_image_views[i], depth_img_view };

			VkFramebufferCreateInfo info {};
			info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			info.renderPass = render_pass;
			info.attachmentCount = static_cast<uint32_t> (attachments.size());
			info.pAttachments = attachments.data ();
			info.width = swapchain_ext.width;
			info.height = swapchain_ext.height;
			info.layers = 1;

			if (vkCreateFramebuffer (device, &info, nullptr, &swapchain_framebufs[i]) != VK_SUCCESS)
				throw std::runtime_error ("failed to create framebuffer!");
		}
	}

	void create_cmd_pool ()
	{
		QueueFamilyIndices idx = find_queue_families (physical_device);

		VkCommandPoolCreateInfo info {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		info.queueFamilyIndex = idx.gfx_family.value ();
		//info.flags = 0; // optional

		if (vkCreateCommandPool (device, &info, nullptr, &cmdpool) != VK_SUCCESS)
			throw std::runtime_error ("failed to create command pool!");
	}

	void create_cmd_buffers ()
	{
		cmdbuffers.resize (swapchain_framebufs.size ());

		VkCommandBufferAllocateInfo allocinfo {};
		allocinfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocinfo.commandPool = cmdpool;
		allocinfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocinfo.commandBufferCount = (uint32_t) cmdbuffers.size ();

		if (vkAllocateCommandBuffers (device, &allocinfo, cmdbuffers.data ()) != VK_SUCCESS)
			throw std::runtime_error ("failed to allocate command buffers!");

		for (size_t i = 0; i < cmdbuffers.size (); i ++)
		{
			VkCommandBufferBeginInfo info {};
			info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			//info.flags = 0; // optional
			//info.pInheritanceInfo = nullptr; // optional

			if (vkBeginCommandBuffer (cmdbuffers[i], &info) != VK_SUCCESS)
				throw std::runtime_error ("failed to begin recording command buffer!");

			std::array<VkClearValue, 2> clear_values = {};
			clear_values[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
			clear_values[1].depthStencil = { 1.0f, 0 };

			VkRenderPassBeginInfo render_pass_info {};
			render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			render_pass_info.renderPass = render_pass;
			render_pass_info.framebuffer = swapchain_framebufs[i];
			render_pass_info.renderArea.offset = { 0, 0 };
			render_pass_info.renderArea.extent = swapchain_ext;
			render_pass_info.clearValueCount = static_cast<uint32_t> (clear_values.size());
			render_pass_info.pClearValues = clear_values.data ();

			vkCmdBeginRenderPass (cmdbuffers[i], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdBindPipeline (cmdbuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

			VkBuffer vx_bufs[] = { vx_buf };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers (cmdbuffers[i], 0, 1, vx_bufs, offsets);

			vkCmdBindIndexBuffer (cmdbuffers[i], idx_buf, 0, VK_INDEX_TYPE_UINT16);

			vkCmdBindDescriptorSets
			(
				cmdbuffers[i],
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipeline_layout,
				0,
				1,
				&descriptor_sets[i],
				0,
				nullptr
			);
			vkCmdDrawIndexed (cmdbuffers[i], static_cast<uint32_t> (indices.size ()), 1, 0, 0, 0);
			vkCmdEndRenderPass (cmdbuffers[i]);

			if (vkEndCommandBuffer (cmdbuffers[i]) != VK_SUCCESS)
				throw std::runtime_error ("failed to record command buffer!");
		}
	}

	void create_sync ()
	{
		img_available.resize (MAX_FRAMES_IN_FLIGHT);
		render_finished.resize (MAX_FRAMES_IN_FLIGHT);
		in_flight_fences.resize (MAX_FRAMES_IN_FLIGHT);
		images_in_flight.resize (swapchain_images.size (), VK_NULL_HANDLE);

		VkSemaphoreCreateInfo semaphore_info {};
		semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fence_info {};
		fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i ++)
			if
			(
				vkCreateSemaphore (device, &semaphore_info, nullptr, &img_available[i]) != VK_SUCCESS ||
				vkCreateSemaphore (device, &semaphore_info, nullptr, &render_finished[i]) != VK_SUCCESS ||
				vkCreateFence (device, &fence_info, nullptr, &in_flight_fences[i]) != VK_SUCCESS
			)
				throw std::runtime_error ("failed to create semaphores!");

	}

	void create_descriptor_set_layout ()
	{
		VkDescriptorSetLayoutBinding ubo_layout_binding {};
		ubo_layout_binding.binding = 0;
		ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubo_layout_binding.descriptorCount = 1;
		ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		ubo_layout_binding.pImmutableSamplers = nullptr; // optional

		VkDescriptorSetLayoutBinding sampler_layout_binding {};
		sampler_layout_binding.binding = 1;
		sampler_layout_binding.descriptorCount = 1;
		sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		sampler_layout_binding.pImmutableSamplers = nullptr;
		sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		std::array<VkDescriptorSetLayoutBinding, 2> bindings =
		{
			ubo_layout_binding, sampler_layout_binding
		};
		VkDescriptorSetLayoutCreateInfo layout_info {};
		layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layout_info.bindingCount = static_cast <uint32_t> (bindings.size ());
		layout_info.pBindings = bindings.data ();

		if (vkCreateDescriptorSetLayout (device, &layout_info, nullptr, &descriptor_set_layout) != VK_SUCCESS)
			throw std::runtime_error ("failed to create descriptor set layout!");
	}

	void create_uniform_buf ()
	{
		VkDeviceSize size = sizeof (UniformBufferObject);

		unif_buf.resize (swapchain_images.size ());
		unif_buf_mem.resize (swapchain_images.size ());

		for (size_t i = 0; i < swapchain_images.size (); i ++)
			create_buffer
			(
				size,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				unif_buf[i],
				unif_buf_mem[i]
			);
	}

	void create_descriptor_pool ()
	{
		std::array<VkDescriptorPoolSize, 2> pool_sizes {};
		pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		pool_sizes[0].descriptorCount = static_cast<uint32_t> (swapchain_images.size ());
		pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pool_sizes[1].descriptorCount = static_cast<uint32_t> (swapchain_images.size ());

		VkDescriptorPoolCreateInfo pool_info {};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.poolSizeCount = static_cast<uint32_t> (pool_sizes.size ());
		pool_info.pPoolSizes = pool_sizes.data ();
		pool_info.maxSets = static_cast<uint32_t> (swapchain_images.size ());

		if (vkCreateDescriptorPool (device, &pool_info, nullptr, &descriptor_pool) != VK_SUCCESS)
			throw std::runtime_error ("failed to create descriptor pool!");
	}

	void create_descriptor_sets ()
	{
		std::vector<VkDescriptorSetLayout> layouts (swapchain_images.size (), descriptor_set_layout);

		VkDescriptorSetAllocateInfo alloc_info {};
		alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_info.descriptorPool = descriptor_pool;
		alloc_info.descriptorSetCount = static_cast<uint32_t> (swapchain_images.size ());
		alloc_info.pSetLayouts = layouts.data ();

		descriptor_sets.resize (swapchain_images.size ());
		if (vkAllocateDescriptorSets (device, &alloc_info, descriptor_sets.data ()) != VK_SUCCESS)
			throw std::runtime_error ("failed to allocate descriptor sets!");

		for (size_t i = 0; i < swapchain_images.size (); i ++)
		{
			VkDescriptorBufferInfo buffer_info {};
			buffer_info.buffer = unif_buf[i];
			buffer_info.offset = 0;
			buffer_info.range = sizeof (UniformBufferObject);

			VkDescriptorImageInfo img_info {};
			img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			img_info.imageView = tex_img_view;
			img_info.sampler = tex_sampler;

			std::array<VkWriteDescriptorSet, 2> writes {};

			writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[0].dstSet = descriptor_sets[i];
			writes[0].dstBinding = 0;
			writes[0].dstArrayElement = 0;
			writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writes[0].descriptorCount = 1;
			writes[0].pBufferInfo = &buffer_info;
			writes[0].pImageInfo = nullptr; // optional
			writes[0].pTexelBufferView = nullptr; // optional

			writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[1].dstSet = descriptor_sets[i];
			writes[1].dstBinding = 1;
			writes[1].dstArrayElement = 0;
			writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writes[1].descriptorCount = 1;
			writes[1].pBufferInfo = nullptr;
			writes[1].pImageInfo = &img_info; // optional
			writes[1].pTexelBufferView = nullptr; // optional

			vkUpdateDescriptorSets
			(
				device, static_cast<uint32_t> (writes.size()), writes.data (), 0, nullptr
			);
		}
	}

	void create_img
	(
		uint32_t w,
		uint32_t h,
		VkFormat fmt,
		VkImageTiling tiling,
		VkImageUsageFlags usage,
		VkMemoryPropertyFlags props,
		VkImage& img,
		VkDeviceMemory& img_mem
	)
	{
		VkImageCreateInfo img_info {};
		img_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		img_info.imageType = VK_IMAGE_TYPE_2D;
		img_info.extent.width = w;
		img_info.extent.height = h;
		img_info.extent.depth = 1;
		img_info.mipLevels = 1;
		img_info.arrayLayers = 1;
		img_info.format = fmt;
		img_info.tiling = tiling;
		img_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		img_info.usage = usage;
		img_info.samples = VK_SAMPLE_COUNT_1_BIT;
		img_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateImage (device, &img_info, nullptr, &img) != VK_SUCCESS)
			throw std::runtime_error ("failed to create image!");

		VkMemoryRequirements memreq;
		vkGetImageMemoryRequirements (device, img, &memreq);

		VkMemoryAllocateInfo alloc_info {};
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize = memreq.size;
		alloc_info.memoryTypeIndex = find_mem_type (memreq.memoryTypeBits, props);

		if (vkAllocateMemory (device, &alloc_info, nullptr, &img_mem) != VK_SUCCESS)
			throw std::runtime_error ("failed to allocate image memory");

		vkBindImageMemory (device, img, img_mem, 0);
	}

	void create_texture_img ()
	{
		int w, h, c;

		stbi_uc *pix = stbi_load ("textures/texture.jpg", &w, &h, &c, STBI_rgb_alpha);
		VkDeviceSize size = w * h * 4;

		if (!pix)
			throw std::runtime_error ("failed to load texture image!");

		VkBuffer staging_buf;
		VkDeviceMemory staging_buf_mem;

		create_buffer
		(
			size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			staging_buf,
			staging_buf_mem
		);

		void *data;
		vkMapMemory (device, staging_buf_mem, 0, size, 0, &data);
		memcpy (data, pix, static_cast<size_t> (size));
		vkUnmapMemory (device, staging_buf_mem);

		stbi_image_free (pix);

		create_img
		(
			w,
			h,
			VK_FORMAT_R8G8B8A8_SRGB,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			tex_img,
			tex_img_mem
		);

		transition_img_layout
		(
			tex_img,
			VK_FORMAT_R8G8B8A8_SRGB,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
		);

		cp_buf_img
		(
			staging_buf,
			tex_img,
			static_cast<uint32_t> (w),
			static_cast<uint32_t> (h)
		);

		transition_img_layout
		(
			tex_img,
			VK_FORMAT_R8G8B8A8_SRGB,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);

		vkDestroyBuffer (device, staging_buf, nullptr);
		vkFreeMemory (device, staging_buf_mem, nullptr);
	}

	void cp_buf_img (VkBuffer buf, VkImage img, uint32_t w, uint32_t h)
	{
		VkCommandBuffer cmdbuf = begin_single_time_cmds ();

		VkBufferImageCopy region {};

		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;

		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = { w, h, 1 };

		vkCmdCopyBufferToImage (cmdbuf, buf, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		end_single_time_cmds (cmdbuf);
	}

	void transition_img_layout
	(
		VkImage img,
		VkFormat fmt,
		VkImageLayout old_layout,
		VkImageLayout new_layout
	)
	{
		VkCommandBuffer cmdbuf = begin_single_time_cmds ();

		VkImageMemoryBarrier barrier {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = old_layout;
		barrier.newLayout = new_layout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = img;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.srcAccessMask = 0; // TODO
		barrier.dstAccessMask = 0; // TODO

		if (new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		{
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

			if (has_stencil_component (fmt))
				barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}

		VkPipelineStageFlags src_stage;
		VkPipelineStageFlags dst_stage;

		if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if
		(
			old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
			new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		)
		{
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else if
		(
			old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
			new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		)
		{
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask =
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			dst_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		}
		else
		{
			throw std::invalid_argument ("unsupported layout transition");
		}

		vkCmdPipelineBarrier (
			cmdbuf,
			src_stage,
			dst_stage,
			0,
			0,
			nullptr,
			0,
			nullptr,
			1,
			&barrier
		);

		end_single_time_cmds (cmdbuf);
	}

	void create_tex_img_view ()
	{
		tex_img_view = create_img_view (tex_img, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
	}

	void create_tex_sampler ()
	{
		VkSamplerCreateInfo info {};
		info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		info.magFilter = VK_FILTER_LINEAR;
		info.minFilter = VK_FILTER_LINEAR;
		info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		info.anisotropyEnable = VK_TRUE;

		VkPhysicalDeviceProperties props {};
		vkGetPhysicalDeviceProperties (physical_device, &props);

		info.maxAnisotropy = props.limits.maxSamplerAnisotropy;
		info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		info.unnormalizedCoordinates = VK_FALSE;
		info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		info.mipLodBias = 0.0f;
		info.minLod = 0.0f;
		info.maxLod = 0.0f;

		if (vkCreateSampler (device, &info, nullptr, &tex_sampler) != VK_SUCCESS)
		{
			throw std::runtime_error ("failed to create texture sampler!");
		}
	}

	VkFormat find_supported_fmt
	(
		const std::vector<VkFormat> candidates,
		VkImageTiling tiling,
		VkFormatFeatureFlags feats
	)
	{
		for (VkFormat fmt : candidates)
		{
			VkFormatProperties p;
			vkGetPhysicalDeviceFormatProperties (physical_device, fmt, &p);

			if (tiling == VK_IMAGE_TILING_LINEAR && (p.linearTilingFeatures & feats) == feats)
			{
				return fmt;
			}
			else if (tiling == VK_IMAGE_TILING_OPTIMAL && (p.optimalTilingFeatures & feats) == feats)
			{
				return fmt;
			}
		}

		throw std::runtime_error ("failed to find supported format");
	}

	VkFormat find_depth_fmt ()
	{
		return find_supported_fmt
		(
			{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
		);
	}

	bool has_stencil_component (VkFormat fmt)
	{
		return fmt == VK_FORMAT_D32_SFLOAT_S8_UINT || fmt == VK_FORMAT_D24_UNORM_S8_UINT;
	}

	void create_depth_buffer ()
	{
		VkFormat depth_fmt = find_depth_fmt ();
		create_img
		(
			swapchain_ext.width,
			swapchain_ext.height,
			depth_fmt,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			depth_img,
			depth_img_mem
		);
		depth_img_view = create_img_view (depth_img, depth_fmt, VK_IMAGE_ASPECT_DEPTH_BIT);

		transition_img_layout
		(
			depth_img,
			depth_fmt,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		);
	}

	void init_vulkan ()
	{
		create_instance ();
		setup_debugger ();
		create_surface ();
		pick_physical_device ();
		create_logical_device ();
		create_swapchain ();
		create_img_views ();
		create_render_pass ();
		create_descriptor_set_layout ();
		create_gfx_pipeline ();
		create_cmd_pool ();
		create_depth_buffer ();
		create_framebuffers ();
		create_texture_img ();
		create_tex_img_view ();
		create_tex_sampler ();
		create_vx_buf ();
		create_idx_buf ();
		create_uniform_buf ();
		create_descriptor_pool ();
		create_descriptor_sets ();
		create_cmd_buffers ();
		create_sync ();
	}

	void update_unif_buf (uint32_t img)
	{
		static auto start_time = std::chrono::high_resolution_clock::now ();

		auto current_time = std::chrono::high_resolution_clock::now ();
		float time = std::chrono::duration<float, std::chrono::seconds::period>
		(
			current_time - start_time
		).count();

		UniformBufferObject ubo {};
		ubo.model = glm::rotate
		(
			glm::mat4 (1.0f), time * glm::radians (90.0f), glm::vec3 (0.0, 0.0, 1.0f)
		);
		ubo.view = glm::lookAt
		(
			glm::vec3 (2.0f, 2.0f, 2.0f), glm::vec3 (0.0f, 0.0f, 0.0f), glm::vec3 (0.0f, 0.0f, 1.0f)
		);
		ubo.proj = glm::perspective
		(
			glm::radians (45.0f), swapchain_ext.width / (float) swapchain_ext.height, 0.1f, 10.0f
		);
		ubo.proj[1][1] *= -1;

		void* data;
		vkMapMemory (device, unif_buf_mem[img], 0, sizeof (ubo), 0, &data);
		memcpy (data, &ubo, sizeof (ubo));
		vkUnmapMemory (device, unif_buf_mem[img]);
	}

	void draw ()
	{
		vkWaitForFences (device, 1, &in_flight_fences[current_frame], VK_TRUE, UINT64_MAX);

		uint32_t img_idx;
		VkResult res = vkAcquireNextImageKHR (
			device,
			swap_chain,
			UINT64_MAX,
			img_available[current_frame],
			VK_NULL_HANDLE,
			&img_idx
		);

		if (res == VK_ERROR_OUT_OF_DATE_KHR)
		{
			recreate_swapchain ();
			return;
		}
		else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR)
			throw std::runtime_error ("failed to acquire swap chain image!");

		// check if a previous frame is using this image (i.e. there is its fence to wait on)
		if (images_in_flight [img_idx] != VK_NULL_HANDLE)
			vkWaitForFences (device, 1, &images_in_flight[img_idx], VK_TRUE, UINT64_MAX);

		// mark the image as now being in use by this frame
		images_in_flight[img_idx] = in_flight_fences[current_frame];

		VkSemaphore wait_semaphores[] = { img_available[current_frame] };
		VkSemaphore sig_semaphores[] = { render_finished[current_frame] };

		VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

		update_unif_buf (img_idx);

		VkSubmitInfo submit_info {};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = wait_semaphores;
		submit_info.pWaitDstStageMask = wait_stages;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &cmdbuffers[img_idx];
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = sig_semaphores;

		vkResetFences (device, 1, &in_flight_fences[current_frame]);

		if (vkQueueSubmit (gfx_queue, 1, &submit_info, in_flight_fences[current_frame]) != VK_SUCCESS)
			throw std::runtime_error ("failed to submit draw command buffer!");

		VkSwapchainKHR swapchains[] = { swap_chain };

		VkPresentInfoKHR present_info {};
		present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		present_info.waitSemaphoreCount = 1;
		present_info.pWaitSemaphores = sig_semaphores;
		present_info.swapchainCount = 1;
		present_info.pSwapchains = swapchains;
		present_info.pImageIndices = &img_idx;
		present_info.pResults = nullptr; // optional

		res = vkQueuePresentKHR (present_queue, &present_info);
		if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || framebuf_resized)
		{
			framebuf_resized = false;
			recreate_swapchain ();
		}
		else if (res != VK_SUCCESS)
			throw std::runtime_error ("failed to present swap chain image!");

		current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	void main ()
	{
		while (!glfwWindowShouldClose (win))
		{
			glfwPollEvents ();
			draw ();
		}

		vkDeviceWaitIdle (device);
	}

	void cleanup ()
	{
		cleanup_swapchain ();

		vkDestroySampler (device, tex_sampler, nullptr);
		vkDestroyImageView (device, tex_img_view, nullptr);
		vkDestroyImage (device, tex_img, nullptr);
		vkFreeMemory (device, tex_img_mem, nullptr);

		vkDestroyDescriptorSetLayout (device, descriptor_set_layout, nullptr);

		vkDestroyBuffer (device, vx_buf, nullptr);
		vkFreeMemory (device, vx_buf_mem, nullptr);

		vkDestroyBuffer (device, idx_buf, nullptr);
		vkFreeMemory (device, idx_buf_mem, nullptr);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i ++)
		{
			vkDestroySemaphore (device, render_finished[i], nullptr);
			vkDestroySemaphore (device, img_available[i], nullptr);
			vkDestroyFence (device, in_flight_fences[i], nullptr);
		}

		vkDestroyCommandPool (device, cmdpool, nullptr);
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
