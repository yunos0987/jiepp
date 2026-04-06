#pragma once

#include <string>
#include <string_view>

namespace Util {

std::string_view ltrim_view(std::string_view s);
std::string_view rtrim_view(std::string_view s);
std::string_view trim_view(std::string_view s);

} // namespace Util
