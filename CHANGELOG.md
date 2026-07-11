# Changelog

All notable changes to this project are documented here.

The format is based on [Keep a Changelog](https://keepachangelog.com/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- (none)

### Fixed
- (none)

### Changed
- (none)

## [1.2.0] — 2026-07-11

### Added
- **VT Disabled toggle** — skip `VirtualTextureMismatch` rule when project doesn't use Virtual Textures (Settings → Project Rendering).
- **Streaming Disabled toggle** — skip `NeverStream` and `TextureLODGroup` rules when texture streaming is disabled.
- **Texture Streaming Budget** rule — flags textures with high estimated VRAM footprint, based on resolution, mip chain, and compression format.
- **Niagara audit** — 3 new rules: high emitter/renderer count, high material reference count, high mesh reference count.
- **Animation audit depth** — 4 new rules: high notify count, high curve count, additive animation marker, compression/frame-rate heuristic.
- **AnimMontage naming prefix** (`AM_`) added to defaults.
- Texture LOD Group advice fixed: no longer suggests removed `TC_EditorIcon`.
- Niagara rules wired into asset-type filter so they actually run.

### Fixed
- Rule `VirtualTextureMismatch` can now be properly suppressed when VT project setting is off.
- `NeverStream` and `TextureLODGroup` rules correctly account for texture streaming being disabled.
- Niagara mesh references now read through typed renderer properties instead of generic object reflection.
- Animation notify/curve rules read from asset-registry tags first (fast, no asset load).
- UE5.5 API compatibility: additive anim type accessor, Niagara emitter data pointer, `FArrayProperty`/`FindFProperty` instead of deprecated `UArrayProperty`/`FindField`.
- Various build blockers (missing `LongAnimSequence` body, BPP float precision, dangling UI slot).

## [1.1.0] — 2026-07-10

### Added
- Platform presets (Mobile/VR, Console, PC) for threshold tuning.
- Individual rule toggles with tooltip descriptions.
- Config profiles (save/load/delete).
- Naming convention prefixes (fully editable per type).
- Material sprawl/consolidation checks.
- Skeletal mesh LOD gate (matches static mesh threshold).
- Scan progress bar with cancel support.
- Multi-select to Content Browser (`Ctrl+B`).
- HTML report export.
- Health score (severity-weighted, non-zero for real projects).
- Sortable/filterable results with severity colouring.

### Fixed
- Texture rules read authored import size instead of placeholder mip.
- Material instance bloat only flags *unused* zero-override instances.
- Animation rules no longer silently misfire in UE5.5.
- Health score no longer collapses to zero on real projects.

## [1.0.0] — 2026-07-08

### Added
- Initial public release.
- Core audit engine with per-asset rule framework.
- Texture, static mesh, skeletal mesh, material, sound, blueprint, and misc hygiene rules.
- CSV export.
- Audit Settings window (rule toggles, thresholds, naming).
- Multi-folder scan from Content Browser context menu.
- Config persistence via `Config/RPDAssetAuditor.ini`.
