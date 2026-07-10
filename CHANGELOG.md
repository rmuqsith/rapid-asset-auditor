# Changelog

All notable changes to **Rapid Asset Auditor** are documented here.
This project follows [Semantic Versioning](https://semver.org): `MAJOR.MINOR.PATCH`.

- **MAJOR** — breaks a saved config or the export format, or a milestone reorganization.
- **MINOR** — new rules or features, backward-compatible.
- **PATCH** — bug fixes, rule wording/threshold tweaks.

## [Unreleased]

_Add changes here as you work on `main`; move them under a new version heading when you cut a release._

## [1.1.0] — 2026-07-10

### Added
- **Export to HTML** — one-click shareable report generated from the current audit results,
  grouped by category (material debt first), severity-colored, with a health-score and counts
  header. Self-contained single file, UTF-8; a lightweight complement to CSV export.

## [1.0.0] — 2026-07-10

First distributable release. A read-only, **materials-first** asset auditor for Unreal Engine 5.5.

### Audit
- **Materials (primary focus):** master-material bloat, one-off masters, duplicate instances,
  unused master parameters, direct-master-on-mesh (static & skeletal), a low-reuse master
  consolidation signal, blend/shading-model mismatch, and high texture sampling.
- **Meshes:** missing LODs (vertex-gated so tiny meshes aren't flagged), NoLODs + Nanite
  sanity (deduplicated so a mesh isn't counted twice), high vertex count, missing
  collision / physics asset.
- **Textures:** authored-size aware — reads the true import size instead of the streamed
  placeholder — and Max-Texture-Size aware; flags large resolution, non-power-of-two,
  NeverStream, and LOD-group issues.
- **Naming conventions:** per-type prefixes as a single source of truth, including a
  surface vs. post-process material split (`M_` / `PPM_`).
- **Sound / Blueprint / Misc:** hygiene checks tuned to avoid false positives — advisory
  rules (e.g. attenuation, concurrency) are not treated as defects.

### Workflow
- One-click full audit with a project **Health Score**, severity filtering, and sortable
  results including a folder Path column.
- **CSV export** — RFC-4180 quoted and UTF-8, safe to open in Excel, Sheets, or pandas.
- **Platform presets** (Mobile/VR · Console · PC) set every budget + Nanite mode at once.
- Editable **thresholds** and named **profiles**, persisted per project across editor restarts.
- Whole-project scans show **progress** and can be **cancelled**.

### Notes
- Read-only: the tool never modifies assets — it only reports.
- Source plugin; requires a C++ project. Developed and tested against UE 5.5.
