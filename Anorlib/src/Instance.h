#pragma once
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include <vector>
#include <iostream>
#include "PhysicalDevice.h"
#include "core.h"
namespace Anor
{
	class Instance
	{
	public:
		Instance			 (const Instance&) = delete;
		Instance			 (Instance&&)      = delete;
		Instance& operator = (const Instance&) = delete;
		Instance& operator = (Instance&&)	   = delete;

		struct ApplicationInfo
		{
			uint32_t	EngineVersion	   = UINT32_MAX;
			uint32_t	ApplicationVersion = UINT32_MAX;
			uint32_t	APIVersion		   = UINT32_MAX;
			const char* EngineName		   = "Anor Engine";
			const char* ApplicationName    = "Nameless Application";

			bool IsEmpty()
			{
				if (EngineVersion == UINT32_MAX && ApplicationVersion == UINT32_MAX && APIVersion == UINT32_MAX && EngineName == "Anor Engine" && ApplicationName == "Nameless Application")
					return true;
				else
					return false;
			}
		};
		Instance();
		~Instance(); 
	public:
		const VkInstance&					  GetVkInstance() const	{ return m_Instance;				}
		std::vector<PhysicalDevice>			  GetVKPhysicalDevices()		{ return m_PhysicalDevices;			}
 	private:
		void								  PrintAvailableExtensions();
		static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
											  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
		const std::vector<const char*> 		  GetRequiredExtensions(bool isValLayersSupported);
		bool							      CheckValidationLayerSupport();
		void							      SetupDebugMessenger();
		VkResult						      CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
											  const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
		void							      DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);
	private:
		VkInstance						      m_Instance = VK_NULL_HANDLE;
		VkDebugUtilsMessengerEXT		      m_DebugMessenger = VK_NULL_HANDLE;
		const std::vector<const char*>		  m_ValidationLayers = { "VK_LAYER_KHRONOS_validation" };
		std::vector<PhysicalDevice>			  m_PhysicalDevices;
	};
}
