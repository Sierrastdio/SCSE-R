# SCSR

**SCSR** (Space Combat Simulation Renderer) is a C++ game front end for the
[SCSE](https://github.com/Sierrastdio/SCSE) simulation engine. It links directly
to SCSE's `scse_core` library and provides an interactive 3D space view, flight
controls, targeting, weapons, and orbit navigation.

The renderer uses SDL2, OpenGL, and Dear ImGui. CMake downloads pinned versions
of these dependencies and SCSE on the first configure. To use an existing SCSE
checkout instead, set `SCSR_SCSE_SOURCE_DIR` to its root directory.

## Build on Windows with MSYS2 UCRT64

Install the compiler, CMake, Ninja, and Lua development files from an MSYS2
UCRT64 terminal:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-lua
```

Configure and build from PowerShell at the SCSR repository root:

```powershell
cmake -S . -B build-ucrt -G Ninja `
  -DCMAKE_MAKE_PROGRAM=C:/msys64/ucrt64/bin/ninja.exe `
  -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe `
  -DLUA_INCLUDE_DIR=C:/msys64/ucrt64/include `
  -DLUA_LIBRARY=C:/msys64/ucrt64/lib/liblua.dll.a
cmake --build build-ucrt --parallel
$env:Path = "C:\msys64\ucrt64\bin;$env:Path"
.\build-ucrt\scsr.exe
```

If SCSE is already checked out locally, add
`-DSCSR_SCSE_SOURCE_DIR=C:/path/to/SCSE` to the configure command. Lua 5.4 is
required by the engine; SDL2 and Dear ImGui are fetched by CMake.

## View and controls

- Right-drag to orbit the camera; Shift + right-drag to pan.
- Use the mouse wheel or `+` / `-` to zoom.
- `1`–`4` select orbital, XY, XZ, and YZ projections.
- Click a ship, base, torpedo, or planet to inspect it.
- `W` accelerates, `S` brakes, `X` coasts, and the arrow keys adjust heading.
- `R` targets and aims at the selected contact; Space fires the phasor, `T`
  launches a torpedo, and `D` detonates an active torpedo.
- The flight panel provides pause/speed controls, throttle stages, weapons, and
  planet/orbit navigation.

The world view retains the Python debug renderer's camera gestures, projection
views, orbit levels, gravity boundaries, team colors, heading/velocity vectors,
and ship trails. The GUI is a separate front end and does not change SCSE's
console or snapshot renderer.

SCSR retains this repository's GPL-3.0 license. SCSE, SDL2, and Dear ImGui
remain separate dependencies under their respective licenses.
