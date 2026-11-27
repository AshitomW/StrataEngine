#pragma once

#include "rhi/RHIDevice.hpp"
#include <array>
#include <memory>
#include <unordered_map>

namespace rhi {

class MetalDevice final : public RHIDevice {
public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    explicit MetalDevice(GLFWwindow* window);
    ~MetalDevice() override;

    BackendType getBackendType() const override { return BackendType::Metal; }
    const char* getBackendName() const override { return "Apple Metal"; }
    std::string getDeviceName() const override;
    bool supportsHardwareRayTracing() const override;

    uint32_t getSwapchainWidth() const override { return m_width; }
    uint32_t getSwapchainHeight() const override { return m_height; }
    float getAspectRatio() const override;
    double getLastGpuFrameTimeMs() const override;

    void onResize(GLFWwindow* window) override;
    void waitIdle() override;

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
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
};

} // namespace rhi
