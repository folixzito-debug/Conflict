# CONFLICT — Ages of War

Conflict is a native Windows 2D historical fighting game. The presentation and Windows application are written in C++17 with Win32 and GDI; the combat rules are being moved into a small Rust library loaded as a native DLL. No Unity, Unreal, Godot, HTML or browser wrapper is used.

## Version 0.3.0 — Arena and Rust combat core

- Redesigned medieval arena drawn directly in C++: fortress wall, central gate, stone columns, banners, torches, audience silhouettes and tiled stone floor.
- More detailed blue and red armored fighters, swords, shields, shadows and hit feedback.
- Player versus a simple computer-controlled rival.
- Health bars, victory/defeat state and round restart.
- Rust combat core supplies hit-range and blocking-damage rules through a small C ABI.
- The game loads `conflict_core.dll` beside `Conflict.exe`. A C++ fallback keeps the prototype playable if the DLL is missing.

## Controls

- **A / D** or **Left / Right** — move
- **J** — attack
- **K** — block
- **R** — restart the round
- **Esc** — return to the menu

## Build on Windows

Requires Windows 10/11, Visual Studio 2022 Build Tools with the Desktop development with C++ workload, CMake and the Rust stable toolchain (Cargo).

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

When Cargo is available, the CMake post-build step compiles `rust/Cargo.toml` and copies `conflict_core.dll` beside the executable. The output is `build/Release/Conflict.exe`.

## Next

The intro/cinematic is planned as a separate step after the arena foundation is tested. Current character art is procedural placeholder artwork, not final sprite animation.
