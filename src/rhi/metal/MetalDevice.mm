#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Cocoa/Cocoa.h>
#import <CoreFoundation/CoreFoundation.h>

#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "rhi/metal/MetalDevice.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_metal.h"

#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace rhi {

namespace {

std::string readMetalShaderSource(const std::string& baseName) {
    const std::string filename = "shaders/" + baseName + ".metal";
    const std::vector<std::filesystem::path> searchPaths = {
        filename,
        std::filesystem::path("build") / filename,
        std::filesystem::path("..") / filename,
        std::filesystem::path("../build") / filename,
        baseName + ".metal",
        std::filesystem::path("build/shaders") / (baseName + ".metal")
    };

    for (const auto& path : searchPaths) {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (file.is_open()) {
            size_t size = static_cast<size_t>(file.tellg());
            std::string buffer(size, '\0');
            file.seekg(0);
            file.read(buffer.data(), static_cast<std::streamsize>(size));
            return buffer;
        }
    }

    throw std::runtime_error("Failed to open Metal shader file: " + filename);
}

MTLPixelFormat toMTLPixelFormat(Format fmt) {
    switch (fmt) {
        case Format::RGBA8_UNORM: return MTLPixelFormatRGBA8Unorm;
        case Format::BGRA8_SRGB:  return MTLPixelFormatBGRA8Unorm_sRGB;
        case Format::RGBA16F:     return MTLPixelFormatRGBA16Float;
        case Format::R16F:        return MTLPixelFormatR16Float;
        case Format::RG16F:       return MTLPixelFormatRG16Float;
        case Format::D32F:        return MTLPixelFormatDepth32Float;
        case Format::R32F:        return MTLPixelFormatR32Float;
        case Format::RG32F:       return MTLPixelFormatRG32Float;
        case Format::RGBA32F:     return MTLPixelFormatRGBA32Float;
        default:                  return MTLPixelFormatRGBA16Float;
    }
}

MTLSamplerMinMagFilter toMTLFilter(Filter f) {
    return (f == Filter::Nearest) ? MTLSamplerMinMagFilterNearest : MTLSamplerMinMagFilterLinear;
}

MTLSamplerAddressMode toMTLAddressMode(AddressMode mode) {
    switch (mode) {
        case AddressMode::Repeat:      return MTLSamplerAddressModeRepeat;
        case AddressMode::ClampToEdge: return MTLSamplerAddressModeClampToEdge;
    }
    return MTLSamplerAddressModeClampToEdge;
}

MTLVertexFormat toMTLVertexFormat(Format fmt) {
    switch (fmt) {
        case Format::R32F:    return MTLVertexFormatFloat;
        case Format::RG32F:   return MTLVertexFormatFloat2;
        case Format::RGB32F:  return MTLVertexFormatFloat3;
        case Format::RGBA32F: return MTLVertexFormatFloat4;
        case Format::RGBA8_UNORM: return MTLVertexFormatUChar4Normalized;
        default: return MTLVertexFormatFloat4;
    }
}

MTLCullMode toMTLCullMode(CullMode mode) {
    switch (mode) {
        case CullMode::None:  return MTLCullModeNone;
        case CullMode::Back:  return MTLCullModeBack;
        case CullMode::Front: return MTLCullModeFront;
    }
    return MTLCullModeNone;
}

MTLTriangleFillMode toMTLFillMode(PolygonMode mode) {
    return (mode == PolygonMode::Line) ? MTLTriangleFillModeLines : MTLTriangleFillModeFill;
}

MTLCompareFunction toMTLCompareOp(CompareOp op) {
    switch (op) {
        case CompareOp::Less:        return MTLCompareFunctionLess;
        case CompareOp::LessOrEqual: return MTLCompareFunctionLessEqual;
        case CompareOp::Greater:     return MTLCompareFunctionGreater;
        case CompareOp::Always:      return MTLCompareFunctionAlways;
    }
    return MTLCompareFunctionLessEqual;
}

class MetalRHIBuffer final : public RHIBuffer {
public:
    MetalRHIBuffer(id<MTLDevice> device, size_t size, MemoryUsage memUsage)
        : m_size(size) {
        MTLResourceOptions opts = (memUsage == MemoryUsage::GpuOnly)
            ? MTLResourceStorageModePrivate
            : MTLResourceStorageModeShared;
        m_buffer = [device newBufferWithLength:static_cast<NSUInteger>(size) options:opts];
        if (!m_buffer) {
            throw std::runtime_error("Failed to allocate Metal buffer");
        }
    }

    void* map() override {
        if (m_buffer.storageMode == MTLStorageModePrivate) {
            return nullptr;
        }
        return [m_buffer contents];
    }

    void unmap() override {}

    void copyFromHost(const void* data, size_t size) override {
        if (size > m_size) {
            throw std::runtime_error("MetalRHIBuffer::copyFromHost: size exceeds capacity");
        }
        void* dst = map();
        if (!dst) {
            throw std::runtime_error("MetalRHIBuffer::copyFromHost called on GpuOnly buffer");
        }
        std::memcpy(dst, data, size);
    }

    size_t getSize() const override { return m_size; }
    id<MTLBuffer> getMTLBuffer() const { return m_buffer; }

private:
    id<MTLBuffer> m_buffer = nil;
    size_t m_size = 0;
};

class MetalRHITexture final : public RHITexture {
public:
    MetalRHITexture(id<MTLDevice> device, const TextureDesc& desc)
        : m_desc(desc) {
        MTLTextureDescriptor* td = [[MTLTextureDescriptor alloc] init];
        td.pixelFormat = toMTLPixelFormat(desc.format);
        td.width = desc.width;
        td.height = desc.height;
        td.depth = desc.depth;
        td.mipmapLevelCount = desc.mipLevels;
        td.arrayLength = desc.arrayLayers;
        if (desc.type == TextureType::Tex2DArray) {
            td.textureType = MTLTextureType2DArray;
        } else if (desc.type == TextureType::Tex3D) {
            td.textureType = MTLTextureType3D;
        } else if (desc.type == TextureType::Cube) {
            td.textureType = MTLTextureTypeCube;
        } else {
            td.textureType = MTLTextureType2D;
        }
        td.storageMode = MTLStorageModePrivate;

        MTLTextureUsage u = 0;
        if (hasFlag(desc.usage, TextureUsage::Sampled)) u |= MTLTextureUsageShaderRead;
        if (hasFlag(desc.usage, TextureUsage::Storage)) u |= MTLTextureUsageShaderWrite | MTLTextureUsageShaderRead;
        if (hasFlag(desc.usage, TextureUsage::ColorAttachment) || hasFlag(desc.usage, TextureUsage::DepthAttachment)) {
            u |= MTLTextureUsageRenderTarget;
        }
        td.usage = u;

        m_texture = [device newTextureWithDescriptor:td];
        if (!m_texture) {
            throw std::runtime_error("Failed to create Metal texture");
        }
    }

    uint32_t getWidth() const override { return m_desc.width; }
    uint32_t getHeight() const override { return m_desc.height; }
    Format getFormat() const override { return m_desc.format; }
    id<MTLTexture> getMTLTexture() const { return m_texture; }

private:
    TextureDesc m_desc;
    id<MTLTexture> m_texture = nil;
};

class MetalRHISampler final : public RHISampler {
public:
    MetalRHISampler(id<MTLDevice> device, const SamplerDesc& desc) {
        MTLSamplerDescriptor* sd = [[MTLSamplerDescriptor alloc] init];
        sd.minFilter = toMTLFilter(desc.minFilter);
        sd.magFilter = toMTLFilter(desc.magFilter);
        sd.mipFilter = (desc.minFilter == Filter::Nearest) ? MTLSamplerMipFilterNearest : MTLSamplerMipFilterLinear;
        sd.sAddressMode = toMTLAddressMode(desc.addressU);
        sd.tAddressMode = toMTLAddressMode(desc.addressV);
        sd.rAddressMode = toMTLAddressMode(desc.addressW);
        sd.maxAnisotropy = static_cast<NSUInteger>(std::max(1.0f, desc.maxAnisotropy));
        if (desc.compareEnable) {
            sd.compareFunction = MTLCompareFunctionLessEqual;
        }
        m_sampler = [device newSamplerStateWithDescriptor:sd];
    }

    id<MTLSamplerState> getMTLSampler() const { return m_sampler; }

private:
    id<MTLSamplerState> m_sampler = nil;
};

class MetalRHIPipeline final : public RHIPipeline {
public:
    bool isValid() const override {
        return isCompute ? (computeState != nil) : (renderState != nil);
    }

    bool isCompute = false;
    id<MTLRenderPipelineState> renderState = nil;
    id<MTLDepthStencilState> depthState = nil;
    MTLCullMode cullMode = MTLCullModeNone;
    MTLTriangleFillMode fillMode = MTLTriangleFillModeFill;

    int32_t vertUboSlot = -1;
    int32_t fragUboSlot = -1;
    int32_t vertSsboSlot = -1;
    int32_t fragSsboSlot = -1;
    int32_t vertPcSlot = -1;
    int32_t fragPcSlot = -1;

    id<MTLComputePipelineState> computeState = nil;
    uint32_t localSizeX = 16;
    uint32_t localSizeY = 16;
    uint32_t localSizeZ = 1;
    int32_t compUboSlot = -1;
    int32_t compSsboSlot = -1;
    int32_t compPcSlot = -1;
};

class MetalRHICommandList final : public RHICommandList {
public:
    MetalRHICommandList(id<MTLCommandBuffer> cmdBuf, MTLRenderPassDescriptor* rpDesc)
        : m_cmdBuf(cmdBuf), m_rpDesc(rpDesc) {}

    ~MetalRHICommandList() override {
        endActiveEncoders();
    }

    void beginRenderPass(const RenderPassDesc& desc) override {
        endActiveEncoders();
        MTLRenderPassDescriptor* rpd = m_rpDesc;
        if (desc.colorTarget != nullptr) {
            rpd = [MTLRenderPassDescriptor renderPassDescriptor];
            const auto& mtlColor = static_cast<const MetalRHITexture&>(*desc.colorTarget);
            rpd.colorAttachments[0].texture = mtlColor.getMTLTexture();
            rpd.colorAttachments[0].clearColor = MTLClearColorMake(desc.clearColor.r, desc.clearColor.g, desc.clearColor.b, desc.clearColor.a);
            rpd.colorAttachments[0].loadAction = desc.clearColorEnable ? MTLLoadActionClear : MTLLoadActionLoad;
            rpd.colorAttachments[0].storeAction = MTLStoreActionStore;

            if (desc.depthTarget != nullptr) {
                const auto& mtlDepth = static_cast<const MetalRHITexture&>(*desc.depthTarget);
                rpd.depthAttachment.texture = mtlDepth.getMTLTexture();
                rpd.depthAttachment.clearDepth = static_cast<double>(desc.clearDepth);
                rpd.depthAttachment.loadAction = desc.clearDepthEnable ? MTLLoadActionClear : MTLLoadActionLoad;
                rpd.depthAttachment.storeAction = MTLStoreActionStore;
            }
        } else {
            rpd.colorAttachments[0].clearColor = MTLClearColorMake(desc.clearColor.r, desc.clearColor.g, desc.clearColor.b, desc.clearColor.a);
            rpd.colorAttachments[0].loadAction = desc.clearColorEnable ? MTLLoadActionClear : MTLLoadActionLoad;
            rpd.colorAttachments[0].storeAction = MTLStoreActionStore;
            rpd.depthAttachment.clearDepth = static_cast<double>(desc.clearDepth);
            rpd.depthAttachment.loadAction = desc.clearDepthEnable ? MTLLoadActionClear : MTLLoadActionLoad;
            rpd.depthAttachment.storeAction = MTLStoreActionDontCare;
        }

        m_renderEncoder = [m_cmdBuf renderCommandEncoderWithDescriptor:rpd];
        m_currentPipeline = nullptr;
    }

    void endRenderPass() override {
        if (m_renderEncoder) {
            [m_renderEncoder endEncoding];
            m_renderEncoder = nil;
        }
        m_currentPipeline = nullptr;
    }

    void setViewport(float x, float y, float width, float height) override {
        if (!m_renderEncoder) return;
        MTLViewport vp{
            static_cast<double>(x),
            static_cast<double>(y),
            static_cast<double>(width),
            static_cast<double>(height),
            0.0,
            1.0
        };
        [m_renderEncoder setViewport:vp];
    }

    void setScissor(int32_t x, int32_t y, uint32_t width, uint32_t height) override {
        if (!m_renderEncoder) return;
        MTLScissorRect rect{
            static_cast<NSUInteger>(std::max(0, x)),
            static_cast<NSUInteger>(std::max(0, y)),
            static_cast<NSUInteger>(width),
            static_cast<NSUInteger>(height)
        };
        [m_renderEncoder setScissorRect:rect];
    }

    void bindGraphicsPipeline(const RHIPipeline& pipeline) override {
        if (!m_renderEncoder) return;
        const auto& mtlPipe = static_cast<const MetalRHIPipeline&>(pipeline);
        m_currentPipeline = &mtlPipe;
        [m_renderEncoder setRenderPipelineState:mtlPipe.renderState];
        [m_renderEncoder setDepthStencilState:mtlPipe.depthState];
        [m_renderEncoder setCullMode:mtlPipe.cullMode];
        [m_renderEncoder setTriangleFillMode:mtlPipe.fillMode];
    }

    void bindComputePipeline(const RHIPipeline& pipeline) override {
        if (m_renderEncoder) {
            [m_renderEncoder endEncoding];
            m_renderEncoder = nil;
        }
        if (!m_computeEncoder) {
            m_computeEncoder = [m_cmdBuf computeCommandEncoder];
        }
        const auto& mtlPipe = static_cast<const MetalRHIPipeline&>(pipeline);
        m_currentPipeline = &mtlPipe;
        [m_computeEncoder setComputePipelineState:mtlPipe.computeState];
    }

    void bindUniformBuffer(uint32_t slot, const RHIBuffer& buffer) override {
        (void)slot;
        if (!m_currentPipeline) return;
        const auto& mtlBuf = static_cast<const MetalRHIBuffer&>(buffer);
        id<MTLBuffer> buf = mtlBuf.getMTLBuffer();
        if (m_currentPipeline->isCompute) {
            if (m_computeEncoder && m_currentPipeline->compUboSlot >= 0) {
                [m_computeEncoder setBuffer:buf offset:0 atIndex:static_cast<NSUInteger>(m_currentPipeline->compUboSlot)];
            }
        } else if (m_renderEncoder) {
            if (m_currentPipeline->vertUboSlot >= 0) {
                [m_renderEncoder setVertexBuffer:buf offset:0 atIndex:static_cast<NSUInteger>(m_currentPipeline->vertUboSlot)];
            }
            if (m_currentPipeline->fragUboSlot >= 0) {
                [m_renderEncoder setFragmentBuffer:buf offset:0 atIndex:static_cast<NSUInteger>(m_currentPipeline->fragUboSlot)];
            }
        }
    }

    void bindStorageBuffer(uint32_t slot, const RHIBuffer& buffer) override {
        (void)slot;
        if (!m_currentPipeline) return;
        const auto& mtlBuf = static_cast<const MetalRHIBuffer&>(buffer);
        id<MTLBuffer> buf = mtlBuf.getMTLBuffer();
        if (m_currentPipeline->isCompute) {
            if (m_computeEncoder && m_currentPipeline->compSsboSlot >= 0) {
                [m_computeEncoder setBuffer:buf offset:0 atIndex:static_cast<NSUInteger>(m_currentPipeline->compSsboSlot)];
            }
        } else if (m_renderEncoder) {
            if (m_currentPipeline->vertSsboSlot >= 0) {
                [m_renderEncoder setVertexBuffer:buf offset:0 atIndex:static_cast<NSUInteger>(m_currentPipeline->vertSsboSlot)];
            }
            if (m_currentPipeline->fragSsboSlot >= 0) {
                [m_renderEncoder setFragmentBuffer:buf offset:0 atIndex:static_cast<NSUInteger>(m_currentPipeline->fragSsboSlot)];
            }
        }
    }

    void bindTexture(uint32_t slot, const RHITexture& texture, const RHISampler& sampler) override {
        const auto& mtlTex = static_cast<const MetalRHITexture&>(texture);
        const auto& mtlSamp = static_cast<const MetalRHISampler&>(sampler);
        if (m_currentPipeline && m_currentPipeline->isCompute) {
            if (m_computeEncoder) {
                [m_computeEncoder setTexture:mtlTex.getMTLTexture() atIndex:static_cast<NSUInteger>(slot)];
                [m_computeEncoder setSamplerState:mtlSamp.getMTLSampler() atIndex:static_cast<NSUInteger>(slot)];
            }
        } else if (m_renderEncoder) {
            [m_renderEncoder setFragmentTexture:mtlTex.getMTLTexture() atIndex:static_cast<NSUInteger>(slot)];
            [m_renderEncoder setFragmentSamplerState:mtlSamp.getMTLSampler() atIndex:static_cast<NSUInteger>(slot)];
        }
    }

    void bindStorageTexture(uint32_t slot, const RHITexture& texture) override {
        if (!m_computeEncoder) return;
        const auto& mtlTex = static_cast<const MetalRHITexture&>(texture);
        [m_computeEncoder setTexture:mtlTex.getMTLTexture() atIndex:static_cast<NSUInteger>(slot)];
    }

    void pushConstants(ShaderStage stages, const void* data, size_t size) override {
        if (!m_currentPipeline) return;
        if (m_currentPipeline->isCompute) {
            if (m_computeEncoder && m_currentPipeline->compPcSlot >= 0) {
                [m_computeEncoder setBytes:data length:static_cast<NSUInteger>(size)
                                   atIndex:static_cast<NSUInteger>(m_currentPipeline->compPcSlot)];
            }
        } else if (m_renderEncoder) {
            if (hasFlag(stages, ShaderStage::Vertex) && m_currentPipeline->vertPcSlot >= 0) {
                [m_renderEncoder setVertexBytes:data length:static_cast<NSUInteger>(size)
                                        atIndex:static_cast<NSUInteger>(m_currentPipeline->vertPcSlot)];
            }
            if (hasFlag(stages, ShaderStage::Fragment) && m_currentPipeline->fragPcSlot >= 0) {
                [m_renderEncoder setFragmentBytes:data length:static_cast<NSUInteger>(size)
                                          atIndex:static_cast<NSUInteger>(m_currentPipeline->fragPcSlot)];
            }
        }
    }

    void bindVertexBuffer(uint32_t binding, const RHIBuffer& buffer) override {
        if (!m_renderEncoder) return;
        const auto& mtlBuf = static_cast<const MetalRHIBuffer&>(buffer);
        [m_renderEncoder setVertexBuffer:mtlBuf.getMTLBuffer()
                                  offset:0
                                 atIndex:static_cast<NSUInteger>(16 + binding)];
    }

    void bindIndexBuffer(const RHIBuffer& buffer) override {
        const auto& mtlBuf = static_cast<const MetalRHIBuffer&>(buffer);
        m_boundIndexBuffer = mtlBuf.getMTLBuffer();
    }

    void draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex) override {
        if (!m_renderEncoder) return;
        [m_renderEncoder drawPrimitives:MTLPrimitiveTypeTriangle
                            vertexStart:static_cast<NSUInteger>(firstVertex)
                            vertexCount:static_cast<NSUInteger>(vertexCount)
                          instanceCount:static_cast<NSUInteger>(instanceCount)];
    }

    void drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex) override {
        if (!m_renderEncoder || !m_boundIndexBuffer) return;
        [m_renderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                    indexCount:static_cast<NSUInteger>(indexCount)
                                     indexType:MTLIndexTypeUInt32
                                   indexBuffer:m_boundIndexBuffer
                             indexBufferOffset:static_cast<NSUInteger>(firstIndex) * sizeof(uint32_t)
                                 instanceCount:static_cast<NSUInteger>(instanceCount)];
    }

    void dispatch(uint32_t groupX, uint32_t groupY, uint32_t groupZ) override {
        if (!m_computeEncoder || !m_currentPipeline) return;
        MTLSize groups = MTLSizeMake(groupX, groupY, groupZ);
        MTLSize threads = MTLSizeMake(m_currentPipeline->localSizeX, m_currentPipeline->localSizeY, m_currentPipeline->localSizeZ);
        [m_computeEncoder dispatchThreadgroups:groups threadsPerThreadgroup:threads];
    }

    void bufferBarrier(const RHIBuffer& buffer) override {
        (void)buffer;
        // Metal tracks resource hazards automatically across encoders/dispatches on the same command buffer.
    }

    void textureBarrier(const RHITexture& texture, TextureLayout oldLayout, TextureLayout newLayout) override {
        (void)texture; (void)oldLayout; (void)newLayout;
        // Metal tracks texture hazards automatically across encoders/dispatches on the same command buffer.
    }

    void endActiveEncoders() {
        if (m_renderEncoder) {
            [m_renderEncoder endEncoding];
            m_renderEncoder = nil;
        }
        if (m_computeEncoder) {
            [m_computeEncoder endEncoding];
            m_computeEncoder = nil;
        }
    }

    id<MTLCommandBuffer> getCommandBuffer() const { return m_cmdBuf; }
    id<MTLRenderCommandEncoder> getRenderEncoder() const { return m_renderEncoder; }

private:
    id<MTLCommandBuffer> m_cmdBuf = nil;
    MTLRenderPassDescriptor* m_rpDesc = nil;
    id<MTLRenderCommandEncoder> m_renderEncoder = nil;
    id<MTLComputeCommandEncoder> m_computeEncoder = nil;
    id<MTLBuffer> m_boundIndexBuffer = nil;
    const MetalRHIPipeline* m_currentPipeline = nullptr;
};

} // namespace

struct MetalDevice::Impl {
    id<MTLDevice> device = nil;
    id<MTLCommandQueue> commandQueue = nil;
    CAMetalLayer* layer = nil;
    id<MTLTexture> depthTexture = nil;
    id<CAMetalDrawable> currentDrawable = nil;
    MTLRenderPassDescriptor* currentRenderPassDesc = nil;
    id<MTLCommandBuffer> currentCommandBuffer = nil;
    std::unique_ptr<MetalRHICommandList> activeCmdList;

    dispatch_semaphore_t inFlightSemaphore = nullptr;
    std::unordered_map<std::string, id<MTLLibrary>> libraryCache;

    std::atomic<double> lastGpuFrameTimeMs{0.0};
    uint32_t frameCounter = 0;
    bool framebufferResized = false;
    bool imGuiInitialized = false;

    void createDepthTexture(uint32_t w, uint32_t h) {
        if (w == 0 || h == 0) return;
        MTLTextureDescriptor* desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                                                        width:w
                                                                                       height:h
                                                                                    mipmapped:NO];
        desc.usage = MTLTextureUsageRenderTarget;
        desc.storageMode = MTLStorageModePrivate;
        depthTexture = [device newTextureWithDescriptor:desc];
    }

    id<MTLLibrary> getOrCompileLibrary(const std::string& shaderName) {
        auto it = libraryCache.find(shaderName);
        if (it != libraryCache.end()) {
            return it->second;
        }
        std::string source = readMetalShaderSource(shaderName);
        NSString* nsSource = [NSString stringWithUTF8String:source.c_str()];
        MTLCompileOptions* opts = [[MTLCompileOptions alloc] init];
        if (@available(macOS 13.0, *)) {
            opts.languageVersion = MTLLanguageVersion3_0;
        }
        NSError* err = nil;
        id<MTLLibrary> lib = [device newLibraryWithSource:nsSource options:opts error:&err];
        if (!lib) {
            std::string errMsg = err ? [[err localizedDescription] UTF8String] : "Unknown MSL compile error";
            throw std::runtime_error("Failed to compile Metal shader '" + shaderName + "': " + errMsg);
        }
        libraryCache[shaderName] = lib;
        return lib;
    }
};

MetalDevice::MetalDevice(GLFWwindow* window)
    : m_impl(std::make_unique<Impl>()) {
    m_impl->device = MTLCreateSystemDefaultDevice();
    if (!m_impl->device) {
        throw std::runtime_error("Failed to create default Metal device!");
    }
    m_impl->commandQueue = [m_impl->device newCommandQueue];
    m_impl->inFlightSemaphore = dispatch_semaphore_create(MAX_FRAMES_IN_FLIGHT);

    NSWindow* nsWindow = glfwGetCocoaWindow(window);
    NSView* contentView = [nsWindow contentView];
    [contentView setWantsLayer:YES];

    m_impl->layer = [CAMetalLayer layer];
    m_impl->layer.device = m_impl->device;
    m_impl->layer.pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
    m_impl->layer.framebufferOnly = YES;
    m_impl->layer.displaySyncEnabled = YES;

    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    m_width = static_cast<uint32_t>(std::max(1, fbW));
    m_height = static_cast<uint32_t>(std::max(1, fbH));
    m_impl->layer.contentsScale = [nsWindow backingScaleFactor];
    m_impl->layer.drawableSize = CGSizeMake(m_width, m_height);

    [contentView setLayer:m_impl->layer];
    m_impl->createDepthTexture(m_width, m_height);
}

MetalDevice::~MetalDevice() {
    waitIdle();
    shutdownImGui();
}

std::string MetalDevice::getDeviceName() const {
    return [[m_impl->device name] UTF8String];
}

bool MetalDevice::supportsHardwareRayTracing() const {
    return [m_impl->device supportsRaytracing];
}

float MetalDevice::getAspectRatio() const {
    if (m_height == 0) return 1.0f;
    return static_cast<float>(m_width) / static_cast<float>(m_height);
}

double MetalDevice::getLastGpuFrameTimeMs() const {
    return m_impl->lastGpuFrameTimeMs.load(std::memory_order_relaxed);
}

void MetalDevice::onResize(GLFWwindow* window) {
    (void)window;
    m_impl->framebufferResized = true;
}

void MetalDevice::waitIdle() {
    if (!m_impl || !m_impl->commandQueue) return;
    if (m_impl->inFlightSemaphore) {
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            dispatch_semaphore_wait(m_impl->inFlightSemaphore, DISPATCH_TIME_FOREVER);
        }
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            dispatch_semaphore_signal(m_impl->inFlightSemaphore);
        }
    }
    id<MTLCommandBuffer> syncCmd = [m_impl->commandQueue commandBuffer];
    [syncCmd commit];
    [syncCmd waitUntilCompleted];
}

std::unique_ptr<RHIBuffer> MetalDevice::createBuffer(size_t size, BufferUsage usage, MemoryUsage memoryUsage) {
    (void)usage;
    return std::make_unique<MetalRHIBuffer>(m_impl->device, size, memoryUsage);
}

void MetalDevice::uploadBuffer(RHIBuffer& dst, const void* data, size_t size) {
    auto& mtlDst = static_cast<MetalRHIBuffer&>(dst);
    if (mtlDst.getMTLBuffer().storageMode != MTLStorageModePrivate) {
        mtlDst.copyFromHost(data, size);
        return;
    }

    id<MTLBuffer> staging = [m_impl->device newBufferWithBytes:data
                                                        length:static_cast<NSUInteger>(size)
                                                       options:MTLResourceStorageModeShared];
    id<MTLCommandBuffer> cmd = [m_impl->commandQueue commandBuffer];
    id<MTLBlitCommandEncoder> blit = [cmd blitCommandEncoder];
    [blit copyFromBuffer:staging
            sourceOffset:0
                toBuffer:mtlDst.getMTLBuffer()
       destinationOffset:0
                    size:static_cast<NSUInteger>(size)];
    [blit endEncoding];
    [cmd commit];
    [cmd waitUntilCompleted];
}

std::unique_ptr<RHITexture> MetalDevice::createTexture(const TextureDesc& desc) {
    return std::make_unique<MetalRHITexture>(m_impl->device, desc);
}

std::unique_ptr<RHISampler> MetalDevice::createSampler(const SamplerDesc& desc) {
    return std::make_unique<MetalRHISampler>(m_impl->device, desc);
}

std::unique_ptr<RHIPipeline> MetalDevice::createGraphicsPipeline(const GraphicsPipelineDesc& desc) {
    auto pipe = std::make_unique<MetalRHIPipeline>();
    pipe->isCompute = false;
    pipe->cullMode = toMTLCullMode(desc.cullMode);
    pipe->fillMode = toMTLFillMode(desc.polygonMode);

    id<MTLLibrary> vertLib = m_impl->getOrCompileLibrary(desc.vertexShader);
    id<MTLLibrary> fragLib = m_impl->getOrCompileLibrary(desc.fragmentShader);

    MTLRenderPipelineDescriptor* rpd = [[MTLRenderPipelineDescriptor alloc] init];
    rpd.vertexFunction = [vertLib newFunctionWithName:@"main0"];
    rpd.fragmentFunction = [fragLib newFunctionWithName:@"main0"];

    if (!rpd.vertexFunction || !rpd.fragmentFunction) {
        throw std::runtime_error("Failed to find main0 entry point in Metal shader");
    }

    if (!desc.vertexAttributes.empty() || !desc.vertexBindings.empty()) {
        MTLVertexDescriptor* vd = [[MTLVertexDescriptor alloc] init];
        for (const auto& b : desc.vertexBindings) {
            NSUInteger idx = 16 + b.binding;
            vd.layouts[idx].stride = b.stride;
            vd.layouts[idx].stepFunction = (b.inputRate == VertexInputRate::Instance)
                ? MTLVertexStepFunctionPerInstance
                : MTLVertexStepFunctionPerVertex;
            vd.layouts[idx].stepRate = 1;
        }
        for (const auto& a : desc.vertexAttributes) {
            vd.attributes[a.location].format = toMTLVertexFormat(a.format);
            vd.attributes[a.location].offset = a.offset;
            vd.attributes[a.location].bufferIndex = 16 + a.binding;
        }
        rpd.vertexDescriptor = vd;
    }

    rpd.colorAttachments[0].pixelFormat = toMTLPixelFormat(desc.colorFormat);
    if (desc.blendEnable) {
        rpd.colorAttachments[0].blendingEnabled = YES;
        rpd.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
        rpd.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
        rpd.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        rpd.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        rpd.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
        rpd.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorZero;
    }
    if (desc.colorFormat == Format::BGRA8_SRGB) {
        rpd.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
    } else {
        rpd.depthAttachmentPixelFormat = desc.hasDepth ? toMTLPixelFormat(desc.depthFormat) : MTLPixelFormatInvalid;
    }

    MTLRenderPipelineReflection* reflection = nil;
    NSError* err = nil;
    pipe->renderState = [m_impl->device newRenderPipelineStateWithDescriptor:rpd
                                                                    options:MTLPipelineOptionBindingInfo
                                                                 reflection:&reflection
                                                                      error:&err];
    if (!pipe->renderState) {
        std::string errMsg = err ? [[err localizedDescription] UTF8String] : "Unknown error";
        throw std::runtime_error("Failed to create Metal render pipeline: " + errMsg);
    }

    auto parseBindings = [](NSArray<id<MTLBinding>>* bindings, int32_t& uboSlot, int32_t& ssboSlot, int32_t& pcSlot) {
        for (id<MTLBinding> b in bindings) {
            if (b.type != MTLBindingTypeBuffer || b.index >= 16) continue;
            if ([b.name isEqualToString:@"ubo"]) {
                uboSlot = static_cast<int32_t>(b.index);
            } else if ([b.name isEqualToString:@"pc"]) {
                pcSlot = static_cast<int32_t>(b.index);
            } else if ([b.name isEqualToString:@"terrainSSBO"] || [b.name isEqualToString:@"terrainBuffer"]) {
                ssboSlot = static_cast<int32_t>(b.index);
            }
        }
    };

    if (@available(macOS 13.0, *)) {
        parseBindings(reflection.vertexBindings, pipe->vertUboSlot, pipe->vertSsboSlot, pipe->vertPcSlot);
        parseBindings(reflection.fragmentBindings, pipe->fragUboSlot, pipe->fragSsboSlot, pipe->fragPcSlot);
    }

    MTLDepthStencilDescriptor* dsd = [[MTLDepthStencilDescriptor alloc] init];
    dsd.depthCompareFunction = desc.depthTestEnable ? toMTLCompareOp(desc.depthCompareOp) : MTLCompareFunctionAlways;
    dsd.depthWriteEnabled = desc.depthWriteEnable ? YES : NO;
    pipe->depthState = [m_impl->device newDepthStencilStateWithDescriptor:dsd];

    return pipe;
}

std::unique_ptr<RHIPipeline> MetalDevice::createComputePipeline(const ComputePipelineDesc& desc) {
    auto pipe = std::make_unique<MetalRHIPipeline>();
    pipe->isCompute = true;
    pipe->localSizeX = desc.localSizeX;
    pipe->localSizeY = desc.localSizeY;
    pipe->localSizeZ = desc.localSizeZ;

    id<MTLLibrary> compLib = m_impl->getOrCompileLibrary(desc.computeShader);
    id<MTLFunction> func = [compLib newFunctionWithName:@"main0"];
    if (!func) {
        throw std::runtime_error("Failed to find main0 entry point in Metal compute shader");
    }

    MTLComputePipelineDescriptor* cpd = [[MTLComputePipelineDescriptor alloc] init];
    cpd.computeFunction = func;

    MTLComputePipelineReflection* reflection = nil;
    NSError* err = nil;
    pipe->computeState = [m_impl->device newComputePipelineStateWithDescriptor:cpd
                                                                      options:MTLPipelineOptionBindingInfo
                                                                   reflection:&reflection
                                                                        error:&err];
    if (!pipe->computeState) {
        std::string errMsg = err ? [[err localizedDescription] UTF8String] : "Unknown error";
        throw std::runtime_error("Failed to create Metal compute pipeline: " + errMsg);
    }

    if (@available(macOS 13.0, *)) {
        for (id<MTLBinding> b in reflection.bindings) {
            if (b.type != MTLBindingTypeBuffer) continue;
            if ([b.name isEqualToString:@"ubo"]) {
                pipe->compUboSlot = static_cast<int32_t>(b.index);
            } else if ([b.name isEqualToString:@"pc"]) {
                pipe->compPcSlot = static_cast<int32_t>(b.index);
            } else if ([b.name isEqualToString:@"terrainBuffer"] || [b.name isEqualToString:@"terrainSSBO"]) {
                pipe->compSsboSlot = static_cast<int32_t>(b.index);
            }
        }
    }

    return pipe;
}

RHICommandList* MetalDevice::beginFrame(GLFWwindow* window, uint32_t& outFrameIndex) {
    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    if (fbW <= 0 || fbH <= 0) {
        return nullptr;
    }

    if (m_impl->framebufferResized || static_cast<uint32_t>(fbW) != m_width || static_cast<uint32_t>(fbH) != m_height) {
        m_impl->framebufferResized = false;
        m_width = static_cast<uint32_t>(fbW);
        m_height = static_cast<uint32_t>(fbH);
        m_impl->layer.contentsScale = [glfwGetCocoaWindow(window) backingScaleFactor];
        m_impl->layer.drawableSize = CGSizeMake(m_width, m_height);
        m_impl->createDepthTexture(m_width, m_height);
    }

    dispatch_semaphore_wait(m_impl->inFlightSemaphore, DISPATCH_TIME_FOREVER);

    @autoreleasepool {
        m_impl->currentDrawable = [m_impl->layer nextDrawable];
    }
    if (!m_impl->currentDrawable) {
        dispatch_semaphore_signal(m_impl->inFlightSemaphore);
        return nullptr;
    }

    m_impl->currentCommandBuffer = [m_impl->commandQueue commandBuffer];

    MTLRenderPassDescriptor* rpd = [MTLRenderPassDescriptor renderPassDescriptor];
    rpd.colorAttachments[0].texture = m_impl->currentDrawable.texture;
    rpd.colorAttachments[0].loadAction = MTLLoadActionClear;
    rpd.colorAttachments[0].storeAction = MTLStoreActionStore;
    rpd.depthAttachment.texture = m_impl->depthTexture;
    rpd.depthAttachment.loadAction = MTLLoadActionClear;
    rpd.depthAttachment.storeAction = MTLStoreActionDontCare;
    m_impl->currentRenderPassDesc = rpd;

    outFrameIndex = m_impl->frameCounter % MAX_FRAMES_IN_FLIGHT;
    m_impl->frameCounter++;

    m_impl->activeCmdList = std::make_unique<MetalRHICommandList>(m_impl->currentCommandBuffer, rpd);
    return m_impl->activeCmdList.get();
}

void MetalDevice::endFrame(GLFWwindow* window) {
    (void)window;
    if (m_impl->activeCmdList) {
        m_impl->activeCmdList->endActiveEncoders();
        m_impl->activeCmdList.reset();
    }

    id<MTLCommandBuffer> cmdBuf = m_impl->currentCommandBuffer;
    id<CAMetalDrawable> drawable = m_impl->currentDrawable;
    dispatch_semaphore_t sem = m_impl->inFlightSemaphore;
    std::atomic<double>* gpuTimePtr = &m_impl->lastGpuFrameTimeMs;

    [cmdBuf addCompletedHandler:^(id<MTLCommandBuffer> completed) {
        double gpuMs = (completed.GPUEndTime - completed.GPUStartTime) * 1000.0;
        if (gpuMs >= 0.0 && gpuMs < 5000.0) {
            gpuTimePtr->store(gpuMs, std::memory_order_relaxed);
        }
        dispatch_semaphore_signal(sem);
    }];

    [cmdBuf presentDrawable:drawable];
    [cmdBuf commit];

    m_impl->currentDrawable = nil;
    m_impl->currentCommandBuffer = nil;
    m_impl->currentRenderPassDesc = nil;
}

void MetalDevice::executeImmediateCompute(const std::function<void(RHICommandList&)>& fn) {
    id<MTLCommandBuffer> cmdBuf = [m_impl->commandQueue commandBuffer];
    MetalRHICommandList cmdList(cmdBuf, nil);
    fn(cmdList);
    cmdList.endActiveEncoders();
    [cmdBuf commit];
    [cmdBuf waitUntilCompleted];
}

void MetalDevice::initImGui(GLFWwindow* window) {
    if (m_impl->imGuiInitialized) return;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOther(window, true);
    ImGui_ImplMetal_Init(m_impl->device);
    m_impl->imGuiInitialized = true;
}

void MetalDevice::beginImGuiFrame() {
    if (!m_impl->currentRenderPassDesc) return;
    ImGui_ImplMetal_NewFrame(m_impl->currentRenderPassDesc);
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void MetalDevice::renderImGui(RHICommandList& cmd) {
    ImGui::Render();
    auto& mtlCmd = static_cast<MetalRHICommandList&>(cmd);
    ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), mtlCmd.getCommandBuffer(), mtlCmd.getRenderEncoder());
}

void MetalDevice::shutdownImGui() {
    if (!m_impl || !m_impl->imGuiInitialized) return;
    waitIdle();
    // Drain any pending main-queue completion blocks queued by ImGui_ImplMetal_RenderDrawData.
    while (CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.005, true) == kCFRunLoopRunHandledSource) {}
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.005, false);
    ImGui_ImplMetal_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    m_impl->imGuiInitialized = false;
}

} // namespace rhi
