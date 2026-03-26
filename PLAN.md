# PaniniWoW Implementation Plan

Panini projection post-process DLL for WoW 1.12.1 (TurtleWoW). Hooks D3D9 EndScene, applies cylindrical projection via pixel shader, configurable through in-game CVar system.

## Runtime Dependencies

- WoW 1.12.1 client
- SuperWoW (required; provides CVar infrastructure and FoV control)

## Build Dependencies

- C/C++ cross-compiler (`i686-w64-mingw32-gcc` via `brew install mingw-w64`)
- Wine (`brew install --cask wine-stable`)
- cmake and ninja (provided by mise)

## Architecture

```
Lua Addon (SetCVar) --> WoW CVar System <-- DLL (reads CVars each frame)
                                              |
                                         EndScene hook
                                              |
                                    StretchRect + panini shader
```

## Implementation Layers (bottom-up)

Tasks are ordered so each layer locks in contracts/types that higher layers consume. No circular dependencies.

### Layer 0: Logging

Replace printf-based logging with a two-level system.

- **Levels:** `TRACE` (development diagnostics, disabled by default) and `INFO` (operational state changes the user cares about)
- **Format:** `[LEVEL] [module] message` with accumulative detail; one log line per significant event, packed with context
- **Wide/fat entries:** each INFO log line includes all relevant state (e.g., `[INFO] [hook] EndScene hooked at 0xDEADBEEF, vtable at 0xCAFE, device 0x1234`) rather than spreading across multiple lines
- **TRACE examples:** per-frame shader constant values, config poll results, state save/restore detail
- **INFO examples:** init complete, hook installed, device reset, config changed, resource created/released
- **No external libs.** `fprintf` to `mods/PaniniWoW.log`, flushed after each write
- **No runtime configuration.** TRACE compiled in for debug builds (`CMAKE_BUILD_TYPE=Debug`), INFO-only for release
- **Files:** `src/log.cpp`, `include/panini.h` (update LogTrace, LogInfo signatures)

### Layer 1: CVar System Access [NEEDS RESEARCH]

Read WoW's CVar values from the DLL without file I/O.

- **Mechanism:** the addon calls `SetCVar("paniniEnabled", "1")` etc; the DLL reads those CVars by calling WoW's internal CVar lookup function or scanning the CVar hash table
- **CVars (account-wide):**
  - `paniniEnabled` (string "0"/"1", default "0")
  - `paniniStrength` (string float "0.0" to "1.0", default "0.5")
  - `paniniVertical` (string float "-1.0" to "1.0", default "0.0")
  - `paniniFill` (string float "0.0" to "1.0", default "0.8")
- **Research needed:**
  - WoW 1.12.1 CVar system memory layout (hash table address, CVar struct fields)
  - Function address for CVar lookup by name (or direct memory scan pattern)
  - Whether `SetCVar` on an unregistered name creates it, or whether the DLL must register CVars first
  - How SuperWoW registers its CVars (function hooks, calling convention)
- **Files:** `src/cvar.cpp` (new), `include/panini.h` (CVar reader declarations)

### Layer 2: Camera FOV Access [NEEDS RESEARCH]

Read the active camera's field of view each frame.

- **Mechanism:** read float at known offset within CCamera/CSimpleCamera struct
- **Known:** FOV at offset 0x40 (CSimpleCamera) or 0x12C (CameraBlock); set by SuperWoW's FoV CVar
- **Research needed:**
  - Static address of active camera pointer for TurtleWoW's 1.12.1 build (may differ from stock 5875)
  - Pattern scan approach if address differs across builds
  - SEH-guarded read with fallback to default 1.5708 rad
- **Files:** `src/panini.cpp` (update ReadCameraFov)

### Layer 3: D3D9 Device Discovery and Vtable Hooking

Find the game's D3D9 device and install EndScene/Reset hooks.

- **Dummy device technique:**
  1. Wait for WoW window (`FindWindowA("GxWindowClass", NULL)`)
  2. Call `Direct3DCreate9(D3D_SDK_VERSION)` to get an IDirect3D9 interface
  3. Create a temporary device with minimal present parameters
  4. Extract vtable from device pointer (`*(void***)pDevice`)
  5. Save original EndScene (vtable[42]) and Reset (vtable[16])
  6. Overwrite with our hooks via `VirtualProtect` + pointer swap
  7. Release dummy device (vtable pointers remain valid; they point to the D3D9 implementation's code)
- **Compatibility:** works with native D3D9, DXVK, ReShade proxy chains; the vtable is the IDirect3DDevice9 interface contract regardless of backend
- **Files:** `src/dllmain.cpp` (implement InitThread), `src/hooks.cpp` (hook installation)

### Layer 4: Panini Post-Process Pass

The core rendering logic in the EndScene hook.

- **Frame flow:**
  1. Read CVars (Layer 1) for enabled/strength/vertical/fill
  2. Early-out if disabled or strength < epsilon
  3. Read camera FOV (Layer 2), compute halfTanFov and fillZoom (already implemented in `panini.cpp`)
  4. Get backbuffer via `GetRenderTarget(0, ...)`
  5. Lazy-create resources on first frame: render target texture (`CreateTexture` with `D3DUSAGE_RENDERTARGET`), pixel shader (`CreatePixelShader` from compiled bytecode)
  6. `SaveD3D9State` (already implemented in `state.cpp`)
  7. `StretchRect` backbuffer to scene texture
  8. Set render target back to backbuffer
  9. Bind scene texture, panini pixel shader, shader constants (c0, c1)
  10. Set render states: Z off, alpha blend off, cull none
  11. Draw fullscreen quad via `DrawPrimitiveUP` (pre-transformed vertices with UVs, D3D9 half-pixel offset applied)
  12. `RestoreD3D9State`
  13. Call original EndScene
- **Shader constants:**
  - c0: `{strength, halfTanFov, fillZoom, 1.0}`
  - c1: `{verticalComp, aspect, 0, 0}`
- **Files:** `src/hooks.cpp` (implement Hooked_EndScene), `src/panini.cpp` (resource creation helpers)

### Layer 5: Device Reset Handling

Handle `IDirect3DDevice9::Reset` (alt-tab, resolution change).

- On Reset: release scene texture surface, scene texture, pixel shader (all `D3DPOOL_DEFAULT` resources)
- Set `resourcesCreated = false`
- Next EndScene call recreates everything (lazy init in Layer 4)
- **Files:** `src/hooks.cpp` (implement Hooked_Reset)

### Layer 6: Lua Addon (CVar-based)

Update addon to use CVars instead of ExportFile/config file.

- On `ADDON_LOADED`: apply SavedVariables defaults, then call `SetCVar` for each panini CVar
- On slash command change: update SavedVariables, call `SetCVar` immediately
- On `PLAYER_ENTERING_WORLD`: re-apply CVars (ensures DLL sees current values after any reload)
- Remove all `ExportFile` and file path references
- Account-wide `SavedVariables` (not `SavedVariablesPerCharacter`)
- **Files:** `PaniniClassicWoW/PaniniClassicWoW.lua` (rewrite config bridge)

## Research Phase

Layers 1 and 2 have unknowns that block implementation. A targeted research task should resolve:

1. WoW 1.12.1 CVar system internals (memory layout, lookup function address, registration mechanism)
2. Active camera pointer address for TurtleWoW's build
3. Whether SuperWoW's CVar registration is cooperative (can a second DLL register CVars alongside it)

Sources: wowdev.wiki, OwnedCore 1.12.1 info dumps, SuperWoW GitHub wiki/issues, namreeb's WoW modding repos.

## Logging Parameters

```
Levels:     TRACE (debug builds only), INFO (all builds)
Format:     [LEVEL] [module] message
Output:     mods/PaniniWoW.log
Flush:      after every write
External:   none
Config:     none (compile-time TRACE gate via NDEBUG / CMAKE_BUILD_TYPE)
```

TRACE provides per-frame detail (shader constants, CVar reads, state operations). INFO provides lifecycle events (init, hook install, config change, resource create/release, device reset). A single INFO line at startup should confirm the full stack: hook address, device pointer, shader compilation status, initial CVar values.
