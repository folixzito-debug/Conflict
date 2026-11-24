# CONFLICT — Native 3D

**Build 0.7.0 · Windows x64**

CONFLICT is a native Windows 3D prototype built with C++17, Direct3D 11 and a Rust simulation library. No HTML, browser wrapper, Unity, Unreal or Godot.

## Sphere-only units

All units remain spheres. Human-shaped models and vehicles have been removed. Each sphere carries a small 3D class marker:
- **Swordsman** — stylised sword accessory.
- **Medic** — white medical kit with red cross; supports nearby allies.
- **Catapult** — compact, stylised catapult model.
- **Modern military** — sphere with a small equipment pack and antenna.

The models are intentionally symbolic and game-like, not realistic weapon replicas. Select a class in the bottom unit bar to add it to the chosen team. Maximum simulated units: 64.

## Menus and controls

- **Esc** — open Options; from Graphics return to Options; from Options return to the game.
- **Options → Graphics** — Low, Medium, High, Ultra.
- **Options → Frame limit** — slider from 0 to 240 FPS; 0 means unlimited.
- **Options → NVIDIA App** — Direct3D 11 overlay compatibility information. Alt+Z is not intercepted. Availability depends on the installed NVIDIA App and driver; not NVIDIA-certified.
- **Start** — choose Blue or Red team.
- **Unit bar** — add Swordsman, Medic, Catapult or Modern Military sphere to your team.
- **Free camera** — WASD to move, Q/E to lower/raise, hold right mouse button and drag to look, mouse wheel to zoom.
- **F1 / F2 / F3 / F4** — quick Low / Medium / High / Ultra selection.
- **F11** — toggle borderless window.

The native menu uses the **Anton** typeface from Google Fonts, bundled as `Anton-Regular.ttf` with its OFL license. The font is loaded privately at runtime and does not need to be installed in Windows.

## Graphics

| Preset | Terrain grid | MSAA target |
|---|---:|---:|
| Low | 24 × 18 | 1× |
| Medium | 48 × 36 | 2× |
| High | 80 × 60 | 4× |
| Ultra | 120 × 90 | 8× |

The renderer checks the GPU's supported multisample levels and falls back automatically if a requested level is unavailable.

## Technology

- **C++17 + Direct3D 11 / DXGI** — native rendering, depth buffer, HLSL lighting, alpha blending and MSAA resolve.
- **Rust** — abstract game simulation, medic support and spawning selected unit classes.
- **NVIDIA App** — designed not to intercept Alt+Z; compatibility depends on system configuration.

## Build on Windows 10/11

Requirements: Visual Studio with Desktop development with C++, CMake 3.24+, and Rust stable with Cargo.

```powershell
cargo test --manifest-path rust/Cargo.toml
cmake -S . -B build -A x64
cmake --build build --config Release
```

Keep `Conflict.exe`, `conflict_core.dll` and `Anton-Regular.ttf` in the same folder.

## Current scope

The terrain and unit props are generated from procedural meshes. Lighting uses a compact diffuse/specular shader rather than a complete texture-based PBR pipeline. This is a playable prototype, not final photorealistic art.
