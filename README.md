<h1 align="center">Animal Crossing PC Port</h1>
<p align="center">
  <sub><b>A PC port of <em>Animal Crossing</em> (GameCube)</b></sub>
</p>

> [!IMPORTANT]
> This project is made to test our Rainfall library, originally built for Courage-Reborn Twilight Princess Port on another game. If you want to contribute, please join our discord : https://discord.gg/3kPcPs9t5y

> [!IMPORTANT]
> This repository does **not** contain any game assets or assembly. A legally obtained copy of the original game is required.

---

## About

Animal Crossing PC Port is a native, cross-platform port of *Animal Crossing* (GameCube), built on top of the [Animal Crossing decompilation](https://github.com/ACreTeam/ac-decomp). The original game code runs unchanged with minor modifications.

Input and audio use [SDL3](https://github.com/libsdl-org/SDL). Rendering uses a custom OpenGL backend by default, or [Rainfall](https://github.com/linifadomra/rainfall) when built with `-DAC_USE_RAINFALL=ON`.

Supported disc version: **GAFE01_00** (USA, Rev 0).

---

## Installation

Pre-built Windows builds are on the [Releases](https://github.com/flyngmt/ACGC-PC-Port/releases) page.

1. Download and extract the latest zip
2. Put your disc image in the `rom/` folder next to the executable
3. Run `AnimalCrossing.exe`

The game loads assets from the disc image at startup. No extraction step is needed.

---

## Supported platforms

- **Windows** (32-bit and 64-bit)
- **macOS** (Apple Silicon and Intel)
- **Linux** (x86_64 and ARM64)

A legally obtained GameCube ISO, GCM, or CISO of Animal Crossing (USA) is required at runtime.

---

## Building

Clone with submodules:

```bash
git clone --recurse-submodules https://github.com/flyngmt/ACGC-PC-Port.git
cd ACGC-PC-Port/pc
mkdir build && cd build
cmake ..
cmake --build . -j$(nproc)
```

**Dependencies:** CMake 3.16+, SDL3, OpenGL. Clang or GCC both work on 64-bit.

**macOS example (64-bit, Rainfall renderer, Clang):**

```bash
brew install sdl3 cmake
git submodule update --init pc/external/rainfall
cd pc/build64
cmake .. -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
         -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/sdl3 -DAC_USE_RAINFALL=ON
cmake --build . -j8
```

GCC also works if you prefer it (`brew install gcc`, then `gcc-16` / `g++-16`).

**Windows (MSYS2):** use the MINGW32 or MINGW64 shell, install `gcc`, `cmake`, and `SDL3`, then run `cmake ..` from `pc/build`.

Place your disc image in `rom/` next to the built executable. The port also checks `orig/` and the current directory.

**Launch options:**
- `--verbose` - extra logging
- `--no-framelimit` - unlock the frame limiter
- `--model-viewer [index]` - debug model viewer
- `--time HOUR` - override the in-game hour (0-23)

**CMake options:**
- `-DAC_USE_RAINFALL=ON` - use the Rainfall GX backend instead of legacy `pc_gx`
- `-DAC_VERSION=0` - game region (0=USA, 1=AUS, 2=JPN)

---

## Controls

Keyboard defaults are in `keybindings.ini` next to the executable. SDL3 gamepads are supported with hotplug.

| Key | Action |
|-----|--------|
| WASD | Move |
| Arrow keys | Camera |
| Space | A |
| Left Shift | B |
| Enter | Start |
| X / Y | X / Y |
| Q / E | L / R |
| Z | Z trigger |
| I / J / K / L | D-pad |

Graphics options live in `settings.ini` (resolution, fullscreen, vsync, MSAA). Dolphin-compatible HD textures go in `texture_pack/`.

Save files use GCI format in the `save/` folder and work with Dolphin exports.

---

## Credits

- [ACreTeam/ac-decomp](https://github.com/ACreTeam/ac-decomp) - Animal Crossing decompilation
- [Linifadomra/rainfall](https://github.com/linifadomra/rainfall) - GX renderer (optional backend)
- [libsdl-org/SDL](https://github.com/libsdl-org/SDL) - Windowing, input, audio
- [FIX94/fixNES](https://github.com/FIX94/fixNES) - NES emulator for the in-game console

---

## License

The decompilation sources follow [CC0 1.0](https://github.com/ACreTeam/ac-decomp). The PC port layer (`pc/` and `TARGET_PC` changes) is [MIT](LICENSE).

This project does not include any Nintendo intellectual property. You need a legally obtained copy of the original game disc image at runtime.
