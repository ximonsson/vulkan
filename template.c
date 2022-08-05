#include <vulkan/vulkan.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define GLFW_INCLUDE_VULKAM
#include <GLFW/glfw3.h>


/**
 *		Define vertices.
 *
 * These should be changed for application specific.
 */
static const float vertices[8][3] =
{
	// first square
	{ -0.5f, -0.5f, 0.0f },
	{ 0.5f, -0.5f, 0.0f },
	{ 0.5f, 0.5f, 0.0f },
	{ -0.5f, 0.5f, 0.0f },
	// second square
	{ -0.5f, -0.5f, -0.5f },
	{ 0.5f, -0.5f, -0.5f },
	{ 0.5f, 0.5f, -0.5f },
	{ -0.5f, 0.5f, -0.5f },
};

/**
 *		Define texture coords.
 *
 * These should be changed for application specific.
 */
static const float tex_coords[8][2] =
{
	// first square
	{ 1.0f, .0f },
	{ .0f, .0f },
	{ .0f, 1.0f },
	{ 1.0f, 1.0f },
	// second square
	{ 1.0f, .0f },
	{ .0f, .0f },
	{ .0f, 1.0f },
	{ 1.0f, 1.0f }
};

/**
 *		Define vertice indices.
 *
 * This should be changed for the applications.
 */
static const uint16_t indices[12] =
{
	0, 1, 2, 2, 3, 0,
	4, 5, 6, 6, 7, 4
};

static const uint32_t WIDTH = 200;
static const uint32_t HEIGHT = 150;
static const int MAX_FRAMES_IN_FLIGHT = 2;

/**
 *		read_file
 *
 * Helper function to read a file such as shader source.
 */
static size_t read_file (const char *fname, char **data)
{
	FILE* fp = fopen (fname, "rb");

	// get file size
	fseek (fp, 0, SEEK_END);
	size_t fsize = ftell (fp);
	rewind (fp);

	// read
	size_t result;
	*data = (char *) malloc (fsize);
	if ((result = fread (*data, 1, fsize, fp)) != fsize)
	{
		return -1;
	}

	return result;
}

/**
 * Validation layours.
 */
#define N_VALIDATION_LAYERS 1
static const const char *validation_layers[N_VALIDATION_LAYERS] =
{
	"VK_LAYER_KHRONOS_validation"
};

/**
 * Device extensions.
 */
#define N_DEVICE_EXTENSIONS 1
static const const char *device_extensions[N_DEVICE_EXTENSIONS] =
{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

typedef struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	VkSurfaceFormatKHR *fmts;
	VkPresentModeKHR *present_modes;
	int32_t nfmts;
	int32_t npresmodes;
} SwapChainSupportDetails;

static void swapchain_support_details_free (SwapChainSupportDetails *details)
{
	if (details->fmts != NULL) free (details->fmts);
	if (details->present_modes != NULL) free (details->present_modes);
	details = NULL;
}

typedef struct QueueFamilyIndices
{
	int32_t gfx_family;
	int32_t present_family;
} QueueFamilyIndices;

void queue_family_indices_init (QueueFamilyIndices *idx)
{
	idx->gfx_family = idx->present_family = -1;
}

int queue_family_indices_is_complete (const QueueFamilyIndices *idx)
{
	// TODO is zero correct to check?
	return (idx->gfx_family != -1) && (idx->present_family != -1);
}


#ifdef DEBUG
/**
 *		check_validation_layer_support
 *
 * Make sure the validation layers are supported.
 */
int check_validation_layer_support ()
{
	uint32_t nlayers;
	vkEnumerateInstanceLayerProperties (&nlayers, NULL);
	VkLayerProperties *available_layers = (VkLayerProperties *) malloc (sizeof (VkLayerProperties) * nlayers);
	vkEnumerateInstanceLayerProperties (&nlayers, available_layers);

	for (int j = 0; j < N_VALIDATION_LAYERS; j ++)
	{
		const char *layer_name = validation_layers[j];
		int found = 0;

		for (int i = 0; i < nlayers; i ++)
		{
			VkLayerProperties layer = available_layers[i];
			if (strcmp (layer.layerName, layer_name) == 0)
			{
				found = 1;
				break;
			}
		}

		if (!found) return 0;
	}

	free (available_layers);
	return 1;
}
#endif

/**
 * Get required extensions from glfw to run.
 */
static const char **get_required_extensions (uint32_t *n)
{
	const char **ext = glfwGetRequiredInstanceExtensions (n);

#ifdef DEBUG
	// add validation layer for debugging
	const char **ext_ = ext;
	ext = malloc (((*n) + 1) * sizeof (char *));
	memcpy (ext, ext_, (*n) * sizeof (char *));
	ext[(*n)] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
	*n += 1;
#endif

	return ext;
}

/**
 * Get supported extensions.
 */
static const VkExtensionProperties *get_supported_extensions (uint32_t *n)
{
	vkEnumerateInstanceExtensionProperties (NULL, n, NULL);
	VkExtensionProperties *ext = malloc ((*n) * sizeof (VkExtensionProperties));
	vkEnumerateInstanceExtensionProperties (NULL, n, ext);
	return ext;
}

/**
 * Function to find and load `vkCreateDebugUtilsMessengerEXT`.
 */
VkResult myCreateDebugUtilsMessengerEXT
(
	VkInstance instance,
	const VkDebugUtilsMessengerCreateInfoEXT *info,
	const VkAllocationCallbacks *allocator,
	VkDebugUtilsMessengerEXT *messenger
)
{
	PFN_vkCreateDebugUtilsMessengerEXT fn = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr
	(
		instance, "vkCreateDebugUtilsMessengerEXT"
	);
	if (!fn) return VK_ERROR_EXTENSION_NOT_PRESENT;
	return fn (instance, info, allocator, messenger);
}

/**
 * Function to find and load `vkDestroyDebugUtilsMessengerEXT`.
 */
void myDestroyDebugUtilsMessengerEXT
(
	VkInstance instance,
	VkDebugUtilsMessengerEXT messenger,
	const VkAllocationCallbacks *allocator
)
{
	PFN_vkDestroyDebugUtilsMessengerEXT fn = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr
	(
		instance, "vkDestroyDebugUtilsMessengerEXT"
	);
	if (fn) fn (instance, messenger, allocator);
}

static VkSurfaceFormatKHR choose_swap_surface_format (const VkSurfaceFormatKHR *fmts, size_t n) {
	for (size_t i = 0; i < n; i ++)
	{
		VkSurfaceFormatKHR fmt = fmts[i];
		if
		(
			fmt.format == VK_FORMAT_B8G8R8A8_SRGB &&
			fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
		)
		{
			return fmt;
		}
	}

	return fmts[0];
}

static VkPresentModeKHR choose_swap_present_mode (const VkPresentModeKHR *modes, size_t n)
{
	for (size_t i = 0; i < n; i ++)
	{
		if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
			return modes[i];
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D choose_swap_extent (VkSurfaceCapabilitiesKHR capabilities)
{
	return capabilities.currentExtent;

	// TODO check if below is needed

	/*
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
	*/
}

/**
 *	VARIABLES --------------------------------------------------------------------------------------------------
 */

static GLFWwindow* win; // TODO what about offscreen rendering?
static VkInstance instance;
static VkDebugUtilsMessengerEXT debug_messenger;
static VkPhysicalDevice physical_device = VK_NULL_HANDLE;
static VkDevice device; // logical device
static VkQueue gfx_queue;
static VkQueue present_queue;
static VkSurfaceKHR surface;
static VkRenderPass render_pass;

/* swapchain */
static VkSwapchainKHR swapchain;
static VkImage *swapchain_imgs;
static uint32_t n_swapchain_imgs;
static VkFramebuffer *swapchain_framebufs;
static VkFormat swapchain_img_fmt;
static VkExtent2D swapchain_ext;
static VkImageView *swapchain_img_views;
static uint32_t n_swapchain_img_views;

/* pipeline */
static VkPipelineLayout pipeline_layout;
static VkPipeline pipeline;

/* commands */
static VkCommandPool cmdpool;
static VkCommandBuffer *cmdbuffers;
static VkSemaphore *img_available;
static VkSemaphore *render_finished;
static VkFence *in_flight_fences;
static VkFence *images_in_flight;

/* vertex buffer */
static VkBuffer vx_buf;
static VkBuffer staging_buf;
static VkDeviceMemory vx_buf_mem;

/* index buffer */
static VkBuffer idx_buf;
static VkDeviceMemory idx_buf_mem;

/* uniform buffer */
static VkBuffer *unif_buf;
static VkDeviceMemory *unif_buf_mem;

/* descriptor */
static VkDescriptorPool descriptor_pool;
static VkDescriptorSet *descriptor_sets;
static VkDescriptorSetLayout descriptor_set_layout;

/* texture */
static VkImage tex_img;
static VkDeviceMemory tex_img_mem;
static VkImageView tex_img_view;
static VkSampler tex_sampler;

/* depth buffer */
static VkImage depth_img;
static VkDeviceMemory depth_img_mem;
static VkImageView depth_img_view;

/* state variables */
static size_t current_frame = 0;
static int framebuf_resized = 0;

/**
 * Queries queue families to find the one that supports the given device.
 */
static int query_queue_families (
	VkPhysicalDevice dev,
	uint32_t *gfx_family,
	uint32_t *present_support
)
{
	uint32_t n = 0;
	vkGetPhysicalDeviceQueueFamilyProperties (dev, &n, NULL);
	VkQueueFamilyProperties families[n];
	vkGetPhysicalDeviceQueueFamilyProperties (dev, &n, families);
	uint8_t found_gfx_family = 0, found_present_support = 0;

	for (int i = 0; i < n; i ++)
	{
		VkQueueFamilyProperties family = families[i];

		if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			*gfx_family = i;
			found_gfx_family = 1;
		}

		VkBool32 sup = 0;
		vkGetPhysicalDeviceSurfaceSupportKHR (dev, i, surface, &sup);

		if (sup)
		{
			*present_support = i;
			found_present_support = 1;
		}

		if (found_gfx_family && found_present_support) break;
	}

	return found_gfx_family && found_present_support;
}

/**
 * Callback when the framebuffer has been resized.
 *
 * TODO this does not feel like it is needed. Or?
 */
static void framebuf_resize_cb (GLFWwindow *win, int w, int h)
{
	// auto app = reinterpret_cast<HelloTriangleApplication*> (glfwGetWindowUserPointer (win));
	framebuf_resized = 1;
}

/**
 * Initialize GLFW window.
 */
static void init_window ()
{
	glfwInit ();
	glfwWindowHint (GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint (GLFW_RESIZABLE, GLFW_FALSE);
	win = glfwCreateWindow (WIDTH, HEIGHT, "Vulkan", NULL, NULL);
	glfwSetFramebufferSizeCallback (win, framebuf_resize_cb);
}

/**
 * No idea what goes on in this function.
 */
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback
(
	VkDebugUtilsMessageSeverityFlagBitsEXT severity,
	VkDebugUtilsMessageTypeFlagsEXT type,
	const VkDebugUtilsMessengerCallbackDataEXT* data,
	void* userd
)
{
	fprintf (stderr, "validation layer: %s\n", data->pMessage);
	return VK_FALSE;
}

#ifdef DEBUG
/**
 * Populate debug message with more info.
 */
static void populate_debug_msg_info (VkDebugUtilsMessengerCreateInfoEXT *info)
{
	info->sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	info->messageSeverity =
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	info->messageType =
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	info->pfnUserCallback = debug_callback;
	info->pUserData = NULL;
}

static void setup_debugger ()
{
	VkDebugUtilsMessengerCreateInfoEXT info = { 0 };
	populate_debug_msg_info (&info);
	VkResult res;
	if ((res = myCreateDebugUtilsMessengerEXT (instance, &info, NULL, &debug_messenger)) != VK_SUCCESS)
	{
		fprintf (stderr, "failed to set up debug messenger: %d!\n", res);
		exit (1);
	}
}
#endif

static void create_instance ()
{
	// application info
	//
	// TODO not sure there is 1.2

	VkApplicationInfo app_info = { 0 };
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "hello madafaka";
	app_info.applicationVersion = VK_MAKE_VERSION (1, 2, 0);
	app_info.pEngineName = "No Engine";
	app_info.engineVersion = VK_MAKE_VERSION (1, 2, 0);
	app_info.apiVersion = VK_API_VERSION_1_2;

	// instance create info

	VkInstanceCreateInfo create_info = { 0 };
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &app_info;

	// extensions
	//
	// get required extensions and make sure they are all supported

	uint32_t n_ext;
	const char **ext = get_required_extensions (&n_ext);

	uint32_t n_vk_ext;
	const VkExtensionProperties *vk_ext = get_supported_extensions (&n_vk_ext);

#ifdef DEBUG
	printf ("%d extensions required\n", n_ext);
#endif

	for (int i = 0; i < n_ext; i ++)
	{
#ifdef DEBUG
		printf ("\t%s => ", ext[i]);
#endif
		int found = 0;
		for (int j = 0; j < n_vk_ext; j ++)
		{
			if (strcmp (ext[i], vk_ext[j].extensionName) == 0)
			{
				found = 1;
				break;
			}
		}
		if (!found)
		{
#ifdef DEBUG
			printf ("not found!\n");
#endif
			fprintf (stderr, "not all extensions supported!\n");
			exit (1);
		}
#ifdef DEBUG
		else printf ("found\n");
#endif
	}
	// all extensions supported
	create_info.enabledExtensionCount = n_ext;
	create_info.ppEnabledExtensionNames = ext;

	// validation layers

#ifdef DEBUG
	VkDebugUtilsMessengerCreateInfoEXT debug_info = { 0 };
	if (!check_validation_layer_support ())
	{
		fprintf (stderr, "validation layers requested but not supported!\n");
		exit (1);
	}
	create_info.enabledLayerCount = N_VALIDATION_LAYERS;
	create_info.ppEnabledLayerNames = validation_layers;

	populate_debug_msg_info (&debug_info);
	create_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT *) &debug_info;
#else
	create_info.enabledLayerCount = 0;
	create_info.pNext = NULL;
#endif

	// create the instance

	VkResult res = vkCreateInstance (&create_info, NULL, &instance);
	if (res != VK_SUCCESS)
	{
		fprintf (stderr, "failed to create instance! error code: %d\n", res);
		exit (1);
	}

#ifdef DEBUG
	// TODO this is ugly
	free (ext);
#endif
	free ((void *) vk_ext);
}

static void create_surface ()
{
	if (glfwCreateWindowSurface (instance, win, NULL, &surface) != VK_SUCCESS)
	{
		fprintf (stderr, "failed to create window surface\n");
		exit (1);
	}
}

int check_device_ext_support (VkPhysicalDevice dev)
{
	uint32_t n;
	vkEnumerateDeviceExtensionProperties (dev, NULL, &n, NULL);
	VkExtensionProperties ext[n];
	vkEnumerateDeviceExtensionProperties (dev, NULL, &n, ext);

	// x will be the number of required device extrensions that we support
	// - then we check later is the same as the number of actual required extensions
	int x = 0;

	for (int j = 0; j < N_DEVICE_EXTENSIONS; j ++)
	{
		const char *req_ext = device_extensions[j];
		for (int i = 0; i < n; i ++)
		{
			VkExtensionProperties e = ext[i];
			if (strcmp(e.extensionName, req_ext) == 0)
				x += 1;
		}
	}

	return x == N_DEVICE_EXTENSIONS;
}

SwapChainSupportDetails query_swapchain_support (VkPhysicalDevice dev)
{
	SwapChainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR (dev, surface, &details.capabilities);

	uint32_t n;
	vkGetPhysicalDeviceSurfaceFormatsKHR (dev, surface, &n, NULL);
	if (n != 0)
	{
		details.fmts = malloc (sizeof (VkSurfaceFormatKHR) * n);
		vkGetPhysicalDeviceSurfaceFormatsKHR (dev, surface, &n, details.fmts);
		details.nfmts = n;
	}

	vkGetPhysicalDeviceSurfacePresentModesKHR (dev, surface, &n, NULL);
	if (n != 0)
	{
		details.present_modes = malloc (sizeof (VkPresentModeKHR) * n);
		vkGetPhysicalDeviceSurfacePresentModesKHR (dev, surface, &n, details.present_modes);
		details.npresmodes = n;
	}

	return details;
}

static int is_device_suitable (VkPhysicalDevice dev)
{
	// Apparently this was just an example on how to do it.
	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties (dev, &props);

	VkPhysicalDeviceFeatures feats;
	vkGetPhysicalDeviceFeatures (dev, &feats);

	int gfx_family, present_support;
	int qfound = query_queue_families (dev, &gfx_family, &present_support);
	int ext_support = check_device_ext_support (dev);

	int swapchain_adequate = 1;
	if (ext_support)
	{
		SwapChainSupportDetails sup = query_swapchain_support (dev);
		swapchain_adequate = sup.fmts && sup.present_modes;
		swapchain_support_details_free (&sup);
	}

#ifdef DEBUG
	printf ("physical device: %s => ", props.deviceName);
#endif

	if (qfound && ext_support && swapchain_adequate && feats.samplerAnisotropy)
	{
#ifdef DEBUG
		printf ("good!\n");
#endif
		return 0;
	}
	else
	{
#ifdef DEBUG
		printf ("no good!\n");
#endif
		return 1;
	}
}

static void pick_physical_device ()
{
	uint32_t n;
	vkEnumeratePhysicalDevices (instance, &n, NULL);
	assert (n > 0);

	VkPhysicalDevice devices[n];
	vkEnumeratePhysicalDevices (instance, &n, devices);

	for (int i = 0; i < n; i ++)
	{
		if (is_device_suitable (devices[i]) == 0)
		{
			physical_device = devices[i];
			break;
		}
	}

	assert (physical_device != VK_NULL_HANDLE);
}

static void create_logical_device ()
{
	uint32_t gfx_family, present_support;
	query_queue_families (physical_device, &gfx_family, &present_support);

	// I find this part with queue families very confusing and static.
	// Can maybe be re-worked one day when I understand better what the hell is going on.

	const uint32_t nfamilies = gfx_family == present_support ? 1 : 2;
	float prio = 1.0f;
	uint32_t families[2] = { gfx_family, present_support };
	VkDeviceQueueCreateInfo qinfos[nfamilies];
	memset(qinfos, 0, sizeof (VkDeviceQueueCreateInfo) * nfamilies);

	for (int i = 0; i < nfamilies; i ++)
	{
		//VkDeviceQueueCreateInfo info = { 0 };
		qinfos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		qinfos[i].queueFamilyIndex = families[i];
		qinfos[i].queueCount = 1;
		qinfos[i].pQueuePriorities = &prio;
		//qinfos[i] = info;
	}

	VkPhysicalDeviceFeatures feats = { 0 };
	feats.samplerAnisotropy = VK_TRUE;

	VkDeviceCreateInfo info = { 0 };
	info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	info.pQueueCreateInfos = qinfos;
	info.queueCreateInfoCount = nfamilies;
	info.pEnabledFeatures = &feats;
	info.enabledExtensionCount = N_DEVICE_EXTENSIONS;
	info.ppEnabledExtensionNames = device_extensions;

#ifdef DEBUG
	info.enabledLayerCount = N_VALIDATION_LAYERS;
	info.ppEnabledLayerNames = validation_layers;
#else
	info.enabledLayerCount = 0;
#endif

	assert (vkCreateDevice (physical_device, &info, NULL, &device) == VK_SUCCESS);

	vkGetDeviceQueue (device, gfx_family, 0, &gfx_queue);
	vkGetDeviceQueue (device, present_support, 0, &present_queue);
}

static QueueFamilyIndices find_queue_families (VkPhysicalDevice dev)
{
	QueueFamilyIndices idx;
	queue_family_indices_init (&idx);

	uint32_t count = 0;

	vkGetPhysicalDeviceQueueFamilyProperties (dev, &count, NULL);
	VkQueueFamilyProperties families[count];
	vkGetPhysicalDeviceQueueFamilyProperties (dev, &count, families);

	int i = 0;
	for (; i < count; i ++)
	{
		VkQueueFamilyProperties *family = &families[i];
		if (family->queueFlags & VK_QUEUE_GRAPHICS_BIT)
			idx.gfx_family = i;

		VkBool32 present_support = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR (dev, i, surface, &present_support);

		if (present_support)
			idx.present_family = i;

		if (queue_family_indices_is_complete (&idx))
			break;
	}

	return idx;
}

static void create_swapchain ()
{
	SwapChainSupportDetails sup = query_swapchain_support (physical_device);

	VkSurfaceFormatKHR fmt = choose_swap_surface_format (sup.fmts, sup.nfmts);
	VkPresentModeKHR mode = choose_swap_present_mode (sup.present_modes, sup.npresmodes);
	VkExtent2D ext = choose_swap_extent (sup.capabilities);

	uint32_t imcount = sup.capabilities.minImageCount + 1;
	if (sup.capabilities.maxImageCount > 0 && imcount > sup.capabilities.maxImageCount)
		imcount = sup.capabilities.maxImageCount;

	VkSwapchainCreateInfoKHR info = { 0 };
	info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	info.surface = surface;
	info.minImageCount = imcount;
	info.imageFormat = fmt.format;
	info.imageColorSpace = fmt.colorSpace;
	info.imageExtent = ext;
	info.imageArrayLayers = 1;
	info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	QueueFamilyIndices idx = find_queue_families (physical_device);
	uint32_t qfamilyidx[] = { idx.gfx_family, idx.present_family };

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
		info.pQueueFamilyIndices = NULL; // optional
	}

	info.preTransform = sup.capabilities.currentTransform;
	info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	info.presentMode = mode;
	info.clipped = VK_TRUE;
	info.oldSwapchain = VK_NULL_HANDLE;

	assert(vkCreateSwapchainKHR (device, &info, NULL, &swapchain) == VK_SUCCESS);

	// create swapchain image handles

	assert(
		vkGetSwapchainImagesKHR (
			device,
			swapchain,
			&n_swapchain_imgs,
			NULL
		) == VK_SUCCESS
	);
	swapchain_imgs = malloc (n_swapchain_imgs * sizeof (VkImage));
	assert(
		vkGetSwapchainImagesKHR (
			device,
			swapchain,
			&n_swapchain_imgs,
			swapchain_imgs
		) == VK_SUCCESS
	);

	swapchain_img_fmt = fmt.format;
	swapchain_ext = ext;
	swapchain_support_details_free (&sup);
}

static void create_img_view
(
	VkImageView *view,
	VkImage img,
	VkFormat fmt,
	VkImageAspectFlags flags
)
{
	VkImageViewCreateInfo info = { 0 };
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
	info.subresourceRange.aspectMask = flags;

	assert (vkCreateImageView (device, &info, NULL, view) == VK_SUCCESS);
}

static void create_img_views ()
{
	n_swapchain_img_views = n_swapchain_imgs;
	swapchain_img_views = (VkImageView *) calloc (n_swapchain_img_views, sizeof (VkImageView));

	for (int i = 0; i < n_swapchain_img_views; i ++)
	{
		create_img_view (
			&swapchain_img_views[i],
			swapchain_imgs[i],
			swapchain_img_fmt,
			VK_IMAGE_ASPECT_COLOR_BIT
		);
	}
}

static VkFormat find_supported_fmt
(
	VkFormat *candidates,
	uint32_t n_candidates,
	VkImageTiling tiling,
	VkFormatFeatureFlags feats
)
{
	for (int i = 0; i < n_candidates; i ++)
	{
		VkFormat fmt = candidates[i];
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

	fprintf (stderr, "failed to find supported format");
	exit (1);
}

static VkFormat find_depth_fmt ()
{
	VkFormat candidates[] =
	{
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT
	};
	return find_supported_fmt
	(
		candidates,
		3,
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);
}

void create_render_pass ()
{
	VkSubpassDependency dep = { 0 };
	dep.srcSubpass = VK_SUBPASS_EXTERNAL;
	dep.dstSubpass = 0;
	dep.srcStageMask =
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dep.srcAccessMask = 0;
	dep.dstStageMask =
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dep.dstAccessMask =
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	VkAttachmentDescription color = { 0 };
	color.format = swapchain_img_fmt;
	color.samples = VK_SAMPLE_COUNT_1_BIT;
	color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference color_ref = { 0 };
	color_ref.attachment = 0;
	color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription depth = { 0 };
	depth.format = find_depth_fmt ();
	depth.samples = VK_SAMPLE_COUNT_1_BIT;
	depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depth_ref = { 0 };
	depth_ref.attachment = 1;
	depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = { 0 };
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_ref;
	subpass.pDepthStencilAttachment = &depth_ref;

	VkAttachmentDescription attachments[2] = { color, depth };
	VkRenderPassCreateInfo info = { 0 };
	info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	info.attachmentCount = 2;
	info.pAttachments = attachments;
	info.subpassCount = 1;
	info.pSubpasses = &subpass;
	info.dependencyCount = 1;
	info.pDependencies = &dep;

	assert (vkCreateRenderPass (device, &info, NULL, &render_pass) == VK_SUCCESS);
}

static void create_descriptor_set_layout ()
{
	VkDescriptorSetLayoutBinding ubo_layout_binding = { 0 };
	ubo_layout_binding.binding = 0;
	ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ubo_layout_binding.descriptorCount = 1;
	ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	ubo_layout_binding.pImmutableSamplers = NULL; // optional

	VkDescriptorSetLayoutBinding sampler_layout_binding = { 0 };
	sampler_layout_binding.binding = 1;
	sampler_layout_binding.descriptorCount = 1;
	sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	sampler_layout_binding.pImmutableSamplers = NULL;
	sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutBinding bindings[] =
	{
		ubo_layout_binding, sampler_layout_binding
	};
	VkDescriptorSetLayoutCreateInfo layout_info = { 0 };
	layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_info.bindingCount = 2;
	layout_info.pBindings = bindings;

	assert (
		vkCreateDescriptorSetLayout (
			device,
			&layout_info,
			NULL,
			&descriptor_set_layout
		) == VK_SUCCESS
	);
}

static VkShaderModule create_shader_module (const char *code, size_t size)
{
	VkShaderModuleCreateInfo info = { 0 };
	info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	info.codeSize = size;
	info.pCode = (const uint32_t*) code;

	VkShaderModule mod;
	assert (vkCreateShaderModule (device, &info, NULL, &mod) == VK_SUCCESS);

	return mod;
}

static VkVertexInputBindingDescription vx_binding_desc ()
{
	VkVertexInputBindingDescription desc = { 0 };
	desc.binding = 0;
	desc.stride = 3 * sizeof (float);
	desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	return desc;
}

static uint32_t vx_attrib_desc (VkVertexInputAttributeDescription **desc)
{
	*desc = calloc (3, sizeof (VkVertexInputAttributeDescription));

	// TODO
	// i am unsure about these offsets

	// position
	(*desc)[0].binding = 0;
	(*desc)[0].location = 0;
	(*desc)[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	(*desc)[0].offset = 0;

	// color
	(*desc)[1].binding = 0;
	(*desc)[1].location = 1;
	(*desc)[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	(*desc)[1].offset = 3 * sizeof (float);

	// texture coordinate
	(*desc)[2].binding = 0;
	(*desc)[2].location = 2;
	(*desc)[2].format = VK_FORMAT_R32G32_SFLOAT;
	(*desc)[2].offset = 6 * sizeof (float);

	return 3;
}

static void create_gfx_pipeline ()
{
	// read shader byte code

	char *vx_shader_code, *frag_shader_code;
	size_t vx_shader_size = read_file ("build/vert.spv", &vx_shader_code);
	size_t fg_shader_size = read_file ("build/frag.spv", &frag_shader_code);

	assert (vx_shader_size > 0);
	assert (fg_shader_size > 0);

	// create shader modules

	VkShaderModule vx_shader_mod = create_shader_module (vx_shader_code, vx_shader_size);
	VkShaderModule frag_shader_mod = create_shader_module (frag_shader_code, fg_shader_size);

	// create shader pipelines

	VkPipelineShaderStageCreateInfo vxinfo = { 0 };
	vxinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vxinfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vxinfo.module = vx_shader_mod;
	vxinfo.pName = "main";

	VkPipelineShaderStageCreateInfo fginfo = { 0 };
	fginfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fginfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fginfo.module = frag_shader_mod;
	fginfo.pName = "main";

	VkPipelineShaderStageCreateInfo stages[] = { vxinfo, fginfo };

	VkVertexInputBindingDescription binding_desc = vx_binding_desc ();
	VkVertexInputAttributeDescription* attrib_desc;
	uint32_t n_attrib_desc = vx_attrib_desc (&attrib_desc);

	VkPipelineVertexInputStateCreateInfo vxinput = { 0 };
	vxinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vxinput.vertexBindingDescriptionCount = 1;
	vxinput.pVertexBindingDescriptions = &binding_desc; // optional
	vxinput.vertexAttributeDescriptionCount = n_attrib_desc;
	vxinput.pVertexAttributeDescriptions = attrib_desc; // optional

	VkPipelineInputAssemblyStateCreateInfo inasm = { 0 };
	inasm.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inasm.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inasm.primitiveRestartEnable = VK_FALSE;

	// viewport
	VkViewport view = { 0 };
	view.x = 0.0f;
	view.y = 0.0f;
	view.width = (float) swapchain_ext.width;
	view.height = (float) swapchain_ext.height;
	view.minDepth = 0.0f;
	view.maxDepth = 1.0f;

	VkOffset2D offset = { 0, 0 };
	VkRect2D scissor = { 0 };
	scissor.offset = offset;
	scissor.extent = swapchain_ext;

	VkPipelineViewportStateCreateInfo viewstate = { 0 };
	viewstate.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewstate.viewportCount = 1;
	viewstate.pViewports = &view;
	viewstate.scissorCount = 1;
	viewstate.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer = { 0 };
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

	VkPipelineMultisampleStateCreateInfo multisampling = { 0 };
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f; // optional
	multisampling.pSampleMask = NULL; // optional
	multisampling.alphaToCoverageEnable = VK_FALSE; // optional
	multisampling.alphaToOneEnable = VK_FALSE; // optional

	VkPipelineColorBlendAttachmentState blend_att = { 0 };
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

	VkPipelineColorBlendStateCreateInfo blend = { 0 };
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

	VkPipelineDynamicStateCreateInfo dynstate = { 0 };
	dynstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynstate.dynamicStateCount = 2;
	dynstate.pDynamicStates = dynstates;

	VkPipelineLayoutCreateInfo pipeline_cinfo = { 0 };
	pipeline_cinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_cinfo.setLayoutCount = 1; // optional
	pipeline_cinfo.pSetLayouts = &descriptor_set_layout; // optional
	//pipeline_cinfo.pushConstantRangeCount = 0; // optional
	//pipeline_cinfo.pPushConstantRanges = NULL; // optional

	assert (
		vkCreatePipelineLayout (device, &pipeline_cinfo, NULL, &pipeline_layout) == VK_SUCCESS
	);

	VkPipelineDepthStencilStateCreateInfo depth_stencil = { 0 };
	depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil.depthTestEnable = VK_TRUE;
	depth_stencil.depthWriteEnable = VK_TRUE;
	depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depth_stencil.depthBoundsTestEnable = VK_FALSE;
	depth_stencil.minDepthBounds = 0.0f; // optional
	depth_stencil.maxDepthBounds = 1.0f; // optional
	depth_stencil.stencilTestEnable = VK_FALSE;
	//depth_stencil.front = { 0 }; // optional
	//depth_stencil.back = { 0 }; // optional

	VkGraphicsPipelineCreateInfo pipeinfo = { 0 };
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
	pipeinfo.pDynamicState = NULL; // optional
	pipeinfo.layout = pipeline_layout;
	pipeinfo.renderPass = render_pass;
	pipeinfo.subpass = 0;
	pipeinfo.basePipelineHandle = VK_NULL_HANDLE; // optional
	pipeinfo.basePipelineIndex = -1; // optional

	assert (
		vkCreateGraphicsPipelines (
			device,
			VK_NULL_HANDLE,
			1,
			&pipeinfo,
			NULL,
			&pipeline
		) == VK_SUCCESS
	);

	// cleanup

	vkDestroyShaderModule (device, vx_shader_mod, NULL);
	vkDestroyShaderModule (device, frag_shader_mod, NULL);
	free (vx_shader_code);
	free (frag_shader_code);
	free (attrib_desc);
}

static int init_vulkan ()
{
	create_instance ();
#ifdef DEBUG
	setup_debugger ();
#endif
	create_surface (); // TODO what about offscreen rendering?
	pick_physical_device ();
	create_logical_device ();
	create_swapchain ();
	create_img_views ();
	create_render_pass ();
	create_descriptor_set_layout ();
	create_gfx_pipeline ();

	return 0;
}

void init ()
{
	init_window ();
	init_vulkan ();
}

static void deinit_vulkan ()
{
	free (swapchain_imgs);
	free (swapchain_img_views);
}

void deinit ()
{
	deinit_vulkan ();
}

// TODO this is to be removed later when we know things work
int main ()
{
	init ();
	deinit ();
}
