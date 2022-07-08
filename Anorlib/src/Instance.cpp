#include "Instance.h"
#include "Utils.h"
namespace Anor
{
    VKAPI_ATTR VkBool32 VKAPI_CALL Instance::DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData)
    {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
        return VK_FALSE;
    }
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
    void Instance::DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
    {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr)
        {
            func(instance, debugMessenger, pAllocator);
        }
    }
    // Validation Layers are here.
    std::vector<const char*> Instance::m_ValidationLayers =
    { 
        "VK_LAYER_KHRONOS_validation"
    };

	Instance::Instance(CreateInfo& createInfo, bool isVerbose)
        :m_IsVerbose(isVerbose)
	{
        if (m_IsVerbose)
        {
            PrintAvailableExtensions();
        }

        if(createInfo.EnableValidation)
        {
            m_EnableValidationLayers = true;
        }
        bool validationLayersSupported = CheckValidationLayerSupport();
        validationLayersSupported = false;
        if (m_EnableValidationLayers && !validationLayersSupported)
        {
            std::cerr << "Validation layers requsted, but one of the layers is not supported!" << std::endl; 
            std::cerr << "Continuing without enabling debug extensions." << std::endl;
            //__debugbreak();
        }
        
        VkApplicationInfo AI{};
        if (createInfo.AppInfo.IsEmpty())
        {
            AI.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            AI.pApplicationName   = createInfo.AppInfo.ApplicationName;
            AI.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
            AI.pEngineName        = createInfo.AppInfo.EngineName;
            AI.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
            AI.apiVersion         = VK_API_VERSION_1_0;
            AI.pNext              = nullptr;
        }
        else
        {
            AI.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            AI.pApplicationName   = createInfo.AppInfo.ApplicationName;
            AI.applicationVersion = VK_MAKE_VERSION(createInfo.AppInfo.EngineVersion, 0, 0);
            AI.pEngineName        = createInfo.AppInfo.EngineName;
            AI.engineVersion      = VK_MAKE_VERSION(createInfo.AppInfo.EngineVersion, 0, 0);
            AI.apiVersion         = VK_API_VERSION_1_0;
            AI.pNext              = nullptr;
        }

        VkInstanceCreateInfo           vkCreateInfo{};
        vkCreateInfo.sType             = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        vkCreateInfo.pApplicationInfo  = &AI;

        std::vector<const char*> requiredExtensions     = GetRequiredExtensions();
        uint32_t                 requiredExtensionCount = requiredExtensions.size();

        for (const auto& userLayer : createInfo.ExtensionNames)
        {
            requiredExtensions.push_back(userLayer); // Add user layers to the "requiredExtensions" array.
        }

        vkCreateInfo.enabledExtensionCount     = requiredExtensionCount + createInfo.ExtensionNames.size();
        vkCreateInfo.ppEnabledExtensionNames   = requiredExtensions.data();

        if (m_EnableValidationLayers && validationLayersSupported) {
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

        if (vkCreateInstance(&vkCreateInfo, nullptr, &m_Instance) != VK_SUCCESS)
        {
            std::cerr << "Failed to create instance!" << std::endl;
            __debugbreak();
        }

        // Create the messenger handle here.
        if (m_EnableValidationLayers)
        {
            SetupDebugMessenger();
        }

        // Setup physical devices here.
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
        if (m_EnableValidationLayers)
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
        std::cout << "Available extensions: \n";
        for (const auto& extension : availableExtensions)
        {
            std::cout << '\t' << extension.extensionName << '\n';
        }
    }
    std::vector<const char*> Instance::GetRequiredExtensions()
    {
        const char** glfwExtensions;
        uint32_t     extensionCount = 0;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&extensionCount);
        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + extensionCount);

        if (m_EnableValidationLayers)
        {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        if (m_IsVerbose)
        {
            std::cout << "Required Instance Extensions: \n";
            for (const auto& extension : extensions)
            {
                std::cout << '\t' << extension << std::endl;
            }
        }
        return extensions;
    }
    bool Instance::CheckValidationLayerSupport()
    {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        if (m_IsVerbose)
        {
            std::cout << "Supported layers: \n";
            for (const auto& layer : availableLayers)
                std::cout << '\t' << layer.layerName << std::endl;
        }

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

        if (CreateDebugUtilsMessengerEXT(m_Instance, &createInfo, nullptr, &m_DebugMessenger) != VK_SUCCESS)
        {
            std::cerr << "Failed to set up debug messenger!" << std::endl;
            __debugbreak();
        }
    }
}