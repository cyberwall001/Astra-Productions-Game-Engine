#!/usr/bin/env bash
# tools/package_windows_zig.sh - build the Windows x64 zip without MSVC or Windows.
#
# The Release workflow builds on a windows-2022 runner with MSVC. When that is unavailable
# (for example while the repository's Actions billing is blocked), this script cross-compiles
# the same targets from Linux with Zig's bundled mingw-w64 toolchain:
#
#   pip install ziglang cmake
#   tools/package_windows_zig.sh
#   -> dist/astra-studio-<version>-windows-x64.zip
#
# Notes:
#   * The result is a mingw-w64 build, not an MSVC build. It links the C++ runtime and libc
#     statically, so the archive needs no extra DLLs beyond the system ones.
#   * It has NOT been run on Windows by the script. Treat it as a preview build; the official
#     asset still comes from .github/workflows/release.yml.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

version="$(sed -n 's/^ *VERSION \([0-9][0-9.]*\).*/\1/p' CMakeLists.txt | head -1)"
[ -n "$version" ] || { echo "could not read VERSION from CMakeLists.txt" >&2; exit 1; }

build_dir="${ASTRA_ZIG_BUILD_DIR:-build/zig-windows}"
glfw_dir="${ASTRA_ZIG_GLFW_DIR:-$build_dir/glfw-3.4}"
staging="$build_dir/stage/astra-studio-$version-windows-x64"
zip_path="dist/astra-studio-$version-windows-x64.zip"
jobs="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 3)}"

python="${PYTHON:-python3}"
cmake="${CMAKE:-cmake}"
zig="$("$python" -c 'import os, ziglang; print(os.path.join(os.path.dirname(ziglang.__file__), "zig"))')"
[ -x "$zig" ] || { echo "zig not found (pip install ziglang)" >&2; exit 1; }

mkdir -p "$build_dir/toolchain"
cat > "$build_dir/toolchain/zigcc" <<EOF
#!/bin/sh
exec "$zig" cc -target x86_64-windows-gnu "\$@"
EOF
cat > "$build_dir/toolchain/zigcxx" <<EOF
#!/bin/sh
exec "$zig" c++ -target x86_64-windows-gnu "\$@"
EOF
chmod +x "$build_dir/toolchain/zigcc" "$build_dir/toolchain/zigcxx"

if [ ! -f "$glfw_dir/CMakeLists.txt" ]; then
    echo "==> fetching GLFW 3.4"
    mkdir -p "$glfw_dir"
    curl -sSL https://codeload.github.com/glfw/glfw/tar.gz/refs/tags/3.4 |
        tar xz -C "$glfw_dir" --strip-components=1
fi

echo "==> configuring Astra Studio $version for Windows x64"
"$cmake" -S . -B "$build_dir/out" -G "Unix Makefiles" \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER="$build_dir/toolchain/zigcc" \
    -DCMAKE_CXX_COMPILER="$build_dir/toolchain/zigcxx" \
    -DCMAKE_BUILD_TYPE=Release \
    -DASTRA_BUILD_STUDIO=ON -DASTRA_BUILD_TOOLS=OFF -DBUILD_TESTING=OFF \
    -DASTRA_FETCH_GLFW=OFF -DASTRA_GLFW_DIR="$glfw_dir" -DASTRA_WARNINGS=OFF

echo "==> building"
make -C "$build_dir/out" -j"$jobs" astra_studio astra_keyforge

echo "==> staging"
rm -rf "$staging"
mkdir -p "$staging/docs"
cp "$build_dir/out/AstraStudio.exe" "$build_dir/out/AstraKeyForge.exe" "$staging/"
cp -r assets "$staging/assets"
cp LICENSE README.md "$staging/"
cp docs/*.md "$staging/docs/" 2>/dev/null || true

cat > "$staging/README.txt" <<EOT
Astra Studio $version - Windows x64
================================

1. Run AstraStudio.exe.
2. First launch shows the boot sequence, then the sign-in screen.
3. Create an account with an access code, or continue in demo mode.

AstraKeyForge.exe is the owner-only tool that mints access codes.

Requires Windows 10 1809+ / Windows 11, 64-bit, and a GPU with OpenGL 3.2 support.

--------------------------------------------------------------------------------
BUILD PROVENANCE
--------------------------------------------------------------------------------
Cross-compiled with Zig's mingw-w64 toolchain from $(git rev-parse --short HEAD 2>/dev/null || echo "this tree"),
GLFW 3.4, Release. The C++ runtime and libc are linked statically, so only system DLLs are
needed (KERNEL32, USER32, GDI32, OPENGL32, SHELL32, CRYPT32, WINHTTP, UCRT).

This is not the MSVC build from the Release workflow, and it was not run on Windows when it
was produced. Docs: docs/BUILD_WINDOWS.md
EOT

mkdir -p dist
rm -f "$zip_path"
( cd "$build_dir/stage" && zip -qr9 "$root/$zip_path" "astra-studio-$version-windows-x64" )
echo "==> $zip_path"
unzip -l "$zip_path" | tail -1
