#pragma once
#include "core.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/glm.hpp>
#include <glm/matrix.hpp>
#include <vector>
#include <random>
#include <vulkan/vulkan.h>

const float M_PI = 3.14159265359;

namespace OVK
{
    class Image;
    struct Particle
    {
        glm::vec4 Position;
        glm::vec4 Color;
        float Alpha;
        float SizeRadius;
        float Rotation;
        float RowOffset = 1.0f;
        float ColumnOffset = 1.0f;
        float RowCellSize = 1.0f;
        float ColumnCellSize = 1.0f;
        // Attributes not used in shader
        glm::vec4 Velocity;
        float RotationSpeed;
        float LifeTime;
        float StartingLifeTime;
        int currentTrailIndex = 0;
    };

    struct ParticleSpecs
    {
        int       ParticleCount;
        float     SphereRadius;
        int       TrailLength;
        float     ParticleSize;
        bool      EnableNoise = true;
        bool      ImmortalParticle = false;
        float     ParticleMinLifetime;
        float     ParticleMaxLifetime;
        glm::vec3 EmitterPos;
        glm::vec3 MinVel;
        glm::vec3 MaxVel;
    };

    class DescriptorLayout;
    class DescriptorPool;
    class ParticleSystem
    {
    public:
        // Variables to animate the sprites.
        float RowOffset = 1.0f;
        float ColumnOffset = 1.0f;
        float RowCellSize = 1.0f;
        float ColumnCellSize = 1.0f;
    public:
        ParticleSystem(const ParticleSpecs& specs, Ref<Image> texture, const Ref<DescriptorLayout>& layout, const Ref<DescriptorPool>& pool);
        ~ParticleSystem();
    private:
        // Per particle system variables.
        int   m_ParticleCount;
        float m_SphereRadius;
        int   m_TrailLength;
        float m_ParticleSize;
        bool  m_EnableNoise = true;
        bool  m_ImmortalParticle = false;

        float m_ParticleMinLifetime;
        float m_ParticleMaxLifetime;

        glm::vec3 m_EmitterPos;
        glm::vec3 m_MinVel;
        glm::vec3 m_MaxVel;

        float trailUpdateRate = 0.013f; // Used to get rid of the frame rate dependency for trails.
        float deltaTimeSum = 0;


        std::vector<Particle> m_Particles;
        std::vector<Particle> m_Trails;

        std::default_random_engine rndEngine;

        // Contains the position of particles.
        VkBuffer       m_ParticleBuffer;
        VkDeviceMemory m_ParticleBufferMemory;
        size_t         m_ParticleBufferSize;
        void* m_MappedParticleBuffer;

        // Contains the position of trail particles if there are any.
        VkBuffer       m_TrailBuffer;
        VkDeviceMemory m_TrailBufferMemory;
        size_t         m_TrailBufferSize;
        void* m_MappedTrailsBuffer;

        VkSampler       m_ParticleSampler;
        Ref<Image>      m_ParticleTexture;

        VkDescriptorSet m_DescriptorSet;
    private:
        // Link the global UBO with this member from outside of the class.
        VkBuffer* m_ParticleUBOBuffer;

        float rnd(float min, float max);
        void InitParticle(Particle* particle, glm::vec3 emitterPos);
        void InitTrail(Particle* particle, glm::vec4 pos, float alpha, float size);
        void SetupParticles();
    public:
        void SetUBO(VkBuffer& buffer, size_t writeRange, size_t offset);
        void UpdateParticles(float deltaTime);
        inline void SetEmitterPosition(const glm::vec3& pos) { m_EmitterPos = pos; }
        void Draw(const VkCommandBuffer& cmdBuffer, const VkPipelineLayout& pipelineLayout);
    };
}
