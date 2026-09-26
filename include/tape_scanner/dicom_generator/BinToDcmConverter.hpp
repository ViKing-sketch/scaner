#pragma once

#include <string>
#include <optional>
#include <functional>

namespace tape_scanner::dicom_generator{

using std::optional;
using std::nullopt;
using std::string;
using string_opt = optional<string>;

void ConvertBinToDcm(string in_bin_file_path, size_t width, size_t height, size_t bits, size_t frames,
                     bool all_frames_as_1st, std::function<void(const string &)> log_callback,
                     string_opt out_dcm_file_path = nullopt);
} // namespace tape_scanner::dicom_generator
