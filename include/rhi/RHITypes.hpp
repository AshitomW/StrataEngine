#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace rhi {

enum class BackendType {
    Vulkan,
    Metal
};

enum class Format {
    RGBA8_UNORM,
    BGRA8_SRGB,
    RGBA16F,
    R16F,
    RG16F,
    D32F,
    R32F,
    RG32F,
    RGB32F,
    RGBA32F
};

enum class BufferUsage : uint32_t {
    None        = 0,
    Vertex      = 1u << 0,
    Index       = 1u << 1,
    Uniform     = 1u << 2,
    Storage     = 1u << 3,
    Indirect    = 1u << 4,
    TransferSrc = 1u << 5,
    TransferDst = 1u << 6
};

inline BufferUsage operator|(BufferUsage a, BufferUsage b) {
    return static_cast<BufferUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline bool hasFlag(BufferUsage mask, BufferUsage flag) {
    return (static_cast<uint32_t>(mask) & static_cast<uint32_t>(flag)) != 0;
}

enum class MemoryUsage {
    GpuOnly,
    CpuToGpu,
    GpuToCpu
};

enum class TextureUsage : uint32_t {
    None            = 0,
    Sampled         = 1u << 0,
    Storage         = 1u << 1,
    ColorAttachment = 1u << 2,
    DepthAttachment = 1u << 3,
    TransferSrc     = 1u << 4,
    TransferDst     = 1u << 5
};

inline TextureUsage operator|(TextureUsage a, TextureUsage b) {
    return static_cast<TextureUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline bool hasFlag(TextureUsage mask, TextureUsage flag) {
    return (static_cast<uint32_t>(mask) & static_cast<uint32_t>(flag)) != 0;
}

enum class TextureType {
    Tex2D,
    Tex2DArray,
    Tex3D,
    Cube
};

enum class Filter {
    Nearest,
    Linear
};

enum class AddressMode {
    Repeat,
    ClampToEdge
};

enum class TextureLayout {
    Undefined,
    ColorAttachment,
    DepthAttachment,
    ShaderReadOnly,
    General,
    TransferSrc,
    TransferDst
};

struct TextureDesc {
    uint32_t width = 1;
    uint32_t height = 1;
    uint32_t depth = 1;
    uint32_t mipLevels = 1;
    uint32_t arrayLayers = 1;
    Format format = Format::RGBA16F;
    TextureUsage usage = TextureUsage::Sampled;
    TextureType type = TextureType::Tex2D;
};

struct SamplerDesc {
    Filter minFilter = Filter::Linear;
    Filter magFilter = Filter::Linear;
    AddressMode addressU = AddressMode::ClampToEdge;
    AddressMode addressV = AddressMode::ClampToEdge;
    AddressMode addressW = AddressMode::ClampToEdge;
    float maxAnisotropy = 1.0f;
    bool compareEnable = false;
};

enum class ShaderStage : uint32_t {
    None     = 0,
    Vertex   = 1u << 0,
    Fragment = 1u << 1,
    Compute  = 1u << 2
};

inline ShaderStage operator|(ShaderStage a, ShaderStage b) {
    return static_cast<ShaderStage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline bool hasFlag(ShaderStage mask, ShaderStage flag) {
    return (static_cast<uint32_t>(mask) & static_cast<uint32_t>(flag)) != 0;
}

enum class CullMode {
    None,
    Back,
    Front
};

enum class PolygonMode {
    Fill,
    Line
};

enum class CompareOp {
    Less,
    LessOrEqual,
    Greater,
    Always
};

enum class VertexInputRate {
    Vertex,
    Instance
};

struct VertexBinding {
    uint32_t binding = 0;
    uint32_t stride = 0;
    VertexInputRate inputRate = VertexInputRate::Vertex;
};

struct VertexAttribute {
    uint32_t location = 0;
    uint32_t binding = 0;
    Format format = Format::RGB32F;
    uint32_t offset = 0;
};

struct GraphicsPipelineDesc {
    std::string vertexShader;
    std::string fragmentShader;

    std::vector<VertexBinding> vertexBindings;
    std::vector<VertexAttribute> vertexAttributes;

    CullMode cullMode = CullMode::None;
    PolygonMode polygonMode = PolygonMode::Fill;
    bool depthTestEnable = true;
    bool depthWriteEnable = true;
    CompareOp depthCompareOp = CompareOp::LessOrEqual;
    bool blendEnable = false;

    Format colorFormat = Format::BGRA8_SRGB;
    Format depthFormat = Format::D32F;
    bool hasDepth = true;

    bool usesUniformBuffer = true;
    bool usesStorageBuffer = false;
    uint32_t numSampledTextures = 0;
    uint32_t pushConstantSize = 0;
    ShaderStage pushConstantStages = ShaderStage::None;
};

struct ComputePipelineDesc {
    std::string computeShader;
    uint32_t numStorageBuffers = 1;
    uint32_t numUniformBuffers = 0;
    uint32_t numSampledTextures = 0;
    uint32_t numStorageTextures = 0;
    uint32_t pushConstantSize = 0;
    uint32_t localSizeX = 16;
    uint32_t localSizeY = 16;
    uint32_t localSizeZ = 1;
};

struct ClearColor {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
};

class RHITexture;

struct RenderPassDesc {
    RHITexture* colorTarget = nullptr; // nullptr = swapchain backbuffer
    RHITexture* depthTarget = nullptr; // nullptr = swapchain depth (if colorTarget == nullptr)
    ClearColor clearColor{};
    float clearDepth = 1.0f;
    bool clearColorEnable = true;
    bool clearDepthEnable = true;
};

} // namespace rhi
