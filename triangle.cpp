#include <vulkan/vulkan.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>

#define GLFW_INCLUDE_VULKAM
#include <GLFW/glfw3.h>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

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

	void init_window ()
	{
		glfwInit ();
		glfwWindowHint (GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
		win = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
	}

	void init_vulkan ()
	{
		create_instance ();
	}

	void create_instance ()
	{
		VkApplicationInfo app_info {};
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pApplicationName = "hello triangle";
		app_info.applicationVersion = VK_MAKE_VERSION (1, 0, 0);
		app_info.pEngineName = "No Engine";
		app_info.engineVersion = VK_MAKE_VERSION (1, 0, 0);
		app_info.apiVersion = VK_API_VERSION_1_0;

		uint32_t extensions_count = 0;
		const char** extensions;
		extensions = glfwGetRequiredInstanceExtensions (&extensions_count);

		VkInstanceCreateInfo create_info {};
		create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		create_info.pApplicationInfo = &app_info;
		create_info.enabledExtensionCount = extensions_count;
		create_info.ppEnabledExtensionNames = extensions;
		create_info.enabledLayerCount = 0;

		VkResult res = vkCreateInstance (&create_info, nullptr, &instance);

		if (vkCreateInstance (&create_info, nullptr, &instance) != VK_SUCCESS)
			throw std::runtime_error ("failed to create instance!");

		vkEnumerateInstanceExtensionProperties (nullptr, &extensions_count, nullptr);
		std::vector<VkExtensionProperties> vk_extensions (extensions_count);
		vkEnumerateInstanceExtensionProperties (nullptr, &extensions_count, vk_extensions.data ());

		std::cout << "available extensions:" << std::endl;
		for (const auto& extension : vk_extensions)
		{
			std::cout << "\t" << extension.extensionName << std::endl;
		}
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
