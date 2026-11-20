# Releasing Astra Studio

Astra Studio ships from `main` continuously: **every merge to `main` publishes a GitHub
release.** The version line deliberately stays **below 1.0.0** until the Vulkan/D3D12
viewport pipeline lands.

## Version policy

| Rule | Detail |
| --- | --- |
| Line | `0.x.y` only. `CMakeLists.txt` refuses to configure at `1.0.0` or above. |
| Who bumps | `.github/workflows/release.yml` bumps the **patch** component (`0.1.0` → `0.1.1`). |
| Who picks minors | A human, by editing `VERSION` in `CMakeLists.txt` in a normal PR (`0.1.x` → `0.2.0`). |
| Source of truth | `VERSION` in the `project()` call. The binary, the package name and the tag all read from it. |

The tag is always `v<version>` (for example `v0.1.0`), and the release carries the
Windows package built from that exact commit.

## What the workflow does

1. Reads `VERSION` from `CMakeLists.txt`.
2. If `v<that version>` already exists, bumps the patch number; otherwise the current
   version becomes the release (so the first release after a merge matches the repo).
3. Refuses to continue if the result is `>= 1.0.0`.
4. Builds with the `windows-msvc` preset, runs `ctest`, and packages the zip
   (`tools/package_windows.ps1`).
5. Commits the bump (`chore(release): vX.Y.Z [skip release] [skip ci]`), tags it, and
   publishes the release with auto-generated notes and the zip attached.

The bump commit is marked `[skip release] [skip ci]`, which is what stops the workflow
from re-triggering itself and stops the Windows job from rebuilding it.

## Skipping a release

Put `[skip release]` in the merge commit message (or run the workflow manually from the
Actions tab with `workflow_dispatch` to publish without a new merge). Docs-only merges do
not need to skip: they still produce a release, by design, so `main` always has a matching
tag.

## Doing it by hand

If Actions is unavailable, the same three steps can be run locally:

```bash
# 1. bump VERSION in CMakeLists.txt (patch), commit it on main
# 2. build + package
cmake --preset windows-msvc && cmake --build --preset windows-msvc-release
pwsh tools/package_windows.ps1 -Configuration Release

# 3. tag and publish
git tag -a v0.1.1 -m "Astra Studio v0.1.1"
git push origin v0.1.1
gh release create v0.1.1 dist/astra-studio-*-windows-x64.zip \
  --title "Astra Studio v0.1.1" --generate-notes --latest
```

## Checklist before merging a release-worthy change

- [ ] `astra_selfcheck` passes (`ctest --preset headless`).
- [ ] `astra_uipreview --smoke` passes at all three window sizes.
- [ ] Screenshots regenerated if the UI changed (`astra_uipreview --out docs/screenshots`
      then `python3 tools/recompress_png.py docs/screenshots`).
- [ ] `docs/ROADMAP.md` reflects what actually shipped.
