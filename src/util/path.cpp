#include "path.hpp"

#include <algorithm>
#include <filesystem>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <locale.h>  // ___lc_codepage_func(): the CRT locale's code page
#else
#include <sys/stat.h>
#endif

namespace fs = std::filesystem;

bool Util::narrow_paths_are_utf8() {
#ifdef _WIN32
    return GetACP() == CP_UTF8 || ___lc_codepage_func() == CP_UTF8;
#else
    return true;
#endif
}

std::string Util::absolute_path(std::string_view path_text) {
    fs::path p = std::string(path_text);
    if (p.is_absolute()) {
        return p.lexically_normal().generic_string();
    } else {
        fs::path a = fs::absolute(p);
        return a.lexically_normal().generic_string();
    }
}

std::string Util::canonical_path(std::string_view path_text) {
    fs::path p = std::string(path_text);
    std::error_code ec;
    fs::path c = fs::canonical(p, ec);
    if (!ec) {
        return c.generic_string();
    }
    return Util::absolute_path(path_text);
}

bool Util::make_file_id(unsigned long long volume, unsigned long long index, FileId& out) {
    if (index == 0)
        return false;
    out.volume = volume;
    out.index  = index;
    return true;
}

bool Util::file_id(std::string_view path_text, FileId& out) {
#ifdef _WIN32
    // Access 0 + full sharing: only the file information is queried, so this
    // works even for a file another process holds open. BACKUP_SEMANTICS lets
    // the same call open a directory.
    HANDLE h = CreateFileW(fs::path(std::string(path_text)).c_str(), 0,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    BY_HANDLE_FILE_INFORMATION info;
    const bool ok = GetFileInformationByHandle(h, &info) != 0;
    CloseHandle(h);
    if (!ok)
        return false;
    return make_file_id(info.dwVolumeSerialNumber,
                        (static_cast<unsigned long long>(info.nFileIndexHigh) << 32) | info.nFileIndexLow,
                        out);
#else
    struct stat st;
    if (::stat(std::string(path_text).c_str(), &st) != 0)
        return false;
    return make_file_id(static_cast<unsigned long long>(st.st_dev),
                        static_cast<unsigned long long>(st.st_ino), out);
#endif
}

// e.g. ("tests/in", "./a.iec") -> "tests/in/a.iec"
std::string Util::reached_path(std::string_view dir, std::string_view name)
{
    const fs::path n{std::string(name)};
    const std::string joined = (dir.empty() || n.is_absolute())
        ? n.generic_string()
        : (fs::path{std::string(dir)} / n).generic_string();
    // A path starting with "//" (UNC, or the "//./" and "//?/" device
    // prefixes, where "." is not a directory) stays as written.
    if (joined.starts_with("//"))
        return joined;
    // Drop "." components, and the empty components right after a dropped
    // one (".//a" is "a", not the rooted "/a"); every other component,
    // ".." included, stays as written.
    std::string r;
    r.reserve(joined.size());
    bool after_dropped_dot = false;
    std::size_t i = 0;
    while (i <= joined.size()) {
        const std::size_t slash = std::min(joined.find('/', i), joined.size());
        const std::string_view part(joined.data() + i, slash - i);
        if (part == "." || (after_dropped_dot && part.empty())) {
            after_dropped_dot = true;
        } else {
            after_dropped_dot = false;
            r += part;
            if (slash < joined.size())
                r += '/';
        }
        i = slash + 1;
    }
    if (r.empty())
        return ".";
    // "a/." leaves "a/": the directory itself, without the separator --
    // unless that separator is the root directory ("/.", "C:/.").
    if (r.back() == '/' && !joined.ends_with('/') && !fs::path(r).relative_path().empty())
        r.pop_back();
    return r;
}

// e.g. "tests/in/main.iec" -> "tests/in"
std::string Util::reached_dir(std::string_view path)
{
    return fs::path{std::string(path)}.parent_path().generic_string();
}
