# Changelog

All notable changes to panini-wow are documented in this file.


## Unreleased

### Features

- Dual Classic 1.12.1 + WotLK 3.3.5a support

### Bug Fixes

- Isolate build Wine prefix to avoid CrossOver wineserver conflict
- **(addon)** WotLK 3.3.5a compatibility
- Address PR review findings
- Address final PR review findings
- **(probe)** Guard fclose against null g_logFile on hot-attach

### Documentation

- Add CREDITS.md acknowledging research and community work
- Replace architecture diagram with user-facing pipeline overview
- Update for dual Classic + WotLK support

### CI/CD

- Distribute Probe.dll in releases

### Maintenance

- **(deps)** Update actions/upload-artifact action to v7
- Update probe for dual-version, add troubleshooting docs

## v0.1.1 - 2026-03-29

### Bug Fixes

- Correct license references to Apache 2.0
- Hook WoW's actual D3D9 device vtable instead of dummy device

### Documentation

- Add README
- Add CONTRIBUTING.md

### Maintenance

- Rename PaniniWoW to PaniniClassicWoW, add LICENSE
- Remove install task (no assumed client path)
- Remove stale PLAN.md, migrate open work to bootstrap

## v0.1.0-alpha.1 - 2026-03-28

### Features

- Initial project scaffold with working cross-compile pipeline
- EndScene hook, CVar system, debug shader, direct FOV write
- FoV swap system, panini toggle UX, inline asm crash fix
- Split debug shader into independent tint/UV subcommands
- Native settings GUI with tabbed layout
- Settings GUI complete, fix bugs
- Add GTest suite, fix saved FoV race condition
- Add internal supersampling for panini projection
- Unified post-process pipeline with FXAA, CAS, and tex2Dgrad
- Add minimap button (sweet roll icon)
- Version pipeline, debug about page, deterministic hook init
- Standalone RenderWorld trampoline, no SuperWoW dependency

### Bug Fixes

- Namespace settings toggle into PaniniClassicWoW table
- **(addon)** 1.12.1 API compat, strength range, slider UX
- Restore FoV on panini disable, address review findings
- Address review findings (run_windows, paths filter, cache keys)
- Pin action SHAs in release workflow (contents: write)
- Remove dead dragging var, preserve minimapPos across reset
- Add missing EnableMouse, SetMovable, RegisterForClicks, Show
- Use full 5-arg SetPoint for 1.12.1 compat, fix drag scale
- Add bottom padding between tab buttons and dialog frame
- Address review findings, remove .vscode from tracking
- Remove SetEditable (TBC+), use OnTextChanged to prevent URL modification

### Documentation

- Update plan with honest status, blockers, v1/v2 scope
- Reorder v2 tasks, drop context-dependent FOV
- Update PLAN.md with GUI status and audit results

### Performance

- Run tests directly via Wine instead of per-test ctest invocations

### Refactoring

- Simplify FoV to single write model, remove SuperAPI dependency

### CI/CD

- Add GitHub Actions workflows, inline mise tasks, add renovate
- Use ubuntu-24.04 runners (blacksmith requires org account)

### Maintenance

- Gate diagnostic logging behind PANINI_DEBUG_LOG, sync PLAN.md
- Fix writing guideline violations
