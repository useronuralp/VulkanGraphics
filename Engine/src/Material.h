// Material.h
#pragma once
#include "core.h"

#include <string>

class Pipeline;
class DescriptorSetLayout;

class Material
{
   public:
    Material(const std::string& InName, Ref<Pipeline> InPipeline, Ref<DescriptorSetLayout> InLayout);

    void                     Bind(VkCommandBuffer InCmd);
    Ref<Pipeline>            GetPipeline() const;
    Ref<DescriptorSetLayout> GetLayout() const;
    VkPipelineLayout         GetPipelineLayout() const;
    const std::string&       GetName() const;

   private:
    std::string              _Name;
    Ref<Pipeline>            _Pipeline;
    Ref<DescriptorSetLayout> _Layout;
};