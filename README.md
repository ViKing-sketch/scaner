# scaner

## DICOM

Просмотр готовых сканов и запись Secondary Capture используют DCMTK **3.6.7 или новее**.
Проверено на Ubuntu 24.04 (DCMTK 3.6.7) и на MSYS2 MinGW64 (`mingw-w64-x86_64-dcmtk` из pacman).

Сборка MSYS2 строже относится к файлам ARRAY: разделитель элемента с длиной `0xFFFFFFFF` и хвост `(FFFC,FFFC)` дают `Illegal call`, а не предупреждение. `LoadDicomScan` чинит эти байты в памяти до разбора, поэтому отдельная версия DCMTK не требуется. Если установленный DCMTK файл всё же отверг, несжатый кадр читается своим разборщиком.

Без камеры и окна собираются `test_scan_geometry` и `test_open_scan`. На Windows в MSYS2 MinGW64: gcc, cmake, ninja, opencv, dcmtk, сборка Ninja Release. Тот же прогон делает workflow `MinGW`.
