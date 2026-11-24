# CONFLICT

Conflict is a native Windows 2D historical fighting game prototype, built with C++17, Win32 and GDI. It does not use Unity, Unreal, Godot, HTML or a browser wrapper.

## Version 0.2.0 — 2D Arena
- Playable arena launched from **Combate rápido**.
- Player versus a basic computer-controlled rival.
- Health bars, victory/defeat state and round restart.
- Medieval courtyard arena and simple fighter/sword artwork drawn in code.
- Keyboard controls: A/D or Left/Right to move, J to attack, K to block, R to restart, Esc to return to menu.

## Current limitations
This is an early gameplay prototype. It currently has placeholder 2D character artwork, one arena, one opponent behavior and no campaign progression, sound, online multiplayer or weapon selection yet.

## Build
Requires Windows 10/11 and Visual Studio 2022 Build Tools with the Desktop development with C++ workload.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

The executable is produced at `build/Release/Conflict.exe`.
