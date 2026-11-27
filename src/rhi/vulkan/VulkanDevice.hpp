#pragma once

#include "rhi/RHIDevice.hpp"
#include "core/VulkanContext.hpp"
#include "core/VulkanSwapchain.hpp"
#include "core/VulkanBuffer.hpp"
#include "core/VulkanResource.hpp"
#include <array>
#include <memory>

namespace rhi {

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

class VulkanRHIPipeline final : public RHIPipeline {
public:
    VulkanRHIPipeline() = default;
    bool isValid() const override { return static_cast<bool>(pipeline); }

    vkh::PipelineLayoutHandle layout;
    vkh::PipelineHandle pipeline;
    bool isCompute = false;
    bool usesUBO = false;
    bool usesSSBO = false;
};

class VulkanRHICommandList final : public RHICommandList {
public:
    VulkanRHICommandList(VkCommandBuffer cmd, VkRenderPass renderPass, VkFramebuffer framebuffer, VkExtent2D extent)
        : m_cmd(cmd), m_renderPass(renderPass), m_framebuffer(framebuffer), m_extent(extent) {}

    void beginRenderPass(const ClearColor& clearColor, float clearDepth = 1.0f) override;
    void endRenderPass() override;

    void setViewport(float x, float y, float width, float height) override;
    void setScissor(int32_t x, int32_t y, uint32_t width, uint32_t height) override;

    void bindGraphicsPipeline(const RHIPipeline& pipeline) override;
    void bindComputePipeline(const RHIPipeline& pipeline) override;

    void bindUniformBuffer(uint32_t slot, const RHIBuffer& buffer) override;
    void bindStorageBuffer(uint32_t slot, const RHIBuffer& buffer) override;
    void pushConstants(ShaderStage stages, const void* data, size_t size) override;

    void bindVertexBuffer(uint32_t binding, const RHIBuffer& buffer) override;
    void bindIndexBuffer(const RHIBuffer& buffer) override;

    void draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0) override;
    void drawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0) override;
    void dispatch(uint32_t groupX, uint32_t groupY, uint32_t groupZ) override;

    void bufferBarrier(const RHIBuffer& buffer) override;

    VkCommandBuffer getVkCommandBuffer() const { return m_cmd; }

private:
    VkCommandBuffer m_cmd = VK_NULL_HANDLE;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkFramebuffer m_framebuffer = VK_NULL_HANDLE;
    VkExtent2D m_extent{0, 0};
    const VulkanRHIPipeline* m_currentPipeline = nullptr;
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

    std::unique_ptr<RHIPipeline> createGraphicsPipeline(const GraphicsPipelineDesc& desc) override;
    std::unique_ptr<RHIPipeline> createComputePipeline(const ComputePipelineDesc& desc) override;

    RHICommandList* beginFrame(GLFWwindow* window, uint32_t& outFrameIndex) override;
    void endFrame(GLFWwindow* window) override;

    void executeImmediateCompute(const std::function<void(RHICommandList&)>& fn) override;

    void initImGui(GLFWwindow* window) override;
    void beginImGuiFrame() override;
    void renderImGui(RHICommandList& cmd) override;
    void shutdownImGui() override;

private:
    void createDescriptorResources();
    void createCommandBuffers();
    void createSyncObjects();
    void createTimestampQueryPool();

private:
    VulkanContext m_context;
    std::unique_ptr<VulkanSwapchain> m_swapchain;

    vkh::DescriptorPoolHandle m_descriptorPool;
    vkh::DescriptorSetLayoutHandle m_uboSetLayout;
    vkh::DescriptorSetLayoutHandle m_ssboSetLayout;

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
