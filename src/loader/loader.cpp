#include "loader.hpp"
#include "../env/issue.hpp"
#include "../util/path.hpp"
#include "lexer.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

std::string Loader::fullpath(const std::string& filepath, LoadType load_type, Env& env) {
    fs::path p(filepath);
    if (p.is_absolute()) {
        std::error_code ec;
        if (fs::exists(p, ec))
            return Util::canonical_path(p.generic_string());
    } else {
        if (load_type == LoadType::INCLUDE) {
            const std::string& cur_file = env.current_file();
            if (!cur_file.empty()) {
                fs::path base_dirpath = fs::path(cur_file).parent_path();
                fs::path candidate = base_dirpath / filepath;
                std::error_code ec;
                if (fs::exists(candidate, ec))
                    return Util::canonical_path(candidate.generic_string());
            } else {
                fs::path candidate = fs::current_path() / p;
                std::error_code ec;
                if (fs::exists(candidate, ec))
                    return Util::canonical_path(candidate.generic_string());
            }
        }
        for (const auto& sp : env.syspaths()) {
            fs::path candidate = fs::path(sp) / filepath;
            std::error_code ec;
            if (fs::exists(candidate, ec))
                return Util::canonical_path(candidate.generic_string());
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
