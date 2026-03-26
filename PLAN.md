# PaniniWoW Implementation Plan

Panini projection post-process DLL for WoW 1.12.1 (TurtleWoW). Hooks CGWorldFrame::RenderWorld, applies cylindrical projection via pixel shader, configurable through in-game CVar system.

## Current Status

All layers implemented and functional at a baseline level. RenderWorld hook chains on existing mod hook. Panini shader produces correct panini-warped UV output (verified via diagnostic mode). Full rendering pipeline confirmed working (StretchRect, CreatePixelShader, DrawPrimitiveUP through DXVK).

### Active Bugs

#### 1. FPU Control Word Corruption (CRITICAL)

**Observed**: `FPUCW = 0x027F` (single precision) instead of `0x037F` (double precision, standard default).

**Effect**: `cosf(1.054)` returns `+infinity` instead of `0.494`. This causes `denom = 0.5 + inf = inf`, cascading through `k`, `edgeX`, `fitX`, `zoom`, `result` as NaN (`0xFFC00000`). ComputeFillZoom returns NaN.

**Workaround**: NaN guard in hooks.cpp falls back to `zoom = 1.273f`. Visual output shows panini warping with excessive black borders (zoom not adaptive to current FOV).

**Root cause**: Someone in the DLL stack (WoW, libSiliconPatch, or Wine) sets FPU to single precision mode. Investigation cleared rosettax87 (operates at Rosetta ptrace layer) and libSiliconPatch (hooks WoW binary functions, no FPUCW modification in code). Suspect remains WoW itself or Wine/DXVK initialization.

**Fix needed**: Set FPUCW to double precision via `_control87(0x037F, 0xFFFF)` in our DLL init, before any math. Alternatively, precompute zoom in the Lua addon (safe from FPU issues) and pass via a new CVar.

#### 2. Printf Float Bug (KNOWN, WORKAROUND IN PLACE)

MinGW 32-bit varargs under Wine makes `%f` display all floats as `inf`. Workaround: log floats as hex via `FloatBits()` + `%08X`. Does NOT affect runtime float values (verified via hex dump).

#### 3. Shader in Diagnostic Mode (INTENTIONAL, TEMPORARY)

panini.hlsl outputs UV color encoding, not texture sample. Must switch to production before release.

## Runtime Dependencies

- WoW 1.12.1 client
- SuperWoW (required for CVar infrastructure)

## Build Dependencies

- C/C++ cross-compiler (`i686-w64-mingw32-gcc` via `brew install mingw-w64`)
- Wine (`brew install --cask wine-stable`)
- cmake and ninja (provided by mise)

## Architecture

```
Lua Addon (SetCVar) --> WoW CVar System <-- DLL (reads CVars each frame)
                                              |
                                    RenderWorld hook (0x00482D70)
                                    (calls existing chain, then post-process)
                                              |
                                    StretchRect + panini shader
                                              |
                                    UI draws on top (undistorted)
```

## v1 Tasks

### P0: Fix FPU Control Word [BLOCKING]

Set FPUCW to standard double precision in DLL init, before any trig calls.

Options (in order of preference):
1. `_control87(0x037F, 0xFFFF)` call in `InitThread()` before `InstallHooks()`
2. Inline `fldcw` in `ComputeFillZoom` before the trig chain
3. Precompute zoom in Lua addon, pass via `paniniZoom` CVar (avoids trig entirely)
4. Custom SSE2-based trig implementation (avoids x87 entirely)

Test: rebuild, relaunch WoW, verify `FPUCW=0x037F` in log and `denom` is valid (~0.994).

### P1: Switch Shader to Production Mode

Replace diagnostic UV output in panini.hlsl with actual texture sampling:
- Remove lines 89-96 (diagnostic block)
- Remove lines 98-99 (dead code)
- Add `tex2Dgrad(sceneTex, srcUV, ddx(srcUV), ddy(srcUV))` with out-of-bounds black
- Use `dFdx`/`dFdy` for derivative computation (Ben Golus: procedural UV derivatives produce artifacts)
- Add `ray.z > 0.0` check from Fabric reference shader

### P2: Remove Diagnostic Logging

Once P0 is verified:
- Remove `[cfz]` trace from ComputeFillZoom (panini.cpp)
- Remove `FPUCW` log
- Remove guard-hit logging
- Remove `post-guard` log
- Remove CVar raw byte dump (cvar.cpp)
- Keep one-shot hex diagnostic for shader constants (useful for ongoing debug)

### P3: Refactor `/panini debug` to Accept Subcommands

Current: `/panini debug` toggles between tint shader (debug.hlsl) and panini shader.
Target: `/panini debug tint` and `/panini debug uv` independently toggle each shader.
Requires: separate CVar (`paniniDebugTint`, `paniniDebugUV`) or subcommand-based shader selection.

### P4: Gate Panini on World-Active State

Already implemented (`IsWorldActive()` check in `Hooked_RenderWorld`). Verify it works by checking that panini does not activate at login screen, character select, or loading screens.

## v2 Tasks (future)

### Dynamic shader enable/disable based on character state

Read character state (mounted, in combat, swimming, etc.) and adjust panini strength or disable it in specific contexts.

### Configurable FOV per-context

Different FOV for exploration, combat, indoor, outdoor. Driven by game state detection.

### Settings GUI

Replace slash commands with an in-game settings frame (slider widgets).

### Pattern scanning for addresses

Replace hardcoded memory addresses with byte-pattern scans to survive TurtleWoW binary updates.

## Completed Layers

### Layer 0: Logging [DONE]
Two-level (TRACE/INFO), hex-bit float logging via `FloatBits()`.

### Layer 1: CVar System [DONE]
Registration via `CVar::Register` at `0x0063DB90`. Lookup via `CVarLookup` at `0x0063DEC0`. Struct offsets verified via hex dump: string +0x20, float +0x24, int +0x28. All CVars read correctly (confirmed via hex).

### Layer 2: RenderWorld Hook [DONE]
JMP chain on existing hook at `0x00482D70`. Calls existing chain via trampoline, then applies panini post-process. ECX preserved via inline asm for __thiscall convention.

### Layer 3: D3D9 EndScene Hook [DONE]
Dummy device vtable technique. EndScene at vtable[42], Reset at vtable[16]. Used for resource management and Reset handling only (not panini pass).

### Layer 4: Panini Post-Process Pass [DONE, NEEDS P0 FIX]
StretchRect, shader constants, fullscreen quad (D3DFVF_XYZRHW), state save/restore. Both panini and debug shaders compile and load. Zoom fallback workaround in place for FPU bug.

### Layer 5: Device Reset [DONE]
Resource release on Reset, lazy recreation on next EndScene.

### Layer 6: Lua Addon [DONE]
CVar-based config, account-wide SavedVariables, slash commands (`/panini on|off|toggle|debug|strength|vertical|fill|status|cvars|debuginfo`), pcall-guarded CVar access. Addon symlinked to WoW Interface/AddOns/.

### Layer 7: World-Active Detection [DONE]
`IsWorldActive()` checks `CGWorldFrame` pointer at `0x00B4B2BC`.

### Layer 8: Shader Compilation Pipeline [DONE]
fxc2 (vendored, patched -Vn bug), invoked via Wine, produces ps_3_0 bytecode headers embedded in DLL.

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
- 2026-03-26: Confirmed FPU control word corruption (0x027F) is root cause of NaN in ComputeFillZoom. cosf returns +infinity for valid input. Workaround: hardcoded zoom fallback. Fix pending. [atlas]
- 2026-03-26: Ruled out rosettax87 and libSiliconPatch as FPU interference sources. rosettax87 operates at Rosetta ptrace layer. libSiliconPatch hooks WoW binary functions only. [atlas + integrator research]
- 2026-03-26: Confirmed CVar float reads are correct via hex dump. Printf bug is display-only, not data corruption. [atlas]
- 2026-03-26: Reverted -msse2 -mfpmath=sse from toolchain (doesn't fix system libm trig which still uses x87). [atlas]
- 2026-03-26: ComputeFillZoom math verified correct via Python trace. Bug is runtime FPU environment, not algorithm. [atlas]
