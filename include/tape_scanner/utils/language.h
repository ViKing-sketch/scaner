#pragma once

#include <string>

// for compilers that support precompilation, includes "wx/wx.h"
#include <wx/wxprec.h>
#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif
#include <wx/msw/rcdefs.h>
#include <wx/notebook.h>

using namespace std;




class Language
{
public:
    wxString get() {
        return lng;
    }

    bool set(wxString language) {
        int i;
        for (i = lng_cnt; i--; ) {
            if (lng_name[i] == language) {
                lng = language;
                lng_id_last = lng_id;
                lng_id = i;
                return true;
            }
        }
        return false;
    }

    wxString translate(wxString str, wxString from = "", wxString to = "") {
        int i;
        // setup source language ID
        int from_id = -1;
        if (from == "") {
            from_id = 0;
        } else if (from == wxT("LAST")) {
            from_id = lng_id_last;
        } else if (from == wxT("CURRENT")) {
            from_id = lng_id;
        } else {
            for (i = lng_cnt; i--; ) {
                if (lng_name[i] == from) {
                    from_id = i;
                    break;
                }
            }
            if (from_id < 0) return str;
        }
        // setup destination language ID
        int to_id = -1;
        if (to == "" || to == wxT("CURRENT")) {
            to_id = lng_id;
        } else if (to == wxT("LAST")) {
            to_id = lng_id_last;
        } else {
            for (i = lng_cnt; i--; ) {
                if (lng_name[i] == to) {
                    to_id = i;
                    break;
                }
            }
            if (to_id < 0) return str;
        }
        // translate
        if (to_id == from_id) return str;
        for (i = lng_str_cnt; i--; ) {
            if (lng_str[from_id][i] == str) {
                return lng_str[to_id][i];
            }
        }
        return str;
    }

    bool translate_frame(wxFrame *frame, wxString from = "", wxString to = "", bool frame_update = true) {
        if (!frame) return false;
        // translate frame title
        frame->SetTitle(translate(frame->GetTitle(), wxT("LAST")));
        // translate status bar
        frame->SetStatusText(translate(frame->GetStatusBar()->GetStatusText(0), from, to), 0);
        frame->SetStatusText(translate(frame->GetStatusBar()->GetStatusText(1), from, to), 1);
        frame->SetStatusText(translate(frame->GetStatusBar()->GetStatusText(2), from, to), 2);
        // translate menu bar
        auto* menubar = frame->GetMenuBar();
        if (menubar && menubar->GetMenuCount()) {
            for (int i = menubar->GetMenuCount(); i--;) {
                auto* menu = menubar->GetMenu(i);
                // translate menu label
                menubar->SetMenuLabel(i, translate(menubar->GetMenuLabelText(i), from, to));
                // translate menu items
                if (menu && menu->GetMenuItemCount()) {
                    for (int j = menu->GetMenuItemCount(); j--;) {
                        auto& items = menu->GetMenuItems();
                        for (auto it = items.begin(); it != items.end(); it++) {
                            auto* item = dynamic_cast<wxMenuItem*>(*it);
                            item->SetItemLabel(translate(item->GetItemLabelText(), from, to));
                        }
                    }
                }
                // TODO - translate sub items
            }
        }
        // translate other elements
        wxWindowList items;
        _get_all_children(frame, items);
        int id = 0;
        for (auto it = items.begin(); it != items.end(); it++, id++) {
            auto* button = dynamic_cast<wxAnyButton*>(*it);
            if (button) {
                button->SetLabelText(translate(button->GetLabelText(), from, to));
                button->Fit();
            }
            auto* static_box = dynamic_cast<wxStaticBox*>(*it);
            if (static_box) {
                static_box->SetLabelText(translate(static_box->GetLabelText(), from, to));
            }
            auto* static_text = dynamic_cast<wxStaticText*>(*it);
            if (static_text) {
                static_text->SetLabelText(translate(static_text->GetLabelText(), from, to));
            }
            auto* notebook = dynamic_cast<wxNotebook*>(*it);
            if (notebook) {
                for (size_t p = notebook->GetPageCount(); p--;) {
                    notebook->SetPageText(p, translate(notebook->GetPageText(p), from, to));
                }
            }
            auto* check_box = dynamic_cast<wxCheckBox*>(*it);
            if (check_box) {
                check_box->SetLabelText(translate(check_box->GetLabelText(), from, to));
            }
            /*
            auto* file_picker = dynamic_cast<wxFilePickerCtrl*>(*it);
            if (file_picker) { // FIXME
                file_picker->SetLabelText(translate(file_picker->GetLabelText(), from, to));
            }
            */
            /*
            auto* view_column = dynamic_cast<wxDataViewColumn*>(*it);
            if (view_column) {
                view_column->SetTitle(
                    translate(view_column->GetTitle(), from, to)
                );
            }
            */
            /*
            auto* view_list = dynamic_cast<wxDataViewListCtrl*>(*it);
            if (view_list) {
                for (size_t p = view_list->GetColumnCount(); p--;) {
                    view_list->GetColumnAt(p)->SetTitle(
                        translate(view_list->GetColumnAt(p)->GetTitle(), from, to)
                    );
                }
            }
            */
            // TODO - translate more elements
        }
        // FIXME - update frame look in different way
        if (frame_update) {
            frame->ShowFullScreen(1);
            //frame->Update();
            frame->ShowFullScreen(0);
        }
        // OK
        return true;
    }

private:
    void _get_all_children(wxWindow *window, wxWindowList &window_list) {
        wxWindowList children = window->GetChildren();
        for (auto it = children.begin(); it != children.end(); it++) {
            auto *child = *it;
            if (child) {
                window_list.Append(child);
                _get_all_children(child, window_list);
            }
        }
    }

    int lng_id = 0;
    int lng_id_last = 0;
    wxString lng = wxT("English");
    static const int lng_cnt = 2;
    wxString lng_name[lng_cnt] = {
        wxT("English"),
        wxT("Russian")
    };
    static const int lng_str_cnt = 29 + 59 + 101 + 8 + 14 + 3;
    wxString lng_str[lng_cnt][lng_str_cnt] = {
        {
            //
            wxT("Welcome"),
            wxT("Medical films scanner app."),
            wxT("About"),
            wxT("Open DCM File.."),
            wxT("Quit"),
            wxT("File"),
            wxT("English"),
            wxT("Russian"),
            wxT("Language"),
            wxT("Help"),
            wxT("Tools"),
            wxT("File read test"),
            wxT("View zone"),
            wxT("Viewer"),
            wxT("Any settings here"),
            wxT("Settings"),
            wxT("Medical films scanner"),
            wxT("LOG:"),
            wxT("Open DCM file.."),
            wxT("Contents"),
            wxT("File contents list"),
            wxT("Data"),
            wxT("File data list"),
            wxT("DICOM files (*.dcm)|*.dcm"),
            wxT("File name: %s\n"),
            wxT("File path: %s\n"),
            wxT("Can't open file for reading.\n"),
            wxT("Patient name is: %s\n"),
            wxT("Patient name isn't found.\n"),
            //
            wxT("DICOM files generator"),
            wxT("--- DICOM FILE OPEN ---"),
            wxT("--- JPEG TO DICOM ---"),
            wxT("ERROR: Input JPEG file doesn't exists.\n"),
            wxT("ERROR: Failed to convert JPEG to DICOM file.\n"),
            wxT("ERROR: Failed to save resulting DICOM file.\n"),
            wxT("Successfuly converted. See - %s\n"),
            wxT("--- BIN/RAW TO DICOM ---"),
            wxT("Input file path: %s\n"),
            wxT("Output images width: %zu px\n"),
            wxT("Output images height: %zu px\n"),
            wxT("Output images color depth: %zu bits\n"),
            wxT("Output file frames count: %zu\n"),
            wxT("All frames as 1st frame: %s\n"),
            wxT("YES"),
            wxT("NO"),
            wxT("Input file minimum size: %zu bytes\n"),
            wxT("ERROR: Can't open input file for reading.\n"),
            wxT("Input file size: %zu bytes\n"),
            wxT("ERROR: Input file size is too small.\n"),
            wxT("ERROR: Failed to allocate RAM buffer.\n"),
            wxT("ERROR: Failed to read input file into RAM buffer.\n"),
            wxT("ERROR: Failed to read whole input file into RAM buffer.\n"),
            wxT("--- PARAMETRIC DICOM GENERATOR ---"),
            wxT("Output file path: %s\n"),
            wxT("Fill value: random (%zu..%zu)\n"),
            wxT("Fill value: %zu\n"),
            wxT("Fill value increment: %zu\n"),
            wxT("Increment for new frames only: %s\n"),
            wxT("Successfuly created. See - %s\n"),
            wxT("DICOM file actions"),
            wxT("Save DCM file as.."),
            wxT("Close DCM file"),
            wxT("(Group,Element)"),
            wxT("Description"),
            wxT("Value"),
            wxT("JPEG to colored DICOM image converter"),
            wxT("Select JPEG file:"),
            wxT("Open JPEG file.."),
            wxT("Convert"),
            wxT("Binary (RAW) to monochrome DICOM image converter"),
            wxT("Select binary file:"),
            wxT("Open binary file.."),
            wxT("Width:"),
            wxT("px"),
            wxT("Height:"),
            wxT("Color depth:"),
            wxT("bits"),
            wxT("Frames:"),
            wxT("all frames as 1st frame"),
            wxT("Parametric monochrome DICOM image generator"),
            wxT("Save DCM file as:"),
            wxT("Select output file.."),
            wxT("Create"),
            wxT("Fill value:"),
            wxT("(any byte/word value, not a number = random value)"),
            wxT("Fill value increment:"),
            wxT("for new frames only"),
            wxT("Successfuly opened. See Data tab.\n"),
            //
            wxT("Controller: "),
            wxT("Failed to send, controller not connected"),
            wxT("Failed to send"),
            wxT("Successfully sent"),
            wxT("Successfully cleared"),
            wxT("Settings file successfully updated"),
            wxT("Failed to update settings file"),
            wxT("Settings file successfully created"),
            wxT("Failed to create settings file"),
            wxT("Connecting to controller.."),
            wxT("Controller connected"),
            wxT("Controller disconnected"),
            wxT("Controller"),
            wxT("Network settings"),
            wxT("IP address:"),
            wxT("Network mask:"),
            wxT("Gateway:"),
            wxT("UDP port:"),
            wxT("Save to controller"),
            wxT("Shell"),
            wxT("Clear"),
            wxT("Send"),
            wxT("Control"),
            wxT("Inputs:"),
            wxT("Outputs:"),
            wxT("Backlight 0"),
            wxT("Backlight 1"),
            wxT("Film start"),
            wxT("24V"),
            wxT("Motor 1 (Feeder):"),
            wxT("Motor 0 (Camera):"),
            wxT("pos"),
            wxT("go to"),
            wxT(" Home "),
            wxT(" Stop "),
            wxT("Initial commands"),
            wxT("Controller init.."),
            wxT("Controller init failed, %li commands failed"),
            wxT("Controller init failed, timeout"),
            wxT("Controller init done"),
            wxT("New network settings were set for controller and the app"),
            wxT("Failed to set new network settings for controller"),
            wxT("Failed to init, controller not connected"),
            wxT("Failed to init, already in progress"),
            wxT("Init"),
            wxT("Camera"),
            wxT("Dynamic IP:"),
            wxT("Use DHCP client"),
            wxT("Save to camera"),
            wxT("Other settings"),
            wxT("Preview"),
            wxT("Connecting to camera.."),
            wxT("Camera connected"),
            wxT("Camera disconnected"),
            wxT("Failed to send, camera not connected"),
            wxT("New network settings were set for camera and the app"),
            wxT("Failed to set new network settings for camera"),
            wxT("Failed to save a frame, camera not connected"),
            wxT("Save as.."),
            wxT("Bitmap files (*.bmp)|*.bmp"),
            wxT("RAW files (*.raw)|*.raw"),
            wxT("Camera frame successfuly saved"),
            wxT("Failed to save camera frame into file"),
            wxT("Failed to get a frame from camera"),
            wxT("Failed to get camera device info"),
            wxT("Exposure:"),
            wxT("us"),
            wxT("Actions with current frame"),
            wxT("Save as RGB8 bitmap"),
            wxT("Save as monochrome RAW16"),
            wxT("Film width:"),
            wxT("Film length:"),
            wxT("Pixels per 1mm:"),
            wxT("mm"),
            wxT("Start"),
            wxT("Pause"),
            wxT("Stop"),
            wxT("Scanning"),
            wxT("Scanning timeout, paused"),
            wxT("Camera homing start"),
            wxT("Scanning successfuly done"),
            wxT("Failed to start scanning, controller not connected"),
            wxT("Failed to start scanning, camera not connected"),
            wxT("Failed to start scanning, already in progress"),
            wxT("Continue scanning.."),
            wxT("Input error, film width must be > 0"),
            wxT("Input error, film height must be > 0"),
            wxT("Input error, `Pixels per 1mm` must be > 0"),
            wxT("Start scanning.."),
            wxT("Failed to pause scanning, controller not connected"),
            wxT("Failed to pause scanning, camera not connected"),
            wxT("Failed to pause scanning, not in progress"),
            wxT("Scanning paused.."),
            wxT("Failed to stop scanning, controller not connected"),
            wxT("Failed to stop scanning, camera not connected"),
            wxT("Failed to stop scanning, not in progress"),
            wxT("Scanning stoped"),
            wxT("Save scanned image as DCM file.."),
            wxT("Camera homing done, scanning.."),
            wxT("Scanning.."),
            wxT("Scanning almost done, going to home.."),
            wxT("Frame:"),
            wxT("Window Level:"),
            wxT("Window Width:"),
            wxT("Rotation:"),
            wxT("Flip:"),
            wxT("Horizontally"),
            wxT("Vertically"),
            wxT("%s (frames: %i, size: %ix%i)"),
            wxT("Resolution:"),
            wxT("600 dpi (42 um)"),
            wxT("1200 dpi (21 um)"),
            wxT("Pixel pitch X, um:"),
            wxT("Pixel pitch Y, um:"),
            wxT("Invert"),
            wxT("Ruler X, mm:"),
            wxT("px:"),
            wxT("Apply X"),
            wxT("Ruler Y, mm:"),
            wxT("Apply Y"),
            wxT("Pick X"),
            wxT("Pick Y"),
            wxT("Pixels are counted on the saved grid; optical pitch divides by bin."),
            wxT("Open scan..."),
            wxT("--- OPEN SCAN ---"),
            wxT("Pixel spacing: %s x %s mm\n"),
        },
        {
            //
            wxT("Добро пожаловать"),
            wxT("Программа для оцифровки медицинских снимков."),
            wxT("О программе"),
            wxT("Открыть DCM файл.."),
            wxT("Выход"),
            wxT("Файл"),
            wxT("Английский"),
            wxT("Русский"),
            wxT("Язык"),
            wxT("Помощь"),
            wxT("Инструменты"),
            wxT("Тест чтения файла"),
            wxT("Зона просмотра"),
            wxT("Просмотр"),
            wxT("Какие-то настройки"),
            wxT("Настройки"),
            wxT("Оцифровщик медицинских снимков"),
            wxT("История:"),
            wxT("Открыть DCM файл.."),
            wxT("Содержимое"),
            wxT("Список содержимого файла"),
            wxT("Данные"),
            wxT("Список данных файла"),
            wxT("Файлы DICOM (*.dcm)|*.dcm"),
            wxT("Имя файла: %s\n"),
            wxT("Путь к файлу: %s\n"),
            wxT("Не удалось открыть файл для чтения.\n"),
            wxT("Имя пациента: %s\n"),
            wxT("Имя пациента не найдено.\n"),
            //
            wxT("Генератор DICOM файлов"),
            wxT("--- ОТКРЫТ DICOM ФАЙЛ ---"),
            wxT("--- JPEG ФАЙЛ В DICOM ---"),
            wxT("ОШИБКА: Исходный JPEG файл не найден.\n"),
            wxT("ОШИБКА: Не удалось сконвертировать JPEG в DICOM файл.\n"),
            wxT("ОШИБКА: Не удалось сохранть данные в DICOM файл.\n"),
            wxT("Файл успешно сконвертирован. Смотрите - %s\n"),
            wxT("--- БИНАРНЫЙ ФАЙЛ В DICOM ---"),
            wxT("Исходный файл: %s\n"),
            wxT("Ширина картинки: %zu пикс\n"),
            wxT("Высота картинки: %zu пикс\n"),
            wxT("Глубина цвета: %zu бит\n"),
            wxT("Кол-во фреймов: %zu\n"),
            wxT("Все фреймы как первый: %s\n"),
            wxT("ДА"),
            wxT("НЕТ"),
            wxT("Мин. размер исходного файла: %zu байт\n"),
            wxT("ОШИБКА: Не удалось открыть файл для чтения.\n"),
            wxT("Размер исходного файла: %zu байт\n"),
            wxT("ОШИБКА: Исходный файл слишком мал.\n"),
            wxT("ОШИБКА: Не удалось создать буфер в ОЗУ.\n"),
            wxT("ОШИБКА: Не удалось прочесть файл в буфер ОЗУ.\n"),
            wxT("ОШИБКА: Не удалось прочесть весь файл в буфер ОЗУ.\n"),
            wxT("--- ПАРАМЕТРИЧЕСКИЙ ГЕНЕРАТОР ---"),
            wxT("Путь к будущему файлу: %s\n"),
            wxT("Заполнить значением: случайное (%zu..%zu)\n"),
            wxT("Заполнить значением: %zu\n"),
            wxT("Увеличивать значение на: %zu\n"),
            wxT("Увеличивать только для новых фреймов: %s\n"),
            wxT("Файл успешно создан. Смотрите - %s\n"),
            wxT("Действия с DICOM файлами"),
            wxT("Сохранить DCM файл как.."),
            wxT("Закрыть DCM файл"),
            wxT("(Группа,Элемент)"),
            wxT("Описание"),
            wxT("Значение"),
            wxT("Конвертер цветных JPEG картинок в файлы DICOM формата"),
            wxT("JPEG картинка:"),
            wxT("Выбор JPEG картинки.."),
            wxT("Конвертировать"),
            wxT("Конвертер бинарных файлов в файлы DICOM формата"),
            wxT("Бинарный файл:"),
            wxT("Выбор бинарного файла.."),
            wxT("Ширина:"),
            wxT("пикс"),
            wxT("Высота:"),
            wxT("Глубина цвета:"),
            wxT("бит"),
            wxT("Фреймы:"),
            wxT("все фреймы как первый"),
            wxT("Параметрический генератор DICOM файлов"),
            wxT("Сохранить как:"),
            wxT("Выбор файла для записи.."),
            wxT("Создать"),
            wxT("Заполнить значением:"),
            wxT("(любое значение байта/слова, не число = случайные)"),
            wxT("Увеличивать значение на:"),
            wxT("только для новых фреймов"),
            wxT("Успешно открыт. Смотрите вкладку Данные.\n"),
            //
            wxT("Контроллер: "),
            wxT("Не удалось отправить, контроллер не подключен"),
            wxT("Не удалось отправить"),
            wxT("Успешно отправлено"),
            wxT("Успешно очищено"),
            wxT("Файл настроек успешно обновлён"),
            wxT("Не удалось обновить файл настроек"),
            wxT("Файл настроек успешно создан"),
            wxT("Не удалось создать файл настроек"),
            wxT("Соединение с контроллером.."),
            wxT("Контроллер подключен"),
            wxT("Контроллер не подключен"),
            wxT("Контроллер"),
            wxT("Сетевые настройки"),
            wxT("IP адрес:"),
            wxT("Маска сети:"),
            wxT("Шлюз:"),
            wxT("UDP порт:"),
            wxT("Сохранить в контроллер"),
            wxT("Терминал"),
            wxT("Очистить"),
            wxT("Отправить"),
            wxT("Управление"),
            wxT("Входы:"),
            wxT("Выходы:"),
            wxT("Подсветка 0"),
            wxT("Подсветка 1"),
            wxT("Начало плёнки"),
            wxT("24В"),
            wxT("Мотор 1 (Подача):"),
            wxT("Мотор 0 (Камера):"),
            wxT("поз"),
            wxT("ехать"),
            wxT(" Домой "),
            wxT(" Стоп "),
            wxT("Команды инициализации"),
            wxT("Инициализация контроллера.."),
            wxT("Ошибка инициализации контроллера, всего ошибок: %li"),
            wxT("Ошибка инициализации контроллера, таймаут"),
            wxT("Инициализация контроллера успешно завершена"),
            wxT("Сетевые настройки успешно сохранены в контроллере и в программе"),
            wxT("Не удалось сохранить сетевые настройки в контроллере"),
            wxT("Не удалось начать инициализацию, контроллер не подключен"),
            wxT("Не удалось начать инициализацию, она уже идёт"),
            wxT("Инициализация"),
            wxT("Камера"),
            wxT("Динамический IP:"),
            wxT("Включить DHCP клиент"),
            wxT("Сохранить в камеру"),
            wxT("Другие настройки"),
            wxT("Предпросмотр"),
            wxT("Соединение с камерой.."),
            wxT("Камера подключена"),
            wxT("Камера не подключена"),
            wxT("Не удалось отправить, камера не подключена"),
            wxT("Сетевые настройки успешно сохранены в камере и в программе"),
            wxT("Не удалось сохранить сетевые настройки в камере"),
            wxT("Не удалось сохранить кадр, камера не подключена"),
            wxT("Сохранить как.."),
            wxT("Bitmap файлы (*.bmp)|*.bmp"),
            wxT("RAW файлы (*.raw)|*.raw"),
            wxT("Кадр камеры успешно сохранён в файл"),
            wxT("Не удалось сохранить кадр камеры в файл"),
            wxT("Не удалось получить кадр от камеры"),
            wxT("Не удалось получить информацию об устройстве камеры"),
            wxT("Экспозиция:"),
            wxT("мкс"),
            wxT("Действия с текущим кадром"),
            wxT("Сохранить в формате RGB8 bitmap"),
            wxT("Сохранить в формате RAW16 mono"),
            wxT("Ширина плёнки:"),
            wxT("Длина плёнки:"),
            wxT("Пикселей в 1мм:"),
            wxT("мм"),
            wxT("Старт"),
            wxT("Пауза"),
            wxT("Стоп"),
            wxT("Сканирование"),
            wxT("Таймаут при сканировании, включена пауза"),
            wxT("Поиск начального датчика камеры"),
            wxT("Сканирование успешно выполнено"),
            wxT("Нельзя начать сканирование, контроллер не подключен"),
            wxT("Нельзя начать сканирование, камера не подключена"),
            wxT("Нельзя начать сканирование, оно уже в процессе"),
            wxT("Сканирование продолжается.."),
            wxT("Ошибка ввода, ширина плёнки должна быть > 0"),
            wxT("Ошибка ввода, длина плёнки должна быть > 0"),
            wxT("Ошибка ввода, кол-во пикселей в 1мм должно быть > 0"),
            wxT("Сканирование начинается.."),
            wxT("Нельзя приостановить сканирование, контроллер не подключен"),
            wxT("Нельзя приостановить сканирование, камера не подключена"),
            wxT("Нельзя приостановить сканирование, оно не в процессе"),
            wxT("Сканирование на паузе.."),
            wxT("Нельзя остановить сканирование, контроллер не подключен"),
            wxT("Нельзя остановить сканирование, камера не подключена"),
            wxT("Нельзя остановить сканирование, оно не в процессе"),
            wxT("Сканирование остановлено"),
            wxT("Сохранить скан как DCM файл.."),
            wxT("Начальный датчика камеры найден, сканирование.."),
            wxT("Сканирование.."),
            wxT("Сканирование завершается, возврат к начальной позиции.."),
            wxT("Кадр:"),
            wxT("Яркость:"),
            wxT("Контраст:"),
            wxT("Повернуть:"),
            wxT("Отзеркалить:"),
            wxT("Горизонтально"),
            wxT("Вертикально"),
            wxT("%s (кадров: %i, размер: %ix%i)"),
            wxT("Разрешение:"),
            wxT("600 dpi (42 мкм)"),
            wxT("1200 dpi (21 мкм)"),
            wxT("Шаг пикселя X, мкм:"),
            wxT("Шаг пикселя Y, мкм:"),
            wxT("Инверсия"),
            wxT("Линейка X, мм:"),
            wxT("пикс.:"),
            wxT("Записать X"),
            wxT("Линейка Y, мм:"),
            wxT("Записать Y"),
            wxT("Указать X"),
            wxT("Указать Y"),
            wxT("Пиксели считаются по сохранённой сетке; оптический шаг делится на bin."),
            wxT("Открыть скан..."),
            wxT("--- ОТКРЫТЬ СКАН ---"),
            wxT("Шаг пикселя: %s x %s мм\n"),
    },
    };
};
