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
 * Кадры пишутся отдельными файлами через saveToDcm.
 * pixel_spacing_* — миллиметры, сначала строки, затем столбцы.
 * Пропуск оставляет номинал bin 2 (0.042 мм). Вызов, которому известна геометрия скана,
 * передаёт RowSpacingMm() и ColSpacingMm(), иначе bin 1 тоже запишется как 0.042.
 * Возвращает путь каждого записанного файла, включая _002, _003, …
 */
std::vector<std::string> ConvertBinToDcm(string in_bin_file_path, size_t width, size_t height, size_t bits,
                                         size_t frames, bool all_frames_as_1st,
                                         std::function<void(const string &)> log_callback,
                                         string_opt out_dcm_file_path = nullopt, double pixel_spacing_row_mm = 0.042,
                                         double pixel_spacing_col_mm = 0.042);
} // namespace tape_scanner::dicom_generator
