#!/usr/bin/env bash
# Renders every Astra Studio screenshot from the real UI code, headlessly.
#
#   tools/capture_screenshots.sh            # -> docs/screenshots
#   tools/capture_screenshots.sh --smoke    # render every screen, discard output
#
# Requires a configured headless build:
#   cmake --preset headless && cmake --build --preset headless
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

preview="build/headless/astra_uipreview"
if [[ ! -x "$preview" ]]; then
    preview="build/astra_uipreview"
fi
if [[ ! -x "$preview" ]]; then
    echo "astra_uipreview not found - build it first: cmake --preset headless && cmake --build --preset headless" >&2
    exit 1
fi

if [[ "${1:-}" == "--smoke" ]]; then
    exec "$preview" --smoke
fi

"$preview" --out docs/screenshots "$@"
python3 tools/recompress_png.py docs/screenshots
