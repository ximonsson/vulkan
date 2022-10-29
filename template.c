#include <vulkan/vulkan.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define GLFW_INCLUDE_VULKAM
#include <GLFW/glfw3.h>


/**
 *		Define vertices.
 *
 * these should be changed for application specific.
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
	if (!fp)
	{
		fprintf (stderr, "could not find file %s!\n", fname);
		exit (1);
	}

	// get file size
	fseek (fp, 0, SEEK_END);
	size_t fsize = ftell (fp);
	rewind (fp);

	// read
	size_t result;
	*data = (char *) malloc (fsize);
	if ((result = fread (*data, 1, fsize, fp)) != fsize)
	{
		fprintf (stderr, "failed to read file contents!\n");
		exit (1);
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
static VkCommandBuffer *cmdbufs;
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

	//
	// TODO remove hard codings towards shader source
	//

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

static void create_cmd_pool ()
{
	QueueFamilyIndices idx = find_queue_families (physical_device);

	VkCommandPoolCreateInfo info = { 0 };
	info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	info.queueFamilyIndex = idx.gfx_family;
	//info.flags = 0; // optional

	assert (vkCreateCommandPool (device, &info, NULL, &cmdpool) == VK_SUCCESS);
}

static uint32_t find_mem_type (uint32_t type_filter, VkMemoryPropertyFlags props)
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

	fprintf (stderr, "failed to find suitable memory type!");
	exit (1);
}

static void create_img
(
	uint32_t w,
	uint32_t h,
	VkFormat fmt,
	VkImageTiling tiling,
	VkImageUsageFlags usage,
	VkMemoryPropertyFlags props,
	VkImage *img,
	VkDeviceMemory *img_mem
)
{
	VkImageCreateInfo img_info = { 0 };
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

	assert (vkCreateImage (device, &img_info, NULL, img) == VK_SUCCESS);

	VkMemoryRequirements memreq;
	vkGetImageMemoryRequirements (device, *img, &memreq);

	VkMemoryAllocateInfo alloc_info = { 0 };
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = memreq.size;
	alloc_info.memoryTypeIndex = find_mem_type (memreq.memoryTypeBits, props);

	assert (vkAllocateMemory (device, &alloc_info, NULL, img_mem) == VK_SUCCESS);

	vkBindImageMemory (device, *img, *img_mem, 0);
}

static VkCommandBuffer begin_single_time_cmds ()
{
	VkCommandBufferAllocateInfo alloc_info = { 0 };
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandPool = cmdpool;
	alloc_info.commandBufferCount = 1;

	VkCommandBuffer cmdbuf;
	vkAllocateCommandBuffers (device, &alloc_info, &cmdbuf);

	VkCommandBufferBeginInfo begin_info = { 0 };
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer (cmdbuf, &begin_info);

	return cmdbuf;
}

static void end_single_time_cmds (VkCommandBuffer cmdbuf)
{
	vkEndCommandBuffer (cmdbuf);

	VkSubmitInfo submit_info = { 0 };
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &cmdbuf;

	vkQueueSubmit (gfx_queue, 1, &submit_info, VK_NULL_HANDLE);
	vkQueueWaitIdle (gfx_queue);

	vkFreeCommandBuffers (device, cmdpool, 1, &cmdbuf);
}

#define HAS_STENCIL_COMPONENT(fmt) \
	( fmt == VK_FORMAT_D32_SFLOAT_S8_UINT || fmt == VK_FORMAT_D24_UNORM_S8_UINT )

static void transition_img_layout
(
	VkImage img,
	VkFormat fmt,
	VkImageLayout old_layout,
	VkImageLayout new_layout
)
{
	VkCommandBuffer cmdbuf = begin_single_time_cmds ();

	VkImageMemoryBarrier barrier = { 0 };
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

		if (HAS_STENCIL_COMPONENT (fmt))
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
		fprintf (stderr, "unsupported layout transition");
		exit (1);
	}

	vkCmdPipelineBarrier (cmdbuf, src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1, &barrier);

	end_single_time_cmds (cmdbuf);
}

static void create_depth_buffer ()
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
		&depth_img,
		&depth_img_mem
	);
	create_img_view (&depth_img_view, depth_img, depth_fmt, VK_IMAGE_ASPECT_DEPTH_BIT);
	transition_img_layout
	(
		depth_img,
		depth_fmt,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	);
}

static void create_framebuffers ()
{
	swapchain_framebufs = calloc (n_swapchain_img_views, sizeof (VkFramebuffer));

	for (size_t i = 0; i < n_swapchain_img_views; i ++)
	{
		VkImageView attachments[] = { swapchain_img_views[i], depth_img_view };

		VkFramebufferCreateInfo info = { 0 };
		info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		info.renderPass = render_pass;
		info.attachmentCount = 2;
		info.pAttachments = attachments;
		info.width = swapchain_ext.width;
		info.height = swapchain_ext.height;
		info.layers = 1;

		assert (vkCreateFramebuffer (device, &info, NULL, &swapchain_framebufs[i]) == VK_SUCCESS);
	}
}

static void create_buffer
(
	VkDeviceSize size,
	VkBufferUsageFlags usage,
	VkMemoryPropertyFlags props,
	VkBuffer *buf,
	VkDeviceMemory *mem
)
{
	VkBufferCreateInfo info = { 0 };
	info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	info.size = size;
	info.usage = usage;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	assert (vkCreateBuffer (device, &info, NULL, buf) == VK_SUCCESS);

	VkMemoryRequirements mem_req;
	vkGetBufferMemoryRequirements (device, *buf, &mem_req);

	VkMemoryAllocateInfo alloc_info = { 0 };
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = find_mem_type
	(
		mem_req.memoryTypeBits,
		props
	);

	assert (vkAllocateMemory (device, &alloc_info, NULL, mem) == VK_SUCCESS);

	vkBindBufferMemory (device, *buf, *mem, 0);
}

static void cp_buf_img (VkBuffer buf, VkImage img, uint32_t w, uint32_t h)
{
	VkCommandBuffer cmdbuf = begin_single_time_cmds ();

	VkBufferImageCopy region = { 0 };

	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;

	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;

	VkOffset3D offset = { 0, 0, 0 };
	VkExtent3D ext = { w, h, 1 };
	region.imageOffset = offset;
	region.imageExtent = ext;

	vkCmdCopyBufferToImage (cmdbuf, buf, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	end_single_time_cmds (cmdbuf);
}

static void create_tex_img ()
{
	int w, h, c;

	stbi_uc *pix = stbi_load ("tutorial/textures/texture.jpg", &w, &h, &c, STBI_rgb_alpha);
	VkDeviceSize size = w * h * 4;

	assert (pix);

	VkBuffer staging_buf;
	VkDeviceMemory staging_buf_mem;

	create_buffer
	(
		size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&staging_buf,
		&staging_buf_mem
	);

	void *data;
	vkMapMemory (device, staging_buf_mem, 0, size, 0, &data);
	memcpy (data, pix, size);
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
		&tex_img,
		&tex_img_mem
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
		w,
		h
	);

	transition_img_layout
	(
		tex_img,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	);

	vkDestroyBuffer (device, staging_buf, NULL);
	vkFreeMemory (device, staging_buf_mem, NULL);
}

static void create_tex_img_view ()
{
	create_img_view (
		&tex_img_view, tex_img, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT
	);
}

static void create_tex_sampler ()
{
	VkSamplerCreateInfo info = { 0 };
	info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	info.magFilter = VK_FILTER_LINEAR;
	info.minFilter = VK_FILTER_LINEAR;
	info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	info.anisotropyEnable = VK_TRUE;

	VkPhysicalDeviceProperties props = { 0 };
	vkGetPhysicalDeviceProperties (physical_device, &props);

	info.maxAnisotropy = props.limits.maxSamplerAnisotropy;
	info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	info.unnormalizedCoordinates = VK_FALSE;
	info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	info.mipLodBias = 0.0f;
	info.minLod = 0.0f;
	info.maxLod = 0.0f;

	assert (vkCreateSampler (device, &info, NULL, &tex_sampler) == VK_SUCCESS);
}

static void copy_buf (VkBuffer src, VkBuffer dst, VkDeviceSize size)
{
	VkCommandBuffer cmdbuf = begin_single_time_cmds ();

	VkBufferCopy copy = { 0 };
	copy.srcOffset = 0; // optional
	copy.dstOffset = 0; // optional
	copy.size = size;
	vkCmdCopyBuffer (cmdbuf, src, dst, 1, &copy);

	end_single_time_cmds (cmdbuf);
}

static void create_vx_buf ()
{
	VkDeviceSize size = sizeof (float) * 8 * 3;

	VkBuffer staging_buf;
	VkDeviceMemory staging_buf_mem;
	create_buffer
	(
		size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&staging_buf,
		&staging_buf_mem
	);

	void *data;
	vkMapMemory (device, staging_buf_mem, 0, size, 0, &data);
	memcpy (data, vertices, (size_t) size);
	vkUnmapMemory (device, staging_buf_mem);

	create_buffer
	(
		size,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&vx_buf,
		&vx_buf_mem
	);

	copy_buf (staging_buf, vx_buf, size);

	vkDestroyBuffer (device, staging_buf, NULL);
	vkFreeMemory (device, staging_buf_mem, NULL);
}

static void create_idx_buf ()
{
	VkDeviceSize size = sizeof (uint16_t) * 12;

	VkBuffer staging_buf;
	VkDeviceMemory staging_buf_mem;
	create_buffer
	(
		size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&staging_buf,
		&staging_buf_mem
	);

	void *data;
	vkMapMemory (device, staging_buf_mem, 0, size, 0, &data);
	memcpy (data, indices, (size_t) size);
	vkUnmapMemory (device, staging_buf_mem);

	create_buffer
	(
		size,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&idx_buf,
		&idx_buf_mem
	);

	copy_buf (staging_buf, idx_buf, size);

	vkDestroyBuffer (device, staging_buf, NULL);
	vkFreeMemory (device, staging_buf_mem, NULL);
}

static void create_uniform_buf ()
{
	// TODO
	// this is 3 4x4 matrices - should it be like this?
	VkDeviceSize size = sizeof (float) * 4 * 4 * 3;

	unif_buf = calloc (n_swapchain_imgs, sizeof (VkBuffer));
	unif_buf_mem = calloc (n_swapchain_imgs, sizeof (VkDeviceMemory));

	for (size_t i = 0; i < n_swapchain_imgs; i ++)
		create_buffer
		(
			size,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&unif_buf[i],
			&unif_buf_mem[i]
		);
}

static void create_descriptor_pool ()
{
	VkDescriptorPoolSize pool_sizes[2] = { 0 };

	pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	pool_sizes[0].descriptorCount = n_swapchain_imgs;
	pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pool_sizes[1].descriptorCount = n_swapchain_imgs;

	VkDescriptorPoolCreateInfo pool_info = { 0 };
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.poolSizeCount = 2;
	pool_info.pPoolSizes = pool_sizes;
	pool_info.maxSets = n_swapchain_imgs;

	assert (vkCreateDescriptorPool (device, &pool_info, NULL, &descriptor_pool) == VK_SUCCESS);
}

static void create_descriptor_sets ()
{
	//std::vector<VkDescriptorSetLayout> layouts (swapchain_images.size (), descriptor_set_layout);
	VkDescriptorSetLayout layouts[n_swapchain_imgs];

	VkDescriptorSetAllocateInfo alloc_info = { 0 };
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = descriptor_pool;
	alloc_info.descriptorSetCount = n_swapchain_imgs;
	alloc_info.pSetLayouts = layouts;

	//descriptor_sets.resize (n_swapchain_imgs);
	descriptor_sets = malloc (n_swapchain_imgs * sizeof (VkDescriptorSet));
	assert (vkAllocateDescriptorSets (device, &alloc_info, descriptor_sets) == VK_SUCCESS);

	for (size_t i = 0; i < n_swapchain_imgs; i ++)
	{
		VkDescriptorBufferInfo buffer_info = { 0 };
		buffer_info.buffer = unif_buf[i];
		buffer_info.offset = 0;
		// TODO what should this be?
		//buffer_info.range = sizeof (UniformBufferObject);
		buffer_info.range = 0;

		VkDescriptorImageInfo img_info = { 0 };
		img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		img_info.imageView = tex_img_view;
		img_info.sampler = tex_sampler;

		VkWriteDescriptorSet writes[2] = { 0 };

		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet = descriptor_sets[i];
		writes[0].dstBinding = 0;
		writes[0].dstArrayElement = 0;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[0].descriptorCount = 1;
		writes[0].pBufferInfo = &buffer_info;
		writes[0].pImageInfo = NULL; // optional
		writes[0].pTexelBufferView = NULL; // optional

		writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].dstSet = descriptor_sets[i];
		writes[1].dstBinding = 1;
		writes[1].dstArrayElement = 0;
		writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[1].descriptorCount = 1;
		writes[1].pBufferInfo = NULL;
		writes[1].pImageInfo = &img_info; // optional
		writes[1].pTexelBufferView = NULL; // optional

		vkUpdateDescriptorSets (device, 2, writes, 0, NULL);
	}
}

static void create_cmdbufs ()
{
	uint32_t n_cmdbufs = n_swapchain_img_views;
	cmdbufs = calloc (n_cmdbufs, sizeof (VkCommandBuffer));

	VkCommandBufferAllocateInfo allocinfo = { 0 };
	allocinfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocinfo.commandPool = cmdpool;
	allocinfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocinfo.commandBufferCount = n_cmdbufs;

	assert (vkAllocateCommandBuffers (device, &allocinfo, cmdbufs) == VK_SUCCESS);

	for (size_t i = 0; i < n_cmdbufs; i ++)
	{
		VkCommandBufferBeginInfo info = { 0 };
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		//info.flags = 0; // optional
		//info.pInheritanceInfo = nullptr; // optional

		assert (vkBeginCommandBuffer (cmdbufs[i], &info) == VK_SUCCESS);

		//std::array<VkClearValue, 2> clear_values = { 0 };
		VkClearColorValue clclrv = { 0.0f, 0.0f, 0.0f, 1.0f };
		VkClearDepthStencilValue clstencilv = { 1.0f, 0.f };
		VkClearValue clear_values[2] = { 0 };
		clear_values[0].color = clclrv;
		clear_values[1].depthStencil = clstencilv;

		VkOffset2D offset = { 0, 0 };
		VkRenderPassBeginInfo render_pass_info = { 0 };
		render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		render_pass_info.renderPass = render_pass;
		render_pass_info.framebuffer = swapchain_framebufs[i];
		render_pass_info.renderArea.offset = offset;
		render_pass_info.renderArea.extent = swapchain_ext;
		render_pass_info.clearValueCount = 2;
		render_pass_info.pClearValues = clear_values;

		vkCmdBeginRenderPass (cmdbufs[i], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline (cmdbufs[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		VkBuffer vx_bufs[] = { vx_buf };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers (cmdbufs[i], 0, 1, vx_bufs, offsets);

		vkCmdBindIndexBuffer (cmdbufs[i], idx_buf, 0, VK_INDEX_TYPE_UINT16);

		vkCmdBindDescriptorSets
		(
			cmdbufs[i],
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipeline_layout,
			0,
			1,
			&descriptor_sets[i],
			0,
			NULL
		);
		// TODO solve this hard coding for number of indices
		vkCmdDrawIndexed (cmdbufs[i], 12, 1, 0, 0, 0);  // number of indices = 12
		vkCmdEndRenderPass (cmdbufs[i]);

		assert (vkEndCommandBuffer (cmdbufs[i]) == VK_SUCCESS);
	}
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
	create_cmd_pool ();
	create_depth_buffer ();
	create_framebuffers ();
	create_tex_img ();
	create_tex_img_view ();
	create_tex_sampler ();
	create_vx_buf ();
	create_idx_buf ();
	create_uniform_buf ();
	create_descriptor_pool ();
	create_descriptor_sets ();
	create_cmdbufs ();
	//create_sync ();

	return 0;
}

void init ()
{
	init_window ();
	init_vulkan ();
}

static void deinit_vulkan ()
{
	free (unif_buf);
	free (unif_buf_mem);
	free (swapchain_imgs);
	free (swapchain_img_views);
	free (swapchain_framebufs);
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
