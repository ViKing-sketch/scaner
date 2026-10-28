#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace tape_scanner::dicom_generator{

using std::optional;
using std::nullopt;
using std::string;
using string_opt = optional<string>;

/**
 * Единственный путь BIN→DCM. Кадры пишутся отдельными файлами через saveToDcm.
 * pixel_spacing_row_mm / pixel_spacing_col_mm — обязательный параметр в миллиметрах
 * (сначала соседние строки, затем соседние столбцы). Значения по умолчанию нет:
 * пропуск не подставляет 0.042 мм и не выводит шаг из bin.
 * Импорт не записывает паспорт bin. У чужого массива происхождение неизвестно.
 * Успех — только возврат путей после записи каждого кадра, включая _002, _003, …
 */
std::vector<std::string> ConvertBinToDcm(string in_bin_file_path, size_t width, size_t height, size_t bits,
                                         size_t frames, bool all_frames_as_1st,
                                         std::function<void(const string &)> log_callback,
                                         double pixel_spacing_row_mm, double pixel_spacing_col_mm,
                                         string_opt out_dcm_file_path = nullopt);
} // namespace tape_scanner::dicom_generator
