#include "app/Application.hpp"
#include "imgui.h"
#include <GLFW/glfw3.h>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <random>
#include <stdexcept>

Application::Application(int width, int height, const char* title, rhi::BackendType backend)
    : m_width(width), m_height(height), m_title(title), m_backend(backend) {
    initWindow();

    m_device = rhi::RHIDevice::create(m_backend, m_window);

    std::string fullTitle = std::string(m_title) + " [" + m_device->getBackendName() + " - " + m_device->getDeviceName() + "]";
    glfwSetWindowTitle(m_window, fullTitle.c_str());

    m_camera = std::make_unique<Camera>(glm::vec3(0.0f, 65.0f, 120.0f));
    m_camera->setMouseCaptured(true);

    m_chunkManager = std::make_unique<ChunkManager>(*m_device, m_config.viewRadius);

    m_renderer = std::make_unique<Renderer>(*m_device, m_window);
    m_camera->setAspectRatio(m_renderer->getAspectRatio());

    m_chunkManager->regenerateAll(*m_device, m_config);
}

Application::~Application() {
    m_renderer.reset();
    m_chunkManager.reset();
    m_camera.reset();
    m_device.reset();

    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }

    glfwTerminate();
}

void Application::initWindow() {
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_window = glfwCreateWindow(m_width, m_height, m_title, nullptr, nullptr);
    if (!m_window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwSetWindowUserPointer(m_window, this);

    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }

    glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);
    glfwSetCursorPosCallback(m_window, cursorPosCallback);
    glfwSetMouseButtonCallback(m_window, mouseButtonCallback);
    glfwSetKeyCallback(m_window, keyCallback);
}

void Application::framebufferResizeCallback(GLFWwindow* window, int, int) {
    auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
    if (app && app->m_device) {
        app->m_device->onResize(window);
        app->m_camera->setAspectRatio(app->m_renderer->getAspectRatio());
    }
}

void Application::cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
    if (app && app->m_camera) {
        app->m_camera->onMouseMove(xpos, ypos);
    }
}

void Application::mouseButtonCallback(GLFWwindow* window, int, int action, int) {
    auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
    if (!app) return;

    if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse && !app->m_camera->isMouseCaptured()) {
        return;
    }

    if (action == GLFW_PRESS) {
        if (!app->m_camera->isMouseCaptured()) {
            app->m_camera->setMouseCaptured(true);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }
}

void Application::keyCallback(GLFWwindow* window, int key, int, int action, int) {
    if (action != GLFW_PRESS) return;

    auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
    if (!app) return;

    if (ImGui::GetCurrentContext() && ImGui::GetIO().WantTextInput) {
        return;
    }

    bool needsRegen = false;

    if (key == GLFW_KEY_ESCAPE) {
        if (app->m_camera->isMouseCaptured()) {
            app->m_camera->setMouseCaptured(false);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    } else if (key == GLFW_KEY_TAB) {
        bool captured = !app->m_camera->isMouseCaptured();
        app->m_camera->setMouseCaptured(captured);
        glfwSetInputMode(window, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    } else if (key == GLFW_KEY_H) {
        app->m_hudInfo.showHUD = !app->m_hudInfo.showHUD;
    } else if (key == GLFW_KEY_R) {
        static std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<uint32_t> dis(1, 999999);
        app->m_config.seed = dis(gen);
        app->m_hudInfo.presetName = "Custom (Random)";
        needsRegen = true;
    } else if (key == GLFW_KEY_1) {
        app->m_config.applyMountains();
        app->m_hudInfo.presetName = "Mountains";
        needsRegen = true;
    } else if (key == GLFW_KEY_2) {
        app->m_config.applyHills();
        app->m_hudInfo.presetName = "Rolling Hills";
        needsRegen = true;
    } else if (key == GLFW_KEY_3) {
        app->m_config.applyCanyons();
        app->m_hudInfo.presetName = "Deep Canyons";
        needsRegen = true;
    } else if (key == GLFW_KEY_4) {
        app->m_config.applyIslands();
        app->m_hudInfo.presetName = "Tropical Islands";
        needsRegen = true;
    } else if (key == GLFW_KEY_5) {
        app->m_config.applyMultiBiome();
        app->m_hudInfo.presetName = "Multi-Biome World";
        needsRegen = true;
    } else if (key == GLFW_KEY_6) {
        app->m_config.applyAlpineValley();
        app->m_hudInfo.presetName = "Alpine Emerald Valley";
        needsRegen = true;
    } else if (key == GLFW_KEY_7) {
        app->m_config.applyTundra();
        app->m_hudInfo.presetName = "Arctic Tundra";
        needsRegen = true;
    } else if (key == GLFW_KEY_8) {
        app->m_config.applyAutumnHighlands();
        app->m_hudInfo.presetName = "Autumn Highlands";
        needsRegen = true;
    } else if (key == GLFW_KEY_9) {
        app->m_config.applyKarstPeaks();
        app->m_hudInfo.presetName = "Karst Peaks & Bamboo";
        needsRegen = true;
    } else if (key == GLFW_KEY_T) {
        app->m_config.toggleTimeCycle();
    } else if (key == GLFW_KEY_LEFT_BRACKET) {
        app->m_config.scrubTime(-0.5f);
    } else if (key == GLFW_KEY_RIGHT_BRACKET) {
        app->m_config.scrubTime(+0.5f);
    } else if (key == GLFW_KEY_U) {
        app->m_config.decreaseFoliageDensity();
    } else if (key == GLFW_KEY_I) {
        app->m_config.increaseFoliageDensity();
    } else if (key == GLFW_KEY_E) {
        app->m_config.toggleFoliage();
    } else if (key == GLFW_KEY_O) {
        app->m_config.decreaseWaterHeight();
        needsRegen = true;
    } else if (key == GLFW_KEY_P) {
        app->m_config.increaseWaterHeight();
        needsRegen = true;
    } else if (key == GLFW_KEY_Z) {
        app->m_config.amplitude = std::max(5.0f, app->m_config.amplitude - 5.0f);
        app->m_hudInfo.presetName = "Custom";
        needsRegen = true;
    } else if (key == GLFW_KEY_X) {
        app->m_config.amplitude += 5.0f;
        app->m_hudInfo.presetName = "Custom";
        needsRegen = true;
    } else if (key == GLFW_KEY_C) {
        app->m_config.frequency = std::max(0.2f, app->m_config.frequency - 0.2f);
        app->m_hudInfo.presetName = "Custom";
        needsRegen = true;
    } else if (key == GLFW_KEY_V) {
        app->m_config.frequency += 0.2f;
        app->m_hudInfo.presetName = "Custom";
        needsRegen = true;
    } else if (key == GLFW_KEY_L) {
        app->m_config.cycleLODMode();
    } else if (key == GLFW_KEY_J || key == GLFW_KEY_DOWN) {
        app->m_config.increaseLOD();
    } else if (key == GLFW_KEY_K || key == GLFW_KEY_UP) {
        app->m_config.decreaseLOD();
    } else if (key == GLFW_KEY_G) {
        app->m_config.toggleWater();
    } else if (key == GLFW_KEY_B) {
        app->m_config.decreaseWaveAmplitude();
    } else if (key == GLFW_KEY_N) {
        app->m_config.increaseWaveAmplitude();
    } else if (key == GLFW_KEY_MINUS || key == GLFW_KEY_COMMA) {
        app->m_config.decreaseViewDistance();
        app->m_chunkManager->setRadius(*app->m_device, app->m_config.viewRadius, app->m_config);
    } else if (key == GLFW_KEY_EQUAL || key == GLFW_KEY_PERIOD) {
        app->m_config.increaseViewDistance();
        app->m_chunkManager->setRadius(*app->m_device, app->m_config.viewRadius, app->m_config);
    } else if (key == GLFW_KEY_M) {
        int currentMode = static_cast<int>(app->m_config.debugMode);
        currentMode = (currentMode + 1) % 4;
        app->m_config.debugMode = static_cast<float>(currentMode);
    } else if (key == GLFW_KEY_F) {
        app->m_config.wireframe = !app->m_config.wireframe;
    }

    if (needsRegen) {
        app->m_chunkManager->regenerateAll(*app->m_device, app->m_config);
    }
}

void Application::run() {
    m_lastFrameTime = static_cast<float>(glfwGetTime());

    while (!glfwWindowShouldClose(m_window)) {
        float currentTime = static_cast<float>(glfwGetTime());
        float deltaTime = currentTime - m_lastFrameTime;
        m_lastFrameTime = currentTime;

        m_config.advanceTime(deltaTime);

        m_fpsTimer += deltaTime;
        m_frameCount++;
        if (m_fpsTimer >= 1.0f) {
            m_currentFPS = static_cast<float>(m_frameCount) / m_fpsTimer;
            m_frameCount = 0;
            m_fpsTimer = 0.0f;
        }

        glfwPollEvents();

        m_camera->update(deltaTime, m_window);
        m_chunkManager->updateCoordsAndLOD(m_camera->getPosition(), m_config);

        m_hudInfo.fps = m_currentFPS;
        m_hudInfo.activeChunks = m_chunkManager->getChunkCount();
        m_hudInfo.mouseCaptured = m_camera->isMouseCaptured();

        m_renderer->renderFrame(
            m_window,
            *m_camera,
            *m_chunkManager,
            m_config,
            m_hudInfo,
            currentTime
        );
    }

    m_device->waitIdle();
}

int Application::runSmokeTest(int numFrames) {
    using clock = std::chrono::high_resolution_clock;
    double totalCpuMs = 0.0;
    int rendered = 0;

    for (int i = 0; i < numFrames + 3 && rendered < numFrames; ++i) {
        glfwPollEvents();
        auto t0 = clock::now();

        m_config.advanceTime(0.016f);
        m_chunkManager->updateCoordsAndLOD(m_camera->getPosition(), m_config);
        m_hudInfo.fps = 60.0f;
        m_hudInfo.activeChunks = m_chunkManager->getChunkCount();
        m_hudInfo.mouseCaptured = false;

        bool ok = m_renderer->renderFrame(
            m_window,
            *m_camera,
            *m_chunkManager,
            m_config,
            m_hudInfo,
            static_cast<float>(i) * 0.016f
        );

        auto t1 = clock::now();
        if (ok) {
            totalCpuMs += std::chrono::duration<double, std::milli>(t1 - t0).count();
            rendered++;
        }
    }

    m_device->waitIdle();

    double avgCpuMs = rendered > 0 ? totalCpuMs / rendered : 0.0;
    std::cout << "[SmokeTest] Backend=" << m_device->getBackendName()
              << " | GPU=\"" << m_device->getDeviceName() << "\""
              << " | HardwareRT=" << (m_device->supportsHardwareRayTracing() ? "YES" : "NO")
              << " | Resolution=" << m_device->getSwapchainWidth() << "x" << m_device->getSwapchainHeight()
              << " | Chunks=" << m_chunkManager->getChunkCount()
              << " | Frames=" << rendered
              << " | AvgCPU=" << std::fixed << std::setprecision(2) << avgCpuMs << "ms"
              << " | LastGPU=" << m_device->getLastGpuFrameTimeMs() << "ms"
              << std::endl;

    return (rendered > 0) ? 0 : 1;
}
