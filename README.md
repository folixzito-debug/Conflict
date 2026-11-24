# CONFLICT — Native 3D

**Build 0.6.0 · Windows x64**

CONFLICT is a native Windows 3D prototype built with C++17, Direct3D 11 and a Rust simulation library. It does not use HTML, a browser wrapper, Unity, Unreal or Godot.

## Graphics quality

Change the graphics preset without opening a menu:

| Key | Preset | Rendering |
|---|---|---|
| F1 | Low | 24 × 18 terrain grid, simpler foliage, 1× sampling |
| F2 | Medium | 48 × 36 terrain grid, detailed foliage, 2× MSAA when supported |
| F3 | High | 80 × 60 terrain grid, extra foliage and ground detail, 4× MSAA when supported |

The renderer checks the GPU's supported multisample levels and falls back to a lower level when necessary. The presets also change the per-pixel specular response and the detail level of vegetation and unit models.

## Scene and unit roles

- White application background, with no in-scene menu, labels, counters or HUD.
- Uneven soil terrain on a visible rocky base, with trees and scattered stones.
- Separate 3D character meshes for line units, medics and engineers.
- An unarmed supply truck model with wheels, cab glass and cargo body.
- Medics and engineers restore nearby damaged allies. Supply trucks provide a wider-area support pulse.
- Simple ground-contact shadows and green support pulses help communicate movement and support actions.
- The existing abstract round simulation continues to reset automatically.

## Controls

- **F1 / F2 / F3** — Low / Medium / High graphics.
- **F11** — Toggle borderless mode.
- **Esc** — Return to windowed mode.

## Technology

- **C++17 + Direct3D 11 / DXGI** — native rendering, depth buffer, HLSL per-pixel lighting, alpha blending and MSAA resolve.
- **Rust** — unit state, movement, support healing and automatic round reset.
- **NVIDIA App overlay** — the application uses Direct3D 11 and does not intercept Alt+Z. Compatibility depends on the installed NVIDIA App, driver and system configuration; it is not NVIDIA-certified.

## Build on Windows 10/11

Requirements: Visual Studio with Desktop development with C++, CMake 3.24+, and Rust stable with Cargo.

\`\`\`powershell
cargo test --manifest-path rust/Cargo.toml
cmake -S . -B build -A x64
cmake --build build --config Release
\`\`\`

Keep \`Conflict.exe\` and \`conflict_core.dll\` together in the same folder.

## Current scope

The terrain, vehicles and units are generated from procedural meshes. Lighting uses a compact real-time diffuse/specular shader rather than a complete texture-based PBR material pipeline. The models are more detailed than the previous sphere-only prototype, but this is still an early 3D prototype, not photorealistic final art.
