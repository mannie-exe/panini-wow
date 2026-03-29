# Contributing to Panini Projection

## Prerequisites

- [MinGW-w64](https://www.mingw-w64.org/) cross-compiler (`i686-w64-mingw32-g++`)
- [Wine](https://www.winehq.org/) (shader compilation via vendored fxc2, test execution)
- [mise](https://mise.jdx.dev/) (task runner and tool manager)

## Getting Started

```bash
git clone https://github.com/mannie-exe/panini-classic-wow.git
cd panini-classic-wow
mise install
mise run build
mise run test
```

## Task Reference

| Task | Alias | Description |
|------|-------|-------------|
| `build` | `b` | Build debug DLL (cross-mingw32, debug logging) |
| `build:release` | `br` | Build release DLL (cross-mingw32) |
| `test` | `t` | Run unit tests (cross-compiled, via Wine) |
| `shader` | *none* | Compile HLSL shaders to bytecode headers |
| `clean` | *none* | Remove build artifacts |

## Testing

Tests are cross-compiled GTest binaries, run directly via Wine:

```bash
mise run test
```

146 tests cover panini math, projection invariants, config validation, shader pipeline selection, slider precision, and runtime robustness.

## Development Workflow

### Making Changes

1. Create a feature branch from `main`
2. Make changes
3. Build: `mise run build`
4. Test: `mise run test`
5. Submit PR against `main`

### Shader Changes

When modifying HLSL files in `shaders/`, the build system recompiles automatically. Shaders can also be compiled independently:

```bash
mise run shader
```

All shaders target ps_3_0 (D3D9 Shader Model 3.0). The compiler is a vendored `fxc2.exe` wrapping `d3dcompiler_47.dll`, invoked via Wine on macOS/Linux.

### CVar Changes

When adding or removing CVars, update both sides of the contract:

- **DLL:** `src/cvar.cpp` (registration + `PostProcessConfig_ReadFromCVars`)
- **Addon:** `PaniniClassicWoW/PaniniClassicWoW.lua` (`defaults` table + `cvarMap` table)
- **Tests:** `tests/unit/ConfigValidationTest.cc` (validation range tests)

## Project Structure

```
panini-classic-wow/
  src/                      DLL source (hooks, CVars, state, logging)
  include/                  Headers (panini.h, panini_math.h, log.h)
  shaders/                  HLSL pixel shaders (ps_3_0)
  PaniniClassicWoW/         Lua addon (settings UI, minimap button)
  cmake/                    Toolchain, shader compilation, version codegen
  tests/                    GTest unit tests (math, config, pipeline)
  tools/fxc2/               Vendored HLSL compiler (d3dcompiler_47.dll)
```

## Commits

Conventional-style prefixes: `feat:`, `fix:`, `chore:`, `ci:`, `docs:`, `test:`, `refactor:`, `perf:`, `build:`

Subject line in imperative mood, under 72 characters. Body explains why, not what. Bullet points for multi-line bodies.

Scopes are optional; use when the change targets a specific subsystem:

```
fix(shader): correct tex2Dgrad derivative propagation
test(pipeline): add FXAA+CAS combo selection test
```

For contributor PRs, the maintainer squash-merges with a clean conventional subject line. You do not need to rewrite your branch history.

## Changelog

Release notes are generated via [git-cliff](https://git-cliff.org/) from commit history. Configuration is in `cliff.toml`. Do not edit CHANGELOG.md manually.

## Code Style

### General

Follow existing patterns in the codebase. When in doubt, match the surrounding code.

### Logging

Use `LOG_INFO` and `LOG_DEBUG` macros from `log.h`. Log floats as hex bits via `FloatBits()` with `%08X` format; MinGW `%f` is broken under Wine.

- `LOG_INFO` for initialization steps, resource creation, hook installation
- `LOG_DEBUG` for per-frame diagnostics (gated behind `PANINI_DEBUG_LOG` compile flag)

### Comments

Default to no comments. Code should be self-explanatory through naming and structure. Comment when:

- The "why" is non-obvious (a workaround, an API quirk, a WoW engine constant)
- The behavior has surprising side effects
- A constant comes from reverse engineering (memory offsets, vtable indices)

```cpp
// WoW 1.12.1 CVar hash table lookup at fixed address.
constexpr uintptr_t CVarLookup_Addr = 0x0063DEC0;
```

Do not comment what the code already says.

### Doc Comments

Every exported type, function, and method in header files gets a doc comment. The first sentence is a summary. Cover what the symbol does, input expectations, side effects, and error conditions.

Write doc comments with future doc-gen tooling in mind: structured, factual, no marketing language.

### Naming

- `PascalCase` for types and exported functions (`PostProcessConfig`, `ReadCameraFov`)
- `camelCase` for local variables and struct fields (`paniniEnabled`, `halfTan`)
- `SCREAMING_SNAKE` for constants (`VTABLE_ENDSCENE`, `PATCH_SIZE`)
- `PascalCase_With_Underscores` for WoW memory offset constants (`CVarLookup_Addr`, `Camera_FOV_Off`)
- Descriptive names; avoid single-letter variables outside loop indices and established math notation (`D`, `S` for panini parameters are acceptable)

### WoW 1.12.1 Lua

- `this` is the implicit self in script handlers, not `self`
- `SetPoint` requires full 5-arg form with offsets: `SetPoint("TOPLEFT", relativeTo, "TOPLEFT", x, y)`
- `for k, v in table do` is valid (Lua 5.0 bare iteration); `pairs()` is optional
- `SetSize()` does not exist; use `SetWidth()` + `SetHeight()`
- `string.gfind` (not `string.gmatch`, which is Lua 5.1+)
- `GetAddOnMetadata(name, field)` (no `C_AddOns` namespace)

## Documentation

### Writing Style

Use complete sentences with natural compound structure. Technical reference tone; the voice of a well-written man page, not a keynote.

**Prohibited in prose:** em-dashes, en-dashes, double-hyphens. Use semicolons, commas, or colons instead. Double-hyphens in code are fine.

**Avoid:** superlatives ("powerful," "elegant," "seamless"), fragment-sentence drama, and long-then-short restatement. Say it once, clearly, and move on.

## License

By contributing, you agree that your contributions will be licensed under the [Apache 2.0 License](LICENSE).
