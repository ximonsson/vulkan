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
	free (ext_);
#endif

	return ext;
}

/**
 * Get supported extensions.
 */
static const VkExtensionProperties *get_supported_extensions (uint32_t *n)
{
	vkEnumerateInstanceExtensionProperties(NULL, n, NULL);
	VkExtensionProperties *ext = malloc ((*n) * sizeof (VkExtensionProperties));
	vkEnumerateInstanceExtensionProperties(NULL, n, ext);
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
 *	VARIABLES ---------------------------------------------------------------------------------------------------------
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

