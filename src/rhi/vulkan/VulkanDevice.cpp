#include "rhi/vulkan/VulkanDevice.hpp"
#include "core/PipelineBuilder.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace rhi {

namespace {

VkFormat toVkFormat(Format fmt) {
    switch (fmt) {
        case Format::RGBA8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
        case Format::BGRA8_SRGB:  return VK_FORMAT_B8G8R8A8_SRGB;
        case Format::RGBA16F:     return VK_FORMAT_R16G16B16A16_SFLOAT;
        case Format::R16F:        return VK_FORMAT_R16_SFLOAT;
        case Format::RG16F:       return VK_FORMAT_R16G16_SFLOAT;
        case Format::D32F:        return VK_FORMAT_D32_SFLOAT;
        case Format::R32F:        return VK_FORMAT_R32_SFLOAT;
        case Format::RG32F:       return VK_FORMAT_R32G32_SFLOAT;
        case Format::RGB32F:      return VK_FORMAT_R32G32B32_SFLOAT;
        case Format::RGBA32F:     return VK_FORMAT_R32G32B32A32_SFLOAT;
    }
    return VK_FORMAT_R32G32B32A32_SFLOAT;
}

VkCullModeFlags toVkCullMode(CullMode mode) {
    switch (mode) {
        case CullMode::None:  return VK_CULL_MODE_NONE;
        case CullMode::Back:  return VK_CULL_MODE_BACK_BIT;
        case CullMode::Front: return VK_CULL_MODE_FRONT_BIT;
    }
    return VK_CULL_MODE_NONE;
}

VkPolygonMode toVkPolygonMode(PolygonMode mode) {
    switch (mode) {
        case PolygonMode::Fill: return VK_POLYGON_MODE_FILL;
        case PolygonMode::Line: return VK_POLYGON_MODE_LINE;
    }
    return VK_POLYGON_MODE_FILL;
}

VkCompareOp toVkCompareOp(CompareOp op) {
    switch (op) {
        case CompareOp::Less:        return VK_COMPARE_OP_LESS;
        case CompareOp::LessOrEqual: return VK_COMPARE_OP_LESS_OR_EQUAL;
        case CompareOp::Greater:     return VK_COMPARE_OP_GREATER;
        case CompareOp::Always:      return VK_COMPARE_OP_ALWAYS;
    }
    return VK_COMPARE_OP_LESS_OR_EQUAL;
}

VkShaderStageFlags toVkShaderStages(ShaderStage stages) {
    VkShaderStageFlags flags = 0;
    if (hasFlag(stages, ShaderStage::Vertex))   flags |= VK_SHADER_STAGE_VERTEX_BIT;
    if (hasFlag(stages, ShaderStage::Fragment)) flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if (hasFlag(stages, ShaderStage::Compute))  flags |= VK_SHADER_STAGE_COMPUTE_BIT;
    return flags;
}

VkBufferUsageFlags toVkBufferUsage(BufferUsage usage) {
    VkBufferUsageFlags flags = 0;
    if (hasFlag(usage, BufferUsage::Vertex))      flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (hasFlag(usage, BufferUsage::Index))       flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (hasFlag(usage, BufferUsage::Uniform))     flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (hasFlag(usage, BufferUsage::Storage))     flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (hasFlag(usage, BufferUsage::Indirect))    flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    if (hasFlag(usage, BufferUsage::TransferSrc)) flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (hasFlag(usage, BufferUsage::TransferDst)) flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    return flags;
}

VkImageUsageFlags toVkImageUsage(TextureUsage usage) {
    VkImageUsageFlags flags = 0;
    if (hasFlag(usage, TextureUsage::Sampled))         flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (hasFlag(usage, TextureUsage::Storage))         flags |= VK_IMAGE_USAGE_STORAGE_BIT;
    if (hasFlag(usage, TextureUsage::ColorAttachment)) flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (hasFlag(usage, TextureUsage::DepthAttachment)) flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (hasFlag(usage, TextureUsage::TransferSrc))     flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (hasFlag(usage, TextureUsage::TransferDst))     flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    return flags;
}

VkMemoryPropertyFlags toVkMemoryProps(MemoryUsage mem) {
    switch (mem) {
        case MemoryUsage::GpuOnly:  return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        case MemoryUsage::CpuToGpu: return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        case MemoryUsage::GpuToCpu: return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }
    return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
}

VkSamplerAddressMode toVkAddressMode(AddressMode mode) {
    switch (mode) {
        case AddressMode::Repeat:      return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case AddressMode::ClampToEdge: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    }
    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
}

VkFilter toVkFilter(Filter f) {
    return (f == Filter::Nearest) ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
}

struct LayoutTransitionInfo {
    VkImageLayout layout;
    VkAccessFlags access;
    VkPipelineStageFlags stage;
};

LayoutTransitionInfo toVkLayoutInfo(TextureLayout layout) {
    switch (layout) {
        case TextureLayout::Undefined:
            return {VK_IMAGE_LAYOUT_UNDEFINED, 0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
        case TextureLayout::ColorAttachment:
            return {VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        case TextureLayout::DepthAttachment:
            return {VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT};
        case TextureLayout::ShaderReadOnly:
            return {VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_ACCESS_SHADER_READ_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT};
        case TextureLayout::General:
            return {VK_IMAGE_LAYOUT_GENERAL,
                    VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT};
        case TextureLayout::TransferSrc:
            return {VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_ACCESS_TRANSFER_READ_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT};
        case TextureLayout::TransferDst:
            return {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT};
    }
    return {VK_IMAGE_LAYOUT_UNDEFINED, 0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
}

} // namespace

VulkanRHIBuffer::VulkanRHIBuffer(
    const VulkanContext& context,
    size_t size,
    BufferUsage usage,
    MemoryUsage memUsage,
    VkDescriptorPool descriptorPool,
    VkDescriptorSetLayout uboLayout,
    VkDescriptorSetLayout ssboLayout
) : m_device(context.getDevice()), m_pool(descriptorPool) {
    m_buffer.create(context, static_cast<VkDeviceSize>(size), toVkBufferUsage(usage), toVkMemoryProps(memUsage));

    auto allocAndUpdate = [&](VkDescriptorSetLayout layout, VkDescriptorType type) -> VkDescriptorSet {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_pool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &layout;

        VkDescriptorSet set = VK_NULL_HANDLE;
        VK_CHECK(vkAllocateDescriptorSets(m_device, &allocInfo, &set), "Failed to allocate buffer descriptor set");

        VkDescriptorBufferInfo bufInfo{};
        bufInfo.buffer = m_buffer.getBuffer();
        bufInfo.offset = 0;
        bufInfo.range = m_buffer.getSize();

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = set;
        write.dstBinding = 0;
        write.descriptorType = type;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufInfo;

        vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
        return set;
    };

    if (hasFlag(usage, BufferUsage::Uniform)) {
        m_uboSet = allocAndUpdate(uboLayout, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    }
    if (hasFlag(usage, BufferUsage::Storage)) {
        m_ssboSet = allocAndUpdate(ssboLayout, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    }
}

VulkanRHIBuffer::~VulkanRHIBuffer() {
    std::vector<VkDescriptorSet> toFree;
    if (m_uboSet != VK_NULL_HANDLE) toFree.push_back(m_uboSet);
    if (m_ssboSet != VK_NULL_HANDLE) toFree.push_back(m_ssboSet);
    if (!toFree.empty() && m_pool != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(m_device, m_pool, static_cast<uint32_t>(toFree.size()), toFree.data());
    }
    m_buffer.destroy();
}

VulkanRHITexture::VulkanRHITexture(VulkanDevice& owner, const VulkanContext& context, const TextureDesc& desc)
    : m_owner(owner), m_desc(desc) {
    m_aspectMask = (desc.format == Format::D32F) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    VkImageType imgType = VK_IMAGE_TYPE_2D;
    VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;
    VkImageCreateFlags flags = 0;
    if (desc.type == TextureType::Tex2DArray) {
        viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    } else if (desc.type == TextureType::Tex3D) {
        imgType = VK_IMAGE_TYPE_3D;
        viewType = VK_IMAGE_VIEW_TYPE_3D;
    } else if (desc.type == TextureType::Cube) {
        viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    m_image.create(
        context,
        desc.width,
        desc.height,
        toVkFormat(desc.format),
        toVkImageUsage(desc.usage),
        m_aspectMask,
        desc.mipLevels,
        desc.arrayLayers,
        desc.depth,
        imgType,
        viewType,
        flags
    );
}

VulkanRHITexture::~VulkanRHITexture() {
    if (m_image.isValid()) {
        m_owner.invalidateFramebuffersForView(m_image.getView());
    }
    m_image.destroy();
}

VulkanRHISampler::VulkanRHISampler(const VulkanContext& context, const SamplerDesc& desc)
    : m_device(context.getDevice()) {
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(context.getPhysicalDevice(), &props);
    VkPhysicalDeviceFeatures feats{};
    vkGetPhysicalDeviceFeatures(context.getPhysicalDevice(), &feats);

    VkSamplerCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.magFilter = toVkFilter(desc.magFilter);
    info.minFilter = toVkFilter(desc.minFilter);
    info.mipmapMode = (desc.minFilter == Filter::Nearest) ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR;
    info.addressModeU = toVkAddressMode(desc.addressU);
    info.addressModeV = toVkAddressMode(desc.addressV);
    info.addressModeW = toVkAddressMode(desc.addressW);
    info.anisotropyEnable = (feats.samplerAnisotropy && desc.maxAnisotropy > 1.0f) ? VK_TRUE : VK_FALSE;
    info.maxAnisotropy = std::min(desc.maxAnisotropy, props.limits.maxSamplerAnisotropy);
    info.compareEnable = desc.compareEnable ? VK_TRUE : VK_FALSE;
    info.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    info.minLod = 0.0f;
    info.maxLod = 16.0f;
    info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

    VK_CHECK(vkCreateSampler(m_device, &info, nullptr, &m_sampler), "Failed to create VkSampler");
}

VulkanRHISampler::~VulkanRHISampler() {
    if (m_sampler != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }
}

void VulkanRHICommandList::beginRenderPass(const RenderPassDesc& desc) {
    if (desc.colorTarget == nullptr && desc.depthTarget == nullptr) {
        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color.float32[0] = desc.clearColor.r;
        clearValues[0].color.float32[1] = desc.clearColor.g;
        clearValues[0].color.float32[2] = desc.clearColor.b;
        clearValues[0].color.float32[3] = desc.clearColor.a;
        clearValues[1].depthStencil = {desc.clearDepth, 0};

        VkRenderPassBeginInfo rpInfo{};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpInfo.renderPass = m_device.getSwapchain().getRenderPass();
        rpInfo.framebuffer = m_device.getSwapchain().getFramebuffer(m_device.getCurrentImageIndex());
        rpInfo.renderArea.offset = {0, 0};
        rpInfo.renderArea.extent = m_device.getSwapchain().getExtent();
        rpInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        rpInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(m_cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
        return;
    }

    auto* colorTex = static_cast<const VulkanRHITexture*>(desc.colorTarget);
    auto* depthTex = static_cast<const VulkanRHITexture*>(desc.depthTarget);
    if (!colorTex) {
        throw std::runtime_error("Offscreen RenderPassDesc requires colorTarget");
    }

    bool hasDepth = (depthTex != nullptr);
    Format depthFmt = hasDepth ? depthTex->getFormat() : Format::D32F;
    VkRenderPass rp = m_device.getOrCreateOffscreenRenderPass(
        colorTex->getFormat(), hasDepth, depthFmt, desc.clearColorEnable, desc.clearDepthEnable
    );
    VkFramebuffer fb = m_device.getOrCreateFramebuffer(
        rp,
        colorTex->getVkImageView(),
        hasDepth ? depthTex->getVkImageView() : VK_NULL_HANDLE,
        colorTex->getWidth(),
        colorTex->getHeight()
    );

    std::vector<VkClearValue> clearValues;
    VkClearValue cVal{};
    cVal.color.float32[0] = desc.clearColor.r;
    cVal.color.float32[1] = desc.clearColor.g;
    cVal.color.float32[2] = desc.clearColor.b;
    cVal.color.float32[3] = desc.clearColor.a;
    clearValues.push_back(cVal);
    if (hasDepth) {
        VkClearValue dVal{};
        dVal.depthStencil = {desc.clearDepth, 0};
        clearValues.push_back(dVal);
    }

    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass = rp;
    rpInfo.framebuffer = fb;
    rpInfo.renderArea.offset = {0, 0};
    rpInfo.renderArea.extent = {colorTex->getWidth(), colorTex->getHeight()};
    rpInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    rpInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(m_cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanRHICommandList::endRenderPass() {
    vkCmdEndRenderPass(m_cmd);
}

void VulkanRHICommandList::setViewport(float x, float y, float width, float height) {
    VkViewport viewport{};
    viewport.x = x;
    viewport.y = y;
    viewport.width = width;
    viewport.height = height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(m_cmd, 0, 1, &viewport);
}

void VulkanRHICommandList::setScissor(int32_t x, int32_t y, uint32_t width, uint32_t height) {
    VkRect2D scissor{};
    scissor.offset = {x, y};
    scissor.extent = {width, height};
    vkCmdSetScissor(m_cmd, 0, 1, &scissor);
}

void VulkanRHICommandList::bindGraphicsPipeline(const RHIPipeline& pipeline) {
    const auto& vkPipe = static_cast<const VulkanRHIPipeline&>(pipeline);
    m_currentPipeline = &vkPipe;
    m_boundTextures.fill({});
    m_boundStorageImages.fill({});
    m_descriptorsDirty = (vkPipe.numSampledTextures > 0);
    vkCmdBindPipeline(m_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipe.pipeline.get());
}

void VulkanRHICommandList::bindComputePipeline(const RHIPipeline& pipeline) {
    const auto& vkPipe = static_cast<const VulkanRHIPipeline&>(pipeline);
    m_currentPipeline = &vkPipe;
    m_boundTextures.fill({});
    m_boundStorageImages.fill({});
    m_descriptorsDirty = (vkPipe.numSampledTextures > 0 || vkPipe.numStorageTextures > 0);
    vkCmdBindPipeline(m_cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vkPipe.pipeline.get());
}

void VulkanRHICommandList::bindUniformBuffer(uint32_t slot, const RHIBuffer& buffer) {
    (void)slot;
    if (!m_currentPipeline) return;
    const auto& vkBuf = static_cast<const VulkanRHIBuffer&>(buffer);
    VkDescriptorSet set = vkBuf.getUboSet();
    VkPipelineBindPoint bp = m_currentPipeline->isCompute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;
    vkCmdBindDescriptorSets(m_cmd, bp, m_currentPipeline->layout.get(), 0, 1, &set, 0, nullptr);
}

void VulkanRHICommandList::bindStorageBuffer(uint32_t slot, const RHIBuffer& buffer) {
    (void)slot;
    if (!m_currentPipeline) return;
    const auto& vkBuf = static_cast<const VulkanRHIBuffer&>(buffer);
    VkDescriptorSet set = vkBuf.getSsboSet();
    VkPipelineBindPoint bp = m_currentPipeline->isCompute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;
    uint32_t firstSet = m_currentPipeline->isCompute ? 0u : 1u;
    vkCmdBindDescriptorSets(m_cmd, bp, m_currentPipeline->layout.get(), firstSet, 1, &set, 0, nullptr);
}

void VulkanRHICommandList::bindTexture(uint32_t slot, const RHITexture& texture, const RHISampler& sampler) {
    if (slot >= MAX_BOUND_SLOTS) return;
    const auto& vkTex = static_cast<const VulkanRHITexture&>(texture);
    const auto& vkSamp = static_cast<const VulkanRHISampler&>(sampler);
    m_boundTextures[slot] = {true, vkTex.getVkImageView(), vkSamp.getVkSampler()};
    m_descriptorsDirty = true;
}

void VulkanRHICommandList::bindStorageTexture(uint32_t slot, const RHITexture& texture) {
    if (slot >= MAX_BOUND_SLOTS) return;
    const auto& vkTex = static_cast<const VulkanRHITexture&>(texture);
    m_boundStorageImages[slot] = {true, vkTex.getVkImageView()};
    m_descriptorsDirty = true;
}

void VulkanRHICommandList::flushDynamicDescriptors() {
    if (!m_descriptorsDirty || !m_currentPipeline || m_currentPipeline->dynamicSetLayout == VK_NULL_HANDLE) {
        return;
    }

    VkDevice device = m_device.getContext().getDevice();
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_dynamicPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_currentPipeline->dynamicSetLayout;

    VkDescriptorSet set = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &set), "Failed to allocate dynamic texture descriptor set");

    std::vector<VkDescriptorImageInfo> imgInfos;
    std::vector<VkWriteDescriptorSet> writes;
    imgInfos.reserve(MAX_BOUND_SLOTS * 2);
    writes.reserve(MAX_BOUND_SLOTS * 2);

    if (!m_currentPipeline->isCompute) {
        for (uint32_t i = 0; i < m_currentPipeline->numSampledTextures; ++i) {
            if (!m_boundTextures[i].valid) continue;
            imgInfos.push_back({m_boundTextures[i].sampler, m_boundTextures[i].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
            VkWriteDescriptorSet w{};
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet = set;
            w.dstBinding = i;
            w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            w.descriptorCount = 1;
            w.pImageInfo = &imgInfos.back();
            writes.push_back(w);
        }
        if (!writes.empty()) {
            vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        }
        vkCmdBindDescriptorSets(m_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_currentPipeline->layout.get(), 2, 1, &set, 0, nullptr);
    } else {
        uint32_t baseTex = m_currentPipeline->numStorageBuffers;
        uint32_t baseImg = baseTex + m_currentPipeline->numSampledTextures;
        for (uint32_t i = 0; i < m_currentPipeline->numSampledTextures; ++i) {
            uint32_t slot = baseTex + i;
            if (!m_boundTextures[slot].valid) continue;
            imgInfos.push_back({m_boundTextures[slot].sampler, m_boundTextures[slot].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
            VkWriteDescriptorSet w{};
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet = set;
            w.dstBinding = slot;
            w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            w.descriptorCount = 1;
            w.pImageInfo = &imgInfos.back();
            writes.push_back(w);
        }
        for (uint32_t i = 0; i < m_currentPipeline->numStorageTextures; ++i) {
            uint32_t slot = baseImg + i;
            if (!m_boundStorageImages[slot].valid) continue;
            imgInfos.push_back({VK_NULL_HANDLE, m_boundStorageImages[slot].view, VK_IMAGE_LAYOUT_GENERAL});
            VkWriteDescriptorSet w{};
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet = set;
            w.dstBinding = slot;
            w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            w.descriptorCount = 1;
            w.pImageInfo = &imgInfos.back();
            writes.push_back(w);
        }
        if (!writes.empty()) {
            vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        }
        vkCmdBindDescriptorSets(m_cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_currentPipeline->layout.get(), 0, 1, &set, 0, nullptr);
    }

    m_descriptorsDirty = false;
}

void VulkanRHICommandList::pushConstants(ShaderStage stages, const void* data, size_t size) {
    if (!m_currentPipeline) return;
    vkCmdPushConstants(
        m_cmd,
        m_currentPipeline->layout.get(),
        toVkShaderStages(stages),
        0,
        static_cast<uint32_t>(size),
        data
    );
}

void VulkanRHICommandList::bindVertexBuffer(uint32_t binding, const RHIBuffer& buffer) {
    const auto& vkBuf = static_cast<const VulkanRHIBuffer&>(buffer);
    VkBuffer buf = vkBuf.getVkBuffer();
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(m_cmd, binding, 1, &buf, &offset);
}

void VulkanRHICommandList::bindIndexBuffer(const RHIBuffer& buffer) {
    const auto& vkBuf = static_cast<const VulkanRHIBuffer&>(buffer);
    vkCmdBindIndexBuffer(m_cmd, vkBuf.getVkBuffer(), 0, VK_INDEX_TYPE_UINT32);
}

void VulkanRHICommandList::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex) {
    flushDynamicDescriptors();
    vkCmdDraw(m_cmd, vertexCount, instanceCount, firstVertex, 0);
}

void VulkanRHICommandList::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex) {
    flushDynamicDescriptors();
    vkCmdDrawIndexed(m_cmd, indexCount, instanceCount, firstIndex, 0, 0);
}

void VulkanRHICommandList::dispatch(uint32_t groupX, uint32_t groupY, uint32_t groupZ) {
    flushDynamicDescriptors();
    vkCmdDispatch(m_cmd, groupX, groupY, groupZ);
}

void VulkanRHICommandList::bufferBarrier(const RHIBuffer& buffer) {
    const auto& vkBuf = static_cast<const VulkanRHIBuffer&>(buffer);
    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = vkBuf.getVkBuffer();
    barrier.offset = 0;
    barrier.size = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(
        m_cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        0,
        0, nullptr,
        1, &barrier,
        0, nullptr
    );
}

void VulkanRHICommandList::textureBarrier(const RHITexture& texture, TextureLayout oldLayout, TextureLayout newLayout) {
    const auto& vkTex = static_cast<const VulkanRHITexture&>(texture);
    auto src = toVkLayoutInfo(oldLayout);
    auto dst = toVkLayoutInfo(newLayout);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = src.layout;
    barrier.newLayout = dst.layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = vkTex.getVkImage();
    barrier.subresourceRange.aspectMask = vkTex.getAspectMask();
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = vkTex.getDesc().mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = vkTex.getDesc().arrayLayers;
    barrier.srcAccessMask = src.access;
    barrier.dstAccessMask = dst.access;

    vkCmdPipelineBarrier(m_cmd, src.stage, dst.stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

VulkanDevice::VulkanDevice(GLFWwindow* window)
    : m_context(window, true) {
    m_swapchain = std::make_unique<VulkanSwapchain>(m_context, window);
    createDescriptorResources();
    createCommandBuffers();
    createSyncObjects();
    createTimestampQueryPool();
}

VulkanDevice::~VulkanDevice() {
    m_context.waitIdle();
    shutdownImGui();
    for (const auto& entry : m_framebufferCache) {
        if (entry.fb != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(m_context.getDevice(), entry.fb, nullptr);
        }
    }
    m_framebufferCache.clear();
    m_renderPassCache.clear();
    m_dynamicLayoutCache.clear();
    if (m_queryPool != VK_NULL_HANDLE) {
        vkDestroyQueryPool(m_context.getDevice(), m_queryPool, nullptr);
        m_queryPool = VK_NULL_HANDLE;
    }
}

void VulkanDevice::createDescriptorResources() {
    VkDevice device = m_context.getDevice();

    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding = 0;
    uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo uboLayoutInfo{};
    uboLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    uboLayoutInfo.bindingCount = 1;
    uboLayoutInfo.pBindings = &uboBinding;

    VkDescriptorSetLayout uboLayout = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &uboLayoutInfo, nullptr, &uboLayout), "Failed to create UBO descriptor set layout");
    m_uboSetLayout = vkh::DescriptorSetLayoutHandle(uboLayout, [device](VkDescriptorSetLayout l) {
        vkDestroyDescriptorSetLayout(device, l, nullptr);
    });

    VkDescriptorSetLayoutBinding ssboBinding{};
    ssboBinding.binding = 0;
    ssboBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    ssboBinding.descriptorCount = 1;
    ssboBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo ssboLayoutInfo{};
    ssboLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ssboLayoutInfo.bindingCount = 1;
    ssboLayoutInfo.pBindings = &ssboBinding;

    VkDescriptorSetLayout ssboLayout = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &ssboLayoutInfo, nullptr, &ssboLayout), "Failed to create SSBO descriptor set layout");
    m_ssboSetLayout = vkh::DescriptorSetLayoutHandle(ssboLayout, [device](VkDescriptorSetLayout l) {
        vkDestroyDescriptorSetLayout(device, l, nullptr);
    });

    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 256};
    poolSizes[1] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1024};

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1280;

    VkDescriptorPool pool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool), "Failed to create RHI descriptor pool");
    m_descriptorPool = vkh::DescriptorPoolHandle(pool, [device](VkDescriptorPool p) {
        vkDestroyDescriptorPool(device, p, nullptr);
    });

    auto createDynamicPool = [&]() {
        std::array<VkDescriptorPoolSize, 2> dynSizes{};
        dynSizes[0] = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256};
        dynSizes[1] = {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 128};

        VkDescriptorPoolCreateInfo dynInfo{};
        dynInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        dynInfo.poolSizeCount = static_cast<uint32_t>(dynSizes.size());
        dynInfo.pPoolSizes = dynSizes.data();
        dynInfo.maxSets = 128;

        VkDescriptorPool dp = VK_NULL_HANDLE;
        VK_CHECK(vkCreateDescriptorPool(device, &dynInfo, nullptr, &dp), "Failed to create dynamic descriptor pool");
        return vkh::DescriptorPoolHandle(dp, [device](VkDescriptorPool p) {
            vkDestroyDescriptorPool(device, p, nullptr);
        });
    };

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        m_dynamicPools[i] = createDynamicPool();
    }
    m_immediateDynamicPool = createDynamicPool();
}

VkDescriptorSetLayout VulkanDevice::getOrCreateGraphicsTexSetLayout(uint32_t numTextures) {
    uint64_t key = (1ull << 60) | static_cast<uint64_t>(numTextures);
    auto it = m_dynamicLayoutCache.find(key);
    if (it != m_dynamicLayoutCache.end()) {
        return it->second.get();
    }

    std::vector<VkDescriptorSetLayoutBinding> bindings(numTextures);
    for (uint32_t i = 0; i < numTextures; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
    }

    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = static_cast<uint32_t>(bindings.size());
    info.pBindings = bindings.data();

    VkDevice device = m_context.getDevice();
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &layout), "Failed to create graphics texture set layout");
    m_dynamicLayoutCache[key] = vkh::DescriptorSetLayoutHandle(layout, [device](VkDescriptorSetLayout l) {
        vkDestroyDescriptorSetLayout(device, l, nullptr);
    });
    return layout;
}

VkDescriptorSetLayout VulkanDevice::getOrCreateComputeSetLayout(uint32_t numStorageBuffers, uint32_t numSampledTextures, uint32_t numStorageTextures) {
    uint64_t key = (2ull << 60) | (static_cast<uint64_t>(numStorageBuffers) << 32) |
                   (static_cast<uint64_t>(numSampledTextures) << 16) | static_cast<uint64_t>(numStorageTextures);
    auto it = m_dynamicLayoutCache.find(key);
    if (it != m_dynamicLayoutCache.end()) {
        return it->second.get();
    }

    std::vector<VkDescriptorSetLayoutBinding> bindings;
    uint32_t bindingIdx = 0;
    for (uint32_t i = 0; i < numStorageBuffers; ++i) {
        VkDescriptorSetLayoutBinding b{};
        b.binding = bindingIdx++;
        b.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        b.descriptorCount = 1;
        b.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        bindings.push_back(b);
    }
    for (uint32_t i = 0; i < numSampledTextures; ++i) {
        VkDescriptorSetLayoutBinding b{};
        b.binding = bindingIdx++;
        b.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        b.descriptorCount = 1;
        b.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        bindings.push_back(b);
    }
    for (uint32_t i = 0; i < numStorageTextures; ++i) {
        VkDescriptorSetLayoutBinding b{};
        b.binding = bindingIdx++;
        b.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        b.descriptorCount = 1;
        b.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        bindings.push_back(b);
    }

    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = static_cast<uint32_t>(bindings.size());
    info.pBindings = bindings.data();

    VkDevice device = m_context.getDevice();
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &layout), "Failed to create compute set layout");
    m_dynamicLayoutCache[key] = vkh::DescriptorSetLayoutHandle(layout, [device](VkDescriptorSetLayout l) {
        vkDestroyDescriptorSetLayout(device, l, nullptr);
    });
    return layout;
}

VkRenderPass VulkanDevice::getOrCreateOffscreenRenderPass(Format colorFormat, bool hasDepth, Format depthFormat, bool clearColor, bool clearDepth) {
    uint64_t key = (static_cast<uint64_t>(colorFormat) << 24) |
                   (static_cast<uint64_t>(hasDepth ? 1 : 0) << 16) |
                   (static_cast<uint64_t>(depthFormat) << 8) |
                   (static_cast<uint64_t>(clearColor ? 1 : 0) << 1) |
                   (clearDepth ? 1ull : 0ull);
    auto it = m_renderPassCache.find(key);
    if (it != m_renderPassCache.end()) {
        return it->second.get();
    }

    std::vector<VkAttachmentDescription> attachments;
    VkAttachmentDescription colorAtt{};
    colorAtt.format = toVkFormat(colorFormat);
    colorAtt.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAtt.loadOp = clearColor ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAtt.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAtt.initialLayout = clearColor ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAtt.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments.push_back(colorAtt);

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    if (hasDepth) {
        VkAttachmentDescription depthAtt{};
        depthAtt.format = toVkFormat(depthFormat);
        depthAtt.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAtt.loadOp = clearDepth ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
        depthAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAtt.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAtt.initialLayout = clearDepth ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAtt.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachments.push_back(depthAtt);
    }

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = hasDepth ? &depthRef : nullptr;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | (hasDepth ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : 0u);

    VkRenderPassCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = static_cast<uint32_t>(attachments.size());
    info.pAttachments = attachments.data();
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dep;

    VkDevice device = m_context.getDevice();
    VkRenderPass rp = VK_NULL_HANDLE;
    VK_CHECK(vkCreateRenderPass(device, &info, nullptr, &rp), "Failed to create offscreen render pass");
    m_renderPassCache[key] = vkh::RenderPassHandle(rp, [device](VkRenderPass p) {
        vkDestroyRenderPass(device, p, nullptr);
    });
    return rp;
}

VkFramebuffer VulkanDevice::getOrCreateFramebuffer(VkRenderPass rp, VkImageView colorView, VkImageView depthView, uint32_t w, uint32_t h) {
    for (const auto& entry : m_framebufferCache) {
        if (entry.rp == rp && entry.colorView == colorView && entry.depthView == depthView) {
            return entry.fb;
        }
    }

    std::vector<VkImageView> views = {colorView};
    if (depthView != VK_NULL_HANDLE) views.push_back(depthView);

    VkFramebufferCreateInfo fbInfo{};
    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass = rp;
    fbInfo.attachmentCount = static_cast<uint32_t>(views.size());
    fbInfo.pAttachments = views.data();
    fbInfo.width = w;
    fbInfo.height = h;
    fbInfo.layers = 1;

    VkFramebuffer fb = VK_NULL_HANDLE;
    VK_CHECK(vkCreateFramebuffer(m_context.getDevice(), &fbInfo, nullptr, &fb), "Failed to create offscreen framebuffer");
    m_framebufferCache.push_back({colorView, depthView, rp, fb});
    return fb;
}

void VulkanDevice::invalidateFramebuffersForView(VkImageView view) {
    VkDevice device = m_context.getDevice();
    m_framebufferCache.erase(
        std::remove_if(m_framebufferCache.begin(), m_framebufferCache.end(), [&](const FramebufferEntry& e) {
            if (e.colorView == view || e.depthView == view) {
                if (e.fb != VK_NULL_HANDLE) {
                    vkDestroyFramebuffer(device, e.fb, nullptr);
                }
                return true;
            }
            return false;
        }),
        m_framebufferCache.end()
    );
}

void VulkanDevice::createCommandBuffers() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_context.getCommandPool();
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

    VK_CHECK(vkAllocateCommandBuffers(m_context.getDevice(), &allocInfo, m_commandBuffers.data()), "Failed to allocate command buffers");
}

void VulkanDevice::createSyncObjects() {
    VkDevice device = m_context.getDevice();
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkSemaphore sem = VK_NULL_HANDLE;
        VK_CHECK(vkCreateSemaphore(device, &semInfo, nullptr, &sem), "Failed to create semaphore");
        m_imageAvailableSemaphores[i] = vkh::SemaphoreHandle(sem, [device](VkSemaphore s) { vkDestroySemaphore(device, s, nullptr); });

        VK_CHECK(vkCreateSemaphore(device, &semInfo, nullptr, &sem), "Failed to create semaphore");
        m_renderFinishedSemaphores[i] = vkh::SemaphoreHandle(sem, [device](VkSemaphore s) { vkDestroySemaphore(device, s, nullptr); });

        VkFence fence = VK_NULL_HANDLE;
        VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &fence), "Failed to create fence");
        m_inFlightFences[i] = vkh::FenceHandle(fence, [device](VkFence f) { vkDestroyFence(device, f, nullptr); });
    }
}

void VulkanDevice::createTimestampQueryPool() {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_context.getPhysicalDevice(), &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> props(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_context.getPhysicalDevice(), &queueFamilyCount, props.data());

    uint32_t gfxFamily = m_context.getQueueFamilies().graphicsFamily.value();
    if (gfxFamily < props.size() && props[gfxFamily].timestampValidBits > 0) {
        VkQueryPoolCreateInfo qpInfo{};
        qpInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        qpInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        qpInfo.queryCount = MAX_FRAMES_IN_FLIGHT * 2;
        if (vkCreateQueryPool(m_context.getDevice(), &qpInfo, nullptr, &m_queryPool) != VK_SUCCESS) {
            m_queryPool = VK_NULL_HANDLE;
        }
    }
}

float VulkanDevice::getAspectRatio() const {
    VkExtent2D extent = m_swapchain->getExtent();
    if (extent.height == 0) return 1.0f;
    return static_cast<float>(extent.width) / static_cast<float>(extent.height);
}

std::unique_ptr<RHIBuffer> VulkanDevice::createBuffer(size_t size, BufferUsage usage, MemoryUsage memoryUsage) {
    return std::make_unique<VulkanRHIBuffer>(
        m_context, size, usage, memoryUsage,
        m_descriptorPool.get(), m_uboSetLayout.get(), m_ssboSetLayout.get()
    );
}

void VulkanDevice::uploadBuffer(RHIBuffer& dst, const void* data, size_t size) {
    auto& vkDst = static_cast<VulkanRHIBuffer&>(dst);
    VulkanBuffer staging(
        m_context,
        static_cast<VkDeviceSize>(size),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    staging.copyFromHost(data, static_cast<VkDeviceSize>(size));
    VulkanBuffer::copyBuffer(m_context, staging.getBuffer(), vkDst.getVkBuffer(), static_cast<VkDeviceSize>(size));
}

std::unique_ptr<RHITexture> VulkanDevice::createTexture(const TextureDesc& desc) {
    return std::make_unique<VulkanRHITexture>(*this, m_context, desc);
}

std::unique_ptr<RHISampler> VulkanDevice::createSampler(const SamplerDesc& desc) {
    return std::make_unique<VulkanRHISampler>(m_context, desc);
}

std::unique_ptr<RHIPipeline> VulkanDevice::createGraphicsPipeline(const GraphicsPipelineDesc& desc) {
    VkDevice device = m_context.getDevice();
    auto pipe = std::make_unique<VulkanRHIPipeline>();
    pipe->isCompute = false;
    pipe->usesUBO = desc.usesUniformBuffer;
    pipe->usesSSBO = desc.usesStorageBuffer;
    pipe->numSampledTextures = desc.numSampledTextures;

    std::vector<VkDescriptorSetLayout> setLayouts;
    if (desc.numSampledTextures > 0) {
        setLayouts.push_back(m_uboSetLayout.get());
        setLayouts.push_back(m_ssboSetLayout.get());
        VkDescriptorSetLayout texLayout = getOrCreateGraphicsTexSetLayout(desc.numSampledTextures);
        pipe->dynamicSetLayout = texLayout;
        setLayouts.push_back(texLayout);
    } else {
        if (desc.usesUniformBuffer) setLayouts.push_back(m_uboSetLayout.get());
        if (desc.usesStorageBuffer) setLayouts.push_back(m_ssboSetLayout.get());
    }

    VkPushConstantRange pcRange{};
    pcRange.stageFlags = toVkShaderStages(desc.pushConstantStages);
    pcRange.offset = 0;
    pcRange.size = desc.pushConstantSize;

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    layoutInfo.pSetLayouts = setLayouts.empty() ? nullptr : setLayouts.data();
    layoutInfo.pushConstantRangeCount = (desc.pushConstantSize > 0) ? 1u : 0u;
    layoutInfo.pPushConstantRanges = (desc.pushConstantSize > 0) ? &pcRange : nullptr;

    VkPipelineLayout layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &layout), "Failed to create Vulkan graphics pipeline layout");
    pipe->layout = vkh::PipelineLayoutHandle(layout, [device](VkPipelineLayout l) {
        vkDestroyPipelineLayout(device, l, nullptr);
    });

    VkShaderModule vert = PipelineBuilder::createShaderModule(device, "shaders/" + desc.vertexShader + ".spv");
    VkShaderModule frag = PipelineBuilder::createShaderModule(device, "shaders/" + desc.fragmentShader + ".spv");

    std::vector<VkVertexInputBindingDescription> bindings;
    for (const auto& b : desc.vertexBindings) {
        bindings.push_back({
            b.binding,
            b.stride,
            b.inputRate == VertexInputRate::Instance ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX
        });
    }

    std::vector<VkVertexInputAttributeDescription> attrs;
    for (const auto& a : desc.vertexAttributes) {
        attrs.push_back({a.location, a.binding, toVkFormat(a.format), a.offset});
    }

    VkRenderPass targetRP = (desc.colorFormat == Format::BGRA8_SRGB)
        ? m_swapchain->getRenderPass()
        : getOrCreateOffscreenRenderPass(desc.colorFormat, desc.hasDepth, desc.depthFormat, true, true);

    PipelineBuilder builder;
    builder.setVertexShader(vert)
        .setFragmentShader(frag)
        .setVertexInput(bindings, attrs)
        .setCullMode(toVkCullMode(desc.cullMode))
        .setPolygonMode(toVkPolygonMode(desc.polygonMode))
        .setDepthState(desc.depthTestEnable, desc.depthWriteEnable, toVkCompareOp(desc.depthCompareOp))
        .setBlendEnable(desc.blendEnable)
        .setPipelineLayout(layout)
        .setRenderPass(targetRP);

    try {
        VkPipeline vkPipe = builder.build(device);
        pipe->pipeline = vkh::PipelineHandle(vkPipe, [device](VkPipeline p) {
            vkDestroyPipeline(device, p, nullptr);
        });
    } catch (const std::exception& e) {
        if (desc.polygonMode == PolygonMode::Line) {
            std::cerr << "Wireframe mode not supported: " << e.what() << std::endl;
        } else {
            vkDestroyShaderModule(device, vert, nullptr);
            vkDestroyShaderModule(device, frag, nullptr);
            throw;
        }
    }

    vkDestroyShaderModule(device, vert, nullptr);
    vkDestroyShaderModule(device, frag, nullptr);
    return pipe;
}

std::unique_ptr<RHIPipeline> VulkanDevice::createComputePipeline(const ComputePipelineDesc& desc) {
    VkDevice device = m_context.getDevice();
    auto pipe = std::make_unique<VulkanRHIPipeline>();
    pipe->isCompute = true;
    pipe->usesUBO = desc.numUniformBuffers > 0;
    pipe->usesSSBO = desc.numStorageBuffers > 0;
    pipe->numStorageBuffers = desc.numStorageBuffers;
    pipe->numSampledTextures = desc.numSampledTextures;
    pipe->numStorageTextures = desc.numStorageTextures;

    std::vector<VkDescriptorSetLayout> setLayouts;
    if (desc.numSampledTextures > 0 || desc.numStorageTextures > 0) {
        VkDescriptorSetLayout compLayout = getOrCreateComputeSetLayout(
            desc.numStorageBuffers, desc.numSampledTextures, desc.numStorageTextures
        );
        pipe->dynamicSetLayout = compLayout;
        setLayouts.push_back(compLayout);
    } else if (desc.numStorageBuffers > 0) {
        setLayouts.push_back(m_ssboSetLayout.get());
    }

    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pcRange.offset = 0;
    pcRange.size = desc.pushConstantSize;

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    layoutInfo.pSetLayouts = setLayouts.empty() ? nullptr : setLayouts.data();
    layoutInfo.pushConstantRangeCount = (desc.pushConstantSize > 0) ? 1u : 0u;
    layoutInfo.pPushConstantRanges = (desc.pushConstantSize > 0) ? &pcRange : nullptr;

    VkPipelineLayout layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &layout), "Failed to create Vulkan compute pipeline layout");
    pipe->layout = vkh::PipelineLayoutHandle(layout, [device](VkPipelineLayout l) {
        vkDestroyPipelineLayout(device, l, nullptr);
    });

    VkShaderModule comp = PipelineBuilder::createShaderModule(device, "shaders/" + desc.computeShader + ".spv");
    VkPipeline vkPipe = PipelineBuilder::createComputePipeline(device, comp, layout);
    pipe->pipeline = vkh::PipelineHandle(vkPipe, [device](VkPipeline p) {
        vkDestroyPipeline(device, p, nullptr);
    });
    vkDestroyShaderModule(device, comp, nullptr);
    return pipe;
}

RHICommandList* VulkanDevice::beginFrame(GLFWwindow* window, uint32_t& outFrameIndex) {
    VkDevice device = m_context.getDevice();
    VkFence fence = m_inFlightFences[m_currentFrame].get();
    vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);

    vkResetDescriptorPool(device, m_dynamicPools[m_currentFrame].get(), 0);

    if (m_queryPool != VK_NULL_HANDLE && m_frameHasTimestamps[m_currentFrame]) {
        uint64_t timestamps[2] = {0, 0};
        VkResult qRes = vkGetQueryPoolResults(
            device, m_queryPool, m_currentFrame * 2, 2, sizeof(timestamps), timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT
        );
        if (qRes == VK_SUCCESS && timestamps[1] >= timestamps[0]) {
            m_lastGpuFrameTimeMs = static_cast<double>(timestamps[1] - timestamps[0]) * static_cast<double>(m_context.getTimestampPeriod()) / 1e6;
        }
    }

    VkResult result = vkAcquireNextImageKHR(
        device,
        m_swapchain->getSwapchain(),
        UINT64_MAX,
        m_imageAvailableSemaphores[m_currentFrame].get(),
        VK_NULL_HANDLE,
        &m_currentImageIndex
    );

    if (result == VK_ERROR_OUT_OF_DATE_KHR || m_framebufferResized) {
        m_framebufferResized = false;
        m_swapchain->recreate(m_context, window);
        return nullptr;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swapchain image!");
    }

    vkResetFences(device, 1, &fence);

    VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo), "Failed to begin command buffer recording");

    if (m_queryPool != VK_NULL_HANDLE) {
        vkCmdResetQueryPool(cmd, m_queryPool, m_currentFrame * 2, 2);
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_queryPool, m_currentFrame * 2);
    }

    outFrameIndex = m_currentFrame;
    m_activeCmdList = std::make_unique<VulkanRHICommandList>(*this, cmd, m_dynamicPools[m_currentFrame].get());
    return m_activeCmdList.get();
}

void VulkanDevice::endFrame(GLFWwindow* window) {
    VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];

    if (m_queryPool != VK_NULL_HANDLE) {
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_queryPool, m_currentFrame * 2 + 1);
        m_frameHasTimestamps[m_currentFrame] = true;
    }

    VK_CHECK(vkEndCommandBuffer(cmd), "Failed to end command buffer recording");

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {m_imageAvailableSemaphores[m_currentFrame].get()};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    VkSemaphore signalSemaphores[] = {m_renderFinishedSemaphores[m_currentFrame].get()};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    VK_CHECK(vkQueueSubmit(m_context.getGraphicsQueue(), 1, &submitInfo, m_inFlightFences[m_currentFrame].get()), "Failed to submit draw command buffer");

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapchains[] = {m_swapchain->getSwapchain()};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &m_currentImageIndex;

    VkResult result = vkQueuePresentKHR(m_context.getPresentQueue(), &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_framebufferResized) {
        m_framebufferResized = false;
        m_swapchain->recreate(m_context, window);
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swapchain image!");
    }

    m_activeCmdList.reset();
    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanDevice::executeImmediateCompute(const std::function<void(RHICommandList&)>& fn) {
    vkResetDescriptorPool(m_context.getDevice(), m_immediateDynamicPool.get(), 0);
    VkCommandBuffer cmd = m_context.beginSingleTimeCommands();
    VulkanRHICommandList cmdList(*this, cmd, m_immediateDynamicPool.get());
    fn(cmdList);
    m_context.executeSingleTimeCommands(cmd);
}

void VulkanDevice::initImGui(GLFWwindow* window) {
    if (m_imGuiInitialized) return;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(window, true);

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = m_context.getInstance();
    initInfo.PhysicalDevice = m_context.getPhysicalDevice();
    initInfo.Device = m_context.getDevice();
    initInfo.QueueFamily = m_context.getQueueFamilies().graphicsFamily.value();
    initInfo.Queue = m_context.getGraphicsQueue();
    initInfo.RenderPass = m_swapchain->getRenderPass();
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = static_cast<uint32_t>(m_swapchain->getImageCount());
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.DescriptorPoolSize = 16;

    if (!ImGui_ImplVulkan_Init(&initInfo)) {
        throw std::runtime_error("Failed to initialize ImGui Vulkan backend");
    }
    ImGui_ImplVulkan_CreateFontsTexture();
    m_imGuiInitialized = true;
}

void VulkanDevice::beginImGuiFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void VulkanDevice::renderImGui(RHICommandList& cmd) {
    ImGui::Render();
    auto& vkCmd = static_cast<VulkanRHICommandList&>(cmd);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vkCmd.getVkCommandBuffer());
}

void VulkanDevice::shutdownImGui() {
    if (!m_imGuiInitialized) return;
    m_context.waitIdle();
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    m_imGuiInitialized = false;
}

} // namespace rhi
