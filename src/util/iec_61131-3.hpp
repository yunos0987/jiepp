#pragma once

#include <string>
#include <string_view>

namespace Util {

// IEC string encoding/decoding
std::string encode_iec_string(std::string_view raw, const char quote = '\'');

}
