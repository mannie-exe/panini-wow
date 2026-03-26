# PaniniWoW Implementation Plan

Panini projection post-process DLL for WoW 1.12.1 (TurtleWoW). Hooks D3D9 EndScene, applies cylindrical projection via pixel shader, configurable through in-game CVar system.

## Current Status

EndScene vtable hook works. CVar registration works. Shader compilation works. Debug shader (red -33%) produces a visible effect, confirming the full rendering pipeline (StretchRect, CreatePixelShader, DrawPrimitiveUP) functions end-to-end through DXVK.

### Blocking Issues

1. **CVar float reads produce incorrect values in log output.** The cached float at CVar struct offset `+0x24` contains correct IEEE 754 bytes (verified via hex dump: `0x3FC90FDA` = 1.5707963), but `vfprintf` with `%f` on MinGW 32-bit under Wine displays them as `inf`. Explicit `(double)` casts did not fix it. Root cause is unclear; may be a MinGW CRT varargs ABI incompatibility under Wine. The actual float values passed to the shader may be correct (the debug shader works visually), but we cannot confirm via logging. Needs binary-level investigation or an alternative diagnostic approach (write values to a binary file, or render numeric debug overlay on screen).

2. **Panini shader warps UI.** EndScene fires after both 3D world and UI rendering. The shader applies to the full frame. Fix requires hooking `CGWorldFrame::RenderWorld` at `0x00482D70` (game function, not D3D9) to apply the panini pass after world rendering but before UI drawing. This needs a trampoline/detour hook (5-byte JMP patch) since RenderWorld is a direct call, not a vtable entry. Research report 06 covers the approach.

3. **FOV not applied by SuperWoW's CVar at runtime.** SuperWoW reads the FoV CVar only during camera init (startup or /reload), not per-frame. The DLL now writes FOV directly to camera memory (`[0x00B4B2BC] -> +0x65B8 -> +0x40`) each frame, bypassing SuperWoW's CVar. This should work once the CVar float read issue is resolved (the `paniniFov` value drives `WriteCameraFov`).

4. **Panini activates in character select/main menu.** EndScene fires on all screens. The DLL should only apply the panini pass when the 3D world is active. Detecting world-active state requires reading a game flag or checking whether `CGWorldFrame` exists.

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
                                    RenderWorld hook (v1 target)
                                         or EndScene hook (current)
                                              |
                                    StretchRect + panini shader
                                              |
                                    UI draws on top (undistorted)
```

## v1 Tasks (remaining)

### Research: MinGW varargs ABI under Wine

The `vfprintf` issue may affect shader constant values too, not just logging. Need to determine:
- Whether `float` -> `double` promotion in varargs is broken at the compiler level or the CRT level
- Whether `SetPixelShaderConstantF` (which takes `float*`, not varargs) is affected (probably not)
- Alternative diagnostic: write CVar float values to a binary log file and inspect with xxd, or render debug values as colored pixels on screen

### Research: trampoline/detour hooking for RenderWorld

The pre-UI hook requires patching the first bytes of `CGWorldFrame::RenderWorld` at `0x00482D70` with a JMP to our hook function. Options:
- Manual 5-byte JMP trampoline (save original bytes, write JMP rel32)
- MinHook library (CPM dependency, well-tested, MIT license)
- hadesmem (what nampower uses, but heavy dependency)

The hook function calls the original RenderWorld, then applies the panini post-process using the device pointer from `[0x00C0ED38] -> +0x38A8`.

### Research: world-active detection

Detect whether the 3D world is rendering (in-game) vs menus/character select. Options:
- Check if `CGWorldFrame` pointer at `0x00B4B2BC` is non-null (null at login screen, non-null in-world)
- Read a game state flag
- Only apply panini if `ReadCameraFov` returns a valid value (non-zero, non-inf)

### Implement: RenderWorld hook

Replace EndScene as the panini application point. EndScene remains for resource management and Reset handling.

### Implement: FOV direct write (verify)

`WriteCameraFov` is implemented but blocked on CVar float read verification. Once we confirm the float values are correct (via non-printf diagnostic), enable per-frame FOV writing.

### Implement: disable panini outside world

Guard the panini pass on world-active state. No shader application at login screen, character select, or loading screens.

## v2 Tasks (future)

### Dynamic shader enable/disable based on character state

Read character state (mounted, in combat, swimming, etc.) and adjust panini strength or disable it in specific contexts. Requires hooking additional game functions or reading character state memory.

### Configurable FOV per-context

Different FOV for exploration, combat, indoor, outdoor. Driven by game state detection.

### Settings GUI

Replace slash commands with an in-game settings frame (slider widgets). Optional dependency on Ace2 or standalone frame creation.

### Pattern scanning for addresses

Replace hardcoded memory addresses with byte-pattern scans to survive TurtleWoW binary updates. Scan for known instruction sequences around the target addresses.

## Completed Layers

### Layer 0: Logging [DONE]
Two-level (TRACE/INFO), wide/fat format, `[LEVEL] [module] message`, no external libs.
Known issue: `%f` format broken in MinGW 32-bit varargs under Wine.

### Layer 1: CVar System [DONE, PARTIALLY VERIFIED]
Registration via `CVar::Register` at `0x0063DB90` (__fastcall). Lookup via `CVarLookup` at `0x0063DEC0` (__fastcall). Struct offsets verified: string at `+0x20`, float at `+0x24`, int at `+0x28`. CVars register and look up correctly. Float read correctness needs non-printf verification.

### Layer 3: D3D9 EndScene Hook [DONE]
Dummy device vtable technique. EndScene at vtable[42], Reset at vtable[16]. Confirmed firing via log and visual debug shader effect.

### Layer 4: Panini Post-Process Pass [DONE, NEEDS CVar FIX]
StretchRect, shader constants, fullscreen quad with half-pixel offset, state save/restore. Both panini and debug shaders compile and load. Blocked on correct CVar value delivery.

### Layer 5: Device Reset [DONE]
Resource release on Reset, lazy recreation on next EndScene.

### Layer 6: Lua Addon [DONE]
CVar-based config, account-wide SavedVariables, slash commands, pcall-guarded CVar access.

### Shader Compilation Pipeline [DONE]
fxc2 (vendored from philippremy/fxc2, patched -Vn bug) compiled with MinGW, invoked via Wine, produces ps_3_0 bytecode headers embedded in DLL.

## Research Reports

| Report | Path | Summary |
|--------|------|---------|
| 01 Modding Landscape | `~/.atlas/integrator/reports/panini-classic-wow/01-*` | WoW 1.12.1 modding techniques, APIs, D3D9 hooking |
| 02 Feasibility | `~/.atlas/integrator/reports/panini-classic-wow/02-*` | Six approaches evaluated; Approach F recommended |
| 03 Design | `~/.atlas/integrator/reports/panini-classic-wow/03-*` | Full architecture, shader, addon, build system |
| 04 Implementation | `~/.atlas/integrator/reports/panini-classic-wow/04-*` | CVar addresses, camera offsets, vtable indices |
| 05 SuperWoW Review | `~/.atlas/integrator/reports/panini-classic-wow/05-*` | SuperWoW internals, nampower hooking, DXVK vtable analysis |
| 06 FOV + Pre-UI Hook | `~/.atlas/integrator/reports/panini-classic-wow/06-*` | Direct FOV write, RenderWorld hook, device pointer chain |
