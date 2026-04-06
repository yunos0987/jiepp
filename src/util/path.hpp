#pragma once

#include <string>
#include <string_view>
#include <filesystem>

namespace Util {

std::string absolute_path(std::string_view path_text);

}
