# Credits

This project builds on the research, reverse engineering, open-source tooling, and community knowledge of many individuals and projects. This file acknowledges their work.

## Research & Theory

**Thomas K. Sharpless, Bruno Postle, Daniel M. German**: "Pannini: A New Projection for Rendering Wide Angle Perspective Images," Eurographics Workshop on Computational Aesthetics, 2010. Defines the stereographic Pannini projection: a cylindrical double-projection that renders fields of view beyond 150 degrees while preserving vertical lines and radial lines through center. The mathematical foundation for this project's post-process shader.
[Paper (PDF)](http://tksharpless.net/vedutismo/Pannini/panini.pdf) | [ResearchGate](https://www.researchgate.net/publication/220795340)

**Jakub Maksymilian Fober**: "Perspective Picture from Visual Sphere: A New Approach to Image Rasterization," arXiv:2003.10558, 2020. Proposes a visual-sphere perspective model for real-time rasterization capable of unlimited field of view and arbitrary projection geometry.
[arXiv](https://arxiv.org/abs/2003.10558)

**Jakub Maksymilian Fober**: "Aximorphic Perspective Projection Model for Immersive Imagery," arXiv:2102.12682, 2021. Extends the visual sphere model into an aximorphic projection that combines different projection types per axis for optimal spatial perception. Informed the approach to configurable vertical compensation in this project's Panini implementation.
[arXiv](https://arxiv.org/abs/2102.12682)

## Shader Techniques

**Timothy Lottes (NVIDIA)**: FXAA 3.11 (Fast Approximate Anti-Aliasing). Screen-space anti-aliasing algorithm using luminance-based edge detection and sub-pixel filtering, originally published 2011. This project's `fxaa.hlsl` is a ps_3_0 single-pass port of Quality preset 12.
[NVIDIA white paper](https://developer.download.nvidia.com/assets/gamedev/files/sdk/11/FXAA_WhitePaper.pdf)

**butterw**: Contrast Adaptive Sharpening (CAS) DX9 HLSL port. Adapted AMD FidelityFX CAS to a DX9/DX11 pixel shader for media players. This project's `cas.hlsl` is ported from this gist.
[Gist](https://gist.github.com/butterw/ceb89a68bc0aa3b0e317660fb4bacaa3)

**AMD GPUOpen**: FidelityFX Contrast Adaptive Sharpening. The original CAS algorithm: per-pixel adaptive sharpening that targets uniform sharpness across the image, sharpening less in already-detailed areas to reduce artifacts.
[GPUOpen](https://gpuopen.com/fidelityfx-cas/) | [GitHub](https://github.com/GPUOpen-Effects/FidelityFX-CAS)

**Jakub Maksymilian Fober (Fubaxiusz)**: PerfectPerspective ReShade shader. Practical implementation of the aximorphic projection model as a post-process shader for ReShade, licensed CC BY-SA 3.0. Reference for projection parameter design.
[GitHub](https://github.com/Fubaxiusz/fubax-shaders) | [Wiki](https://github.com/Fubaxiusz/fubax-shaders/wiki/PerfectPerspective)

## WoW Modding Community

**Balake (balakethelock)**: [SuperWoW](https://github.com/balakethelock/SuperWoW) and [SuperAPI](https://github.com/balakethelock/SuperAPI). Client mod for WoW 1.12.1 that expands Lua API functionality and fixes client bugs via DLL injection. SuperWoW's `dlls.txt` loader convention and `FoV` CVar are used by this project for mod loading and field-of-view integration.

**namreeb**: [nampower](https://github.com/namreeb/nampower). Client mod that eliminates the cast-completion round-trip delay in vanilla WoW, improving cast efficiency. Foundational work in 1.12.1 client hooking and memory patching.

**pepopo978**: [nampower fork](https://github.com/pepopo978/nampower). Extended namreeb's nampower with spell queueing, configurable queue windows, and a custom Lua API for cast management. Widely used on private servers.

**brndd**: [vanilla-tweaks](https://github.com/brndd/vanilla-tweaks). Executable patcher for WoW 1.12.1 applying widescreen FoV correction, sound channel increase, farclip extension, nameplate range adjustment, and large address aware flag. An alternative `d3d9.dll` loader pathway referenced in this project's setup instructions.

**hannesmann**: [VanillaFixes](https://github.com/hannesmann/vanillafixes). Client launcher for WoW 1.6.1-1.12.1 that eliminates stutter by forcing high-precision timers, bundles DXVK, and provides DLL loading via `dlls.txt`. The primary DLL loader used with this project on Windows.

**marco**: libSiliconPatch. A compatibility patch library integrated into the TurtleSilicon performance stack for running 1.12.1 on Apple Silicon.

**OwnedCore community**: [WoW Memory Editing forum](https://www.ownedcore.com/forums/world-of-warcraft/world-of-warcraft-bots-programs/wow-memory-editing/). Community knowledge base for WoW memory structures, function addresses, and client internals. The [1.12.1.5875 Info Dump Thread](https://www.ownedcore.com/forums/world-of-warcraft/world-of-warcraft-bots-programs/wow-memory-editing/328263-wow-1-12-1-5875-info-dump-thread.html) and [3.3.5.12340 Offsets List](https://www.ownedcore.com/forums/world-of-warcraft/world-of-warcraft-bots-programs/wow-memory-editing/305416-3-3-5-12340-offsets-list.html) are key references for the static addresses and vtable offsets used in this project's hooks.

**DrFrugal**: [CVarRegisterSpreadsheet](https://git.drfrugal.xyz/DrFrugal/CVarRegisterSpreadsheet). Detours-based CVar__Register hook for WoW 3.3.5a that logs all CVar registration calls with parameters. Confirmed the CVar__Register function signature (9 parameters) and address (0x00767FC0) used by this project's WotLK support.

**FrostAtom**: [awesome_wotlk](https://github.com/FrostAtom/awesome_wotlk). Collection of WoW 3.3.5a client research and offset tables. Reference for WotLK memory layout and function addresses.

**fosley**: [WowFovChanger](https://github.com/fosley/WowFovChanger). FoV changer for WoW that documents the FoVRatio difference between Vanilla/TBC (41) and Wrath+ (50). Reference for understanding how the same raw radian value at camera+0x40 produces different perceived degrees across client versions.

**wowdev.wiki contributors**: [wowdev.wiki](https://wowdev.wiki/). Community-maintained wiki documenting WoW client internals, file formats, rendering flags, and memory structures. Reference for CVar system behavior, camera structures, render pipeline, and D3D device layout.

## Runtime Dependencies & Tooling

**Philip Rebohle (doitsujin)**: [DXVK](https://github.com/doitsujin/dxvk). Vulkan-based translation layer for D3D8/D3D9/D3D10/D3D11. Provides the `d3d9.dll` that this project's DLL hooks into on Linux/macOS via Wine, and is bundled with VanillaFixes on Windows. Licensed zlib/libpng.

**Wine project**: [winehq.org](https://www.winehq.org/). Compatibility layer for running Windows applications on POSIX systems. Build dependency for shader compilation (fxc2 runs under Wine) and test execution (GTest binaries run under Wine).

**Lifeisawful**: [rosettax87](https://github.com/Lifeisawful/rosettax87), maintained at [WineAndAqua/rosettax87](https://github.com/WineAndAqua/rosettax87). Replaces Apple Rosetta 2's x87 FPU instruction handlers with faster reduced-precision alternatives, providing 4-10x performance improvement for x87-heavy applications like WoW 1.12.1 on Apple Silicon.

**Gcenx**: [winerosetta](https://github.com/Gcenx/winerosetta). Wine compatibility library for Apple Silicon providing support for instructions not natively handled by Rosetta 2 in 32-bit WoW clients.

**tairasu**: [TurtleSilicon](https://github.com/tairasu/TurtleSilicon). Apple Silicon launcher for 32-bit WoW clients, wrapping rosettax87, winerosetta, and d9vk into a one-click patching GUI via CrossOver.

**Mozilla**: [fxc2](https://github.com/mozilla/fxc2). Minimal HLSL shader compiler wrapping `d3dcompiler_47.dll`, runnable under Wine. Vendored in this project at `tools/fxc2/` for build-time shader compilation. Licensed MPL 2.0.

**MinGW-w64 project**: [mingw-w64.org](https://www.mingw-w64.org/). Cross-compiler toolchain (`i686-w64-mingw32-g++`) used to build this project's 32-bit Windows DLL from macOS/Linux.

**Google**: [Google Test (GTest)](https://github.com/google/googletest). C++ testing framework used for this project's unit test suite. Fetched via CPM at build time.

**Kitware**: [CMake](https://cmake.org/). Build system generator. This project requires CMake 3.25+.

**Ninja-build contributors**: [Ninja](https://ninja-build.org/). Build system used as the CMake generator backend.

**Lars Melchior**: [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake). CMake's missing package manager, vendored for dependency fetching. Licensed MIT.

**git-cliff contributors**: [git-cliff](https://git-cliff.org/). Changelog generator from conventional commits, used to produce release notes. Configuration in `cliff.toml`.

## FrameXML Sources

**Ketho**: [wow-ui-source-vanilla](https://github.com/Ketho/wow-ui-source-vanilla). Extracted WoW 1.12.1 FrameXML source. Reference for Classic addon API behavior, frame templates, and the `this`-as-self handler convention.

**wowgaming**: [3.3.5-interface-files](https://github.com/wowgaming/3.3.5-interface-files). Extracted WoW 3.3.5a FrameXML source. Used to debug the WotLK slash command dispatch chain (`ChatEdit_ParseText`, `hash_SlashCmdList` lazy population, `string.gfind` trap function).

## UI & Addon References

**shagu**: [pfUI](https://github.com/shagu/pfUI). Complete UI replacement addon for WoW Vanilla and TBC, written from scratch. Reference for WoW 1.12.1 Lua addon patterns, frame construction, and the `this`-as-self convention.

**shagu**: [pfQuest](https://github.com/shagu/pfQuest). Quest helper and database addon for WoW Vanilla and TBC, powered by the VMaNGOS database. Reference for minimap button implementation and SavedVariables patterns.

**TurtleWoW team and community**: [turtle-wow.org](https://turtle-wow.org/) | [turtlecraft.gg](https://turtlecraft.gg/). Vanilla+ private server. TurtleWoW's SuperWoW integration, custom CVar support, and modding-friendly `dlls.txt` loader make client mods like this one possible.

**ChromieCraft team**: [chromiecraft.com](https://www.chromiecraft.com/). WotLK 3.3.5a private server using stock client binaries. Test environment for WotLK support development.
