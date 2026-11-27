# vulkan-terrain-engine

Real-time procedural terrain rendering engine built with C++17, an RHI supporting **native Apple Metal** and **Vulkan** (via `glslc` + `spirv-cross`), linear HDR PBR lighting, physical atmosphere LUTs, 4-cascade CSM shadows, volumetric clouds & god-rays, spectral clipmap water with screen-space reflections, dynamic weather, and toroidal zero-stall chunk streaming.

<video
  src="https://github.com/user-attachments/assets/5cdfa1aa-619e-4c56-b038-13edf11fd92b"
  width="100%"
  controls></video>

---

## Features

- **Dual-Backend RHI (`Metal` & `Vulkan`)**:
  - Unified command list, buffer, texture, sampler, and pipeline interface.
  - Native **Apple Metal** backend with runtime MSL 3.0 compilation and SPIRV-Cross reflection binding maps.
  - **Vulkan** backend with dynamic descriptor pools and `vkCmd` multi-pass rendering.
- **Zero-Stall Compute Terrain & CDLOD Streaming**:
  - In-frame GPU compute chunk generation (`terrain.comp`) with toroidal chunk ring allocation, distance-sorted streaming budgets, frustum culling, and continuous CDLOD vertex morphing (`LOD 0..3`).
- **9 Procedural Biomes (`1`–`9`)**:
  - **Mountains (`1`)**: Alpine granite peaks, snow-capped ridges, and river valleys.
  - **Rolling Hills (`2`)**: Pastoral emerald slopes, meadows, and carved streams.
  - **Deep Canyons (`3`)**: Terraced red sandstone gorges, plateaus, and buttes.
  - **Tropical Islands (`4`)**: Volcanic coastal peaks, sandy beaches, and palm groves.
  - **Multi-Biome World (`5`)**: Continuous continental, moisture, and temperature blending across biomes.
  - **Alpine Emerald Valley (`6`)**: Glacial U-shaped emerald valleys flanked by steep spires and snow summits.
  - **Arctic Tundra & Fjords (`7`)**: Glacial fjord channels, permafrost/lichen lowlands, and frost-dusted pines.
  - **Autumn Highlands (`8`)**: Misty highland lochs, heather/leaf-litter floors, and autumn gold/crimson foliage.
  - **Karst Peaks & Bamboo (`9`)**: Vertical limestone karst towers, winding river plains, and tall bamboo clusters.
- **HDR Lighting, Atmosphere, Shadows & Post-Processing**:
  - Hillaire 2020 transmittance, multi-scattering, and sky-view atmosphere LUTs (`24h` smooth day/night transition, stars, sun/moon).
  - 3D Perlin-Worley volumetric cloud raymarching, dynamic weather presets (Clear, Partly Cloudy, Overcast, Rain, Thunderstorm, Snow, Foggy), lightning flashes, and instanced rain/snow particles.
  - `4096×4096` 4-cascade texel-snapped CSM shadow atlas, half-resolution GTAO + bilateral blur, and quarter-resolution volumetric god-rays.
  - 8-layer `1024×1024` GPU-baked PBR material arrays with stochastic non-repeating UV tiling, triplanar steep cliffs, surface wetness, rain puddles, and dynamic snow accumulation.
  - 5-level Call-of-Duty Karis HDR bloom, AgX / ACES / Reinhard tone mapping, and Contrast-Adaptive Sharpening (CAS).
- **Physical Clipmap Water**:
  - `192×192` concentric radial clipmap mesh driven by compute JONSWAP wave spectra (`water_spectrum.comp`) and interactive ripple ping-pong (`water_ripples.comp`).
  - Depth-buffer Beer-Lambert refraction, seabed caustics, screen-space raymarched reflections (SSR), Fresnel specular glint, shoreline foam, and underwater distortion/absorption.
- **Cached Instanced Foliage**:
  - Layered pines, multi-lobe oaks, segmented palms, dual-arm cacti, granite/sandstone boulders, wild grass tufts, and bamboo clusters with procedural bark, foliage, rock grain shaders, and per-chunk CPU scatter caching.

---

## Controls & CLI

### CLI Options
```bash
./build/terrain                     # Interactive launch (Metal default on macOS, Vulkan elsewhere)
./build/terrain --backend metal     # Force Apple Metal backend
./build/terrain --backend vulkan    # Force Vulkan backend
./build/terrain --smoke-test        # Render 5 frames cleanly and print GPU/CPU timings
```

### Keyboard & Mouse
| Key | Description |
| :--- | :--- |
| **W, A, S, D** | Fly camera forward, left, backward, right |
| **Shift** | Turbo speed boost |
| **Space / Q** | Fly upward / downward |
| **Tab / Esc** | Toggle mouse capture (unlock cursor for ImGui panel) / Exit |
| **1 - 9** | Switch biome presets (Mountains, Hills, Canyons, Islands, Multi-Biome, Alpine Valley, Arctic Tundra, Autumn Highlands, Karst Peaks) |
| **R** | Randomize world seed |
| **T / [ / ]** | Pause/resume or scrub time of day (`30 min` steps) |
| **G / B / N / O / P** | Toggle water, adjust wave amplitude, raise/lower water level |
| **E / U / I** | Toggle foliage and adjust foliage density |
| **Z / X / C / V** | Adjust terrain amplitude and noise frequency |
| **- / =** | Adjust chunk view radius |
| **L / J / K** | Toggle auto/manual LOD and step LOD level |
| **M / F / H** | Cycle debug modes, toggle wireframe, toggle HUD/ImGui panel |

---

## Project Structure

```
vulkan-terrain/
├── CMakeLists.txt
├── scripts/
│   └── gen_metal_map.py
├── include/
│   ├── app/Application.hpp
│   ├── camera/Camera.hpp
│   ├── core/
│   ├── renderer/
│   ├── rhi/
│   └── terrain/
├── shaders/
│   ├── atmo_*.comp, cloud_noise_bake.comp, bloom_*.comp
│   ├── gtao*.comp, material_bake.comp, volumetric_rays.comp
│   ├── terrain.{comp,vert,frag}, shadow_terrain.vert
│   ├── foliage.{vert,frag}, shadow_foliage.vert
│   ├── sky.{vert,frag}, precipitation.{vert,frag}, postprocess.{vert,frag}
│   └── water*.{comp,vert,frag}
└── src/
    ├── app/
    ├── camera/
    ├── core/
    ├── main.cpp
    ├── renderer/
    ├── rhi/{RHIFactory.cpp, metal/, vulkan/}
    └── terrain/
```

---

## Building & Running

### Prerequisites
1. **C++17 Compiler** (Clang 12+, GCC 10+, or MSVC 2019+) & Objective-C++ on macOS
2. **CMake 3.22+** & **Python 3** (for SPIRV-Cross reflection binding maps on macOS)
3. **Vulkan SDK / `glslc`** (and `spirv-cross` on macOS for Metal MSL cross-compilation)
4. **GLFW3**, **GLM**, and Dear ImGui in `vendor/imgui`

### macOS (Apple Silicon & Intel)
```bash
brew install cmake ninja glfw glm shaderc spirv-cross
# Or install LunarG Vulkan SDK (includes glslc, spirv-cross, and MoltenVK)

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
./build/terrain --backend metal
```

### Linux (Ubuntu / Debian / Arch)
```bash
# Ubuntu / Debian:
sudo apt update && sudo apt install build-essential cmake ninja-build libvulkan-dev libglfw3-dev libglm-dev glslc

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/terrain --backend vulkan
```

### Windows (MSVC + Vulkan SDK + vcpkg)
```cmd
vcpkg install glfw3:x64-windows glm:x64-windows
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=[path-to-vcpkg]/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
.\build\Release\terrain.exe
```
