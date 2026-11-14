# THE WALL

Windows desktop launcher UI built with C++17, Win32, DirectX 11 and Dear ImGui.

## GitHub Actions build

Every push to `main` and manual workflow dispatch builds `TheWall.exe` on a Windows x64 runner. Download the artifact named **THE-WALL-Windows-x64** from the completed workflow run.

## Local build

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

CMake downloads Dear ImGui v1.91.8 during configuration. The app selects and launches a local `.exe`; it does not inject into or modify another process.
