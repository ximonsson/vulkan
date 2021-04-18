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
#include <glm/glm.hpp>
#include <array>

#define GLFW_INCLUDE_VULKAM
#include <GLFW/glfw3.h>

struct Vertex
{
	glm::vec2 pos;
	glm::vec3 color;

	static VkVertexInputBindingDescription get_binding_desc ()
	{
		VkVertexInputBindingDescription desc {};
		desc.binding = 0;
		desc.stride = sizeof (Vertex);
		desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return desc;
	}

	static std::array<VkVertexInputAttributeDescription, 2> get_attrib_desc ()
	{
		std::array<VkVertexInputAttributeDescription, 2> desc;

		desc[0].binding = 0;
		desc[0].location = 0;
		desc[0].format = VK_FORMAT_R32G32_SFLOAT;
		desc[0].offset = offsetof (Vertex, pos);

		desc[1].binding = 0;
		desc[1].location = 1;
		desc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		desc[1].offset = offsetof (Vertex, color);

		return desc;
	}
};

const std::vector<Vertex> vertices =
{
	{ { 0.0f, -0.5f }, { 1.0f,  0.0f, 0.0f } },
	{ { 0.5f, 0.5f }, { 0.0f,  1.0f, 0.0f } },
	{ { -0.5f, 0.5f }, { 0.0f,  0.0f, 1.0f } }
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
	VkSwapchainKHR swap_chain;
	std::vector<VkImage> swapchain_images;
	VkFormat swapchain_img_fmt;
	VkExtent2D swapchain_ext;
	std::vector<VkImageView> swapchain_image_views;
	VkRenderPass render_pass;
	VkPipelineLayout pipeline_layout;
	VkPipeline pipeline;
	std::vector<VkFramebuffer> swapchain_framebufs;
	VkCommandPool cmdpool;
	std::vector<VkCommandBuffer> cmdbuffers;
	std::vector<VkSemaphore> img_available;
	std::vector<VkSemaphore> render_finished;
	std::vector<VkFence> in_flight_fences;
	std::vector<VkFence> images_in_flight;
	size_t current_frame = 0;
	bool framebuf_resized = false;
	VkBuffer vx_buf;
	VkDeviceMemory vx_buf_mem;

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
		swapchain_images.resize (imcount);
		vkGetSwapchainImagesKHR (device, swap_chain, &imcount, swapchain_images.data ());

		swapchain_img_fmt = fmt.format;
		swapchain_ext = ext;
	}

	void cleanup_swapchain ()
	{
		for (auto buf : swapchain_framebufs)
			vkDestroyFramebuffer (device, buf, nullptr);

		vkFreeCommandBuffers (device, cmdpool, static_cast<uint32_t> (cmdbuffers.size ()), cmdbuffers.data ());

		vkDestroyPipeline (device, pipeline, nullptr);
		vkDestroyPipelineLayout (device, pipeline_layout, nullptr);
		vkDestroyRenderPass (device, render_pass, nullptr);

		for (auto v : swapchain_image_views)
			vkDestroyImageView (device, v, nullptr);

		vkDestroySwapchainKHR (device, swap_chain, nullptr);
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
		create_image_views ();
		create_render_pass ();
		create_gfx_pipeline ();
		create_framebuffers ();
		create_cmd_buffers ();
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

		create_buffer
		(
			size,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			vx_buf,
			vx_buf_mem
		);

		void *data;
		vkMapMemory (device, vx_buf_mem, 0, size, 0, &data);
		memcpy (data, vertices.data (), (size_t) size);
		vkUnmapMemory (device, vx_buf_mem);
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
		rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
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
		pipeline_cinfo.setLayoutCount = 0; // optional
		pipeline_cinfo.pSetLayouts = nullptr; // optional
		pipeline_cinfo.pushConstantRangeCount = 0; // optional
		pipeline_cinfo.pPushConstantRanges = nullptr; // optional

		if (vkCreatePipelineLayout (device, &pipeline_cinfo, nullptr, &pipeline_layout) != VK_SUCCESS)
			throw std::runtime_error ("failed to create pipeline layout!");

		VkGraphicsPipelineCreateInfo pipeinfo {};
		pipeinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeinfo.stageCount = 2;
		pipeinfo.pStages = stages;
		pipeinfo.pVertexInputState = &vxinput;
		pipeinfo.pInputAssemblyState = &inasm;
		pipeinfo.pViewportState = &viewstate;
		pipeinfo.pRasterizationState = &rasterizer;
		pipeinfo.pMultisampleState = &multisampling;
		pipeinfo.pDepthStencilState = nullptr; // optional
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
		dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dep.srcAccessMask = 0;
		dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkAttachmentDescription color {};
		color.format = swapchain_img_fmt;
		color.samples = VK_SAMPLE_COUNT_1_BIT;
		color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference ref {};
		ref.attachment = 0;
		ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &ref;

		VkRenderPassCreateInfo info {};
		info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		info.attachmentCount = 1;
		info.pAttachments = &color;
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
			VkImageView attachments[] = { swapchain_image_views[i] };

			VkFramebufferCreateInfo info {};
			info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			info.renderPass = render_pass;
			info.attachmentCount = 1;
			info.pAttachments = attachments;
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

			VkClearValue clear_clr = { 0.0f, 0.0f, 0.0f, 1.0f };

			VkRenderPassBeginInfo render_pass_info {};
			render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			render_pass_info.renderPass = render_pass;
			render_pass_info.framebuffer = swapchain_framebufs[i];
			render_pass_info.renderArea.offset = { 0, 0 };
			render_pass_info.renderArea.extent = swapchain_ext;
			render_pass_info.clearValueCount = 1;
			render_pass_info.pClearValues = &clear_clr;

			vkCmdBeginRenderPass (cmdbuffers[i], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdBindPipeline (cmdbuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

			VkBuffer vx_bufs[] = { vx_buf };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers (cmdbuffers[i], 0, 1, vx_bufs, offsets);

			vkCmdDraw (cmdbuffers[i], static_cast<uint32_t> (vertices.size ()), 1, 0, 0);
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

	void init_vulkan ()
	{
		create_instance ();
		setup_debugger ();
		create_surface ();
		pick_physical_device ();
		create_logical_device ();
		create_swapchain ();
		create_image_views ();
		create_render_pass ();
		create_gfx_pipeline ();
		create_framebuffers ();
		create_cmd_pool ();
		create_vx_buf ();
		create_cmd_buffers ();
		create_sync ();
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

		vkDestroyBuffer (device, vx_buf, nullptr);
		vkFreeMemory (device, vx_buf_mem, nullptr);

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
