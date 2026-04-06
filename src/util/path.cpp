#include "path.hpp"

#include <filesystem>

namespace fs = std::filesystem;

std::string Util::absolute_path(std::string_view path_text) {
    fs::path p = std::string(path_text);
    if (p.is_absolute()) {
        return p.lexically_normal().generic_string();
    } else {
        fs::path a = fs::absolute(p);
        return a.lexically_normal().generic_string();
    }
}
