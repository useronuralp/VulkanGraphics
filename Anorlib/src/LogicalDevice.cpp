#include "LogicalDevice.h"
#include "PhysicalDevice.h"
#include "Instance.h"
#include "Window.h"
#include "Surface.h"
namespace Anor
{
	LogicalDevice::LogicalDevice(CreateInfo& createInfo, std::vector<QueueCreateInfo>& queueCreateInfos)
        :m_PhysicalDevice(createInfo.pPhysicalDevice), m_Window(createInfo.pWindow), m_Surface(createInfo.pSurface),
        m_Extensions(createInfo.ppEnabledExtensionNames), m_Layers(createInfo.pEnabledLayerNames)
	{
        std::vector<QueueFamily> queueFamilies = m_PhysicalDevice->GetQueueFamilies();
        VkBool32 supported = false;
        std::vector<VkDeviceQueueCreateInfo> deviceQueueCreateInfos;

        for (const auto& qci : queueCreateInfos)
        {
            if (qci.SearchForPresentSupport)
            {
                supported = m_PhysicalDevice->CheckPresentSupport(qci.QueueFamilyIndex, m_Surface->GetVKSurface());
                if (!supported)
                {
                    std::cerr << "The queue family you requested doesn't support present operations!!" << std::endl;
                    __debugbreak();
                }
            }
            VkDeviceQueueCreateInfo QCI{};
            QCI.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            QCI.queueFamilyIndex = qci.QueueFamilyIndex;
            QCI.queueCount = qci.QueueCount;
            QCI.pQueuePriorities = qci.pQueuePriorities;

            deviceQueueCreateInfos.push_back(QCI);
        }

        VkPhysicalDeviceFeatures deviceFeatures{};
        if (m_PhysicalDevice->GetVKFeatures().samplerAnisotropy)
        {
            // Enable anisotrophy here.
            deviceFeatures.samplerAnisotropy = true;
        }
        else
        {
            std::cerr << "Anisothrophy is not supported on your card." << std::endl;
            __debugbreak();
        }

        VkPhysicalDeviceFeatures pDeviceFeatures[] =
        {
            deviceFeatures
        };

        VkDeviceCreateInfo CI {};
        CI.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        CI.queueCreateInfoCount = deviceQueueCreateInfos.size();
        CI.pQueueCreateInfos = deviceQueueCreateInfos.data(); // Multiple create infos will come here FIX.
        CI.pEnabledFeatures = createInfo.pEnabledFeatures.data();
        CI.enabledExtensionCount = m_Extensions.size();
        CI.ppEnabledExtensionNames = m_Extensions.data();
        CI.pEnabledFeatures = pDeviceFeatures;

        if (m_Layers.size() == 0)
        {
            CI.enabledLayerCount = 0;
        }
        else if (createInfo.pEnabledLayerNames.size() > 0)
        {
            CI.ppEnabledLayerNames = m_Layers.data();
            CI.enabledLayerCount = m_Layers.size();
        }


        if (vkCreateDevice(m_PhysicalDevice->GetVKPhysicalDevice(), &CI, nullptr, &m_Device) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create logical device!");
        }
        else
        {
            std::cout << "Logical device has been created." << std::endl;
        }

        for (const auto& queueCreateInfo : queueCreateInfos)
        {
            VkQueue queue;
            vkGetDeviceQueue(m_Device, queueCreateInfo.QueueFamilyIndex, 0, &queue);
            if (queueCreateInfo.QueueType & VK_QUEUE_GRAPHICS_BIT)
            {
                m_GraphicsQueue = queue;
            }
        }
	}

	Anor::LogicalDevice::~LogicalDevice()
	{
        vkDestroyDevice(m_Device, nullptr);
	}

}