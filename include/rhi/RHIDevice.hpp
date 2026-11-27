#pragma once

#include "rhi/RHITypes.hpp"
#include <functional>
#include <memory>
#include <string>

struct GLFWwindow;

namespace rhi {

class RHIBuffer {
public:
    virtual ~RHIBuffer() = default;
    virtual void* map() = 0;
    virtual void unmap() = 0;
    virtual void copyFromHost(const void* data, size_t size) = 0;
    virtual size_t getSize() const = 0;
};

class RHITexture {
public:
    virtual ~RHITexture() = default;
    virtual uint32_t getWidth() const = 0;
    virtual uint32_t getHeight() const = 0;
    virtual Format getFormat() const = 0;
};

class RHISampler {
public:
    virtual ~RHISampler() = default;
};

class RHIPipeline {
public:
    virtual ~RHIPipeline() = default;
    virtual bool isValid() const = 0;
};

class RHICommandList {
public:
    virtual ~RHICommandList() = default;

    virtual void beginRenderPass(const RenderPassDesc& desc) = 0;
    virtual void beginRenderPass(const ClearColor& clearColor, float clearDepth = 1.0f) {
        RenderPassDesc desc{};
        desc.clearColor = clearColor;
        desc.clearDepth = clearDepth;
        beginRenderPass(desc);
    }
    virtual void endRenderPass() = 0;

    virtual void setViewport(float x, float y, float width, float height) = 0;
    virtual void setScissor(int32_t x, int32_t y, uint32_t width, uint32_t height) = 0;

    virtual void bindGraphicsPipeline(const RHIPipeline& pipeline) = 0;
    virtual void bindComputePipeline(const RHIPipeline& pipeline) = 0;

    virtual void bindUniformBuffer(uint32_t slot, const RHIBuffer& buffer) = 0;
    virtual void bindStorageBuffer(uint32_t slot, const RHIBuffer& buffer) = 0;
    virtual void bindTexture(uint32_t slot, const RHITexture& texture, const RHISampler& sampler) = 0;
    virtual void bindStorageTexture(uint32_t slot, const RHITexture& texture) = 0;
    virtual void pushConstants(ShaderStage stages, const void* data, size_t size) = 0;

    virtual void bindVertexBuffer(uint32_t binding, const RHIBuffer& buffer) = 0;
    virtual void bindIndexBuffer(const RHIBuffer& buffer) = 0;

    virtual void draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0) = 0;
    virtual void drawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0) = 0;
    virtual void dispatch(uint32_t groupX, uint32_t groupY, uint32_t groupZ) = 0;

    virtual void bufferBarrier(const RHIBuffer& buffer) = 0;
    virtual void textureBarrier(const RHITexture& texture, TextureLayout oldLayout, TextureLayout newLayout) = 0;
};

class RHIDevice {
public:
    virtual ~RHIDevice() = default;

    static std::unique_ptr<RHIDevice> create(BackendType backend, GLFWwindow* window);

    virtual BackendType getBackendType() const = 0;
    virtual const char* getBackendName() const = 0;
    virtual std::string getDeviceName() const = 0;
    virtual bool supportsHardwareRayTracing() const { return false; }

    virtual uint32_t getSwapchainWidth() const = 0;
    virtual uint32_t getSwapchainHeight() const = 0;
    virtual float getAspectRatio() const = 0;
    virtual double getLastGpuFrameTimeMs() const = 0;

    virtual void onResize(GLFWwindow* window) = 0;
    virtual void waitIdle() = 0;

    virtual std::unique_ptr<RHIBuffer> createBuffer(size_t size, BufferUsage usage, MemoryUsage memoryUsage) = 0;
    virtual void uploadBuffer(RHIBuffer& dst, const void* data, size_t size) = 0;

    virtual std::unique_ptr<RHITexture> createTexture(const TextureDesc& desc) = 0;
    virtual std::unique_ptr<RHISampler> createSampler(const SamplerDesc& desc) = 0;

    virtual std::unique_ptr<RHIPipeline> createGraphicsPipeline(const GraphicsPipelineDesc& desc) = 0;
    virtual std::unique_ptr<RHIPipeline> createComputePipeline(const ComputePipelineDesc& desc) = 0;

    virtual RHICommandList* beginFrame(GLFWwindow* window, uint32_t& outFrameIndex) = 0;
    virtual void endFrame(GLFWwindow* window) = 0;

    virtual void executeImmediateCompute(const std::function<void(RHICommandList&)>& fn) = 0;

    virtual void initImGui(GLFWwindow* window) = 0;
    virtual void beginImGuiFrame() = 0;
    virtual void renderImGui(RHICommandList& cmd) = 0;
    virtual void shutdownImGui() = 0;
};

} // namespace rhi
