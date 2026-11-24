# CONFLICT — Real-Time 3D

**Build 0.5.0 · 2026 direction**

The 2D territory interface, troop counters and menus have been replaced by a real-time 3D scene.

## Visual direction
- White application background with no visible menu, counters, labels or HUD.
- A 3D soil clearing sits on a visible stone-rock base.
- Trees and scattered stones frame the central field.
- Small colored sphere units represent the two teams.
- The camera looks down at the entire 3D model from an isometric angle.

## Real-time simulation
- 20 sphere units per team at round start.
- Units continuously identify and approach the nearest opponent.
- Units engage when close enough; defeated units disappear.
- The remaining team wins the round.
- After the winner has been decided, the next round resets automatically.

## Technology
- **C++17 + Direct3D 11 / DXGI** — native Windows window, 3D mesh rendering, depth buffer and presentation.
- **Rust** — real-time unit state, movement, target selection, engagement and automatic round reset.
- No HTML, browser wrapper, Unity, Unreal or Godot.
- F11 toggles borderless mode; Esc returns to the windowed mode.

## NVIDIA overlay
The application uses Direct3D 11 and DXGI and does not capture Alt+Z or block external overlays. NVIDIA App overlay compatibility is intended, but availability depends on the installed NVIDIA App, driver and system configuration. This is not an NVIDIA-certified integration.

## Build on Windows 10/11
Requirements: Visual Studio with Desktop development with C++, CMake 3.24+, and Rust stable with Cargo.

\`\`\`powershell
cargo test --manifest-path rust/Cargo.toml
cmake -S . -B build -A x64
cmake --build build --config Release
\`\`\`

Keep \`Conflict.exe\` and \`conflict_core.dll\` together in the same folder.

## Current scope
The terrain, stone base, trees and troop spheres are procedurally generated low-poly 3D geometry. They are a first 3D prototype, not final photorealistic game assets or a finished cinematic.
