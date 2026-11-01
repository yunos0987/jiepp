#pragma once

#include <string>
#include <string_view>

namespace Util {

// IEC string encoding/decoding
std::string encode_iec_string(std::string_view raw, const char quote = '\'');

// File name inside a line marker ({#:N '...'} / (*{#:N '...'}*)): as
// encode_iec_string with a single quote, except that "$", "{" and "}" are
// written as "$24", "$7B" and "$7D", so the marker text never contains a raw
// brace and a reader can end the pragma at the first "}".
std::string encode_line_marker_path(std::string_view raw);

}
