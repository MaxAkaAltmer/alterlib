#include "avk_context.h"
#include "avk_texture.h"

#include <iostream>
#include <assert.h>
#include <memory.h>

using namespace alt;

static void vk_check(VkResult err)
{
    if (err == 0)
        return;
    std::cout << "[vulkan] Error: VkResult = " << err << std::endl;
    if (err < 0)
        abort();
}

static bool IsExtensionAvailable(const std::vector<VkExtensionProperties>& properties, const char* extension)
{
    for (const VkExtensionProperties& p : properties)
        if (strcmp(p.extensionName, extension) == 0)
            return true;
    return false;
}

VKContext::VKContext(VkDescriptorSet (*cb_addTexture)(VkSampler, VkImageView , VkImageLayout),
                     void (*cb_removeTexture)(VkDescriptorSet),
                     std::vector<const char*> instance_extensions)
    : m_addTextureCB(cb_addTexture)
    , m_removeTextureCB(cb_removeTexture)
{

    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

    uint32_t properties_count;
    std::vector<VkExtensionProperties> properties;
    vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, nullptr);
    properties.resize(properties_count);
    vk_check(vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, properties.data()));

    if (IsExtensionAvailable(properties, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
        instance_extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
    if (IsExtensionAvailable(properties, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
    {
        instance_extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }
#endif
    create_info.enabledExtensionCount = (uint32_t)instance_extensions.size();
    create_info.ppEnabledExtensionNames = instance_extensions.data();
    vk_check(vkCreateInstance(&create_info, m_allocator, &m_instance));


    uint32_t gpu_count = 0;

    vk_check(vkEnumeratePhysicalDevices(m_instance, &gpu_count, nullptr));
    assert(gpu_count > 0);

    std::vector<VkPhysicalDevice> gpus;
    gpus.resize(gpu_count);

    vk_check(vkEnumeratePhysicalDevices(m_instance, &gpu_count, gpus.data()));

    for (VkPhysicalDevice& device : gpus)
    {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(device, &properties);

        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            m_physicalDevice = device;
    }

    if (!m_physicalDevice && gpu_count > 0)
        m_physicalDevice = gpus[0];


    uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &count, nullptr);
    VkQueueFamilyProperties* queues = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * count);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &count, queues);

    for (uint32_t i = 0; i < count; i++)
    {
        if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            m_queueFamily = i;
            break;
        }
    }
    free(queues);
    assert(m_queueFamily != (uint32_t)-1);

    std::vector<const char*> device_extensions;
    device_extensions.push_back("VK_KHR_swapchain");

    uint32_t dev_properties_count;
    std::vector<VkExtensionProperties> dev_properties;
    vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &dev_properties_count, nullptr);
    dev_properties.resize(dev_properties_count);
    vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &dev_properties_count, dev_properties.data());
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
    if (IsExtensionAvailable(properties, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
        device_extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif

    const float queue_priority[] = { 1.0f };
    VkDeviceQueueCreateInfo queue_info[1] = {};
    queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info[0].queueFamilyIndex = m_queueFamily;
    queue_info[0].queueCount = 1;
    queue_info[0].pQueuePriorities = queue_priority;
    VkDeviceCreateInfo dev_create_info = {};
    dev_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dev_create_info.queueCreateInfoCount = sizeof(queue_info) / sizeof(queue_info[0]);
    dev_create_info.pQueueCreateInfos = queue_info;
    dev_create_info.enabledExtensionCount = (uint32_t)device_extensions.size();
    dev_create_info.ppEnabledExtensionNames = device_extensions.data();

    vk_check(vkCreateDevice(m_physicalDevice, &dev_create_info, m_allocator, &m_device));
    vkGetDeviceQueue(m_device, m_queueFamily, 0, &m_queue);


    const uint32_t pool_size = 1024;

    // Задаём размеры пула для различных типов дескрипторов
    VkDescriptorPoolSize pool_sizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER, pool_size },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, pool_size },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, pool_size },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, pool_size },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, pool_size },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, pool_size },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, pool_size },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, pool_size },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, pool_size },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, pool_size },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, pool_size }
    };
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = pool_size * (sizeof(pool_sizes)/sizeof (pool_sizes[0]));
    pool_info.poolSizeCount = (uint32_t)(sizeof(pool_sizes)/sizeof (pool_sizes[0]));
    pool_info.pPoolSizes = pool_sizes;
    vk_check(vkCreateDescriptorPool(m_device, &pool_info, m_allocator, &m_descriptorPool));


    VKTexture::initCtx(this);

}

VKContext::~VKContext()
{       
    vkDestroyDescriptorPool(m_device, m_descriptorPool, m_allocator);
    vkDestroyDevice(m_device, m_allocator);
    vkDestroyInstance(m_instance, m_allocator);
}

void VKContext::startFrame(VkFence Fence, VkCommandPool CommandPool, VkCommandBuffer CommandBuffer,
                           VkRenderPass RenderPass, VkFramebuffer Framebuffer, VkClearValue *ClearValue,
                           VkSemaphore image_acquired_semaphore, VkSemaphore render_complete_semaphore,
                           int w, int h, uint32_t frame_index)
{
    m_Fence = Fence;
    m_frameIndex = frame_index;
    m_commandPool = CommandPool;
    m_image_acquired_semaphore = image_acquired_semaphore;
    m_render_complete_semaphore = render_complete_semaphore;

    m_currentFrameIndex = (m_currentFrameIndex + 1) % m_imageCount;

    vk_check(vkWaitForFences(m_device, 1, &Fence, VK_TRUE, UINT64_MAX));
    vk_check(vkResetFences(m_device, 1, &Fence));

    for (int i=0; i < m_resourceFreeQueue[m_currentFrameIndex].size();i++)
    {
        m_removeTextureCB(m_resourceFreeQueue[m_currentFrameIndex][i]->m_DescriptorSet);

        vkDestroySampler(m_device, m_resourceFreeQueue[m_currentFrameIndex][i]->m_Sampler, nullptr);
        vkDestroyImageView(m_device, m_resourceFreeQueue[m_currentFrameIndex][i]->m_ImageView, nullptr);
        vkDestroyImage(m_device, m_resourceFreeQueue[m_currentFrameIndex][i]->m_Image, nullptr);
        vkFreeMemory(m_device, m_resourceFreeQueue[m_currentFrameIndex][i]->m_Memory, nullptr);
        vkDestroyBuffer(m_device, m_resourceFreeQueue[m_currentFrameIndex][i]->m_StagingBuffer, nullptr);
        vkFreeMemory(m_device, m_resourceFreeQueue[m_currentFrameIndex][i]->m_StagingBufferMemory, nullptr);

        delete m_resourceFreeQueue[m_currentFrameIndex][i];
    }
    m_resourceFreeQueue[m_currentFrameIndex].clear();

    auto& allocatedCommandBuffers = m_allocatedCommandBuffers[m_frameIndex];
    if (allocatedCommandBuffers.size() > 0)
    {
        vkFreeCommandBuffers(m_device, CommandPool, (uint32_t)allocatedCommandBuffers.size(), allocatedCommandBuffers());
        allocatedCommandBuffers.clear();
    }

    vk_check(vkResetCommandPool(m_device, CommandPool, 0));

    VkCommandBufferBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vk_check(vkBeginCommandBuffer(CommandBuffer, &info));

    VkRenderPassBeginInfo render_info = {};
    render_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_info.renderPass = RenderPass;
    render_info.framebuffer = Framebuffer;
    render_info.renderArea.extent.width = w;
    render_info.renderArea.extent.height = h;
    render_info.clearValueCount = 1;
    render_info.pClearValues = ClearValue;

    vkCmdBeginRenderPass(CommandBuffer, &render_info, VK_SUBPASS_CONTENTS_INLINE);

}

void VKContext::endFrame(VkCommandBuffer CommandBuffer)
{
    vkCmdEndRenderPass(CommandBuffer);

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &m_image_acquired_semaphore;
    info.pWaitDstStageMask = &wait_stage;
    info.commandBufferCount = 1;
    info.pCommandBuffers = &CommandBuffer;
    info.signalSemaphoreCount = 1;
    info.pSignalSemaphores = &m_render_complete_semaphore;

    vk_check(vkEndCommandBuffer(CommandBuffer));
    vk_check(vkQueueSubmit(m_queue, 1, &info, m_Fence));

}

VkCommandBuffer VKContext::commandBuffer()
{
    VkCommandBufferAllocateInfo cmdBufAllocateInfo = {};
    cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdBufAllocateInfo.commandPool = m_commandPool;
    cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdBufAllocateInfo.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    vk_check(vkAllocateCommandBuffers(m_device, &cmdBufAllocateInfo, &command_buffer));

    m_allocatedCommandBuffers[m_frameIndex].append(command_buffer);

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vk_check(vkBeginCommandBuffer(command_buffer, &begin_info));

    return command_buffer;
}

void VKContext::flushCommandBuffer(VkCommandBuffer commandBuffer)
{
    const uint64_t DEFAULT_FENCE_TIMEOUT = 10000000000;

    VkSubmitInfo end_info = {};
    end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    end_info.commandBufferCount = 1;
    end_info.pCommandBuffers = &commandBuffer;

    vk_check(vkEndCommandBuffer(commandBuffer));

    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = 0;

    VkFence fence;
    vk_check(vkCreateFence(m_device, &fenceCreateInfo, nullptr, &fence));

    vk_check(vkQueueSubmit(m_queue, 1, &end_info, fence));
    vk_check(vkWaitForFences(m_device, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));

    vkDestroyFence(m_device, fence, nullptr);
}
