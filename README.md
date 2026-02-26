# VOS — Virtual OS

A privacy-focused, portable virtual operating environment.

## Features
- **IP Rotation**: Automatic identity cycling every 10 seconds
- **Mesh Networking**: P2P data transfer without internet (Wi-Fi Direct / Bluetooth)
- **Lockdown Mode**: Timer-based restriction to Calls, SMS, Camera only
- **Cross-Platform**: Desktop (Windows/Linux) and Android

## Build (Desktop)

```bash
cmake -B build
cmake --build build
./build/vos_desktop
```

## Architecture
- `src/core/`     — Platform-agnostic C/C++ engine
- `src/platform/` — OS-specific implementations
- `src/shell/`    — Desktop GUI (SDL2 + ImGui)
- `src/apps/`     — Built-in apps (Dialer, SMS, Camera)
- `tests/`        — Unit tests

## License
Proprietary — All rights reserved.
