#pragma once
#include <memory>
#include <string>
#include <vector>
#include "../env/env.hpp"
#include "token.hpp"

class Loader {
public:
    enum class LoadType { INCLUDE, SINCLUDE };

    // Resolve fullpath of a file (relative to current file or syspaths).
    // Returns empty string if not found.
    static std::string fullpath(const std::string& filepath, LoadType load_type, Env& env);

    // Load tokens from file (with caching and comment stripping per env settings).
    // On a cache hit with comment removal off, the returned pointer aliases the
    // cached RAW token vector directly (no copy); otherwise a filtered copy is
    // materialised.
    static std::shared_ptr<const std::vector<Token>> tokens(const std::string& path, Env& env);
};
