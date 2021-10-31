#include <vulkan/vulkan.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

static const uint32_t WIDTH = 800;
static const uint32_t HEIGHT = 600;
static const int MAX_FRAMES_IN_FLIGHT = 2;

/**
 *		read_file
 *
 * Helper function to read a file such as shader source.
 */
static int read_file (const char *fname, char **data)
{
	FILE* fp = fopen (fname, "rb");

	// get file size
	fseek (fp, 0, SEEK_END);
	size_t fsize = ftell (fp);
	rewind (fp);

	// read
	int result;
	*data = (char *) malloc (fsize);
	if ((result = fread (data, 1, fsize - 1, fp)) != fsize - 1)
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
static const const char *device_extensions[1] =
{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

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
	PFN_vkCreateDebugUtilsMessengerEXT fn =
		(PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr (instance, "vkCreateDebugUtilsMessengerEXT");
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
	PFN_vkDestroyDebugUtilsMessengerEXT fn =
		(PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr (instance, "vkDestroyDebugUtilsMessengerEXT");
	if (fn) fn (instance, messenger, allocator);
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
static VkSwapchainKHR swap_chain;
static VkImage *swapchain_images;
static VkFramebuffer *swapchain_framebufs;
static VkFormat swapchain_img_fmt;
static VkExtent2D swapchain_ext;
static VkImageView *swapchain_image_views;

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
void init_window ()
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
		printf ("\t%s =>", ext[i]);
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
	create_info.flags = 0;

	// validation layers

#ifdef DEBUG
	VkDebugUtilsMessengerCreateInfoEXT debug_info;
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

	free (ext);
	free ((void *) vk_ext);
}

static void init_vulkan ()
{
	create_instance ();
#ifdef DEBUG
	setup_debugger ();
#endif
}

void init ()
{
	init_window ();
	init_vulkan ();
}

// TODO this is to be removed later when we know things work
int main ()
{
	init ();
}
