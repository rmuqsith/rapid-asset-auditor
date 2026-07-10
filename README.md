# Rapid Asset Auditor

An editor-only Unreal Engine plugin that scans project assets and flags the small problems
that quietly pile up — missing LODs, oversized textures, redundant or one-off materials,
off-convention names, and plenty more. Read-only: it never touches assets, just reports.

## Requirements

- **Unreal Engine 5.5** — built and tested on 5.5. May work on nearby versions, but older
  releases may need a tweak (a couple of engine headers moved).

## Installing

1. Drop the `RPDAssetAuditor` folder into your project's `Plugins/` folder.
2. Reopen the project and let it rebuild.
3. If it doesn't show up, enable it under **Edit → Plugins → Editor** and restart.

## Using it

Open it from **RPD → Rapid Asset Auditor** in the menu bar. Point it at a folder or the whole
project, tick the asset types to include, and hit **Run Audit** — large scans show a progress bar
and can be cancelled.

Results are sortable and filterable by severity, with a project health score. Click an asset
to sync it in the Content Browser, or **Ctrl+B** to browse multiple selected rows at once.
Right-click to jump to an asset, copy details, or ignore it from future scans.

Right-click any folder in the Content Browser to **Audit Selected Folder(s)** or **List Assets
in Selected Folder(s)** — works on single or multiple folders at once.

**List Assets** does the same scan but runs no rules — every asset appears once, with
parent/child relationships for materials. An asset with three issues shows three times in an
audit, but once in a listing.

Export results to **CSV** for spreadsheets, or **HTML** for a self-contained report.

![Rapid Asset Auditor](/Resources/screenshot.png)

Under **Settings**: toggle individual rules, set thresholds, apply platform presets (Mobile/VR,
Console, PC), edit naming prefixes, and save named profiles. Everything persists per-project
in `Config/RPDAssetAuditor.ini`.

## What it checks

- **Materials** — bloated masters, one-off masters, duplicate instances, unused parameters,
  direct-master-on-mesh (instead of via an instance).
- **Meshes** — missing LODs (skips meshes too small to benefit), Nanite/LOD sanity, high vertex
  counts, missing collision or physics.
- **Textures** — oversized, non-power-of-two, never-stream, wrong LOD group. Reads the *authored*
  size, so a streamed-in placeholder won't fool it.
- **Naming** — per-type prefixes as a single source of truth.
- Sound, blueprint, and misc hygiene checks.

## Credits

Built with AI assistance.

## License

MIT — use freely, keep the copyright notice. See [LICENSE](LICENSE).


https://rmuqsith.github.io
