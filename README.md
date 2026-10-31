# scaner

## DICOM

Просмотр готовых сканов и запись Secondary Capture используют DCMTK **3.6.7 или новее**.
Проверено на Ubuntu 24.04 (DCMTK 3.6.7) и на MSYS2 MinGW64 (`mingw-w64-x86_64-dcmtk` из pacman).

Сборка MSYS2 строже относится к файлам ARRAY: разделитель элемента с длиной `0xFFFFFFFF` и хвост `(FFFC,FFFC)` дают `Illegal call`, а не предупреждение. `LoadDicomScan` чинит эти байты в памяти до разбора, поэтому отдельная версия DCMTK не требуется. Если установленный DCMTK файл всё же отверг, несжатый кадр читается своим разборщиком.

Без камеры и окна собираются `test_scan_geometry` и `test_open_scan`. На Windows в MSYS2 MinGW64: gcc, cmake, ninja, opencv, dcmtk, сборка Ninja Release. Тот же прогон делает workflow `MinGW`.

Без контроллера и камеры окно не ждёт сеть. Поиск TCP-порта контроллера идёт в фоне (таймаут 400 мс), перечисление камеры тоже не на потоке интерфейса. Статус: «Контроллер не найден» / «Камера не найдена». Повтор — кнопка на вкладке контроллера или следующий цикл таймера. В `settings.ini` ключ `[hardware] connect_on_startup` (`1` или `0`) и такая же галочка отключают опрос при старте. Адрес контроллера — только числовой IPv4: в wxWidgets 3.2 его задаёт `Hostname()`, без DNS.

Окно (`tape_scanner`) собирается с wxWidgets 3.2 msw и glm. В свежих репозиториях MSYS2 пакеты wx 3.2 и DCMTK есть в среде UCRT64 (`mingw-w64-ucrt-x86_64-wxwidgets3.2-msw`, `mingw-w64-ucrt-x86_64-dcmtk`), не в classic mingw64. Workflow `MinGW` собирает цель `tape_scanner` в UCRT64, не только тесты.
