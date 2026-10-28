#include <tape_scanner/scanner/SessionStore.hpp>

#include <tape_scanner/scanner/FileCommit.hpp>
#include <tape_scanner/scanner/ScanLog.hpp>

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcfilefo.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <random>
#include <set>
#include <system_error>

namespace {

constexpr uint64_t kMaxTiffBytes = 0xFFFFF000ull;

uint32_t Rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }

constexpr uint32_t kSha256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

void CompressSha256(uint32_t state[8], const uint8_t block[64]) {
  uint32_t w[64];
  for (int i = 0; i < 16; ++i) {
    w[i] = (uint32_t(block[i * 4]) << 24) | (uint32_t(block[i * 4 + 1]) << 16) |
           (uint32_t(block[i * 4 + 2]) << 8) | uint32_t(block[i * 4 + 3]);
  }
  for (int i = 16; i < 64; ++i) {
    const uint32_t s0 = Rotr(w[i - 15], 7) ^ Rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
    const uint32_t s1 = Rotr(w[i - 2], 17) ^ Rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }
  uint32_t a = state[0];
  uint32_t b = state[1];
  uint32_t c = state[2];
  uint32_t d = state[3];
  uint32_t e = state[4];
  uint32_t f = state[5];
  uint32_t g = state[6];
  uint32_t h = state[7];
  for (int i = 0; i < 64; ++i) {
    const uint32_t s1 = Rotr(e, 6) ^ Rotr(e, 11) ^ Rotr(e, 25);
    const uint32_t ch = (e & f) ^ ((~e) & g);
    const uint32_t t1 = h + s1 + ch + kSha256[i] + w[i];
    const uint32_t s0 = Rotr(a, 2) ^ Rotr(a, 13) ^ Rotr(a, 22);
    const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
    const uint32_t t2 = s0 + maj;
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }
  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
  state[5] += f;
  state[6] += g;
  state[7] += h;
}

struct Sha256Hasher {
  uint32_t state[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                       0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  uint8_t block[64] = {};
  size_t filled = 0;
  uint64_t nbytes = 0;

  void Update(const uint8_t *data, size_t len) {
    nbytes += len;
    while (len > 0) {
      const size_t n = std::min(len, size_t(64) - filled);
      std::memcpy(block + filled, data, n);
      filled += n;
      data += n;
      len -= n;
      if (filled == 64) {
        CompressSha256(state, block);
        filled = 0;
      }
    }
  }

  void Digest(uint8_t out[32]) const {
    Sha256Hasher copy = *this;
    const uint64_t bits = copy.nbytes * 8;
    auto push = [&](uint8_t value) {
      copy.block[copy.filled++] = value;
      if (copy.filled == 64) {
        CompressSha256(copy.state, copy.block);
        copy.filled = 0;
      }
    };
    push(0x80);
    while (copy.filled != 56) {
      push(0);
    }
    for (int i = 0; i < 8; ++i) {
      push(static_cast<uint8_t>((bits >> (56 - 8 * i)) & 0xff));
    }
    for (int i = 0; i < 8; ++i) {
      out[i * 4] = static_cast<uint8_t>(copy.state[i] >> 24);
      out[i * 4 + 1] = static_cast<uint8_t>(copy.state[i] >> 16);
      out[i * 4 + 2] = static_cast<uint8_t>(copy.state[i] >> 8);
      out[i * 4 + 3] = static_cast<uint8_t>(copy.state[i]);
    }
  }
};

std::string Hex32(const uint8_t digest[32]) {
  static const char kHex[] = "0123456789abcdef";
  std::string out(64, '0');
  for (int i = 0; i < 32; ++i) {
    out[static_cast<size_t>(i) * 2] = kHex[digest[i] >> 4];
    out[static_cast<size_t>(i) * 2 + 1] = kHex[digest[i] & 0x0f];
  }
  return out;
}

void Put16(uint8_t *p, uint16_t value) {
  p[0] = static_cast<uint8_t>(value & 0xff);
  p[1] = static_cast<uint8_t>((value >> 8) & 0xff);
}

void Put32(uint8_t *p, uint32_t value) {
  p[0] = static_cast<uint8_t>(value & 0xff);
  p[1] = static_cast<uint8_t>((value >> 8) & 0xff);
  p[2] = static_cast<uint8_t>((value >> 16) & 0xff);
  p[3] = static_cast<uint8_t>((value >> 24) & 0xff);
}

uint16_t Get16(const uint8_t *p) { return static_cast<uint16_t>(p[0] | (uint16_t(p[1]) << 8)); }

uint32_t Get32(const uint8_t *p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

std::string JsonString(const std::string &text) {
  std::string out = "\"";
  for (unsigned char c : text) {
    switch (c) {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      if (c < 0x20) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "\\u%04x", c);
        out += buf;
      } else {
        out.push_back(static_cast<char>(c));
      }
      break;
    }
  }
  out.push_back('"');
  return out;
}

std::string JsonDouble(double value) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.17g", value);
  return buf;
}

std::string StemOf(const std::filesystem::path &chosen) {
  std::string stem = chosen.stem().string();
  if (stem.empty() || stem == "." || stem == "..") {
    return "session";
  }
  return stem;
}

bool ResolveInside(const std::filesystem::path &root, const std::string &relative, std::filesystem::path &full) {
  if (relative.empty() || relative.front() == '/' || relative.find('\\') != std::string::npos) {
    return false;
  }
  const std::filesystem::path rel(relative);
  if (rel.is_absolute()) {
    return false;
  }
  for (const auto &part : rel) {
    if (part == "..") {
      return false;
    }
  }
  full = root / rel;
  const std::string root_text = root.lexically_normal().generic_string();
  const std::string full_text = full.lexically_normal().generic_string();
  return full_text == root_text || full_text.rfind(root_text + "/", 0) == 0;
}

struct Json {
  enum class Type { Null, Bool, Number, String, Array, Object };
  Type type = Type::Null;
  bool b = false;
  double n = 0;
  std::string s;
  std::vector<Json> a;
  std::vector<std::pair<std::string, Json>> o;

  const Json *Find(const std::string &key) const {
    if (type != Type::Object) {
      return nullptr;
    }
    for (const auto &item : o) {
      if (item.first == key) {
        return &item.second;
      }
    }
    return nullptr;
  }
};

class JsonParser {
public:
  explicit JsonParser(const std::string &text) : s_(text) {}

  bool Parse(Json &out) {
    if (!ParseValue(out)) {
      return false;
    }
    Skip();
    return i_ == s_.size();
  }

private:
  const std::string &s_;
  size_t i_ = 0;

  void Skip() {
    while (i_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[i_]))) {
      ++i_;
    }
  }

  bool Consume(char expected) {
    Skip();
    if (i_ < s_.size() && s_[i_] == expected) {
      ++i_;
      return true;
    }
    return false;
  }

  static int Hex(char c) {
    if (c >= '0' && c <= '9') {
      return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
      return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
      return c - 'A' + 10;
    }
    return -1;
  }

  static void AppendUtf8(std::string &out, uint32_t code) {
    if (code < 0x80) {
      out.push_back(static_cast<char>(code));
    } else if (code < 0x800) {
      out.push_back(static_cast<char>(0xc0 | (code >> 6)));
      out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
    } else {
      out.push_back(static_cast<char>(0xe0 | (code >> 12)));
      out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
      out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
    }
  }

  bool ParseString(std::string &out) {
    Skip();
    if (i_ >= s_.size() || s_[i_] != '"') {
      return false;
    }
    ++i_;
    out.clear();
    while (i_ < s_.size()) {
      const unsigned char c = static_cast<unsigned char>(s_[i_++]);
      if (c == '"') {
        return true;
      }
      if (c == '\\') {
        if (i_ >= s_.size()) {
          return false;
        }
        const char escaped = s_[i_++];
        switch (escaped) {
        case '"':
        case '\\':
        case '/':
          out.push_back(escaped);
          break;
        case 'b':
          out.push_back('\b');
          break;
        case 'f':
          out.push_back('\f');
          break;
        case 'n':
          out.push_back('\n');
          break;
        case 'r':
          out.push_back('\r');
          break;
        case 't':
          out.push_back('\t');
          break;
        case 'u': {
          if (i_ + 4 > s_.size()) {
            return false;
          }
          uint32_t code = 0;
          for (int n = 0; n < 4; ++n) {
            const int digit = Hex(s_[i_++]);
            if (digit < 0) {
              return false;
            }
            code = (code << 4) | static_cast<uint32_t>(digit);
          }
          AppendUtf8(out, code);
          break;
        }
        default:
          return false;
        }
      } else if (c < 0x20) {
        return false;
      } else {
        out.push_back(static_cast<char>(c));
      }
    }
    return false;
  }

  bool ParseNumber(Json &out) {
    Skip();
    const size_t start = i_;
    if (i_ < s_.size() && s_[i_] == '-') {
      ++i_;
    }
    if (i_ >= s_.size() || !std::isdigit(static_cast<unsigned char>(s_[i_]))) {
      return false;
    }
    while (i_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[i_]))) {
      ++i_;
    }
    if (i_ < s_.size() && s_[i_] == '.') {
      ++i_;
      if (i_ >= s_.size() || !std::isdigit(static_cast<unsigned char>(s_[i_]))) {
        return false;
      }
      while (i_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[i_]))) {
        ++i_;
      }
    }
    if (i_ < s_.size() && (s_[i_] == 'e' || s_[i_] == 'E')) {
      ++i_;
      if (i_ < s_.size() && (s_[i_] == '+' || s_[i_] == '-')) {
        ++i_;
      }
      if (i_ >= s_.size() || !std::isdigit(static_cast<unsigned char>(s_[i_]))) {
        return false;
      }
      while (i_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[i_]))) {
        ++i_;
      }
    }
    char *end = nullptr;
    out.n = std::strtod(s_.c_str() + start, &end);
    if (end != s_.c_str() + i_) {
      return false;
    }
    out.type = Json::Type::Number;
    return true;
  }

  bool ParseArray(Json &out) {
    if (!Consume('[')) {
      return false;
    }
    out.type = Json::Type::Array;
    if (Consume(']')) {
      return true;
    }
    while (true) {
      Json value;
      if (!ParseValue(value)) {
        return false;
      }
      out.a.push_back(std::move(value));
      if (Consume(']')) {
        return true;
      }
      if (!Consume(',')) {
        return false;
      }
    }
  }

  bool ParseObject(Json &out) {
    if (!Consume('{')) {
      return false;
    }
    out.type = Json::Type::Object;
    if (Consume('}')) {
      return true;
    }
    while (true) {
      std::string key;
      if (!ParseString(key) || !Consume(':')) {
        return false;
      }
      Json value;
      if (!ParseValue(value)) {
        return false;
      }
      out.o.emplace_back(std::move(key), std::move(value));
      if (Consume('}')) {
        return true;
      }
      if (!Consume(',')) {
        return false;
      }
    }
  }

  bool ParseValue(Json &out) {
    Skip();
    if (i_ >= s_.size()) {
      return false;
    }
    const char c = s_[i_];
    if (c == '"') {
      out.type = Json::Type::String;
      return ParseString(out.s);
    }
    if (c == '{') {
      return ParseObject(out);
    }
    if (c == '[') {
      return ParseArray(out);
    }
    if (c == 't' && s_.compare(i_, 4, "true") == 0) {
      i_ += 4;
      out.type = Json::Type::Bool;
      out.b = true;
      return true;
    }
    if (c == 'f' && s_.compare(i_, 5, "false") == 0) {
      i_ += 5;
      out.type = Json::Type::Bool;
      out.b = false;
      return true;
    }
    if (c == 'n' && s_.compare(i_, 4, "null") == 0) {
      i_ += 4;
      out.type = Json::Type::Null;
      return true;
    }
    if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
      return ParseNumber(out);
    }
    return false;
  }
};

bool AsString(const Json *value, std::string &out) {
  if (value == nullptr || value->type != Json::Type::String) {
    return false;
  }
  out = value->s;
  return true;
}

bool AsInt(const Json *value, int &out) {
  if (value == nullptr || value->type != Json::Type::Number || std::floor(value->n) != value->n) {
    return false;
  }
  out = static_cast<int>(value->n);
  return true;
}

bool AsDouble(const Json *value, double &out) {
  if (value == nullptr || value->type != Json::Type::Number) {
    return false;
  }
  out = value->n;
  return true;
}

bool FillManifest(const Json &json, SessionManifest &manifest) {
  if (json.type != Json::Type::Object) {
    return false;
  }
  std::string format;
  int version = 0;
  std::string status;
  if (!AsString(json.Find("format"), format) || format != kSessionFormatName ||
      !AsInt(json.Find("format_version"), version) || version != kSessionFormatVersion ||
      !AsString(json.Find("session_id"), manifest.session_id) || manifest.session_id.empty() ||
      !AsString(json.Find("status"), status) || !SessionStatusFromName(status, manifest.status) ||
      !AsString(json.Find("base_path"), manifest.base_path) || !AsString(json.Find("study_uid"), manifest.study_uid) ||
      !AsString(json.Find("series_uid"), manifest.series_uid)) {
    return false;
  }
  manifest.format_version = version;
  const Json *geometry = json.Find("geometry");
  if (geometry == nullptr || !AsInt(geometry->Find("bin"), manifest.bin) ||
      !AsDouble(geometry->Find("pixel_um_x"), manifest.pixel_um_x) ||
      !AsDouble(geometry->Find("pixel_um_y"), manifest.pixel_um_y)) {
    return false;
  }
  manifest.calibration_ref.clear();
  if (const Json *calibration = json.Find("calibration_ref")) {
    if (calibration->type == Json::Type::String) {
      manifest.calibration_ref = calibration->s;
    } else if (calibration->type != Json::Type::Null) {
      return false;
    }
  }

  const Json *tiles = json.Find("tiles");
  const Json *segments = json.Find("segments");
  if (tiles == nullptr || tiles->type != Json::Type::Array || segments == nullptr ||
      segments->type != Json::Type::Array) {
    return false;
  }
  for (const Json &item : tiles->a) {
    SessionTileRecord tile;
    if (!AsString(item.Find("tile_id"), tile.tile_id) || !AsString(item.Find("path"), tile.relative_path) ||
        !AsString(item.Find("sha256"), tile.sha256) || !AsInt(item.Find("x"), tile.x) ||
        !AsInt(item.Find("y"), tile.y) || !AsInt(item.Find("rows"), tile.rows) ||
        !AsInt(item.Find("cols"), tile.cols)) {
      return false;
    }
    manifest.tiles.push_back(std::move(tile));
  }
  for (const Json &item : segments->a) {
    SessionSegmentRecord segment;
    if (!AsInt(item.Find("instance"), segment.instance) || !AsString(item.Find("path"), segment.relative_path) ||
        !AsString(item.Find("sha256"), segment.sha256) || !AsString(item.Find("sop_uid"), segment.sop_uid) ||
        !AsInt(item.Find("rows"), segment.rows) || !AsInt(item.Find("cols"), segment.cols) ||
        !AsInt(item.Find("y_origin"), segment.y_origin)) {
      return false;
    }
    if (const Json *sources = item.Find("source_tile_ids")) {
      if (sources->type != Json::Type::Array) {
        return false;
      }
      for (const Json &source : sources->a) {
        if (source.type != Json::Type::String) {
          return false;
        }
        segment.source_tile_ids.push_back(source.s);
      }
    }
    manifest.segments.push_back(std::move(segment));
  }
  return manifest.status != SessionCommitStatus::Empty;
}

std::string ManifestJson(const SessionManifest &manifest) {
  std::string out = "{\n";
  out += "  \"format\": " + JsonString(kSessionFormatName) + ",\n";
  out += "  \"format_version\": " + std::to_string(manifest.format_version) + ",\n";
  out += "  \"session_id\": " + JsonString(manifest.session_id) + ",\n";
  out += "  \"status\": " + JsonString(SessionStatusName(manifest.status)) + ",\n";
  out += "  \"base_path\": " + JsonString(manifest.base_path) + ",\n";
  out += "  \"study_uid\": " + JsonString(manifest.study_uid) + ",\n";
  out += "  \"series_uid\": " + JsonString(manifest.series_uid) + ",\n";
  out += "  \"geometry\": {\n";
  out += "    \"bin\": " + std::to_string(manifest.bin) + ",\n";
  out += "    \"pixel_um_x\": " + JsonDouble(manifest.pixel_um_x) + ",\n";
  out += "    \"pixel_um_y\": " + JsonDouble(manifest.pixel_um_y) + ",\n";
  out += "    \"row_spacing_mm\": " + JsonDouble(manifest.pixel_um_y * manifest.bin / 1000.0) + ",\n";
  out += "    \"col_spacing_mm\": " + JsonDouble(manifest.pixel_um_x * manifest.bin / 1000.0) + "\n";
  out += "  },\n";
  out += "  \"blend\": {\"seam\": \"linear-overlap\", \"bin\": " + std::to_string(manifest.bin) +
         ", \"crop\": null},\n";
  out += "  \"calibration_ref\": " + JsonString(manifest.calibration_ref) + ",\n";
  out += "  \"quantization\": {\"export\": \"uint16\", \"scale\": 1, \"offset\": 0, \"bits_stored\": 16, "
         "\"note\": \"BitsStored 16 is the width of the stored mosaic samples. It is not a measured sensor bit "
         "depth.\"},\n";
  out += "  \"valid_pixel_mask\": null,\n";
  out += "  \"expected_tile_plan\": null,\n";
  out += "  \"analysis\": [],\n";
  out += "  \"tiles\": [";
  for (size_t i = 0; i < manifest.tiles.size(); ++i) {
    const SessionTileRecord &tile = manifest.tiles[i];
    if (i != 0) {
      out += ",";
    }
    out += "\n    {\"tile_id\": " + JsonString(tile.tile_id) + ", \"path\": " + JsonString(tile.relative_path) +
           ", \"sha256\": " + JsonString(tile.sha256) + ", \"x\": " + std::to_string(tile.x) +
           ", \"y\": " + std::to_string(tile.y) + ", \"rows\": " + std::to_string(tile.rows) +
           ", \"cols\": " + std::to_string(tile.cols) + ", \"format\": \"tiff-mono16-uncompressed\"}";
  }
  out += manifest.tiles.empty() ? "],\n" : "\n  ],\n";
  out += "  \"segments\": [";
  for (size_t i = 0; i < manifest.segments.size(); ++i) {
    const SessionSegmentRecord &segment = manifest.segments[i];
    if (i != 0) {
      out += ",";
    }
    out += "\n    {\"instance\": " + std::to_string(segment.instance) +
           ", \"path\": " + JsonString(segment.relative_path) + ", \"sha256\": " + JsonString(segment.sha256) +
           ", \"sop_uid\": " + JsonString(segment.sop_uid) + ", \"rows\": " + std::to_string(segment.rows) +
           ", \"cols\": " + std::to_string(segment.cols) + ", \"y_origin\": " + std::to_string(segment.y_origin) +
           ", \"source_tile_ids\": [";
    for (size_t t = 0; t < segment.source_tile_ids.size(); ++t) {
      if (t != 0) {
        out += ", ";
      }
      out += JsonString(segment.source_tile_ids[t]);
    }
    out += "]}";
  }
  out += manifest.segments.empty() ? "],\n" : "\n  ],\n";
  out += "  \"storage\": {\"file_barrier\": \"fsync\", \"directory_barrier\": \"fsync\", "
         "\"publish\": \"hard-link-without-replace\", \"confirmed_by\": \"manifest-entry-and-sha256\", "
         "\"note\": \"Rename is not the power-loss barrier. A file is confirmed only after its bytes are fsynced, "
         "the directory entry is fsynced, and this manifest lists the relative path and sha256. This describes a "
         "local filesystem that honors fsync. It is not a measured result on a scanner PC.\"}\n";
  out += "}\n";
  return out;
}

bool WriteSessionManifest(const std::filesystem::path &directory, const SessionManifest &manifest) {
  const std::filesystem::path final_path = directory / "manifest.json";
  const std::filesystem::path temp_path = directory / "manifest.json.partial";
  {
    std::ofstream out(temp_path, std::ios::binary | std::ios::trunc);
    if (!out) {
      return false;
    }
    const std::string json = ManifestJson(manifest);
    out.write(json.data(), static_cast<std::streamsize>(json.size()));
    out.flush();
    if (!out) {
      return false;
    }
  }
  if (!PublishReplacingFile(temp_path, final_path)) {
    std::error_code error;
    std::filesystem::remove(temp_path, error);
    return false;
  }
  return true;
}

bool WriteMono16Tiff(const std::filesystem::path &path, const cv::Mat &image) {
  if (image.empty() || image.type() != CV_16UC1 || image.rows <= 0 || image.cols <= 0) {
    return false;
  }
  const uint64_t nbytes = static_cast<uint64_t>(image.rows) * static_cast<uint64_t>(image.cols) * 2ull;
  if (nbytes == 0 || nbytes > kMaxTiffBytes) {
    return false;
  }
  constexpr int kEntries = 11;
  const uint32_t ifd = static_cast<uint32_t>(8 + nbytes);
  std::vector<uint8_t> bytes(static_cast<size_t>(ifd) + 2 + kEntries * 12 + 4, 0);
  bytes[0] = 'I';
  bytes[1] = 'I';
  Put16(bytes.data() + 2, 42);
  Put32(bytes.data() + 4, ifd);

  const cv::Mat continuous = image.isContinuous() ? image : image.clone();
  uint8_t *dst = bytes.data() + 8;
  for (int row = 0; row < continuous.rows; ++row) {
    const uint16_t *src = continuous.ptr<uint16_t>(row);
    for (int col = 0; col < continuous.cols; ++col) {
      const uint16_t value = src[col];
      *dst++ = static_cast<uint8_t>(value & 0xff);
      *dst++ = static_cast<uint8_t>((value >> 8) & 0xff);
    }
  }

  uint8_t *entry = bytes.data() + ifd;
  Put16(entry, kEntries);
  entry += 2;
  auto write_entry = [&](uint16_t tag, uint16_t type, uint32_t count, uint32_t value) {
    Put16(entry, tag);
    Put16(entry + 2, type);
    Put32(entry + 4, count);
    Put32(entry + 8, value);
    entry += 12;
  };
  constexpr uint16_t kShort = 3;
  constexpr uint16_t kLong = 4;
  write_entry(256, kLong, 1, static_cast<uint32_t>(image.cols));
  write_entry(257, kLong, 1, static_cast<uint32_t>(image.rows));
  write_entry(258, kShort, 1, 16);
  write_entry(259, kShort, 1, 1);
  write_entry(262, kShort, 1, 1);
  write_entry(273, kLong, 1, 8);
  write_entry(277, kShort, 1, 1);
  write_entry(278, kLong, 1, static_cast<uint32_t>(image.rows));
  write_entry(279, kLong, 1, static_cast<uint32_t>(nbytes));
  write_entry(284, kShort, 1, 1);
  write_entry(339, kShort, 1, 1);
  Put32(entry, 0);

  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    return false;
  }
  out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  out.flush();
  return static_cast<bool>(out);
}

bool TiffMatches(const std::filesystem::path &path, const cv::Mat &image) {
  cv::Mat stored;
  if (!ReadMono16Tiff(path, stored) || stored.size() != image.size()) {
    return false;
  }
  const cv::Mat continuous = image.isContinuous() ? image : image.clone();
  for (int row = 0; row < continuous.rows; ++row) {
    if (std::memcmp(stored.ptr(row), continuous.ptr(row), static_cast<size_t>(continuous.cols) * sizeof(uint16_t)) !=
        0) {
      return false;
    }
  }
  return true;
}

std::string ReadSopUid(const std::filesystem::path &path) {
  DcmFileFormat file;
  if (file.loadFile(path.string().c_str()).bad()) {
    return {};
  }
  OFString sop;
  if (file.getDataset()->findAndGetOFString(DCM_SOPInstanceUID, sop).bad()) {
    return {};
  }
  return std::string(sop.c_str());
}

bool FileHashOk(const std::filesystem::path &root, const std::string &relative, const std::string &hash) {
  std::filesystem::path full;
  if (hash.size() != 64 || !ResolveInside(root, relative, full)) {
    return false;
  }
  return Sha256FileHex(full) == hash;
}

int CountOrphans(const std::filesystem::path &root, const SessionManifest &manifest) {
  std::set<std::string> known;
  for (const SessionTileRecord &tile : manifest.tiles) {
    known.insert(tile.relative_path);
  }
  for (const SessionSegmentRecord &segment : manifest.segments) {
    known.insert(segment.relative_path);
  }
  int orphans = 0;
  for (const char *folder_name : {"tiles", "segments"}) {
    const std::filesystem::path folder = root / folder_name;
    std::error_code error;
    if (!std::filesystem::is_directory(folder, error) || error) {
      continue;
    }
    for (const auto &entry : std::filesystem::directory_iterator(folder, error)) {
      if (error || !entry.is_regular_file()) {
        continue;
      }
      const std::string name = entry.path().filename().string();
      if (name.size() >= 8 && name.compare(name.size() - 8, 8, ".partial") == 0) {
        continue;
      }
      const std::string relative = std::string(folder_name) + "/" + name;
      if (known.find(relative) == known.end()) {
        ++orphans;
      }
    }
  }
  return orphans;
}

bool Closed(SessionCommitStatus status) {
  return status == SessionCommitStatus::Complete || status == SessionCommitStatus::Fault;
}

} // namespace

std::string MakeSessionId() {
  std::random_device device;
  uint64_t high = (static_cast<uint64_t>(device()) << 32) ^ static_cast<uint64_t>(device());
  uint64_t low = (static_cast<uint64_t>(device()) << 32) ^ static_cast<uint64_t>(device());
  high ^= static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
  char buf[33];
  std::snprintf(buf, sizeof(buf), "%016llx%016llx", static_cast<unsigned long long>(high),
                static_cast<unsigned long long>(low));
  return std::string(buf);
}

std::string Sha256Hex(const void *data, size_t size) {
  Sha256Hasher hasher;
  if (size > 0 && data != nullptr) {
    hasher.Update(static_cast<const uint8_t *>(data), size);
  }
  uint8_t digest[32];
  hasher.Digest(digest);
  return Hex32(digest);
}

std::string Sha256FileHex(const std::filesystem::path &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return {};
  }
  Sha256Hasher hasher;
  char buffer[1 << 16];
  while (in) {
    in.read(buffer, sizeof(buffer));
    const std::streamsize got = in.gcount();
    if (got > 0) {
      hasher.Update(reinterpret_cast<const uint8_t *>(buffer), static_cast<size_t>(got));
    }
  }
  if (in.bad()) {
    return {};
  }
  uint8_t digest[32];
  hasher.Digest(digest);
  return Hex32(digest);
}

const char *SessionStatusName(SessionCommitStatus status) {
  switch (status) {
  case SessionCommitStatus::Empty:
    return "empty";
  case SessionCommitStatus::Open:
    return "open";
  case SessionCommitStatus::Partial:
    return "partial";
  case SessionCommitStatus::Complete:
    return "complete";
  case SessionCommitStatus::Fault:
    return "fault";
  }
  return "fault";
}

bool SessionStatusFromName(const std::string &name, SessionCommitStatus &status) {
  if (name == "open") {
    status = SessionCommitStatus::Open;
    return true;
  }
  if (name == "partial") {
    status = SessionCommitStatus::Partial;
    return true;
  }
  if (name == "complete") {
    status = SessionCommitStatus::Complete;
    return true;
  }
  if (name == "fault") {
    status = SessionCommitStatus::Fault;
    return true;
  }
  return false;
}

bool ReadMono16Tiff(const std::filesystem::path &path, cv::Mat &image) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (bytes.size() < 8 || bytes[0] != 'I' || bytes[1] != 'I' || Get16(bytes.data() + 2) != 42) {
    return false;
  }
  const uint32_t ifd = Get32(bytes.data() + 4);
  if (static_cast<uint64_t>(ifd) + 2 > bytes.size()) {
    return false;
  }
  const int count = Get16(bytes.data() + ifd);
  if (static_cast<uint64_t>(ifd) + 2 + static_cast<uint64_t>(count) * 12 + 4 > bytes.size()) {
    return false;
  }
  auto tag_value = [&](uint16_t tag, uint32_t &value) {
    for (int i = 0; i < count; ++i) {
      const uint8_t *entry = bytes.data() + ifd + 2 + static_cast<uint32_t>(i) * 12;
      if (Get16(entry) == tag) {
        value = Get32(entry + 8);
        return true;
      }
    }
    return false;
  };
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t bits = 0;
  uint32_t compression = 0;
  uint32_t photometric = 0;
  uint32_t offset = 0;
  uint32_t samples = 0;
  uint32_t byte_count = 0;
  if (!tag_value(256, width) || !tag_value(257, height) || !tag_value(258, bits) || !tag_value(259, compression) ||
      !tag_value(262, photometric) || !tag_value(273, offset) || !tag_value(277, samples) ||
      !tag_value(279, byte_count)) {
    return false;
  }
  if (width == 0 || height == 0 || bits != 16 || compression != 1 || photometric != 1 || samples != 1) {
    return false;
  }
  const uint64_t need = static_cast<uint64_t>(width) * static_cast<uint64_t>(height) * 2ull;
  if (need != byte_count || static_cast<uint64_t>(offset) + need > bytes.size()) {
    return false;
  }
  image.create(static_cast<int>(height), static_cast<int>(width), CV_16UC1);
  const uint8_t *src = bytes.data() + offset;
  for (uint32_t row = 0; row < height; ++row) {
    uint16_t *dst = image.ptr<uint16_t>(static_cast<int>(row));
    for (uint32_t col = 0; col < width; ++col) {
      dst[col] = Get16(src);
      src += 2;
    }
  }
  return true;
}

bool ReadSessionManifest(const std::filesystem::path &manifest_path, SessionManifest &manifest) {
  std::ifstream in(manifest_path, std::ios::binary);
  if (!in) {
    return false;
  }
  std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  Json json;
  SessionManifest parsed;
  if (!JsonParser(text).Parse(json) || !FillManifest(json, parsed)) {
    return false;
  }
  manifest = std::move(parsed);
  return true;
}

SessionStore::SessionStore(std::filesystem::path chosen_path, std::string session_id, ScanGeometry geometry,
                           std::string study_uid, std::string series_uid)
    : m_chosen(std::move(chosen_path)), m_stem(StemOf(m_chosen)) {
  const std::filesystem::path parent = m_chosen.parent_path().empty() ? std::filesystem::path(".") : m_chosen.parent_path();
  m_directory = parent / (m_stem + ".session-" + session_id);
  geometry = ScanGeometry::Normalized(geometry);
  m_manifest.format_version = kSessionFormatVersion;
  m_manifest.session_id = std::move(session_id);
  m_manifest.status = SessionCommitStatus::Open;
  m_manifest.base_path = m_chosen.string();
  m_manifest.study_uid = std::move(study_uid);
  m_manifest.series_uid = std::move(series_uid);
  m_manifest.bin = geometry.bin;
  m_manifest.pixel_um_x = geometry.pixel_um_x;
  m_manifest.pixel_um_y = geometry.pixel_um_y;
}

bool SessionStore::EnsureDirectory() {
  if (m_ready) {
    return true;
  }
  std::error_code error;
  if (std::filesystem::exists(m_directory, error) || error) {
    ScanLogError("SessionStore: directory already exists and is not reused: " + m_directory.string());
    return false;
  }
  const std::filesystem::path parent = m_directory.parent_path();
  if (parent.empty() || !std::filesystem::is_directory(parent, error) || error) {
    ScanLogError("SessionStore: parent directory is missing for " + m_chosen.string());
    return false;
  }

  struct Rollback {
    std::filesystem::path directory;
    bool armed = false;
    ~Rollback() {
      if (armed) {
        std::error_code ec;
        std::filesystem::remove_all(directory, ec);
      }
    }
  } rollback{m_directory, false};

  error.clear();
  if (!std::filesystem::create_directory(m_directory, error) || error) {
    return false;
  }
  rollback.armed = true;
  error.clear();
  if (!std::filesystem::create_directory(m_directory / "segments", error) || error) {
    return false;
  }
  error.clear();
  if (!std::filesystem::create_directory(m_directory / "tiles", error) || error) {
    return false;
  }
  if (!FsyncDirectory(m_directory / "segments") || !FsyncDirectory(m_directory / "tiles") ||
      !FsyncDirectory(m_directory) || !FsyncDirectory(parent)) {
    return false;
  }
  m_manifest.status = SessionCommitStatus::Open;
  if (!WriteManifest()) {
    return false;
  }
  if (!AppendJournal("{\"event\":\"directory_created\",\"session_id\":" + JsonString(m_manifest.session_id) + "}")) {
    return false;
  }
  rollback.armed = false;
  m_ready = true;
  return true;
}

bool SessionStore::WriteManifest() { return WriteSessionManifest(m_directory, m_manifest); }

bool SessionStore::AppendJournal(const std::string &object_json) {
  if (object_json.empty() || object_json.find('\n') != std::string::npos ||
      object_json.find('\r') != std::string::npos) {
    return false;
  }
  const std::filesystem::path path = m_directory / "journal.jsonl";
  std::error_code error;
  const bool existed = std::filesystem::exists(path, error);
  {
    std::ofstream out(path, std::ios::binary | std::ios::app);
    if (!out) {
      return false;
    }
    out << object_json << '\n';
    out.flush();
    if (!out) {
      return false;
    }
  }
  if (!FsyncFile(path)) {
    return false;
  }
  if (!existed && !FsyncDirectory(m_directory)) {
    return false;
  }
  return true;
}

std::string SessionStore::SegmentRelative(int instance) const {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "_%03d.dcm", instance);
  return std::string("segments/") + m_stem + buf;
}

bool SessionStore::RememberFile(const std::filesystem::path &final_path) {
  return FsyncFile(final_path) && FsyncDirectory(final_path.parent_path());
}

bool SessionStore::CommitTile(const cv::Mat &image, int x, int y, std::string &tile_id) {
  if (image.empty() || image.type() != CV_16UC1 || x < 0 || y < 0) {
    ScanLogError("SessionStore: tile must be a non-empty CV_16UC1 with a non-negative origin");
    return false;
  }
  if (!EnsureDirectory() || Closed(m_manifest.status)) {
    return false;
  }

  char id_buf[16];
  std::snprintf(id_buf, sizeof(id_buf), "t%04d", static_cast<int>(m_manifest.tiles.size()) + 1);
  const std::string id = id_buf;
  const std::string relative = std::string("tiles/") + id + ".tif";
  const std::filesystem::path final_path = m_directory / relative;
  const std::filesystem::path temp_path = m_directory / "tiles" / (id + ".tif.partial");
  std::error_code error;
  std::filesystem::remove(temp_path, error);
  if (!WriteMono16Tiff(temp_path, image)) {
    std::filesystem::remove(temp_path, error);
    ScanLogError("SessionStore: failed to write " + temp_path.string());
    return false;
  }
  if (!PublishNewFile(temp_path, final_path)) {
    const bool same = std::filesystem::exists(final_path, error) && !error && TiffMatches(final_path, image);
    std::filesystem::remove(temp_path, error);
    if (!same) {
      ScanLogError("SessionStore: failed to publish " + final_path.string());
      return false;
    }
  }
  if (!RememberFile(final_path)) {
    return false;
  }
  const std::string hash = Sha256FileHex(final_path);
  if (hash.size() != 64) {
    return false;
  }
  for (const SessionTileRecord &existing : m_manifest.tiles) {
    if (existing.relative_path == relative) {
      tile_id = existing.tile_id;
      return existing.sha256 == hash;
    }
  }

  if (!AppendJournal("{\"event\":\"tile_committed\",\"tile_id\":" + JsonString(id) +
                     ",\"path\":" + JsonString(relative) + ",\"sha256\":" + JsonString(hash) + "}")) {
    return false;
  }
  SessionTileRecord record;
  record.tile_id = id;
  record.relative_path = relative;
  record.sha256 = hash;
  record.x = x;
  record.y = y;
  record.rows = image.rows;
  record.cols = image.cols;
  m_manifest.tiles.push_back(record);
  if (!WriteManifest()) {
    m_manifest.tiles.pop_back();
    return false;
  }
  tile_id = id;
  return true;
}

bool SessionStore::CommitSegment(const cv::Mat &image, const DicomWriteParams &params, int y_origin,
                                 const std::vector<std::string> &source_tile_ids, std::string &out_path) {
  if (image.empty() || image.type() != CV_16UC1 || params.instance_number < 1 || y_origin < 0) {
    ScanLogError("SessionStore: segment is empty or its index is invalid");
    return false;
  }
  if (!EnsureDirectory() || Closed(m_manifest.status)) {
    return false;
  }

  const std::string relative = SegmentRelative(params.instance_number);
  const std::filesystem::path final_path = m_directory / relative;
  std::error_code error;
  if (std::filesystem::exists(final_path, error)) {
    if (error || !DicomFileMatches(final_path.string(), image, params)) {
      ScanLogError("SessionStore: refuse to overwrite " + final_path.string());
      return false;
    }
  } else if (!saveToDcm(image, final_path.string(), params)) {
    ScanLogError("SessionStore: failed to save " + final_path.string());
    return false;
  }
  if (!RememberFile(final_path)) {
    return false;
  }
  const std::string hash = Sha256FileHex(final_path);
  const std::string sop = ReadSopUid(final_path);
  if (hash.size() != 64 || sop.empty()) {
    return false;
  }
  for (const SessionSegmentRecord &existing : m_manifest.segments) {
    if (existing.instance == params.instance_number || existing.relative_path == relative) {
      out_path = final_path.string();
      return existing.sha256 == hash && existing.instance == params.instance_number;
    }
  }

  if (!AppendJournal("{\"event\":\"segment_committed\",\"instance\":" + std::to_string(params.instance_number) +
                     ",\"path\":" + JsonString(relative) + ",\"sha256\":" + JsonString(hash) + "}")) {
    return false;
  }
  SessionSegmentRecord record;
  record.instance = params.instance_number;
  record.relative_path = relative;
  record.sha256 = hash;
  record.sop_uid = sop;
  record.rows = image.rows;
  record.cols = image.cols;
  record.y_origin = y_origin;
  record.source_tile_ids = source_tile_ids;
  m_manifest.segments.push_back(record);
  if (!WriteManifest()) {
    m_manifest.segments.pop_back();
    return false;
  }
  out_path = final_path.string();
  return true;
}

bool SessionStore::HashesMatch() const {
  if (!std::filesystem::is_directory(m_directory)) {
    return false;
  }
  for (const SessionTileRecord &tile : m_manifest.tiles) {
    if (!FileHashOk(m_directory, tile.relative_path, tile.sha256)) {
      return false;
    }
  }
  for (const SessionSegmentRecord &segment : m_manifest.segments) {
    if (!FileHashOk(m_directory, segment.relative_path, segment.sha256)) {
      return false;
    }
  }
  return true;
}

bool SessionStore::SetStatus(SessionCommitStatus status) {
  if (!m_ready || status == SessionCommitStatus::Empty) {
    return false;
  }
  if (m_manifest.status == SessionCommitStatus::Complete && status != SessionCommitStatus::Complete) {
    return false;
  }
  if (status == SessionCommitStatus::Complete && (m_manifest.segments.empty() || !HashesMatch())) {
    return false;
  }
  if (m_manifest.status == status) {
    return true;
  }
  const SessionCommitStatus previous = m_manifest.status;
  m_manifest.status = status;
  if (!AppendJournal("{\"event\":\"status\",\"status\":" + JsonString(SessionStatusName(status)) + "}") ||
      !WriteManifest()) {
    m_manifest.status = previous;
    return false;
  }
  return true;
}

SessionRecovery SessionStore::Recover(const std::filesystem::path &session_dir) {
  SessionRecovery recovery;
  if (!ReadSessionManifest(session_dir / "manifest.json", recovery.manifest)) {
    recovery.status = SessionCommitStatus::Fault;
    return recovery;
  }
  recovery.manifest_read = true;
  for (const SessionTileRecord &tile : recovery.manifest.tiles) {
    if (FileHashOk(session_dir, tile.relative_path, tile.sha256)) {
      ++recovery.confirmed_tiles;
    } else {
      ++recovery.broken_records;
    }
  }
  for (const SessionSegmentRecord &segment : recovery.manifest.segments) {
    if (FileHashOk(session_dir, segment.relative_path, segment.sha256)) {
      ++recovery.confirmed_segments;
    } else {
      ++recovery.broken_records;
    }
  }
  recovery.orphan_files = CountOrphans(session_dir, recovery.manifest);

  const bool false_complete =
      recovery.manifest.status == SessionCommitStatus::Complete && recovery.manifest.segments.empty();
  if (recovery.broken_records > 0 || false_complete) {
    recovery.status = SessionCommitStatus::Fault;
    if (recovery.manifest.status != SessionCommitStatus::Fault) {
      recovery.manifest.status = SessionCommitStatus::Fault;
      WriteSessionManifest(session_dir, recovery.manifest);
    }
    return recovery;
  }
  recovery.status = recovery.manifest.status;
  return recovery;
}
