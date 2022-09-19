#include "ParticleSystem.h"
#include "Utils.h"
#include "LogicalDevice.h"
#include "VulkanApplication.h"
#include "Texture.h"
#include "DescriptorSet.h"

//External
#include <Curl.h>
#include <simplexnoise.h>
namespace OVK
{
    ParticleSystem::ParticleSystem(const ParticleSpecs& specs, Ref<Texture> texture, const Ref<DescriptorLayout>& layout, const Ref<DescriptorPool>& pool)
        :m_ParticleTexture(texture)
    {
        // Create shared descriptor set for all particle systems.
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = pool->GetDescriptorPool();
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &layout->GetDescriptorLayout();

        VkResult rslt = vkAllocateDescriptorSets(VulkanApplication::s_Device->GetVKDevice(), &allocInfo, &m_DescriptorSet);
        ASSERT(rslt == VK_SUCCESS, "Failed to allocate descriptor sets!");

        m_ParticleCount = specs.ParticleCount;
        m_SphereRadius = specs.SphereRadius;
        m_TrailLength = specs.TrailLength;
        m_ParticleSize = specs.ParticleSize;
        m_EnableNoise = specs.EnableNoise;
        m_ImmortalParticle = specs.ImmortalParticle;
        m_ParticleMinLifetime = specs.ParticleMinLifetime;
        m_ParticleMaxLifetime = specs.ParticleMaxLifetime;
        m_EmitterPos = specs.EmitterPos;
        m_MinVel = specs.MinVel;
        m_MaxVel = specs.MaxVel;
        SetupParticles();
    }
    ParticleSystem::~ParticleSystem()
    {
        vkUnmapMemory(VulkanApplication::s_Device->GetVKDevice(), m_ParticleBufferMemory);
        if (m_TrailLength > 0)
            vkUnmapMemory(VulkanApplication::s_Device->GetVKDevice(), m_TrailBufferMemory);
        vkDestroyBuffer(VulkanApplication::s_Device->GetVKDevice(), m_ParticleBuffer, nullptr);
        vkFreeMemory(VulkanApplication::s_Device->GetVKDevice(), m_ParticleBufferMemory, nullptr);
        if (m_TrailLength > 0)
            vkDestroyBuffer(VulkanApplication::s_Device->GetVKDevice(), m_TrailBuffer, nullptr);
        if (m_TrailLength > 0)
            vkFreeMemory(VulkanApplication::s_Device->GetVKDevice(), m_TrailBufferMemory, nullptr);
        vkDestroySampler(VulkanApplication::s_Device->GetVKDevice(), m_ParticleSampler, nullptr);
    }
    float ParticleSystem::rnd(float min, float max)
    {
        std::uniform_real_distribution<float> rndDist(min, max);
        return rndDist(rndEngine);
    }
    void ParticleSystem::InitParticle(Particle* particle, glm::vec3 emitterPos)
    {
        particle->Velocity = glm::vec4(m_MinVel.x + rnd(0.0f, m_MaxVel.x - m_MinVel.x), m_MinVel.y + rnd(0.0f, m_MaxVel.y - m_MinVel.y), m_MinVel.z + rnd(0.0f, m_MaxVel.z - m_MinVel.z), 0.0f);
        particle->Alpha = 1.0f;
        particle->SizeRadius = m_ParticleSize;
        particle->Color = glm::vec4(1.0f);
        particle->Rotation = rnd(0.0f, 2.0f * M_PI);
        particle->RotationSpeed = rnd(0.0f, 2.0f) - rnd(0.0f, 2.0f);
        particle->LifeTime = rnd(m_ParticleMinLifetime, m_ParticleMaxLifetime);
        particle->StartingLifeTime = particle->LifeTime;
        particle->RowOffset = RowOffset;
        particle->ColumnOffset = ColumnOffset;
        particle->RowCellSize = RowCellSize;
        particle->ColumnCellSize = ColumnCellSize;

        float u = rnd(0.0f, 1.0f);
        float v = rnd(0.0f, 1.0f);
        float theta = u * 2.0f * M_PI;
        float phi = acos(2.0 * v - 1.0f);
        float r = rnd(0.0f, m_SphereRadius);

        particle->Position.x = r * rnd(-1.0f, 1.0f);
        particle->Position.y = r * rnd(-1.0f, 1.0f);
        particle->Position.z = r * rnd(-1.0f, 1.0f);

        particle->Position += glm::vec4(emitterPos, 0.0f);
    }
    void ParticleSystem::InitTrail(Particle* particle, glm::vec4 pos, float alpha, float size)
    {
        particle->Position = pos;
        particle->Alpha = alpha;
        particle->SizeRadius = size;
        particle->Color = glm::vec4(1.0f);
        particle->Rotation = rnd(0.0f, 2.0f * M_PI);
        particle->RotationSpeed = rnd(0.0f, 2.0f) - rnd(0.0f, 2.0f);
        particle->LifeTime = 0.04f;
    }
    void ParticleSystem::SetupParticles()
    {
        m_Particles.resize(m_ParticleCount);
        m_Trails.resize(m_ParticleCount * m_TrailLength);

        for (auto& particle : m_Particles)
        {
            InitParticle(&particle, m_EmitterPos);
        }

        m_TrailBufferSize = m_Trails.size() * sizeof(Particle);
        m_ParticleBufferSize = m_Particles.size() * sizeof(Particle);

        // Create a buffer and copy over the particle data into it. We also map this buffer so that we can change the data contained in there every frame.
        Utils::CreateVKBuffer(m_ParticleBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_ParticleBuffer, m_ParticleBufferMemory);
        ASSERT(vkMapMemory(VulkanApplication::s_Device->GetVKDevice(), m_ParticleBufferMemory, 0, m_ParticleBufferSize, 0, &m_MappedParticleBuffer) == VK_SUCCESS, "Failed to map the memory");
        memcpy(m_MappedParticleBuffer, m_Particles.data(), m_ParticleBufferSize);


        if (m_TrailLength > 0)
        {
            // Create a buffer and copy over the particle data into it. We also map this buffer so that we can change the data contained in there every frame.
            Utils::CreateVKBuffer(m_TrailBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_TrailBuffer, m_TrailBufferMemory);
            ASSERT(vkMapMemory(VulkanApplication::s_Device->GetVKDevice(), m_TrailBufferMemory, 0, m_TrailBufferSize, 0, &m_MappedTrailsBuffer) == VK_SUCCESS, "Failed to map the memory");
            memcpy(m_MappedTrailsBuffer, m_Trails.data(), m_TrailBufferSize);
        }

        rndEngine.seed((unsigned)time(nullptr));

        // Create the sampler for the particle texture.
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;

        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 0;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        ASSERT(vkCreateSampler(VulkanApplication::s_Device->GetVKDevice(), &samplerInfo, nullptr, &m_ParticleSampler) == VK_SUCCESS, "Failed to create particle sampler!");

        // Write the desriptor set with the above sampler.
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_ParticleTexture->GetImage()->GetImageView();
        imageInfo.sampler = m_ParticleSampler;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = m_DescriptorSet;
        descriptorWrite.dstBinding = 1;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(VulkanApplication::s_Device->GetVKDevice(), 1, &descriptorWrite, 0, nullptr);
    }

    void ParticleSystem::SetUBO(VkBuffer& buffer, size_t writeRange)
    {
        m_ParticleUBOBuffer = &buffer;

        VkWriteDescriptorSet descriptorWrite{};
        VkDescriptorBufferInfo bufferInfo{};

        bufferInfo.buffer = *m_ParticleUBOBuffer;
        bufferInfo.offset = 0;
        bufferInfo.range = writeRange;

        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = m_DescriptorSet;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;
        descriptorWrite.pImageInfo = nullptr; // Optional
        descriptorWrite.pTexelBufferView = nullptr; // Optional

        vkUpdateDescriptorSets(VulkanApplication::s_Device->GetVKDevice(), 1, &descriptorWrite, 0, nullptr);
    }

    void ParticleSystem::UpdateParticles(float deltaTime)
    {
        float particleTimer = deltaTime * 0.01f;
        for (uint64_t i = 0; i < m_Particles.size(); i++)
        {
            CurlNoise::float3 curlNoise = CurlNoise::ComputeCurlNoBoundaries(Vectormath::Aos::Vector3(m_Particles[i].Position.x, m_Particles[i].Position.y, m_Particles[i].Position.z));
            float noise = scaled_raw_noise_3d(-20, 20, m_Particles[i].Position.x, m_Particles[i].Position.y, m_Particles[i].Position.z);
            m_Particles[i].Position.y += m_Particles[i].Velocity.y * 3.0f * (m_EnableNoise ? (curlNoise.val[1] / 10) + 1 : 1) * particleTimer * 3.5f;
            m_Particles[i].Position.x += m_Particles[i].Velocity.x * (m_EnableNoise ? curlNoise.val[0] : 1) * particleTimer * 3.5f;
            m_Particles[i].Position.z += m_Particles[i].Velocity.z * (m_EnableNoise ? curlNoise.val[2] : 1) * particleTimer * 3.5f;
            m_Particles[i].Velocity = m_Particles[i].Velocity;
            m_Particles[i].LifeTime -= 1.0f * deltaTime;
            m_Particles[i].Alpha = m_ImmortalParticle ? 1.0f : m_Particles[i].LifeTime / m_Particles[i].StartingLifeTime;
            m_Particles[i].SizeRadius = m_Particles[i].SizeRadius;
            m_Particles[i].Color = m_Particles[i].Color;

            m_Particles[i].Rotation += particleTimer * m_Particles[i].RotationSpeed;
            m_Particles[i].RowOffset = RowOffset;
            m_Particles[i].ColumnOffset = ColumnOffset;
            m_Particles[i].RowCellSize = RowCellSize;
            m_Particles[i].ColumnCellSize = ColumnCellSize;

            // Decrease the life time of the trail particles every frame here.
            for (uint64_t j = 0; j < m_TrailLength; j++)
            {
                m_Trails[(i * m_TrailLength) + j].LifeTime -= 1.0f * deltaTime;
                m_Trails[(i * m_TrailLength) + j].Alpha = m_Particles[i].Alpha;
            }


            // Check if any of the trails has died. If yes we re-init a new particle.
            for (uint64_t k = 0; k < m_TrailLength; k++)
            {
                if (m_Trails[(i * m_TrailLength) + k].LifeTime <= 0.0f)
                {
                    InitTrail(&m_Trails[(i * m_TrailLength) + k], m_Particles[i].Position, m_Particles[i].Alpha, m_Particles[i].SizeRadius);
                    break;
                }
            }

            // If particle dies, re-init.
            if (m_Particles[i].LifeTime <= 0.0f)
            {
                InitParticle(&m_Particles[i], m_EmitterPos);
                for (uint64_t k = 0; k < m_TrailLength; k++)
                    InitTrail(&m_Trails[(i * m_TrailLength) + k], m_Particles[i].Position, m_Particles[i].Alpha, m_Particles[i].SizeRadius);
            }
        }
        size_t size = m_Particles.size() * sizeof(Particle);
        memcpy(m_MappedParticleBuffer, m_Particles.data(), size);

        if (m_TrailLength > 0)
        {
            size = m_Trails.size() * sizeof(Particle);
            memcpy(m_MappedTrailsBuffer, m_Trails.data(), size);
        }
    }

    void ParticleSystem::Draw(const VkCommandBuffer& cmdBuffer, const VkPipelineLayout& pipelineLayout)
    {
        // IMPORTANT: The shared pipeline for particle systems must be bound outside the class.
        VkDeviceSize offsets[1] = { 0 };
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &m_DescriptorSet, 0, nullptr);
        vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &m_ParticleBuffer, offsets);
        vkCmdDraw(cmdBuffer, m_ParticleCount, 1, 0, 0);

        // Only draw the trails if there are trails. Pretty obvious.
        if (m_TrailLength > 0)
        {
            vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &m_TrailBuffer, offsets);
            vkCmdDraw(cmdBuffer, m_ParticleCount * m_TrailLength, 1, 0, 0);
        }
    }

}
