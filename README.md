# Сканер радиографической плёнки

Настольное приложение оцифровки проявленной плёнки. Профиль поставки — MSVC x64, CMake и vcpkg в режиме manifest. Обязательные тесты ядра собираются и запускаются без wxWidgets, без SDK камеры и без подключённого сканера.

## Зависимости

Версии портов vcpkg зафиксированы baseline `9eec002df1cd109f8f104cd966a763daf9751489` (тег реестра `2026.10.16`) в `vcpkg.json`.

| Что | Откуда | Когда нужно |
| --- | --- | --- |
| OpenCV, DCMTK | vcpkg, порты `opencv4` и `dcmtk` | ядро и `ctest` |
| wxWidgets, glm, OpenGL | vcpkg feature `app` и Windows SDK | только `BUILD_APP` |
| MVCAMSDK x64 | файлы `lib/MVCAMSDK_X64.lib` и `lib/MVCAMSDK_X64.dll` в репозитории | только приложение и аппаратные программы |

SDK камеры через vcpkg не ставится. Архив поставщика MindVision/HuaTeng лежит в `materials/` и хранится в Git LFS; для сборки приложения достаточно уже лежащих в `lib/` библиотек x64. Другие копии подставлять не нужно. Контрольные суммы SHA-256:

- `lib/MVCAMSDK_X64.lib` — `05c10dce3cab7737a7a288f842c50f5a7f6eabd2dcdc2963f5a85fcdb895cabd`
- `lib/MVCAMSDK_X64.dll` — `f6cf90c22b59e36403576a9914757713cb5851a5bda35c48542428a4dc51ba2e`

Каталог `VCPKG_ROOT` должен указывать на клон [vcpkg](https://github.com/microsoft/vcpkg). В командную строку разработчика x64 его можно добавить так:

```bat
set VCPKG_ROOT=C:\src\vcpkg
```

## Сборка

Нужны CMake 3.25 или новее и Visual Studio 2022 с инструментом MSVC x64.

Приложение и ядро (официальный профиль). Если нет wxWidgets, OpenGL, glm или файлов SDK, CMake завершается с ошибкой и не пропускает цель окна:

```bat
cmake --preset windows-msvc-x64
cmake --build --preset windows-msvc-x64
```

Только ядро и обязательный тест, без wxWidgets и без SDK камеры:

```bat
cmake --preset windows-msvc-x64-core
cmake --build --preset windows-msvc-x64-core
ctest --preset mandatory-windows --output-on-failure
```

Тот же прогон из PowerShell: `scripts/test_scan_geometry.ps1` (читает `VCPKG_ROOT`, не содержит путей конкретной машины).

На Linux обязательный набор собирается системными OpenCV и DCMTK, тоже без wxWidgets и SDK:

```bash
sudo apt install build-essential ninja-build pkg-config libopencv-dev libdcmtk-dev libnsl-dev
cmake --preset linux-core
cmake --build --preset linux-core
ctest --preset mandatory --output-on-failure
```

`ctest --test-dir build/linux-core --output-on-failure` запускает тот же набор. Рабочий каталог теста — каталог сборки, не дерево исходников.

## Что попадает в ctest

По умолчанию зарегистрирован один тест: `test_scan_geometry` (метка `mandatory`). Он проверяет геометрию и DICOM и не открывает камеру, контроллер и окно.

`BUILD_HARDWARE_TESTS` только собирает программы, которым нужны камера и движение (`test_hdr`, `test_exposes`, `test_scale`), и интерактивный просмотр (`test_film`, `test_region`, `test_region_reverse`, `test_analyze_cadr`, `test_save_dcm`). Обычный `ctest` их не выполняет. Регистрация аппаратных тестов — отдельная конфигурация с явным стендом, после чего их выбирают меткой, а не общим прогоном:

```bat
cmake --preset windows-msvc-x64 -DBUILD_HARDWARE_TESTS=ON -DSCANNER_RUN_HARDWARE=ON -DSCANNER_STAND_ID=bench-01
ctest -C Release -L hardware --test-dir build/windows-msvc-x64
```

Пустой `SCANNER_STAND_ID` при `SCANNER_RUN_HARDWARE=ON` — ошибка конфигурации. Пресет `mandatory-windows-app` по-прежнему берёт только метку `mandatory`.

## Параметры CMake

- `BUILD_CORE` — библиотека `tape_scanner_scan` и `test_scan_geometry`. По умолчанию включено.
- `BUILD_APP` — исполняемый файл `tape_scanner`. В пресете `windows-msvc-x64` включено. На Windows без пресета тоже включено; на остальных системах выключено, чтобы обязательный `ctest` не требовал окно и SDK.
- `BUILD_HARDWARE_TESTS` — по умолчанию выключено.
- `SCANNER_RUN_HARDWARE`, `SCANNER_STAND_ID` — явная регистрация аппаратных тестов в CTest.
