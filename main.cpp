#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <iostream>

int main ()
{
	glfwInit ();

	glfwWindowHint (GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* win = glfwCreateWindow (800, 600, "Vulkan", nullptr, nullptr);

	uint32_t extension_count = 0;
	vkEnumerateInstanceExtensionProperties (nullptr, &extension_count, nullptr);

	std::cout << extension_count << " extensions supported" << std::endl;

	glm::mat4 matrix;
	glm::vec4 vec;
	auto test = matrix * vec;

	while (!glfwWindowShouldClose (win))
	{
		glfwPollEvents ();
	}

	glfwDestroyWindow (win);
	glfwTerminate ();

	return 0;
}
