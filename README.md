# CONFLICT

Conflict is a lightweight 2D historical fighting game prototype. The first milestone is a native Windows menu and interface, built without Unity, Unreal, Godot, or a browser-based wrapper.

## Current milestone
- Native Windows desktop application (Win32 + GDI).
- Main menu with Campaign, Quick Fight, Arsenal, Eras, Settings, and Exit.
- Dark medieval visual direction with a timeline spanning year 0 to the modern era.
- Keyboard and mouse navigation.

## Build
Requires Windows 10/11 and Visual Studio 2022 Build Tools with the Desktop development with C++ workload.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

The executable is produced at `build/Release/Conflict.exe`.

## Controls
- Mouse: select menu items.
- Up / Down: move selection.
- Enter: activate selection.
- Escape: return to the main menu.

This is the interface milestone. Combat systems and historical equipment will be added in later versions.