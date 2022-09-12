#pragma once
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include <vector>
#include <iostream>
#include "PhysicalDevice.h"
#include "core.h"
namespace OVK
{
	class Instance
	{
	public:
		Instance			 (const Instance&) = delete;
		Instance			 (Instance&&)      = delete;
		Instance& operator = (const Instance&) = delete;
		Instance& operator = (Instance&&)	   = delete;
	public:
		// Constructors / Destructors
		Instance();
		~Instance(); 
	public:
		const std::vector<const char*> 		  GetRequiredExtensions(bool isValLayersSupported);
		const VkInstance&					  GetVkInstance() const	 { return m_Instance; }
		std::vector<PhysicalDevice>			  GetVKPhysicalDevices() { return m_PhysicalDevices; }
 	private:
		void								  PrintAvailableExtensions();
		static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
											  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
		bool							      CheckValidationLayerSupport();
		void							      SetupDebugMessenger();
		VkResult						      CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
											  const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
		void							      DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);
	private:
		VkInstance						      m_Instance		 = VK_NULL_HANDLE;
		VkDebugUtilsMessengerEXT		      m_DebugMessenger	 = VK_NULL_HANDLE;
		const std::vector<const char*>		  m_ValidationLayers = {"VK_LAYER_LUNARG_monitor"};
		std::vector<PhysicalDevice>			  m_PhysicalDevices;
	};
}
