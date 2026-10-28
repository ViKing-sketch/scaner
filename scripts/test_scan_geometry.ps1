# Собирает ядро и запускает обязательный ctest (test_scan_geometry).
# Камера, wxWidgets и MVCAMSDK не нужны.
# Нужны CMake 3.25+, Visual Studio 2022 (MSVC x64) и переменная VCPKG_ROOT.
$ErrorActionPreference = "Stop"

$repo = Split-Path -Parent $PSScriptRoot
Set-Location $repo

$toolchain = Join-Path $env:VCPKG_ROOT "scripts\buildsystems\vcpkg.cmake"
if (-not $env:VCPKG_ROOT -or -not (Test-Path $toolchain)) {
    throw "Set VCPKG_ROOT to a vcpkg checkout. Package versions come from the builtin-baseline in vcpkg.json."
}

cmake --preset windows-msvc-x64-core
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

cmake --build --preset windows-msvc-x64-core --target test_scan_geometry
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

ctest --preset mandatory-windows --output-on-failure
exit $LASTEXITCODE
