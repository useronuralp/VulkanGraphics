#pragma once
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include <vector>
#include <iostream>
#include "PhysicalDevice.h"
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

		struct CreateInfo
		{
			ApplicationInfo		     AppInfo;
			VkInstanceCreateFlags    Flags = 0;
			bool                     EnableValidation = false;
			std::vector<const char*> ExtensionNames;
			VkAllocationCallbacks*   pVkAllocator = nullptr;
		};

		Instance(CreateInfo& createInfo, bool isVerbose = true);
		~Instance(); 
	public:
		static std::vector<const char*>		  GetValidationLayers()			{ return m_ValidationLayers;	    }
		const VkInstance&					  GetVkInstance()				{ return m_Instance;				}
		bool								  AreValidationLayersEnabled()	{ return m_EnableValidationLayers;  }
		std::vector<PhysicalDevice>			  GetVKPhysicalDevices()		{ return m_PhysicalDevices;			}
		void								  PrintAvailableExtensions();
 	private:
		static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
											  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
		std::vector<const char*>		      GetRequiredExtensions();
		bool							      CheckValidationLayerSupport();
		void							      SetupDebugMessenger();
		VkResult						      CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
											  const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
		void							      DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);
	private:
		VkInstance						      m_Instance = VK_NULL_HANDLE;
		bool							      m_EnableValidationLayers = false;
		static std::vector<const char*>		  m_ValidationLayers;
		bool							      m_IsVerbose;
		VkDebugUtilsMessengerEXT		      m_DebugMessenger;
		std::vector<PhysicalDevice>			  m_PhysicalDevices;
	};
}
