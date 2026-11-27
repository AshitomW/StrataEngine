#include "app/Application.hpp"
#include "core/EngineConstants.hpp"
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    try {
#ifdef TERRAIN_ENABLE_METAL
        rhi::BackendType backend = rhi::BackendType::Metal;
#else
        rhi::BackendType backend = rhi::BackendType::Vulkan;
#endif
        bool smokeTest = false;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--backend" && i + 1 < argc) {
                std::string val = argv[++i];
                if (val == "metal") {
                    backend = rhi::BackendType::Metal;
                } else if (val == "vulkan") {
                    backend = rhi::BackendType::Vulkan;
                } else {
                    std::cerr << "Unknown backend: " << val << " (expected 'metal' or 'vulkan')\n";
                    return 1;
                }
            } else if (arg == "--smoke-test") {
                smokeTest = true;
            }
        }

        Application app(
            EngineConstants::Window::DEFAULT_WIDTH,
            EngineConstants::Window::DEFAULT_HEIGHT,
            EngineConstants::Window::TITLE,
            backend
        );

        if (smokeTest) {
            return app.runSmokeTest(5);
        }

        app.run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n[Fatal Error]: " << e.what() << std::endl;
        return 1;
    }
}
