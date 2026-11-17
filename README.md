# SCSR

**SCSR** (Space Combat Simulation Renderer) is a C++ game front end for the
[SCSE](https://github.com/Sierrastdio/SCSE) simulation engine. It links directly
to SCSE's `scse_core` library and provides an interactive 3D space view, flight
controls, targeting, weapons, and orbit navigation.

The renderer uses SDL2, OpenGL, and Dear ImGui. CMake downloads pinned versions
of these dependencies and SCSE on the first configure. To use an existing SCSE
checkout instead, set `SCSR_SCSE_SOURCE_DIR` to its root directory.

## Build on Windows with MSYS2 UCRT64

Install the compiler, CMake, and Ninja from an MSYS2 UCRT64 terminal:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja
```

Configure and build from PowerShell at the SCSR repository root:

```powershell
cmake -S . -B build-ucrt -G Ninja `
  -DCMAKE_MAKE_PROGRAM=C:/msys64/ucrt64/bin/ninja.exe `
  -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe
cmake --build build-ucrt --parallel
$env:Path = "C:\msys64\ucrt64\bin;$env:Path"
.\build-ucrt\scsr.exe
```

SCSR expects an SCSE checkout with the C++ `Simulation(World)` API, the
`SelectShips` command, and `Player.selectedShips` state. Until that engine
revision is included in the pinned SCSE revision, configure against the
matching local checkout with `-DSCSR_SCSE_SOURCE_DIR=C:/path/to/SCSE`. SCSE
provides its scenario and game rules in C++; Lua is not required. SDL2 and Dear
ImGui are fetched by CMake.

## Build on Arch Linux

Install the C++ toolchain, CMake, Ninja, Git (for CMake `FetchContent`), and
OpenGL and SDL2's common X11/Wayland development dependencies:

```bash
sudo pacman -S --needed base-devel cmake ninja git pkgconf libglvnd mesa \
  libx11 libxext libxrandr libxcursor libxinerama libxi libxfixes libxss libxtst \
  wayland wayland-protocols libxkbcommon
```

From the SCSR repository root, configure an out-of-source Ninja build:

```bash
cmake -S . -B build-arch -G Ninja -DCMAKE_BUILD_TYPE=Release
```

Here, `-S .` selects the source directory, `-B build-arch` keeps generated
files out of the source tree, and `-G Ninja` tells CMake to generate
`build.ninja`. SDL2, Dear ImGui, and the pinned SCSE revision are downloaded
during the first configure, so that step requires network access.

Build and run:

```bash
cmake --build build-arch --parallel
./build-arch/scsr
```

`cmake --build` selects Ninja from the generated build tree automatically.
Alternatively, run `ninja -C build-arch` directly. To change CMake options,
repeat the configure command with the same build directory. Do not reuse a
build directory when switching generators; configure a new one instead.

To build against a compatible local SCSE checkout rather than the pinned
revision, pass its path at configure time:

```bash
cmake -S . -B build-arch -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DSCSR_SCSE_SOURCE_DIR="$HOME/문서/clone/NEONspasim"
cmake --build build-arch --parallel
```

## View and controls

- Right-drag to orbit the camera; Shift + right-drag to pan.
- Use the mouse wheel or `+` / `-` to zoom.
- `1`–`4` select orbital, XY, XZ, and YZ projections.
- Click a friendly ship to select it; Shift-click adds or removes it from the
  control group. Drag a box to select friendly ships, or Shift-drag to add them.
- Click an enemy ship, base, torpedo, or planet to inspect it.
- Use **Ship by ID** in the targeting panel to select a specific engine ship
  when several practice targets are close together; then use **Set Target / Aim**
  (or `R`) to lock it.
- `W` accelerates, `S` brakes, `X` coasts, and the arrow keys adjust heading.
- `R` targets and aims at the selected contact; Space fires the phasor, `T`
  launches a torpedo, and `D` detonates an active torpedo.
- Per-weapon horizontal and vertical firing limits are configured in SCSE's
  `WeaponProfile::aim` settings (`maximumHorizontalFiringAngleDegrees` and
  `maximumVerticalFiringAngleDegrees`) in `src/rules/scenario.cpp`. Both are
  relative to the ship's facing.
- The selected weapon's range and horizontal/vertical firing limits are shown
  as a translucent fan around the selected ships; explosive weapons use an
  orange fan and other weapons use blue.
- The flight panel provides pause/speed controls, throttle stages, weapons, and
  planet/orbit navigation.
- Thrust, heading, target, and phasor commands apply to the selected group.
  Torpedo launch and planet/orbit navigation also apply to the selected group.
- Selecting a weapon target does not cancel an active course or orbit-transfer
  autopilot; without an active navigation order, ships aim toward the target.
- Non-torpedo weapons create simulated projectiles; shields and hull are damaged
  only when a projectile reaches its target. Torpedoes remain separate simulated
  entities.

The world view provides camera gestures, projection views, orbit levels, gravity boundaries, team colors, 3D ship markers, target-aim arrows, and ship trails.

The pinned SCSE scenario provides two stationary practice targets for each
team. The targeting panel lists each active ship by engine ID, team, and name.

## Source layout

- `src/app/`: SDL/OpenGL application lifecycle and frame loop.
- `src/game/`: SCSE simulation session, player commands, and ship trails.
- `src/input/`: camera, selection, and keyboard/mouse input.
- `src/renderer/`: world projection, contact picking, and scene drawing.
- `src/ui/`: flight, targeting, navigation, and contact HUD panels.
- `src/main.cpp`: application entry point.

SCSR retains this repository's GPL-3.0 license. SCSE, SDL2, and Dear ImGui
remain separate dependencies under their respective licenses.
