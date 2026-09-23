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
    // If found_directory is non-null, it is set to true the first time a
    // candidate exists but is not a regular file (e.g. a directory); the
    // search continues over remaining candidates regardless.
    static std::string fullpath(const std::string& filepath, LoadType load_type, Env& env,
                                 bool* found_directory = nullptr);

    // Load tokens from file (with caching and comment stripping per env settings).
    // On a cache hit with comment removal off, the returned pointer aliases the
    // cached RAW token vector directly (no copy); otherwise a filtered copy is
    // materialised.
    static std::shared_ptr<const std::vector<Token>> tokens(const std::string& path, Env& env);
};
