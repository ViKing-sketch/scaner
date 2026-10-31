#include <tape_scanner/scanner/DicomScan.hpp>
#include <tape_scanner/scanner/DicomNumbers.hpp>

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcmetinf.h>
#include <dcmtk/dcmdata/dcobject.h>
#include <dcmtk/dcmdata/dcdatset.h>
#include <dcmtk/dcmdata/dcitem.h>
#include <dcmtk/dcmdata/dcsequen.h>
#include <dcmtk/dcmdata/dcxfer.h>
#include <dcmtk/dcmimgle/dcmimage.h>
#include <dcmtk/dcmjpeg/djdecode.h>

#include <dcmtk/dcmdata/dcistrmb.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <set>

namespace {

std::string TrimDicom(std::string text) {
  while (!text.empty() && (text.back() == '\0' || text.back() == ' ')) {
    text.pop_back();
  }
  size_t begin = 0;
  while (begin < text.size() && (text[begin] == '\0' || text[begin] == ' ')) {
    ++begin;
  }
  return text.substr(begin);
}

void AppendUtf8(std::string &out, uint32_t codepoint) {
  if (codepoint < 0x80) {
    out.push_back(static_cast<char>(codepoint));
  } else if (codepoint < 0x800) {
    out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
    out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  } else if (codepoint < 0x10000) {
    out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
    out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  } else {
    out.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
    out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  }
}

uint32_t FromIso88595(unsigned char byte) {
  if (byte < 0xA0) {
    return byte;
  }
  if (byte >= 0xB0 && byte <= 0xEF) {
    return 0x0410u + static_cast<uint32_t>(byte - 0xB0);
  }
  switch (byte) {
  case 0xA0: return 0x00A0;
  case 0xA1: return 0x0401;
  case 0xA2: return 0x0402;
  case 0xA3: return 0x0403;
  case 0xA4: return 0x0404;
  case 0xA5: return 0x0405;
  case 0xA6: return 0x0406;
  case 0xA7: return 0x0407;
  case 0xA8: return 0x0408;
  case 0xA9: return 0x0409;
  case 0xAA: return 0x040A;
  case 0xAB: return 0x040B;
  case 0xAC: return 0x040C;
  case 0xAD: return 0x00AD;
  case 0xAE: return 0x040E;
  case 0xAF: return 0x040F;
  case 0xF0: return 0x2116;
  case 0xF1: return 0x0451;
  case 0xF2: return 0x0452;
  case 0xF3: return 0x0453;
  case 0xF4: return 0x0454;
  case 0xF5: return 0x0455;
  case 0xF6: return 0x0456;
  case 0xF7: return 0x0457;
  case 0xF8: return 0x0458;
  case 0xF9: return 0x0459;
  case 0xFA: return 0x045A;
  case 0xFB: return 0x045B;
  case 0xFC: return 0x045C;
  case 0xFD: return 0x00A7;
  case 0xFE: return 0x045E;
  case 0xFF: return 0x045F;
  default: return byte;
  }
}

uint32_t FromCp1251(unsigned char byte) {
  if (byte < 0x80) {
    return byte;
  }
  // 0x80..0xBF. 0x98 в cp1251 не задан.
  static const uint16_t kHigh[64] = {
      0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021, 0x20AC, 0x2030, 0x0409,
      0x2039, 0x040A, 0x040C, 0x040B, 0x040F, 0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022,
      0x2013, 0x2014, 0xFFFD, 0x2122, 0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F, 0x00A0,
      0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6, 0x00A7, 0x0401, 0x00A9, 0x0404, 0x00AB,
      0x00AC, 0x00AD, 0x00AE, 0x0407, 0x00B0, 0x00B1, 0x0406, 0x0456, 0x0491, 0x00B5, 0x00B6,
      0x00B7, 0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 0x0405, 0x0455, 0x0457};
  if (byte < 0xC0) {
    return kHigh[byte - 0x80];
  }
  return 0x0410u + static_cast<uint32_t>(byte - 0xC0);
}

std::string DecodeBytes(std::string_view bytes, bool (*is_single)(unsigned char), uint32_t (*map)(unsigned char)) {
  std::string out;
  out.reserve(bytes.size());
  for (unsigned char byte : bytes) {
    if (byte == 0) {
      break;
    }
    AppendUtf8(out, is_single(byte) ? byte : map(byte));
  }
  return out;
}

enum class CharsetKind { Ascii, Utf8, Latin1, Iso88595, Cp1251 };

CharsetKind ClassifyCharset(std::string_view specific_character_set, std::string_view bytes) {
  std::string folded;
  folded.reserve(specific_character_set.size());
  for (unsigned char ch : specific_character_set) {
    if (ch == ' ' || ch == '_' || ch == '-' || ch == '\\') {
      continue;
    }
    folded.push_back(static_cast<char>(std::toupper(ch)));
  }
  if (folded.find("192") != std::string::npos) {
    return CharsetKind::Utf8;
  }
  if (folded.find("144") != std::string::npos) {
    return CharsetKind::Iso88595;
  }
  if (folded.find("100") != std::string::npos || folded.find("101") != std::string::npos ||
      folded.find("109") != std::string::npos || folded.find("110") != std::string::npos) {
    return CharsetKind::Latin1;
  }
  const bool high = std::any_of(bytes.begin(), bytes.end(), [](char ch) { return static_cast<unsigned char>(ch) >= 0x80; });
  if (folded.empty() || folded.find("ISOIR6") != std::string::npos) {
    return high ? CharsetKind::Cp1251 : CharsetKind::Ascii;
  }
  return high ? CharsetKind::Cp1251 : CharsetKind::Ascii;
}

std::string ReadElementString(DcmItem *item, const DcmTagKey &key) {
  if (!item) {
    return {};
  }
  OFString value;
  if (item->findAndGetOFStringArray(key, value).bad()) {
    return {};
  }
  return TrimDicom(value.c_str());
}

bool ReadSpacing(DcmItem *item, double &row_mm, double &col_mm) {
  if (!item) {
    return false;
  }
  const DcmTagKey keys[] = {DCM_PixelSpacing, DCM_ImagerPixelSpacing, DCM_NominalScannedPixelSpacing};
  for (const DcmTagKey &key : keys) {
    OFString first;
    if (item->findAndGetOFString(key, first, 0).bad()) {
      continue;
    }
    const auto row = ParseDicomDecimal(first.c_str());
    if (!row) {
      continue;
    }
    OFString second;
    const auto col = item->findAndGetOFString(key, second, 1).good() ? ParseDicomDecimal(second.c_str()) : row;
    if (!col) {
      continue;
    }
    row_mm = PixelSpacingToMm(*row);
    col_mm = PixelSpacingToMm(*col);
    if (row_mm > 0.0 && col_mm > 0.0) {
      return true;
    }
  }
  return false;
}

bool HasImagePixels(DcmItem *item) {
  if (!item) {
    return false;
  }
  Uint16 rows = 0;
  if (item->findAndGetUint16(DCM_Rows, rows, 0, OFFalse).bad() || rows == 0) {
    return false;
  }
  DcmElement *pixels = nullptr;
  return item->findAndGetElement(DCM_PixelData, pixels, OFFalse).good() && pixels != nullptr;
}

DcmItem *ImageItem(DcmDataset *dataset) {
  if (HasImagePixels(dataset)) {
    return dataset;
  }
  DcmSequenceOfItems *sequence = nullptr;
  if (dataset->findAndGetSequence(DCM_BasicGrayscaleImageSequence, sequence, OFFalse).good() && sequence != nullptr &&
      sequence->card() > 0) {
    DcmItem *item = sequence->getItem(0);
    if (HasImagePixels(item)) {
      return item;
    }
  }
  return nullptr;
}

bool IsTextVr(DcmEVR vr) {
  switch (vr) {
  case EVR_AE:
  case EVR_AS:
  case EVR_CS:
  case EVR_DA:
  case EVR_DS:
  case EVR_DT:
  case EVR_IS:
  case EVR_LO:
  case EVR_LT:
  case EVR_PN:
  case EVR_SH:
  case EVR_ST:
  case EVR_TM:
  case EVR_UC:
  case EVR_UI:
  case EVR_UR:
  case EVR_UT:
    return true;
  default:
    return false;
  }
}

bool MostlyPrintable(std::string_view bytes) {
  if (bytes.size() < 2) {
    return false;
  }
  int printable = 0;
  for (unsigned char byte : bytes) {
    if (byte == 0) {
      break;
    }
    if (byte < 0x20 && byte != '\t') {
      return false;
    }
    if ((byte >= 0x20 && byte <= 0x7E) || byte >= 0x80) {
      ++printable;
    }
  }
  return printable >= 2;
}

void AppendItemTags(DcmItem *item, const std::string &charset, std::vector<DicomTagText> &tags, std::set<std::string> &seen) {
  if (!item) {
    return;
  }
  const unsigned long count = item->card();
  for (unsigned long index = 0; index < count; ++index) {
    DcmElement *element = item->getElement(index);
    if (!element) {
      continue;
    }
    DcmTag tag = element->getTag();
    if (tag.getGroup() == 0x7FE0 || tag.getEVR() == EVR_SQ || tag.getEVR() == EVR_ox || tag.getEVR() == EVR_px) {
      continue;
    }
    const std::string key = tag.toString().c_str();
    if (!seen.insert(key).second) {
      continue;
    }
    if (IsTextVr(tag.getEVR())) {
      OFString raw;
      if (element->getOFStringArray(raw).bad()) {
        continue;
      }
      std::string value = DecodeDicomString(raw.c_str(), charset);
      if (value.size() > 1024) {
        value.resize(1024);
      }
      const char *name = tag.getTagName();
      tags.push_back(DicomTagText{key, name ? name : "", std::move(value)});
      continue;
    }
    const bool private_group = (tag.getGroup() & 1u) != 0;
    if (!private_group || (tag.getEVR() != EVR_OB && tag.getEVR() != EVR_UN)) {
      continue;
    }
    Uint8 *data = nullptr;
    if (element->getUint8Array(data).bad() || data == nullptr || element->getLength() < 2 || element->getLength() > 4096) {
      continue;
    }
    const auto length = static_cast<size_t>(element->getLength());
    size_t text_len = 0;
    while (text_len < length && data[text_len] != 0) {
      ++text_len;
    }
    const std::string_view prefix(reinterpret_cast<const char *>(data), text_len);
    if (!MostlyPrintable(prefix)) {
      continue;
    }
    std::string value = DecodeDicomString(prefix, charset);
    if (value.empty()) {
      continue;
    }
    tags.push_back(DicomTagText{key, "Private", std::move(value)});
  }
}

bool CopyFrames(const uint8_t *data, size_t samples, int rows, int cols, int frames, int cv_type, size_t pixel_bytes,
                std::vector<cv::Mat> &frames_out) {
  if (!data || rows < 1 || cols < 1 || frames < 1 || samples < 1 || pixel_bytes < 1) {
    return false;
  }
  const size_t frame_bytes = static_cast<size_t>(rows) * static_cast<size_t>(cols) * samples * pixel_bytes;
  for (int frame = 0; frame < frames; ++frame) {
    cv::Mat image(rows, cols, cv_type);
    std::memcpy(image.data, data + frame_bytes * static_cast<size_t>(frame), frame_bytes);
    frames_out.push_back(std::move(image));
  }
  return true;
}

int FrameCount(DcmItem *item, size_t pixels_per_frame, size_t value_count) {
  int declared = 1;
  OFString text;
  if (item->findAndGetOFString(DCM_NumberOfFrames, text).good()) {
    declared = std::max(1, std::atoi(text.c_str()));
  }
  if (pixels_per_frame == 0 || value_count < pixels_per_frame) {
    return 0;
  }
  if (value_count % pixels_per_frame != 0) {
    return declared * pixels_per_frame <= value_count ? declared : 0;
  }
  const int from_length = static_cast<int>(value_count / pixels_per_frame);
  if (from_length > 0 && (declared == 1 || declared != from_length)) {
    return from_length;
  }
  return declared;
}

bool ExtractUncompressed(DcmItem *item, DicomScan &scan) {
  Uint16 rows = 0;
  Uint16 cols = 0;
  Uint16 bits_allocated = 0;
  Uint16 samples = 1;
  item->findAndGetUint16(DCM_Rows, rows);
  item->findAndGetUint16(DCM_Columns, cols);
  item->findAndGetUint16(DCM_BitsAllocated, bits_allocated);
  item->findAndGetUint16(DCM_SamplesPerPixel, samples);
  if (rows == 0 || cols == 0 || samples == 0) {
    return false;
  }
  const size_t pixels = static_cast<size_t>(rows) * static_cast<size_t>(cols) * samples;
  if (bits_allocated == 16 && samples == 1) {
    const Uint16 *data = nullptr;
    unsigned long count = 0;
    if (item->findAndGetUint16Array(DCM_PixelData, data, &count).bad() || data == nullptr) {
      return false;
    }
    const int frames = FrameCount(item, pixels, count);
    if (frames < 1) {
      return false;
    }
    scan.rows = rows;
    scan.cols = cols;
    scan.samples_per_pixel = samples;
    return CopyFrames(reinterpret_cast<const uint8_t *>(data), 1, rows, cols, frames, CV_16UC1, sizeof(Uint16), scan.frames);
  }
  if (bits_allocated == 8 && (samples == 1 || samples == 3)) {
    const Uint8 *data = nullptr;
    unsigned long count = 0;
    if (item->findAndGetUint8Array(DCM_PixelData, data, &count).bad() || data == nullptr) {
      return false;
    }
    const int frames = FrameCount(item, pixels, count);
    if (frames < 1) {
      return false;
    }
    scan.rows = rows;
    scan.cols = cols;
    scan.samples_per_pixel = samples;
    const int type = samples == 1 ? CV_8UC1 : CV_8UC3;
    return CopyFrames(data, samples, rows, cols, frames, type, 1, scan.frames);
  }
  return false;
}

bool ExtractWithDicomImage(DcmItem *item, E_TransferSyntax xfer, DicomScan &scan) {
  static bool codecs = false;
  if (!codecs) {
    DJDecoderRegistration::registerCodecs();
    codecs = true;
  }
  DicomImage image(item, xfer);
  if (image.getStatus() != EIS_Normal || image.getWidth() == 0 || image.getHeight() == 0) {
    return false;
  }
  image.setNoVoiTransformation();
  const int depth = image.getDepth() <= 8 ? 8 : 16;
  const bool mono = image.isMonochrome();
  const int type = mono ? (depth == 8 ? CV_8UC1 : CV_16UC1) : CV_8UC3;
  const unsigned long frames = std::max(1ul, image.getFrameCount());
  scan.rows = static_cast<int>(image.getHeight());
  scan.cols = static_cast<int>(image.getWidth());
  scan.samples_per_pixel = mono ? 1 : 3;
  scan.bits_allocated = depth;
  for (unsigned long frame = 0; frame < frames; ++frame) {
    const void *data = image.getOutputData(depth, frame);
    if (!data) {
      return false;
    }
    cv::Mat mat(scan.rows, scan.cols, type);
    std::memcpy(mat.data, data, mat.total() * mat.elemSize());
    scan.frames.push_back(std::move(mat));
  }
  return !scan.frames.empty();
}

} // namespace

bool FileHasDicomMarker(const std::string &path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return false;
  }
  char buffer[132];
  input.read(buffer, sizeof(buffer));
  return input.gcount() == static_cast<std::streamsize>(sizeof(buffer)) && std::memcmp(buffer + 128, "DICM", 4) == 0;
}

std::string DecodeDicomString(std::string_view bytes, std::string_view specific_character_set) {
  while (!bytes.empty() && (bytes.back() == '\0' || bytes.back() == ' ')) {
    bytes.remove_suffix(1);
  }
  while (!bytes.empty() && (bytes.front() == '\0' || bytes.front() == ' ')) {
    bytes.remove_prefix(1);
  }
  if (bytes.empty()) {
    return {};
  }
  const CharsetKind kind = ClassifyCharset(specific_character_set, bytes);
  if (kind == CharsetKind::Utf8 || kind == CharsetKind::Ascii) {
    std::string text(bytes);
    while (!text.empty() && text.back() == '\0') {
      text.pop_back();
    }
    return text;
  }
  if (kind == CharsetKind::Latin1) {
    return DecodeBytes(
        bytes, [](unsigned char) { return false; }, [](unsigned char byte) { return static_cast<uint32_t>(byte); });
  }
  if (kind == CharsetKind::Iso88595) {
    return DecodeBytes(
        bytes, [](unsigned char byte) { return byte < 0xA0; }, FromIso88595);
  }
  return DecodeBytes(
      bytes, [](unsigned char byte) { return byte < 0x80; }, FromCp1251);
}

struct RawHit {
  int rows = 0;
  int cols = 0;
  int bits_allocated = 0;
  int bits_stored = 0;
  int high_bit = 0;
  int samples = 1;
  int frames = 1;
  std::string photometric;
  std::string spacing;
  std::string modality;
  std::string patient;
  std::string charset;
  std::string sop_class;
  std::vector<DicomTagText> tags;
  size_t pixel_off = 0;
  size_t pixel_bytes = 0;
};

class DicomQuirks {
public:
  std::vector<uint8_t> bytes;
  RawHit hit;
  std::string error;

  bool Parse() {
    if (bytes.size() < 16) {
      error = "too small";
      return false;
    }
    size_t i = 0;
    bool explicit_vr = true;
    if (bytes.size() >= 132 && std::memcmp(bytes.data() + 128, "DICM", 4) == 0) {
      i = 132;
      std::string transfer_syntax;
      if (!ParseMeta(i, transfer_syntax)) {
        return false;
      }
      explicit_vr = TransferIsExplicit(transfer_syntax);
    } else if (i + 6 <= bytes.size()) {
      explicit_vr = std::isupper(bytes[i + 4]) && std::isupper(bytes[i + 5]);
    }
    Module module;
    const EndReason reason = ParseDataset(i, bytes.size(), explicit_vr, module, 0);
    if (reason == EndReason::Broken && hit.pixel_bytes == 0) {
      return false;
    }
    return Finish(explicit_vr);
  }

private:
  enum class EndReason { Boundary, ItemDelim, SeqDelim, Broken };

  struct Module {
    int rows = 0;
    int cols = 0;
    int bits_allocated = 0;
    int bits_stored = 0;
    int high_bit = 0;
    int samples = 1;
    int frames = 1;
    std::string photometric;
    std::string spacing;
  };

  static bool TransferIsExplicit(const std::string &uid) {
    if (uid.find("1.2.840.10008.1.2.1") != std::string::npos || uid.find("1.2.840.10008.1.2.2") != std::string::npos) {
      return true;
    }
    if (uid == "1.2.840.10008.1.2") {
      return false;
    }
    return true;
  }

  static bool LongVr(const char *vr) {
    return std::strcmp(vr, "OB") == 0 || std::strcmp(vr, "OD") == 0 || std::strcmp(vr, "OF") == 0 ||
           std::strcmp(vr, "OL") == 0 || std::strcmp(vr, "OV") == 0 || std::strcmp(vr, "OW") == 0 ||
           std::strcmp(vr, "SQ") == 0 || std::strcmp(vr, "SV") == 0 || std::strcmp(vr, "UC") == 0 ||
           std::strcmp(vr, "UN") == 0 || std::strcmp(vr, "UR") == 0 || std::strcmp(vr, "UT") == 0 ||
           std::strcmp(vr, "UV") == 0;
  }

  static bool TextVr(const char *vr) {
    return std::strcmp(vr, "AE") == 0 || std::strcmp(vr, "AS") == 0 || std::strcmp(vr, "CS") == 0 ||
           std::strcmp(vr, "DA") == 0 || std::strcmp(vr, "DS") == 0 || std::strcmp(vr, "DT") == 0 ||
           std::strcmp(vr, "IS") == 0 || std::strcmp(vr, "LO") == 0 || std::strcmp(vr, "LT") == 0 ||
           std::strcmp(vr, "PN") == 0 || std::strcmp(vr, "SH") == 0 || std::strcmp(vr, "ST") == 0 ||
           std::strcmp(vr, "TM") == 0 || std::strcmp(vr, "UC") == 0 || std::strcmp(vr, "UI") == 0 ||
           std::strcmp(vr, "UR") == 0 || std::strcmp(vr, "UT") == 0;
  }

  bool Need(size_t i, size_t n) const { return n <= bytes.size() && i <= bytes.size() - n; }

  uint16_t Read16(size_t &i) const {
    const uint16_t value = static_cast<uint16_t>(bytes[i] | (bytes[i + 1] << 8));
    i += 2;
    return value;
  }

  uint32_t Read32(size_t &i) const {
    const uint32_t value = static_cast<uint32_t>(bytes[i]) | (static_cast<uint32_t>(bytes[i + 1]) << 8) |
                           (static_cast<uint32_t>(bytes[i + 2]) << 16) | (static_cast<uint32_t>(bytes[i + 3]) << 24);
    i += 4;
    return value;
  }

  static std::string TagKey(uint16_t group, uint16_t element) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "(%04X,%04X)", group, element);
    return buf;
  }

  std::string Slice(size_t at, size_t len) const {
    if (!Need(at, len)) {
      return {};
    }
    return std::string(reinterpret_cast<const char *>(bytes.data() + at), len);
  }

  void NoteText(uint16_t group, uint16_t element, const std::string &value) {
    if (value.empty() || hit.tags.size() >= 200) {
      return;
    }
    hit.tags.push_back(DicomTagText{TagKey(group, element), "", value});
  }

  void NotePixel(const Module &module, size_t offset, size_t length) {
    if (module.rows <= 0 || module.cols <= 0 || length <= hit.pixel_bytes) {
      return;
    }
    hit.rows = module.rows;
    hit.cols = module.cols;
    hit.bits_allocated = module.bits_allocated;
    hit.bits_stored = module.bits_stored;
    hit.high_bit = module.high_bit;
    hit.samples = module.samples;
    hit.frames = module.frames;
    hit.photometric = module.photometric;
    hit.spacing = module.spacing;
    hit.pixel_off = offset;
    hit.pixel_bytes = length;
  }

  void FixGroupLength(size_t meta_end, size_t gl_value, size_t gl_after) {
    if (gl_value == static_cast<size_t>(-1) || meta_end < gl_after || meta_end - gl_after > 65535) {
      return;
    }
    const uint32_t actual = static_cast<uint32_t>(meta_end - gl_after);
    const uint32_t stored = static_cast<uint32_t>(bytes[gl_value]) | (static_cast<uint32_t>(bytes[gl_value + 1]) << 8) |
                            (static_cast<uint32_t>(bytes[gl_value + 2]) << 16) |
                            (static_cast<uint32_t>(bytes[gl_value + 3]) << 24);
    if (stored == actual) {
      return;
    }
    bytes[gl_value] = static_cast<uint8_t>(actual & 0xff);
    bytes[gl_value + 1] = static_cast<uint8_t>((actual >> 8) & 0xff);
    bytes[gl_value + 2] = static_cast<uint8_t>((actual >> 16) & 0xff);
    bytes[gl_value + 3] = static_cast<uint8_t>((actual >> 24) & 0xff);
  }

  bool ParseMeta(size_t &i, std::string &transfer_syntax) {
    size_t gl_value = static_cast<size_t>(-1);
    size_t gl_after = 0;
    while (Need(i, 8)) {
      const size_t tag_at = i;
      const uint16_t group = Read16(i);
      const uint16_t element = Read16(i);
      if (group != 0x0002) {
        i = tag_at;
        FixGroupLength(i, gl_value, gl_after);
        return true;
      }
      if (!Need(i, 2)) {
        return false;
      }
      char vr[3] = {static_cast<char>(bytes[i]), static_cast<char>(bytes[i + 1]), 0};
      i += 2;
      if (!std::isupper(static_cast<unsigned char>(vr[0])) || !std::isupper(static_cast<unsigned char>(vr[1]))) {
        i = tag_at;
        return true;
      }
      uint32_t len = 0;
      if (LongVr(vr)) {
        if (!Need(i, 6)) {
          return false;
        }
        i += 2;
        len = Read32(i);
      } else {
        if (!Need(i, 2)) {
          return false;
        }
        len = Read16(i);
      }
      if (len == 0xFFFFFFFFu || !Need(i, len)) {
        return false;
      }
      if (element == 0x0000 && len == 4) {
        gl_value = i;
        gl_after = i + 4;
      } else if (element == 0x0010) {
        transfer_syntax = TrimDicom(Slice(i, len));
      } else if (element == 0x0002) {
        hit.sop_class = TrimDicom(Slice(i, len));
        NoteText(group, element, hit.sop_class);
      }
      i += len;
      if (len % 2) {
        if (!Need(i, 1)) {
          return false;
        }
        ++i;
      }
    }
    return true;
  }

  void Remember(uint16_t group, uint16_t element, const char *vr, size_t at, uint32_t len, Module &module) {
    if (std::strcmp(vr, "US") == 0 && len == 2 && Need(at, 2)) {
      const int value = bytes[at] | (bytes[at + 1] << 8);
      if (group == 0x0028 && element == 0x0010) {
        module.rows = value;
      } else if (group == 0x0028 && element == 0x0011) {
        module.cols = value;
      } else if (group == 0x0028 && element == 0x0100) {
        module.bits_allocated = value;
      } else if (group == 0x0028 && element == 0x0101) {
        module.bits_stored = value;
      } else if (group == 0x0028 && element == 0x0102) {
        module.high_bit = value;
      } else if (group == 0x0028 && element == 0x0002) {
        module.samples = value == 0 ? 1 : value;
      }
      return;
    }
    if (!TextVr(vr) || len > 1024) {
      return;
    }
    const std::string text = TrimDicom(Slice(at, len));
    if (text.empty()) {
      return;
    }
    NoteText(group, element, text);
    if (group == 0x0008 && element == 0x0060) {
      hit.modality = text;
    } else if (group == 0x0008 && element == 0x0005) {
      hit.charset = text;
    } else if (group == 0x0010 && element == 0x0010) {
      hit.patient = text;
    } else if (group == 0x0008 && element == 0x0016) {
      hit.sop_class = text;
    } else if (group == 0x0028 && element == 0x0004) {
      module.photometric = text;
    } else if (group == 0x0028 && element == 0x0030 || group == 0x0018 && element == 0x1164 ||
               group == 0x0018 && element == 0x2010) {
      if (module.spacing.empty()) {
        module.spacing = text;
      }
    } else if (group == 0x0028 && element == 0x0008) {
      module.frames = std::max(1, std::atoi(text.c_str()));
    }
  }

  void RememberPrivate(uint16_t group, uint16_t element, size_t at, uint32_t len) {
    if ((group & 1u) == 0 || len < 2 || len > 4096 || !Need(at, len)) {
      return;
    }
    size_t text_len = 0;
    while (text_len < len && bytes[at + text_len] != 0) {
      ++text_len;
    }
    const std::string_view prefix(reinterpret_cast<const char *>(bytes.data() + at), text_len);
    if (!MostlyPrintable(prefix)) {
      return;
    }
    const std::string decoded = DecodeDicomString(prefix, hit.charset);
    if (!decoded.empty()) {
      NoteText(group, element, decoded);
    }
  }

  EndReason ParseDataset(size_t &i, size_t end, bool explicit_vr, Module module, int depth) {
    if (depth > 32) {
      return EndReason::Broken;
    }
    const size_t limit = std::min(end, bytes.size());
    while (Need(i, 8) && i < limit) {
      const size_t start = i;
      const uint16_t group = Read16(i);
      const uint16_t element = Read16(i);
      if (group == 0xFFFE) {
        if (!Need(i, 4)) {
          return EndReason::Broken;
        }
        const size_t len_at = i;
        uint32_t len = Read32(i);
        if (element == 0xE00D || element == 0xE0DD) {
          if (len == 0xFFFFFFFFu) {
            bytes[len_at] = bytes[len_at + 1] = bytes[len_at + 2] = bytes[len_at + 3] = 0;
          }
          return element == 0xE00D ? EndReason::ItemDelim : EndReason::SeqDelim;
        }
        if (element == 0xE000) {
          if (len == 0xFFFFFFFFu) {
            const EndReason nested = ParseDataset(i, limit, explicit_vr, module, depth + 1);
            if (nested == EndReason::Broken) {
              return nested;
            }
            continue;
          }
          if (!Need(i, len)) {
            return EndReason::Broken;
          }
          const size_t item_end = i + len;
          ParseDataset(i, item_end, explicit_vr, module, depth + 1);
          i = item_end;
          continue;
        }
        return EndReason::Broken;
      }
      if (group == 0xFFFC && element == 0xFFFC) {
        const bool vr_ok = Need(i, 2) && std::isupper(bytes[i]) && std::isupper(bytes[i + 1]);
        if (!vr_ok) {
          bytes.resize(start);
          if (bytes.size() % 2) {
            bytes.push_back(0);
          }
          return EndReason::Boundary;
        }
      }
      char vr[3] = "UN";
      uint32_t len = 0;
      bool sequence = false;
      if (explicit_vr) {
        if (!Need(i, 2) || !std::isupper(bytes[i]) || !std::isupper(bytes[i + 1])) {
          return EndReason::Broken;
        }
        vr[0] = static_cast<char>(bytes[i]);
        vr[1] = static_cast<char>(bytes[i + 1]);
        i += 2;
        if (LongVr(vr)) {
          if (!Need(i, 6)) {
            return EndReason::Broken;
          }
          i += 2;
          len = Read32(i);
          sequence = std::strcmp(vr, "SQ") == 0;
        } else {
          if (!Need(i, 2)) {
            return EndReason::Broken;
          }
          len = Read16(i);
        }
      } else {
        if (!Need(i, 4)) {
          return EndReason::Broken;
        }
        len = Read32(i);
        sequence = len == 0xFFFFFFFFu;
      }
      if (len == 0xFFFFFFFFu) {
        if (!sequence) {
          // Сжатые пиксели. Дальше не чиним, чтобы не задеть кадр.
          return EndReason::Boundary;
        }
        const EndReason nested = ParseDataset(i, limit, explicit_vr, module, depth + 1);
        if (nested == EndReason::Broken) {
          return nested;
        }
        continue;
      }
      if (!Need(i, len)) {
        return EndReason::Broken;
      }
      if (sequence) {
        const size_t seq_end = i + len;
        ParseDataset(i, seq_end, explicit_vr, module, depth + 1);
        i = seq_end;
        continue;
      }
      if (group == 0x7FE0 && element == 0x0010) {
        NotePixel(module, i, len);
      } else if (std::strcmp(vr, "OB") == 0 || std::strcmp(vr, "UN") == 0) {
        RememberPrivate(group, element, i, len);
      } else {
        Remember(group, element, vr, i, len, module);
      }
      i += len;
      if ((len % 2) != 0) {
        if (i < bytes.size()) {
          ++i;
        }
      }
      if (i <= start) {
        return EndReason::Broken;
      }
    }
    return EndReason::Boundary;
  }

  bool Finish(bool) {
    if (hit.pixel_bytes == 0 || hit.rows <= 0 || hit.cols <= 0 || hit.pixel_off >= bytes.size()) {
      error = "no pixel data";
      return false;
    }
    return true;
  }
};

bool FillFromCapture(const DicomQuirks &parsed, DicomScan &scan) {
  const RawHit &hit = parsed.hit;
  const int bits = hit.bits_allocated == 8 ? 8 : 16;
  const int samples = hit.samples <= 0 ? 1 : hit.samples;
  const size_t pixel_size = bits / 8;
  const size_t frame_bytes =
      static_cast<size_t>(hit.rows) * static_cast<size_t>(hit.cols) * static_cast<size_t>(samples) * pixel_size;
  if (frame_bytes == 0 || hit.pixel_off + frame_bytes > parsed.bytes.size()) {
    return false;
  }
  int frames = 1;
  if (hit.pixel_bytes % frame_bytes == 0) {
    frames = static_cast<int>(hit.pixel_bytes / frame_bytes);
  }
  if (frames < 1) {
    frames = 1;
  }
  const int type = bits == 8 ? (samples == 3 ? CV_8UC3 : CV_8UC1) : CV_16UC1;
  if (bits == 16 && samples != 1) {
    return false;
  }
  scan = {};
  scan.rows = hit.rows;
  scan.cols = hit.cols;
  scan.bits_allocated = hit.bits_allocated == 0 ? bits : hit.bits_allocated;
  scan.bits_stored = hit.bits_stored == 0 ? scan.bits_allocated : hit.bits_stored;
  scan.high_bit = hit.high_bit == 0 ? scan.bits_stored - 1 : hit.high_bit;
  scan.samples_per_pixel = samples;
  scan.photometric = hit.photometric;
  scan.monochrome1 = scan.photometric == "MONOCHROME1";
  scan.modality = hit.modality;
  scan.charset = hit.charset;
  scan.sop_class_uid = hit.sop_class;
  scan.patient_name = DecodeDicomString(hit.patient, hit.charset);
  scan.tags = hit.tags;
  std::string row_text = hit.spacing;
  std::string col_text;
  const auto slash = row_text.find('\\');
  if (slash != std::string::npos) {
    col_text = row_text.substr(slash + 1);
    row_text.resize(slash);
  }
  const auto row = ParseDicomDecimal(row_text);
  const auto col = col_text.empty() ? row : ParseDicomDecimal(col_text);
  if (row && col) {
    scan.spacing_row_mm = PixelSpacingToMm(*row);
    scan.spacing_col_mm = PixelSpacingToMm(*col);
  }
  for (int frame = 0; frame < frames; ++frame) {
    const size_t off = hit.pixel_off + static_cast<size_t>(frame) * frame_bytes;
    if (off + frame_bytes > parsed.bytes.size()) {
      break;
    }
    cv::Mat image(hit.rows, hit.cols, type);
    std::memcpy(image.data, parsed.bytes.data() + off, frame_bytes);
    scan.frames.push_back(std::move(image));
  }
  return !scan.frames.empty();
}

std::vector<uint8_t> ReadEntireFile(const std::string &path, std::string &error) {
  std::ifstream input(std::filesystem::path(path), std::ios::binary);
  if (!input) {
    error = "Error open file: " + path;
    return {};
  }
  input.seekg(0, std::ios::end);
  const std::streamoff length = input.tellg();
  if (length < 0) {
    error = "Error open file: " + path;
    return {};
  }
  input.seekg(0);
  std::vector<uint8_t> bytes(static_cast<size_t>(length));
  if (length > 0) {
    input.read(reinterpret_cast<char *>(bytes.data()), length);
    if (!input) {
      error = "Error open file: " + path;
      return {};
    }
  }
  return bytes;
}

bool ReadMemory(const std::vector<uint8_t> &bytes, DcmFileFormat &file, std::string &error) {
  if (bytes.size() < 8 || (bytes.size() % 2) != 0) {
    error = "truncated DICOM";
    return false;
  }
  DcmInputBufferStream stream;
  stream.setBuffer(bytes.data(), static_cast<offile_off_t>(bytes.size()));
  stream.setEos();
  file.transferInit();
  const OFCondition status = file.read(stream, EXS_Unknown, EGL_noChange, 0xffffffffu);
  file.transferEnd();
  if (status.bad()) {
    error = status.text();
    return false;
  }
  return file.getDataset() != nullptr;
}

bool FillFromFormat(DcmFileFormat &file, const std::string &path, DicomScan &scan, std::string &error) {
  scan = {};
  DcmDataset *dataset = file.getDataset();
  if (!dataset) {
    error = "Error open file: no dataset in " + path;
    return false;
  }
  DcmItem *item = ImageItem(dataset);
  if (!item) {
    error = "Error open file: no pixel data in " + path;
    return false;
  }

  Uint16 bits_allocated = 0;
  Uint16 bits_stored = 0;
  Uint16 high_bit = 0;
  Uint16 samples = 1;
  item->findAndGetUint16(DCM_BitsAllocated, bits_allocated);
  item->findAndGetUint16(DCM_BitsStored, bits_stored);
  item->findAndGetUint16(DCM_HighBit, high_bit);
  item->findAndGetUint16(DCM_SamplesPerPixel, samples);
  scan.bits_allocated = bits_allocated;
  scan.bits_stored = bits_stored;
  scan.high_bit = high_bit;
  scan.samples_per_pixel = samples == 0 ? 1 : samples;

  const E_TransferSyntax xfer = dataset->getOriginalXfer();
  const bool uncompressed = xfer == EXS_LittleEndianExplicit || xfer == EXS_LittleEndianImplicit ||
                            xfer == EXS_BigEndianExplicit || xfer == EXS_Unknown;
  bool extracted = false;
  if (uncompressed) {
    extracted = ExtractUncompressed(item, scan);
  }
  if (!extracted) {
    scan.frames.clear();
    extracted = ExtractWithDicomImage(item, xfer, scan);
  }
  if (!extracted || scan.frames.empty()) {
    error = "Error open file: no pixel data in " + path;
    return false;
  }

  scan.charset = ReadElementString(dataset, DCM_SpecificCharacterSet);
  if (scan.charset.empty()) {
    scan.charset = ReadElementString(item, DCM_SpecificCharacterSet);
  }
  scan.photometric = ReadElementString(item, DCM_PhotometricInterpretation);
  if (scan.photometric.empty()) {
    scan.photometric = ReadElementString(dataset, DCM_PhotometricInterpretation);
  }
  scan.monochrome1 = scan.photometric == "MONOCHROME1";
  scan.modality = ReadElementString(dataset, DCM_Modality);
  if (scan.modality.empty()) {
    scan.modality = ReadElementString(item, DCM_Modality);
  }
  const std::string patient_raw = ReadElementString(item, DCM_PatientName).empty()
                                      ? ReadElementString(dataset, DCM_PatientName)
                                      : ReadElementString(item, DCM_PatientName);
  scan.patient_name = DecodeDicomString(patient_raw, scan.charset);

  if (!ReadSpacing(item, scan.spacing_row_mm, scan.spacing_col_mm)) {
    ReadSpacing(dataset, scan.spacing_row_mm, scan.spacing_col_mm);
  }

  OFString sop_class;
  if (dataset->findAndGetOFString(DCM_SOPClassUID, sop_class).bad() && file.getMetaInfo() != nullptr) {
    file.getMetaInfo()->findAndGetOFString(DCM_MediaStorageSOPClassUID, sop_class);
  }
  scan.sop_class_uid = TrimDicom(sop_class.c_str());

  std::set<std::string> seen;
  AppendItemTags(dataset, scan.charset, scan.tags, seen);
  if (item != dataset) {
    AppendItemTags(item, scan.charset, scan.tags, seen);
  }
  if (scan.patient_name.empty()) {
    for (const DicomTagText &tag : scan.tags) {
      if (tag.key == "(0010,0010)" && !tag.value.empty()) {
        scan.patient_name = tag.value;
        break;
      }
    }
  }
  return true;
}

bool ParseRawDicom(const uint8_t *data, size_t size, DicomScan &scan, std::string &error) {
  scan = {};
  error.clear();
  if (data == nullptr || size < 8) {
    error = "Error open file: too small";
    return false;
  }
  try {
    DicomQuirks parsed;
    parsed.bytes.assign(data, data + size);
    if (!parsed.Parse() || !FillFromCapture(parsed, scan) || scan.frames.empty()) {
      scan = {};
      error = parsed.error.empty() ? "Error open file: no pixel data" : "Error open file: " + parsed.error;
      return false;
    }
    return true;
  } catch (const std::exception &ex) {
    scan = {};
    error = std::string("Error open file: ") + ex.what();
    return false;
  } catch (...) {
    scan = {};
    error = "Error open file: unexpected failure";
    return false;
  }
}

bool LoadDicomScan(const std::string &path, DicomScan &scan, std::string &error) {
  scan = {};
  error.clear();
  if (path.empty()) {
    error = "Error open file: empty path";
    return false;
  }
  try {
    std::string read_error;
    std::vector<uint8_t> bytes = ReadEntireFile(path, read_error);
    if (bytes.empty()) {
      error = read_error.empty() ? "Error open file: " + path : read_error;
      return false;
    }
    DicomQuirks parsed;
    parsed.bytes = std::move(bytes);
    const bool raw_ok = parsed.Parse();
    DicomScan raw_scan;
    const bool raw_image = raw_ok && FillFromCapture(parsed, raw_scan) && !raw_scan.frames.empty();

    // ARRAY пишет длину разделителя 0xFFFFFFFF и хвост (FFFC,FFFC).
    // DCMTK из MSYS2 из-за этого отвергает весь файл. В parsed.bytes это уже исправлено.
    dcmIgnoreParsingErrors.set(OFTrue);
    dcmIgnoreFileMetaInformationGroupLength.set(OFTrue);
    DcmFileFormat file;
    std::string dcm_error;
    DicomScan decoded;
    std::string fill_error;
    if (ReadMemory(parsed.bytes, file, dcm_error) && FillFromFormat(file, path, decoded, fill_error)) {
      scan = std::move(decoded);
      return true;
    }
    if (raw_image) {
      scan = std::move(raw_scan);
      return true;
    }
    error = "Error open file: " + path;
    if (!dcm_error.empty()) {
      error += " (" + dcm_error + ")";
    } else if (!fill_error.empty()) {
      error = fill_error;
    } else if (!parsed.error.empty()) {
      error += " (" + parsed.error + ")";
    }
    scan = {};
    return false;
  } catch (const std::exception &ex) {
    scan = {};
    error = std::string("Error open file: ") + path + " (" + ex.what() + ")";
    return false;
  } catch (...) {
    scan = {};
    error = "Error open file: " + path;
    return false;
  }
}
