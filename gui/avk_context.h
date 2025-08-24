#ifndef AVK_CONTEXT_H
#define AVK_CONTEXT_H

#include <vulkan/vulkan.h>
#include <functional>
#include "../at_array.h"

namespace alt {

struct VKApiResInternal
{
    size_t m_AlignedSize = 0;

    VkImage m_Image = nullptr;
    VkImageView m_ImageView = nullptr;
    VkDeviceMemory m_Memory = nullptr;
    VkSampler m_Sampler = nullptr;
    VkBuffer m_StagingBuffer = nullptr;
    VkDeviceMemory m_StagingBufferMemory = nullptr;
    VkDescriptorSet m_DescriptorSet = nullptr;
};

class VKContext
{
public:

    VKContext(VkDescriptorSet (*cb_addTexture)(VkSampler, VkImageView , VkImageLayout),
              void (*cb_removeTexture)(VkDescriptorSet),
              std::vector<const char *> instance_extensions = std::vector<const char *>());
    ~VKContext();

    void destoyResources()
    {
        for(int j=0; j < m_resourceFreeQueue.size(); j++)
        for(int i=0; i < m_resourceFreeQueue[j].size(); i++)
        {
            m_removeTextureCB(m_resourceFreeQueue[j][i]->m_DescriptorSet);

            vkDestroySampler(m_device, m_resourceFreeQueue[j][i]->m_Sampler, nullptr);
            vkDestroyImageView(m_device, m_resourceFreeQueue[j][i]->m_ImageView, nullptr);
            vkDestroyImage(m_device, m_resourceFreeQueue[j][i]->m_Image, nullptr);
            vkFreeMemory(m_device, m_resourceFreeQueue[j][i]->m_Memory, nullptr);
            vkDestroyBuffer(m_device, m_resourceFreeQueue[j][i]->m_StagingBuffer, nullptr);
            vkFreeMemory(m_device, m_resourceFreeQueue[j][i]->m_StagingBufferMemory, nullptr);

            delete m_resourceFreeQueue[j][i];
        }
        m_resourceFreeQueue.clear();
    }

    void initWindowStuff(uint32 image_count)
    {
        m_imageCount = image_count;
        m_allocatedCommandBuffers.resize(m_imageCount);
        m_resourceFreeQueue.resize(m_imageCount);
    }

    void resetCommandBuffers(uint32 image_count)
    {
        m_imageCount = image_count;
        m_allocatedCommandBuffers.clear(true);
        m_allocatedCommandBuffers.resize(m_imageCount);
        m_swapChainRebuild = false;
    }

    void setSwapChainRebuild(bool val)
    {
        m_swapChainRebuild = val;
    }

    VkPhysicalDevice physicalDevice() { return m_physicalDevice; }
    VkDevice device() { return m_device; }
    VkInstance instance() { return m_instance; }
    VkAllocationCallbacks* allocator() { return m_allocator; }
    uint32_t queueFamily() { return m_queueFamily; }
    VkQueue queue() { return m_queue; }
    VkPipelineCache pipelineCache() { return m_pipelineCache; }
    VkDescriptorPool descriptorPool() { return m_descriptorPool; }
    int minImageCount() { return m_minImageCount; }
    bool swapChainRebuild() { return m_swapChainRebuild; }

    VkCommandBuffer commandBuffer();
    void flushCommandBuffer(VkCommandBuffer commandBuffer);

    void startFrame(VkFence Fence, VkCommandPool CommandPool, VkCommandBuffer CommandBuffer,
                    VkRenderPass RenderPass, VkFramebuffer Framebuffer, VkClearValue *ClearValue, VkSemaphore image_acquired_semaphore, VkSemaphore render_complete_semaphore,
                    int w, int h, uint32_t frame_index);

    void endFrame(VkCommandBuffer CommandBuffer);

    VkDescriptorSet addTexture(VkSampler sampler, VkImageView image_view, VkImageLayout image_layout)
    {
        return (*m_addTextureCB)(sampler,image_view,image_layout);
    }
    void submitResourceFree(VKApiResInternal *res)
    {
        m_resourceFreeQueue[m_currentFrameIndex].append(res);
    }

private:

    VkDescriptorSet (*m_addTextureCB)(VkSampler, VkImageView , VkImageLayout) = nullptr;
    void (*m_removeTextureCB)(VkDescriptorSet) = nullptr;

    VkSemaphore m_image_acquired_semaphore = nullptr;
    VkSemaphore m_render_complete_semaphore = nullptr;
    VkFence m_Fence = nullptr;

    VkCommandPool            m_commandPool = nullptr;
    VkAllocationCallbacks*   m_allocator = nullptr;
    VkInstance               m_instance = nullptr;
    VkPhysicalDevice         m_physicalDevice = nullptr;
    VkDevice                 m_device = nullptr;
    uint32_t                 m_queueFamily = -1;
    VkQueue                  m_queue = nullptr;
    VkPipelineCache          m_pipelineCache = nullptr;
    VkDescriptorPool         m_descriptorPool = nullptr;

    VkPipeline               m_PipelineWithBlending = nullptr;
    VkPipeline               m_PipelineWithoutBlending = nullptr;
    VkPipelineLayout         m_PipelineLayout = nullptr;

    int                      m_minImageCount = 2;
    bool                     m_swapChainRebuild = false;

    uint32                   m_imageCount = 0;
    uint32_t                 m_frameIndex;
    uint32_t                 m_currentFrameIndex = 0;

    array<array<VkCommandBuffer>>   m_allocatedCommandBuffers;
    array<array<VKApiResInternal*>> m_resourceFreeQueue;
};

}

#endif // AVK_CONTEXT_H
