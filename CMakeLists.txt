cmake_minimum_required(VERSION 3.22)
project(vulkan_terrain LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_VULKAN "Enable Vulkan RHI backend" ON)
if(APPLE)
    option(ENABLE_METAL "Enable native Apple Metal RHI backend" ON)
    enable_language(OBJCXX)
else()
    option(ENABLE_METAL "Enable native Apple Metal RHI backend" OFF)
endif()

find_package(glfw3 REQUIRED)
find_path(GLM_INCLUDE_DIR glm/glm.hpp
    HINTS
        /opt/homebrew/include
        /usr/local/include
)
if(NOT GLM_INCLUDE_DIR)
    message(FATAL_ERROR "GLM not found. Try: brew install glm")
endif()

if(ENABLE_VULKAN)
    find_package(Vulkan REQUIRED)
endif()

find_program(GLSLC glslc HINTS /opt/homebrew/bin /usr/local/bin $ENV{VULKAN_SDK}/bin)
if(NOT GLSLC)
    message(FATAL_ERROR "glslc not found. Did you source the Vulkan SDK setup script or install shaderc?")
endif()

if(ENABLE_METAL)
    find_program(SPIRV_CROSS spirv-cross HINTS /opt/homebrew/bin /usr/local/bin $ENV{VULKAN_SDK}/bin)
    if(NOT SPIRV_CROSS)
        message(FATAL_ERROR "spirv-cross not found. Try: brew install spirv-cross")
    endif()
endif()

set(SHADER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADER_BIN ${CMAKE_CURRENT_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_BIN})

set(SHADERS
    atmo_transmittance.comp
    atmo_multiscatter.comp
    atmo_skyview.comp
    bloom_downsample.comp
    bloom_upsample.comp
    postprocess.vert
    postprocess.frag
    terrain.comp
    terrain.vert
    terrain.frag
    foliage.vert
    foliage.frag
    sky.vert
    sky.frag
    water.vert
    water.frag
)

set(SHADER_OUTPUTS "")
foreach(SHADER ${SHADERS})
    set(SPV_FILE ${SHADER_BIN}/${SHADER}.spv)
    add_custom_command(
        OUTPUT ${SPV_FILE}
        COMMAND ${GLSLC} ${SHADER_SRC}/${SHADER} -o ${SPV_FILE}
        DEPENDS ${SHADER_SRC}/${SHADER}
        COMMENT "Compiling ${SHADER} -> SPIR-V"
    )
    list(APPEND SHADER_OUTPUTS ${SPV_FILE})

    if(ENABLE_METAL)
        set(MSL_FILE ${SHADER_BIN}/${SHADER}.metal)
        if(SHADER MATCHES "\\.vert$")
            set(MSL_EXTRA_FLAGS --flip-vert-y)
        else()
            set(MSL_EXTRA_FLAGS "")
        endif()
        add_custom_command(
            OUTPUT ${MSL_FILE}
            COMMAND ${SPIRV_CROSS} ${SPV_FILE} --msl --msl-version 30000 ${MSL_EXTRA_FLAGS} --output ${MSL_FILE}
            DEPENDS ${SPV_FILE}
            COMMENT "Cross-compiling ${SHADER}.spv -> MSL"
        )
        list(APPEND SHADER_OUTPUTS ${MSL_FILE})
    endif()
endforeach()
add_custom_target(shaders ALL DEPENDS ${SHADER_OUTPUTS})

# Dear ImGui static library
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
)
if(ENABLE_VULKAN)
    list(APPEND IMGUI_SOURCES ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp)
endif()
if(ENABLE_METAL)
    list(APPEND IMGUI_SOURCES ${IMGUI_DIR}/backends/imgui_impl_metal.mm)
endif()

add_library(terrain_imgui STATIC ${IMGUI_SOURCES})
target_include_directories(terrain_imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)
target_link_libraries(terrain_imgui PUBLIC glfw)
if(ENABLE_VULKAN)
    target_link_libraries(terrain_imgui PUBLIC Vulkan::Vulkan)
endif()
if(ENABLE_METAL)
    set_source_files_properties(${IMGUI_DIR}/backends/imgui_impl_metal.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
endif()

# Engine sources
set(ENGINE_SOURCES
    src/main.cpp
    src/app/Application.cpp
    src/camera/Camera.cpp
    src/renderer/AtmosphereRenderer.cpp
    src/renderer/FoliageRenderer.cpp
    src/renderer/Renderer.cpp
    src/renderer/SkyRenderer.cpp
    src/renderer/WaterRenderer.cpp
    src/terrain/ChunkManager.cpp
    src/terrain/ComputeTerrainGenerator.cpp
    src/terrain/TerrainChunk.cpp
    src/rhi/RHIFactory.cpp
)

if(ENABLE_VULKAN)
    list(APPEND ENGINE_SOURCES
        src/core/PipelineBuilder.cpp
        src/core/VulkanBuffer.cpp
        src/core/VulkanContext.cpp
        src/core/VulkanImage.cpp
        src/core/VulkanSwapchain.cpp
        src/rhi/vulkan/VulkanDevice.cpp
    )
endif()

if(ENABLE_METAL)
    list(APPEND ENGINE_SOURCES
        src/rhi/metal/MetalDevice.mm
    )
    set_source_files_properties(src/rhi/metal/MetalDevice.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
endif()

add_executable(terrain ${ENGINE_SOURCES})
add_dependencies(terrain shaders)

target_include_directories(terrain PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${GLM_INCLUDE_DIR}
)

if(ENABLE_VULKAN)
    target_compile_definitions(terrain PRIVATE TERRAIN_ENABLE_VULKAN=1)
    target_link_libraries(terrain PRIVATE Vulkan::Vulkan)
endif()

if(ENABLE_METAL)
    target_compile_definitions(terrain PRIVATE TERRAIN_ENABLE_METAL=1)
    target_link_libraries(terrain PRIVATE
        "-framework Metal"
        "-framework QuartzCore"
        "-framework Foundation"
        "-framework Cocoa"
    )
endif()

target_link_libraries(terrain PRIVATE
    terrain_imgui
    glfw
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(terrain PRIVATE -Wall -Wextra)
endif()
