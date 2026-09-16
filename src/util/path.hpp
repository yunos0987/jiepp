#pragma once

#include <string>
#include <string_view>
#include <filesystem>

namespace Util {

std::string absolute_path(std::string_view path_text);

// Returns the canonical (symlink-resolved, OS-normalised) form of path_text.
// path_text must already be known to exist. On failure (e.g. a race where the
// file disappears between the existence check and this call), falls back to
// absolute_path()'s purely lexical normalisation.
std::string canonical_path(std::string_view path_text);

}
