#include "loader.hpp"
#include "../env/issue.hpp"
#include "../util/path.hpp"
#include "lexer.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

// Returns true (and canonicalises into `out`) only when `candidate` exists
// and is a regular file. When it exists but is not a regular file (e.g. a
// directory), records that fact into `*found_directory` (if non-null,
// without clearing an already-set flag) and reports no match, so callers
// keep searching remaining candidates.
bool try_resolve(const fs::path& candidate, bool* found_directory, std::string& out) {
    std::error_code ec;
    if (!fs::exists(candidate, ec))
        return false;
    if (!fs::is_regular_file(candidate, ec)) {
        if (found_directory)
            *found_directory = true;
        return false;
    }
    out = Util::canonical_path(candidate.generic_string());
    return true;
}

} // namespace

std::string Loader::fullpath(const std::string& filepath, LoadType load_type, Env& env,
                              bool* found_directory) {
    fs::path p(filepath);
    std::string out;
    if (p.is_absolute()) {
        if (try_resolve(p, found_directory, out))
            return out;
    } else {
        if (load_type == LoadType::INCLUDE) {
            const std::string& cur_file = env.current_file();
            if (!cur_file.empty()) {
                fs::path base_dirpath = fs::path(cur_file).parent_path();
                fs::path candidate = base_dirpath / filepath;
                if (try_resolve(candidate, found_directory, out))
                    return out;
            } else {
                fs::path candidate = fs::current_path() / p;
                if (try_resolve(candidate, found_directory, out))
                    return out;
            }
        }
        for (const auto& sp : env.syspaths()) {
            fs::path candidate = fs::path(sp) / filepath;
            if (try_resolve(candidate, found_directory, out))
                return out;
        }
    }
    return "";
}

std::shared_ptr<const std::vector<Token>> Loader::tokens(const std::string& path, Env& env) {
    std::shared_ptr<const std::vector<Token>> cached = env.get_cache(path);
    if (!cached) {
        std::ifstream f(path, std::ios::binary);
        if (!f) {
            ISSUE(FILE_ERROR, path);
            return std::make_shared<const std::vector<Token>>();
        }
        std::vector<Token> raw = iec3_tokens(f, false, 1);
        env.set_cache(path, std::move(raw));
        cached = env.get_cache(path);
    }
    if (!env.get_remove_comments())
        return cached;

    // Comment removal is a runtime-mutable setting (Preprocessor::set_remove_comments()
    // can flip it mid-run), so the cache always stores RAW tokens; materialise a
    // filtered copy here instead of baking the filter into the cached entry.
    auto filtered = std::make_shared<std::vector<Token>>(*cached);
    for (auto& t : *filtered)
        if (t.type == Token::C)
            t = (t.num_of_lines > 0) ? Token::newline(t.num_of_lines) : Token::create(Token::WS, " ");
    return filtered;
}
