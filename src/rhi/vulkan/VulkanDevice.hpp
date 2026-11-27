#pragma once

#include "rhi/RHIDevice.hpp"
#include "core/VulkanContext.hpp"
#include "core/VulkanSwapchain.hpp"
#include "core/VulkanBuffer.hpp"
#include "core/VulkanImage.hpp"
#include "core/VulkanResource.hpp"
#include <array>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace rhi {

class VulkanDevice;

class VulkanRHIBuffer final : public RHIBuffer {
public:
    VulkanRHIBuffer(const VulkanContext& context, size_t size, BufferUsage usage, MemoryUsage memUsage,
                    VkDescriptorPool descriptorPool, VkDescriptorSetLayout uboLayout, VkDescriptorSetLayout ssboLayout);
    ~VulkanRHIBuffer() override;

    void* map() override { return m_buffer.map(); }
    void unmap() override { m_buffer.unmap(); }
    void copyFromHost(const void* data, size_t size) override { m_buffer.copyFromHost(data, size); }
    size_t getSize() const override { return static_cast<size_t>(m_buffer.getSize()); }

    VkBuffer getVkBuffer() const { return m_buffer.getBuffer(); }
    VkDescriptorSet getUboSet() const { return m_uboSet; }
    VkDescriptorSet getSsboSet() const { return m_ssboSet; }

private:
    VulkanBuffer m_buffer;
    VkDevice m_device = VK_NULL_HANDLE;
    VkDescriptorPool m_pool = VK_NULL_HANDLE;
    VkDescriptorSet m_uboSet = VK_NULL_HANDLE;
    VkDescriptorSet m_ssboSet = VK_NULL_HANDLE;
};

class VulkanRHITexture final : public RHITexture {
public:
    VulkanRHITexture(VulkanDevice& owner, const VulkanContext& context, const TextureDesc& desc);
    ~VulkanRHITexture() override;

    uint32_t getWidth() const override { return m_desc.width; }
    uint32_t getHeight() const override { return m_desc.height; }
    Format getFormat() const override { return m_desc.format; }

    VkImage getVkImage() const { return m_image.getImage(); }
    VkImageView getVkImageView() const { return m_image.getView(); }
    VkFormat getVkFormat() const { return m_image.getFormat(); }
    VkImageAspectFlags getAspectMask() const { return m_aspectMask; }
    const TextureDesc& getDesc() const { return m_desc; }

private:
    VulkanDevice& m_owner;
    VulkanImage m_image;
    TextureDesc m_desc;
    VkImageAspectFlags m_aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
};

class VulkanRHISampler final : public RHISampler {
public:
    VulkanRHISampler(const VulkanContext& context, const SamplerDesc& desc);
    ~VulkanRHISampler() override;

    VkSampler getVkSampler() const { return m_sampler; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
};

class VulkanRHIPipeline final : public RHIPipeline {
public:
    VulkanRHIPipeline() = default;
    bool isValid() const override { return static_cast<bool>(pipeline); }

    vkh::PipelineLayoutHandle layout;
    vkh::PipelineHandle pipeline;
    VkDescriptorSetLayout dynamicSetLayout = VK_NULL_HANDLE; // set 2 (graphics) or set 0 (compute with textures)
    bool isCompute = false;
    bool usesUBO = false;
    bool usesSSBO = false;
    uint32_t numStorageBuffers = 0;
    uint32_t numSampledTextures = 0;
    uint32_t numStorageTextures = 0;
};

class VulkanRHICommandList final : public RHICommandList {
public:
    VulkanRHICommandList(VulkanDevice& device, VkCommandBuffer cmd, VkDescriptorPool dynamicPool)
        : m_device(device), m_cmd(cmd), m_dynamicPool(dynamicPool) {}

    void beginRenderPass(const RenderPassDesc& desc) override;
    void endRenderPass() override;

    void setViewport(float x, float y, float width, float height) override;
    void setScissor(int32_t x, int32_t y, uint32_t width, uint32_t height) override;

    void bindGraphicsPipeline(const RHIPipeline& pipeline) override;
    void bindComputePipeline(const RHIPipeline& pipeline) override;

    void bindUniformBuffer(uint32_t slot, const RHIBuffer& buffer) override;
    void bindStorageBuffer(uint32_t slot, const RHIBuffer& buffer) override;
    void bindTexture(uint32_t slot, const RHITexture& texture, const RHISampler& sampler) override;
    void bindStorageTexture(uint32_t slot, const RHITexture& texture) override;
    void pushConstants(ShaderStage stages, const void* data, size_t size) override;

    void bindVertexBuffer(uint32_t binding, const RHIBuffer& buffer) override;
    void bindIndexBuffer(const RHIBuffer& buffer) override;

    void draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0) override;
    void drawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0) override;
    void dispatch(uint32_t groupX, uint32_t groupY, uint32_t groupZ) override;

    void bufferBarrier(const RHIBuffer& buffer) override;
    void textureBarrier(const RHITexture& texture, TextureLayout oldLayout, TextureLayout newLayout) override;

    VkCommandBuffer getVkCommandBuffer() const { return m_cmd; }

private:
    void flushDynamicDescriptors();

    VulkanDevice& m_device;
    VkCommandBuffer m_cmd = VK_NULL_HANDLE;
    VkDescriptorPool m_dynamicPool = VK_NULL_HANDLE;
    const VulkanRHIPipeline* m_currentPipeline = nullptr;

    struct BoundTexture {
        bool valid = false;
        VkImageView view = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
    };
    struct BoundStorageImage {
        bool valid = false;
        VkImageView view = VK_NULL_HANDLE;
    };

    static constexpr size_t MAX_BOUND_SLOTS = 8;
    std::array<BoundTexture, MAX_BOUND_SLOTS> m_boundTextures{};
    std::array<BoundStorageImage, MAX_BOUND_SLOTS> m_boundStorageImages{};
    bool m_descriptorsDirty = false;
};

class VulkanDevice final : public RHIDevice {
public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    explicit VulkanDevice(GLFWwindow* window);
    ~VulkanDevice() override;

    BackendType getBackendType() const override { return BackendType::Vulkan; }
    const char* getBackendName() const override { return "Vulkan"; }
    std::string getDeviceName() const override { return m_context.getDeviceName(); }

    uint32_t getSwapchainWidth() const override { return m_swapchain->getExtent().width; }
    uint32_t getSwapchainHeight() const override { return m_swapchain->getExtent().height; }
    float getAspectRatio() const override;
    double getLastGpuFrameTimeMs() const override { return m_lastGpuFrameTimeMs; }

    void onResize(GLFWwindow* window) override { m_framebufferResized = true; (void)window; }
    void waitIdle() override { m_context.waitIdle(); }

    std::unique_ptr<RHIBuffer> createBuffer(size_t size, BufferUsage usage, MemoryUsage memoryUsage) override;
    void uploadBuffer(RHIBuffer& dst, const void* data, size_t size) override;

    std::unique_ptr<RHITexture> createTexture(const TextureDesc& desc) override;
    std::unique_ptr<RHISampler> createSampler(const SamplerDesc& desc) override;

    std::unique_ptr<RHIPipeline> createGraphicsPipeline(const GraphicsPipelineDesc& desc) override;
    std::unique_ptr<RHIPipeline> createComputePipeline(const ComputePipelineDesc& desc) override;

    RHICommandList* beginFrame(GLFWwindow* window, uint32_t& outFrameIndex) override;
    void endFrame(GLFWwindow* window) override;

    void executeImmediateCompute(const std::function<void(RHICommandList&)>& fn) override;

    void initImGui(GLFWwindow* window) override;
    void beginImGuiFrame() override;
    void renderImGui(RHICommandList& cmd) override;
    void shutdownImGui() override;

    // Helpers for VulkanRHICommandList & VulkanRHITexture
    const VulkanContext& getContext() const { return m_context; }
    const VulkanSwapchain& getSwapchain() const { return *m_swapchain; }
    uint32_t getCurrentImageIndex() const { return m_currentImageIndex; }
    VkRenderPass getOrCreateOffscreenRenderPass(bool hasColor, Format colorFormat, bool hasDepth, Format depthFormat, bool clearColor, bool clearDepth);
    VkFramebuffer getOrCreateFramebuffer(VkRenderPass rp, VkImageView colorView, VkImageView depthView, uint32_t w, uint32_t h);
    void invalidateFramebuffersForView(VkImageView view);

private:
    void createDescriptorResources();
    void createCommandBuffers();
    void createSyncObjects();
    void createTimestampQueryPool();

    VkDescriptorSetLayout getOrCreateGraphicsTexSetLayout(uint32_t numTextures);
    VkDescriptorSetLayout getOrCreateComputeSetLayout(uint32_t numStorageBuffers, uint32_t numSampledTextures, uint32_t numStorageTextures);

private:
    VulkanContext m_context;
    std::unique_ptr<VulkanSwapchain> m_swapchain;

    vkh::DescriptorPoolHandle m_descriptorPool;
    vkh::DescriptorSetLayoutHandle m_uboSetLayout;
    vkh::DescriptorSetLayoutHandle m_ssboSetLayout;

    std::array<vkh::DescriptorPoolHandle, MAX_FRAMES_IN_FLIGHT> m_dynamicPools;
    vkh::DescriptorPoolHandle m_immediateDynamicPool;
    std::unordered_map<uint64_t, vkh::DescriptorSetLayoutHandle> m_dynamicLayoutCache;

    std::unordered_map<uint64_t, vkh::RenderPassHandle> m_renderPassCache;
    struct FramebufferEntry {
        VkImageView colorView = VK_NULL_HANDLE;
        VkImageView depthView = VK_NULL_HANDLE;
        VkRenderPass rp = VK_NULL_HANDLE;
        VkFramebuffer fb = VK_NULL_HANDLE;
    };
    std::vector<FramebufferEntry> m_framebufferCache;

    std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> m_commandBuffers{};
    std::array<vkh::SemaphoreHandle, MAX_FRAMES_IN_FLIGHT> m_imageAvailableSemaphores;
    std::array<vkh::SemaphoreHandle, MAX_FRAMES_IN_FLIGHT> m_renderFinishedSemaphores;
    std::array<vkh::FenceHandle, MAX_FRAMES_IN_FLIGHT> m_inFlightFences;

    std::unique_ptr<VulkanRHICommandList> m_activeCmdList;
    uint32_t m_currentFrame = 0;
    uint32_t m_currentImageIndex = 0;
    bool m_framebufferResized = false;

    VkQueryPool m_queryPool = VK_NULL_HANDLE;
    std::array<bool, MAX_FRAMES_IN_FLIGHT> m_frameHasTimestamps{};
    double m_lastGpuFrameTimeMs = 0.0;
    bool m_imGuiInitialized = false;
};

} // namespace rhi
