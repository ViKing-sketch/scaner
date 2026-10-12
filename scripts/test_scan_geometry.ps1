# Собирает и запускает проверку режимов 600/1200 dpi и нарезки DICOM.
# Камера и сканер не нужны: они могут быть выключены.
$ErrorActionPreference = "Stop"

$repo = Split-Path -Parent $PSScriptRoot
$vcpkg = "C:\scanner\.tools\vcpkg\scripts\buildsystems\vcpkg.cmake"
if (-not (Test-Path $vcpkg)) {
    throw "vcpkg toolchain was not found at $vcpkg. Install OpenCV and DCMTK first: vcpkg install opencv4:x64-windows dcmtk:x64-windows"
}

$toolRoot = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC"
$toolset = Get-ChildItem $toolRoot -Directory -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending |
    Where-Object { Test-Path (Join-Path $_.FullName "lib\x64\msvcrt.lib") } |
    Select-Object -First 1
if (-not $toolset) {
    throw "No complete MSVC x64 toolset was found. Visual Studio C++ tools are installed only in part."
}
# v143 по умолчанию смотрит на набор 14.43, в нём нет x64 CRT. Берём полный набор.
$env:VCToolsVersion = $toolset.Name

$build = Join-Path $repo "build"
cmake -S $repo -B $build -G "Visual Studio 17 2022" -A x64 -T "version=$($toolset.Name)" "-DCMAKE_TOOLCHAIN_FILE=$vcpkg"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

cmake --build $build --config Release --target test_scan_geometry
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$candidates = @(
    (Join-Path $build "Release\test_scan_geometry.exe"),
    (Join-Path $build "bin\Release\test_scan_geometry.exe")
)
$exe = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $exe) {
    throw "test_scan_geometry.exe was not found after the build."
}

& $exe
exit $LASTEXITCODE
