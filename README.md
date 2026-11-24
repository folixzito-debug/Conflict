# CONFLICT — Strategy System
**Build 0.4.0 · 2026 foundation**

Conflict has been rebuilt as a modern native Windows strategy prototype. The old medieval arena and menu have been removed.

## New direction
- Clean dark interface with a compact navigation sidebar.
- A fictional regional map is the main view.
- Troops are represented by small colored circles grouped by territory.
- Each region displays its troop count.
- Select a region, recruit 50 units, and advance turns.
- Army view lists forces in all six regions.
- Report view shows total and per-region distribution.
- Six fictional regions and three abstract factions are starter data.

## Technology
- **C++17** — Windows application, Direct3D 11 renderer, DXGI swap chain, Direct2D drawing and DirectWrite text.
- **Rust** — simulation state, troop totals, recruitment, turns and validated territory transfers.
- The Rust library is built as \`conflict_core.dll\` and loaded by the C++ application.
- Native Windows app; no browser, HTML, Unity, Unreal or Godot.

## NVIDIA App overlay
The renderer uses a Direct3D 11 / DXGI presentation path and does not block external overlays or capture Alt+Z. NVIDIA overlay availability and individual features still depend on the installed NVIDIA App version, driver, GPU and application compatibility. This project does not claim official NVIDIA certification.

Press **F11** to toggle borderless mode. Press **Esc** to leave borderless mode.

## Controls
- Click a region to select it.
- **Recruit +50** adds units to the selected territory.
- **Next Turn** advances the simulation and applies small automatic reinforcements.
- Sidebar buttons switch between Map, Army, Report and Settings.
- **F11** toggles borderless mode.

## Build on Windows 10/11
Requires Visual Studio 2022 Build Tools or Visual Studio with Desktop development with C++, CMake 3.24+, and Rust stable with Cargo.

\`\`\`powershell
cargo test --manifest-path rust/Cargo.toml
cmake -S . -B build -A x64
cmake --build build --config Release
\`\`\`

Output:
- \`build/Release/Conflict.exe\`
- \`build/Release/conflict_core.dll\`

Keep both files in the same folder when running the game.

## Roadmap
This is the new foundation. Next milestones: a larger world map, detailed territory borders, interactive transfers between adjacent regions, save/load and a cinematic intro. The current map and faction names are prototype content.
