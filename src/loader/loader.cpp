#include "loader.hpp"
#include "../env/issue.hpp"
#include "../util/path.hpp"
#include "lexer.hpp"

#include <cstdio>
#ifdef _WIN32
#include <share.h>
#endif
#include <filesystem>
#include <istream>
#include <new>
#include <stdexcept>
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
    // A9: defense in depth. The filesystem calls below take C strings, so a
    // NUL would silently shorten the path; no file has a NUL in its name.
    // {#include} / {#syspath} reject such a path with PP47 before this, and
    // this makes __has_include answer 0 for it.
    if (filepath.find('\0') != std::string::npos)
        return "";
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
        // C11: read (and count) only on a cache miss; an open failure or a
        // read error is neither processed nor cached.
        std::optional<std::string> text = read_file(path, env);
        if (!text)
            return std::make_shared<const std::vector<Token>>();
        std::vector<Token> raw = iec3_tokens_from_string(*text, false, 1);
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

std::optional<std::string> Loader::read_file(const std::string& path, Env& env) {
    // Same narrow path string as the std::ifstream this replaces, binary
    // mode. On Windows the MSVC ifstream opens through the CRT's narrow
    // _Fiopen, which calls _fsopen(name, mode, _SH_DENYNO); the same call is
    // used here (the CRT headers also mark plain fopen as deprecated).
#ifdef _WIN32
    std::FILE* f = _fsopen(path.c_str(), "rb", _SH_DENYNO);
#else
    std::FILE* f = std::fopen(path.c_str(), "rb");
#endif
    if (!f) {
        ISSUE(FILE_ERROR, path);
        return std::nullopt;
    }
    struct Closer {
        std::FILE* f;
        ~Closer() { std::fclose(f); }
    } closer{f};

    // One chunk buffer per thread, reused by every file read: a run with
    // many small {#include} files then pays no 64 KiB allocation and
    // zero-fill per file. Nothing below can re-enter read_file().
    static thread_local std::unique_ptr<char[]> chunk(new char[READ_CHUNK]);
    std::string text;
    for (;;) {
        const std::size_t n = std::fread(chunk.get(), 1, READ_CHUNK, f);
        text.append(chunk.get(), n);
        // A short count is end of file or an error; ferror() tells which.
        if (n < READ_CHUNK && std::ferror(f)) {
            ISSUE(FILE_ERROR, path);
            return std::nullopt;
        }
        env.charge_input_bytes(n);
        if (n < READ_CHUNK)
            break;
    }
    return text;
}

std::optional<std::string> Loader::read_stream(std::istream& input, Env& env,
                                               const std::string& context,
                                               bool check_stdio_stdin) {
    std::string text;
    std::streambuf* sb = input.rdbuf();
    if (!sb)
        return text;
    // A per-call chunk buffer (not zero-filled): the caller's streambuf may
    // run arbitrary code, so a shared buffer could be overwritten by a
    // nested call.
    std::unique_ptr<char[]> chunk(new char[READ_CHUNK]);
    for (;;) {
        std::streamsize n = 0;
        bool failed = false;
        try {
            n = sb->sgetn(chunk.get(), static_cast<std::streamsize>(READ_CHUNK));
        } catch (const std::bad_alloc&) {
            throw;
        } catch (const std::exception&) {
            // Not catch (...): glibc's forced unwind must pass through.
            failed = true;
        }
        if (n < 0)
            n = 0;
        // Outside the try above: std::bad_alloc here propagates as before.
        text.append(chunk.get(), static_cast<std::size_t>(n));
        if (failed) {
            ISSUE(FILE_ERROR, context);
            return std::nullopt;
        }
        env.charge_input_bytes(static_cast<std::uint64_t>(n));
        // End of input is sgetn() returning 0: a streambuf may hand out
        // fewer bytes than asked for without being at the end.
        if (n == 0)
            break;
    }
    if (check_stdio_stdin && std::ferror(stdin)) {
        ISSUE(FILE_ERROR, context);
        return std::nullopt;
    }
    return text;
}
