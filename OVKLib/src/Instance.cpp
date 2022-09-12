#include "Instance.h"
#include "Utils.h"
namespace Anor
{
    // A callback that will print error messages when validation layers are enabled.
    VKAPI_ATTR VkBool32 VKAPI_CALL Instance::DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData)
    {
        std::cerr << "!!! VALIDATION LAYER !!!: " << pCallbackData->pMessage << std::endl;
        return VK_FALSE;
    }
    // A function pointer caller that creates a DebugMessengar object.
    VkResult Instance::CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
    {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr)
        {
            return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
        }
        else
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }
    // A function pointer caller that destroys a DebugMessengar object.
    void Instance::DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
    {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr)
        {
            func(instance, debugMessenger, pAllocator);
        }
    }
	Instance::Instance()
	{
        PrintAvailableExtensions();

        bool validationLayersSupported = CheckValidationLayerSupport();

        if (!validationLayersSupported)
        {
            std::cerr << "Validation layers requsted, but it is not supported..." << std::endl; 
            std::cerr << "Continuing without enabling debug extensions." << std::endl;
        }
        
        // Optional but good to have.
        VkApplicationInfo AI{};
        AI.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        AI.pApplicationName   = "Vulkan Graphics";
        AI.pEngineName        = "No Engine";
        AI.pNext              = nullptr;
        AI.apiVersion         = VK_API_VERSION_1_0;
        AI.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        AI.engineVersion      = VK_MAKE_VERSION(1, 0, 0);

        // Create Info Configuration.
        VkInstanceCreateInfo           vkCreateInfo{};
        vkCreateInfo.sType             = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        vkCreateInfo.pApplicationInfo  = &AI;

        std::vector<const char*> requiredExtensions     = GetRequiredExtensions(validationLayersSupported);
        uint32_t                 requiredExtensionCount = requiredExtensions.size();

        vkCreateInfo.enabledExtensionCount     = requiredExtensionCount;
        vkCreateInfo.ppEnabledExtensionNames   = requiredExtensions.data();

        if (validationLayersSupported) {
            VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
            vkCreateInfo.enabledLayerCount     = static_cast<uint32_t>(m_ValidationLayers.size());
            vkCreateInfo.ppEnabledLayerNames   = m_ValidationLayers.data();

            Utils::PopulateDebugMessengerCreateInfo(debugCreateInfo, DebugCallback);
            vkCreateInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
        }
        else
        {
            vkCreateInfo.enabledLayerCount = 0;
            vkCreateInfo.pNext             = nullptr;
        }

        ASSERT(vkCreateInstance(&vkCreateInfo, nullptr, &m_Instance) == VK_SUCCESS, "Failed to create instance.");

        std::cout << "Instance has been created." << std::endl;

        // Create the messenger handle here.
        if (validationLayersSupported)
        {
            SetupDebugMessenger();
        }

        // Fetch availabe physical devices and store.
        uint32_t deviceCount = 0;
        std::vector<VkPhysicalDevice> VKdevices;
        vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);

        VKdevices.resize(deviceCount);

        vkEnumeratePhysicalDevices(m_Instance, &deviceCount, VKdevices.data());
        for (const auto& device : VKdevices)
        {
            PhysicalDevice ANORdevice(m_Instance, device);
            m_PhysicalDevices.push_back(ANORdevice);
        }
	}
    Instance::~Instance()
    {
        if (m_DebugMessenger)
        {
            DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
        }
        vkDestroyInstance(m_Instance, nullptr);
    }
    void Instance::PrintAvailableExtensions()
    {
        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());
        std::cout << "Supported Instance Extensions: \n";
        for (const auto& extension : availableExtensions)
        {
            std::cout << '\t' << extension.extensionName << '\n';
        }
    }
    const std::vector<const char*> Instance::GetRequiredExtensions(bool isValLayersSupported)
    {
        const char** glfwExtensions;
        uint32_t     extensionCount = 0;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&extensionCount);
        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + extensionCount);

        if (isValLayersSupported)
        {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        //extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
        std::cout << "Required Instance Extensions: \n";
        for (const auto& extension : extensions)
        {
            std::cout << '\t' << extension << std::endl;
        }

        return extensions;
    }
    bool Instance::CheckValidationLayerSupport()
    {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        std::cout << "Supported Layers: \n";
        for (const auto& layer : availableLayers)
            std::cout << '\t' << layer.layerName << std::endl;

        for (const char* layerName : m_ValidationLayers)
        {
            bool layerFound = false;
            for (const auto& layerProperties : availableLayers)
            {
                if (strcmp(layerName, layerProperties.layerName) == 0)
                {
                    layerFound = true;
                    break;
                }
            }
            if (!layerFound)
                return false;
        }
        return true;
    }
    void Instance::SetupDebugMessenger()
    {
        VkDebugUtilsMessengerCreateInfoEXT createInfo{};
        Utils::PopulateDebugMessengerCreateInfo(createInfo, DebugCallback);
        ASSERT(CreateDebugUtilsMessengerEXT(m_Instance, &createInfo, nullptr, &m_DebugMessenger) == VK_SUCCESS, "Failed to create the Debug Messenger. Tip: Maybe try building in release mode.");
    }
}