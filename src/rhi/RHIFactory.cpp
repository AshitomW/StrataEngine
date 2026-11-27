#include "rhi/RHIDevice.hpp"
#include <stdexcept>

#ifdef TERRAIN_ENABLE_VULKAN
#include "rhi/vulkan/VulkanDevice.hpp"
#endif

#ifdef TERRAIN_ENABLE_METAL
#include "rhi/metal/MetalDevice.hpp"
#endif

namespace rhi {

std::unique_ptr<RHIDevice> RHIDevice::create(BackendType backend, GLFWwindow* window) {
    switch (backend) {
        case BackendType::Vulkan:
#ifdef TERRAIN_ENABLE_VULKAN
            return std::make_unique<VulkanDevice>(window);
#else
            throw std::runtime_error("Vulkan backend is not enabled in this build.");
#endif
        case BackendType::Metal:
#ifdef TERRAIN_ENABLE_METAL
            return std::make_unique<MetalDevice>(window);
#else
            throw std::runtime_error("Metal backend is not enabled in this build.");
#endif
    }
    throw std::runtime_error("Unknown RHI backend requested.");
}

} // namespace rhi
