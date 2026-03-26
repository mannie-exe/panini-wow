# PaniniWoW Implementation Plan

Panini projection post-process DLL for WoW 1.12.1 (TurtleWoW). Hooks CGWorldFrame::RenderWorld, applies cylindrical projection via pixel shader, configurable through in-game CVar system.

## Current Status

v1 feature-complete. All layers implemented and tested. Shader produces correct panini-warped output. FoV swap system allows seamless toggle without manual FOV management. Building under SSE2 eliminates x87 FPU precision issues entirely.

## Runtime Dependencies

- WoW 1.12.1 client (TurtleWoW)
- SuperWoW (required for CVar infrastructure and FoV ownership)

## Build Dependencies

- C/C++ cross-compiler (`i686-w64-mingw32-gcc` via `brew install mingw-w64`)
- Wine (`brew install --cask wine-stable`)
- cmake and ninja (provided by mise)

## Architecture

```
Lua Addon (SetCVar) --> WoW CVar System <-- DLL (reads CVars each frame)
                                              |
                                    RenderWorld hook (0x00482D70)
                                    UpdateCameraFov() writes camera+0x40
                                    calls original chain (3D renders)
                                    calls ApplyPaniniPass() (post-process)
                                              |
                                    StretchRect + panini shader
                                              |
                                    UI draws on top (undistorted)
```

## Completed Layers

### Layer 0: Logging [DONE]
INFO-level always on. DEBUG-level gated behind `PANINI_DEBUG_LOG` CMake flag. Hex-bit float logging via `FloatBits()` (MinGW `%f` broken under Wine).

### Layer 1: CVar System [DONE]
Registration via `CVar::Register` at `0x0063DB90`. Lookup via `CVarLookup` at `0x0063DEC0`. 7 CVars registered: paniniEnabled, paniniStrength, paniniVertComp, paniniFill, paniniDebug, paniniFov, paniniUserFov.

### Layer 2: RenderWorld Hook [DONE]
JMP chain on existing hook at `0x00482D70`. Calls existing chain via trampoline with ECX clobber-safe inline asm. UpdateCameraFov writes camera+0x40 before 3D render. ApplyPaniniPass runs after 3D render.

### Layer 3: D3D9 EndScene Hook [DONE]
Dummy device vtable technique. EndScene at vtable[42], Reset at vtable[16]. Used for resource management (lazy create on resize, release on Reset).

### Layer 4: Panini Post-Process Pass [DONE]
StretchRect backbuffer to texture, fullscreen quad (D3DFVF_XYZRHW) with panini pixel shader, state save/restore. Production shader with `tex2D` sampling and out-of-bounds black. ComputeFillZoom with SSE2 math (no x87).

### Layer 5: Device Reset [DONE]
Resource release on Reset, lazy recreation on next EndScene.

### Layer 6: Lua Addon [DONE]
CVar-based config, SavedVariables, slash commands (`/panini on|off|toggle|debug|strength|vertical|fill|fov|reset|status|cvars|debuginfo`). FoV swap on toggle (paniniFov <-> paniniUserFov). External FoV change tracking via OnUpdate.

### Layer 7: World-Active Detection [DONE]
`IsWorldActive()` checks `CGWorldFrame` pointer at `0x00B4B2BC`. Panini pass skipped at login screen, character select, loading screens.

### Layer 8: Shader Compilation Pipeline [DONE]
fxc2 (vendored, patched -Vn bug), invoked via Wine, produces ps_3_0 bytecode headers embedded in DLL.

### Layer 9: FoV Swap System [DONE]
UpdateCameraFov() writes camera+0x40 directly from RenderWorld hook (before 3D render). Lua addon manages paniniFov (panini FOV) and paniniUserFov (user's non-panini FOV). On enable: camera gets paniniFov. On disable: camera gets paniniUserFov. External FoV changes tracked and saved to paniniUserFov.

### Layer 10: FPU Precision [DONE]
`-msse2 -mfpmath=sse` in toolchain forces all float math through SSE2 XMM registers. Eliminates x87 extended precision and FPUCW corruption entirely. ComputeFillZoom produces correct results (verified via hex dump).

## Known Issues (non-blocking)

- **MinGW printf float bug**: `%f` displays all floats as `inf` under Wine. Workaround: `FloatBits()` + `%08X`. Display-only, does not affect runtime values.
- **One-frame FoV mismatch on toggle**: `SetCVar("FoV", v)` updates CVar but not CGCamera's dirty flag. UpdateCameraFov compensates by writing camera+0x40 directly before 3D render.

## v2 Tasks

### Context-Aware Strength
Read character state (mounted, in combat, swimming, indoors) and adjust panini strength or disable automatically. Requires game state detection hooks.

### Per-Context FOV
Different FOV for exploration, combat, indoor, outdoor. Driven by game state detection.

### Settings GUI
Replace slash commands with an in-game settings frame (slider widgets). Ace3 or native WoW frame XML.

### Pattern Scanning for Addresses
Replace hardcoded memory addresses (`0x0063DEC0`, `0x00B4B2BC`, etc.) with byte-pattern scans to survive TurtleWoW binary updates.

### Debug Shader Subcommands
`/panini debug tint` and `/panini debug uv` independently. Currently single toggle.

## Research Reports

| Report | Path | Summary |
|--------|------|---------|
| 01 Modding Landscape | `~/.atlas/integrator/reports/panini-classic-wow/01-*` | WoW 1.12.1 modding techniques, APIs, D3D9 hooking |
| 02 Feasibility | `~/.atlas/integrator/reports/panini-classic-wow/02-*` | Six approaches evaluated; Approach F recommended |
| 03 Design | `~/.atlas/integrator/reports/panini-classic-wow/03-*` | Full architecture, shader, addon, build system |
| 04 Implementation | `~/.atlas/integrator/reports/panini-classic-wow/04-*` | CVar addresses, camera offsets, vtable indices |
| 05 SuperWoW Review | `~/.atlas/integrator/reports/panini-classic-wow/05-*` | SuperWoW internals, nampower hooking, DXVK vtable analysis |
| 06 FOV + Pre-UI Hook | `~/.atlas/integrator/reports/panini-classic-wow/06-*` | Direct FOV write, RenderWorld hook, device pointer chain |
| 07 MinGW NaN | `~/.atlas/integrator/analysis/panini-classic-wow/mingw-nan-research-*` | x87 80-bit precision, SSE2 fix analysis |
| 08 D3D9 Post-Process | `~/.atlas/integrator/analysis/panini-classic-wow/d3d9-postprocess-research-*` | WoW pipeline order, fullscreen quad, DXVK compatibility |
| 09 CVar Float | `~/.atlas/integrator/analysis/panini-classic-wow/cvar-float-research-*` | CVar struct layout, float init timing |
| 10 Panini Projection | `~/.atlas/integrator/analysis/panini-classic-wow/panini-projection-research-*` | Fill-zoom validation, tex2Dgrad recommendation |
| 11 libSilicon FPU | `~/.atlas/integrator/analysis/panini-classic-wow/libsilicon-fpu-research-*` | rosettax87/libSiliconPatch analysis, FPUCW investigation |

## Decisions Log
- 2026-03-25: Approach F selected — Hybrid DLL (dlls.txt) + Lua Addon [atlas]
- 2026-03-25: Config file IPC chosen over CVars or shared memory — zero coupling with SuperWoW [integrator]
- 2026-03-25: Pre-compiled DXSO bytecode preferred over runtime D3DXCompileShader [integrator]
- 2026-03-25: FOV read from camera memory, not CVar system [integrator] — later reversed: CVar-first, camera as fallback
- 2026-03-25: UI distortion accepted for v1; UI exclusion deferred to v2 [integrator]
- 2026-03-26: SSE2 toolchain flags eliminate x87 FPU entirely — `_control87` workaround not needed [atlas]
- 2026-03-26: ComputeFillZoom math verified correct via Python trace. Bug was runtime FPU, not algorithm [atlas]
- 2026-03-26: ReadCameraFov changed to CVar-first priority (camera memory is stale between SetCVar and reload) [atlas]
- 2026-03-26: FoV swap system via paniniFov/paniniUserFov CVars + direct camera+0x40 write [atlas]
- 2026-03-26: Inline asm crash fix — added "ecx" to clobber list to prevent register allocation collision [atlas]
