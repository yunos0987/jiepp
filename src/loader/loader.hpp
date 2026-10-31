#pragma once
#include <cstddef>
#include <iosfwd>
#include <memory>
#include <optional>
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

    // C11: the bounded readers of every source the preprocessor reads. Both
    // read in chunks of READ_CHUNK bytes and charge each chunk to the input
    // size budget (Env::charge_input_bytes(), PP67), so reading stops at
    // most one chunk after the limit. A read error is PP10 at the current
    // Issue location; if a caller unblocked PP10 the source reads as empty
    // (std::nullopt), and the partly read text is never returned.
    static constexpr std::size_t READ_CHUNK = 64 * 1024;

    // Reads a file through std::FILE (fopen "rb", fread, ferror, fclose) on
    // every OS: an MSVC std::ifstream reports a read error only as end of
    // file. An open failure is PP10 `path` (as before); a read error is
    // PP10 `path` too.
    static std::optional<std::string> read_file(const std::string& path, Env& env);

    // Reads `input` through its streambuf (rdbuf()->sgetn) until sgetn
    // returns 0, ignoring the stream state (a stream with eofbit or failbit
    // still reads from its buffer; a null rdbuf reads nothing), as 1.0.0's
    // istreambuf_iterator did. A std::exception thrown by the streambuf
    // (other than std::bad_alloc) is a read error: PP10 `context`. Only the
    // sgetn call is inside the try, so std::bad_alloc while growing the
    // text propagates as before. With check_stdio_stdin, std::ferror(stdin)
    // is also a read error (libstdc++'s cin is synced with stdio and sets no
    // badbit on a read error). A streambuf that reports an error only as end
    // of input cannot be detected.
    static std::optional<std::string> read_stream(std::istream& input, Env& env,
                                                  const std::string& context,
                                                  bool check_stdio_stdin = false);
};
