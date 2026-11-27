#pragma once

#include "rhi/RHIDevice.hpp"
#include "terrain/ChunkManager.hpp"
#include "renderer/Renderer.hpp"
#include "camera/Camera.hpp"
#include "terrain/TerrainTypes.hpp"
#include <memory>
#include <string>

struct GLFWwindow;

class Application {
public:
    Application(
        int width = 1280,
        int height = 720,
        const char* title = "Next-Gen Procedural Terrain Engine",
        rhi::BackendType backend = rhi::BackendType::Vulkan
    );
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void run();
    int runSmokeTest(int numFrames = 5);

private:
    void initWindow();

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

private:
    int m_width;
    int m_height;
    const char* m_title;
    rhi::BackendType m_backend;

    GLFWwindow* m_window = nullptr;

    std::unique_ptr<rhi::RHIDevice> m_device;
    std::unique_ptr<ChunkManager> m_chunkManager;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<Camera> m_camera;

    TerrainConfig m_config;
    HUDInfo m_hudInfo;

    float m_lastFrameTime = 0.0f;
    float m_fpsTimer = 0.0f;
    int m_frameCount = 0;
    float m_currentFPS = 0.0f;
};
