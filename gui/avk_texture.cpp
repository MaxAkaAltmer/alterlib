#include "avk_texture.h"
#include <iostream>
#include <memory.h>

using namespace alt;

thread_local VKContext* VKTexture::ctx = nullptr;

static void vk_check(VkResult err)
{
    if (err == 0)
        return;
    std::cout << "Vulkan error: VkResult = " << err << std::endl;
    if (err < 0)
        abort();
}

static uint32_t vulkanMemoryType(VkPhysicalDevice pdev, VkMemoryPropertyFlags properties, uint32_t type_bits)
{
    VkPhysicalDeviceMemoryProperties prop;

    vkGetPhysicalDeviceMemoryProperties(pdev, &prop);
    for (uint32_t i = 0; i < prop.memoryTypeCount; i++)
    {
        if ((prop.memoryTypes[i].propertyFlags & properties) == properties && type_bits & (1 << i))
            return i;
    }
    return 0xffffffff;
}

static VkFormat vulkanFormat(uint32 format)
{
    if(format==(32<<16) || format==0x8888)
    {
        return VK_FORMAT_R8G8B8A8_UNORM;
    }
    else if(format==(24<<16) || format==0x888)
    {
        return VK_FORMAT_B8G8R8_UNORM;
    }
    else if(format==0x4444)
    {
        return VK_FORMAT_R4G4B4A4_UNORM_PACK16;
    }
    else if(format==(16<<16) || format==0x565)
    {
        return VK_FORMAT_R5G6B5_UNORM_PACK16;
    }
    else if(format==(8<<16) || format==0x8000)
    {
        return VK_FORMAT_R8_UNORM;
    }
    else if(format==0x1008888)
    {
        return VK_FORMAT_R8G8B8A8_UINT;
    }
    else if(format == 0x4f)
    {
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    }
    return VK_FORMAT_UNDEFINED;
}

void VKTexture::resize(int w, int h, uint32 format, int filter)
{
    if(hand && hand->api_related && hand->alheight == h && hand->alwidth == w && hand->pixelFormat == format)
    {
        return;
    }

    free();

    hand = new Internal;
    hand->alheight = h;
    hand->alwidth = w;
    hand->height = h;
    hand->width = w;
    hand->pixelFormat = format;
    hand->refcount = 1;
    hand->api_related = new VKApiResInternal;

    VKApiResInternal *api = (VKApiResInternal*)hand->api_related;
    VkDevice device = ctx->device();
    VkFormat vulkan_format = vulkanFormat(format);

    VkImageCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = vulkan_format;
    info.extent.width = w;
    info.extent.height = h;
    info.extent.depth = 1;
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    vk_check(vkCreateImage(device, &info, nullptr, &api->m_Image));
    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(device, api->m_Image, &req);
    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = req.size;

    alloc_info.memoryTypeIndex = vulkanMemoryType(ctx->physicalDevice(),VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, req.memoryTypeBits);
    vk_check(vkAllocateMemory(device, &alloc_info, nullptr, &api->m_Memory));
    vk_check(vkBindImageMemory(device, api->m_Image, api->m_Memory, 0));


    VkImageViewCreateInfo view_info = {};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = ((VKApiResInternal*)hand->api_related)->m_Image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = vulkan_format;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.layerCount = 1;
    vk_check(vkCreateImageView(device, &view_info, nullptr, &api->m_ImageView));


    VkSamplerCreateInfo sampler_info = {};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    if(filter<0)
    {
        sampler_info.magFilter = VK_FILTER_NEAREST;
        sampler_info.minFilter = VK_FILTER_NEAREST;
    }
    else
    {
        sampler_info.magFilter = VK_FILTER_LINEAR;
        sampler_info.minFilter = VK_FILTER_LINEAR;
    }
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.minLod = -1000;
    sampler_info.maxLod = 1000;
    sampler_info.maxAnisotropy = 1.0f;
    vk_check(vkCreateSampler(device, &sampler_info, nullptr, &api->m_Sampler));

    api->m_DescriptorSet = (VkDescriptorSet)ctx->addTexture(api->m_Sampler, api->m_ImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void VKTexture::update(int x, int y, const void *buff, int w, int h, uint32 format)
{
    if(!hand || !hand->api_related) return;
    VKApiResInternal *api = (VKApiResInternal*)hand->api_related;

    VkDevice device = ctx->device();
    size_t upload_size = hand->alwidth * hand->alheight * pixelSize(hand->pixelFormat);

    VkBuffer m_StagingBuffer = nullptr;
    VkDeviceMemory m_StagingBufferMemory = nullptr;
    VkMemoryRequirements req;

    {
        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = upload_size;
        buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vk_check(vkCreateBuffer(device, &buffer_info, nullptr, &m_StagingBuffer));
        vkGetBufferMemoryRequirements(device, m_StagingBuffer, &req);
        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = req.size;

        alloc_info.memoryTypeIndex = vulkanMemoryType(ctx->physicalDevice(),VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, req.memoryTypeBits);
        vk_check(vkAllocateMemory(device, &alloc_info, nullptr, &m_StagingBufferMemory));
        vk_check(vkBindBufferMemory(device, m_StagingBuffer, m_StagingBufferMemory, 0));
    }

    char* map = NULL;

    vk_check(vkMapMemory(device, m_StagingBufferMemory, 0, req.size, 0, (void**)(&map)));

    memcpy(map,buff,upload_size);

    VkMappedMemoryRange range[1] = {};
    range[0].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range[0].memory = m_StagingBufferMemory;
    range[0].size = req.size;
    vk_check(vkFlushMappedMemoryRanges(device, 1, range));
    vkUnmapMemory(device, m_StagingBufferMemory);


    VkCommandBuffer command_buffer = ctx->commandBuffer();

    VkImageMemoryBarrier copy_barrier = {};
    copy_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    copy_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    copy_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    copy_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    copy_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    copy_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    copy_barrier.image = ((VKApiResInternal*)hand->api_related)->m_Image;
    copy_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy_barrier.subresourceRange.levelCount = 1;
    copy_barrier.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &copy_barrier);

    VkBufferImageCopy region = {};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent.width = w;
    region.imageExtent.height = h;
    region.imageExtent.depth = 1;
    region.imageOffset.x = x;
    region.imageOffset.y = y;
    region.imageOffset.z = 0;
    vkCmdCopyBufferToImage(command_buffer, m_StagingBuffer, api->m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    VkImageMemoryBarrier use_barrier = {};
    use_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    use_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    use_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    use_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    use_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    use_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    use_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    use_barrier.image = api->m_Image;
    use_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    use_barrier.subresourceRange.levelCount = 1;
    use_barrier.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &use_barrier);

    ctx->flushCommandBuffer(command_buffer);

    vkDestroyBuffer(device, m_StagingBuffer, nullptr);
    vkFreeMemory(device, m_StagingBufferMemory, nullptr);

}

void VKTexture::free()
{
    if(!hand) return;
    hand->refcount --;

	if(!hand->refcount)
	{
		if(hand->api_related)
		{
			VKApiResInternal *api = (VKApiResInternal*)hand->api_related;
			ctx->submitResourceFree(api);
		}
		delete hand;
	}
    hand = nullptr;
}

void VKTexture::draw(vec3d<real32> *vertex, vec2d<real32> *coord, int count, bool strip, alt::colorRGBA col) const
{
    if(!hand || !hand->api_related) return;
    VKApiResInternal *api = (VKApiResInternal*)hand->api_related;

    //todo
}

void* VKTexture::mapData()
{
    if(!hand || !hand->api_related) return nullptr;

    VkDevice device = ctx->device();
    size_t upload_size = hand->alwidth * hand->alheight * pixelSize(hand->pixelFormat);

    if (!((VKApiResInternal*)hand->api_related)->m_StagingBuffer)
    {
        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = upload_size;
        buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vk_check(vkCreateBuffer(device, &buffer_info, nullptr, &((VKApiResInternal*)hand->api_related)->m_StagingBuffer));
        VkMemoryRequirements req;
        vkGetBufferMemoryRequirements(device, ((VKApiResInternal*)hand->api_related)->m_StagingBuffer, &req);
        ((VKApiResInternal*)hand->api_related)->m_AlignedSize = req.size;
        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = req.size;

        alloc_info.memoryTypeIndex = vulkanMemoryType(ctx->physicalDevice(),VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, req.memoryTypeBits);
        vk_check(vkAllocateMemory(device, &alloc_info, nullptr, &((VKApiResInternal*)hand->api_related)->m_StagingBufferMemory));
        vk_check(vkBindBufferMemory(device, ((VKApiResInternal*)hand->api_related)->m_StagingBuffer, ((VKApiResInternal*)hand->api_related)->m_StagingBufferMemory, 0));
    }

    char* map = NULL;

    vk_check(vkMapMemory(device, ((VKApiResInternal*)hand->api_related)->m_StagingBufferMemory, 0, ((VKApiResInternal*)hand->api_related)->m_AlignedSize, 0, (void**)(&map)));

    return map;
}

void VKTexture::unmapData()
{
    if(!hand || !hand->api_related) return;

    VkDevice device = ctx->device();

    VkMappedMemoryRange range[1] = {};
    range[0].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range[0].memory = ((VKApiResInternal*)hand->api_related)->m_StagingBufferMemory;
    range[0].size = ((VKApiResInternal*)hand->api_related)->m_AlignedSize;
    vk_check(vkFlushMappedMemoryRanges(device, 1, range));
    vkUnmapMemory(device, ((VKApiResInternal*)hand->api_related)->m_StagingBufferMemory);

    VkCommandBuffer command_buffer = ctx->commandBuffer();

    VkImageMemoryBarrier copy_barrier = {};
    copy_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    copy_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    copy_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    copy_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    copy_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    copy_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    copy_barrier.image = ((VKApiResInternal*)hand->api_related)->m_Image;
    copy_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy_barrier.subresourceRange.levelCount = 1;
    copy_barrier.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &copy_barrier);

    VkBufferImageCopy region = {};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent.width = hand->alwidth;
    region.imageExtent.height = hand->alheight;
    region.imageExtent.depth = 1;
    vkCmdCopyBufferToImage(command_buffer, ((VKApiResInternal*)hand->api_related)->m_StagingBuffer, ((VKApiResInternal*)hand->api_related)->m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    VkImageMemoryBarrier use_barrier = {};
    use_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    use_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    use_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    use_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    use_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    use_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    use_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    use_barrier.image = ((VKApiResInternal*)hand->api_related)->m_Image;
    use_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    use_barrier.subresourceRange.levelCount = 1;
    use_barrier.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &use_barrier);

    ctx->flushCommandBuffer(command_buffer);

}
