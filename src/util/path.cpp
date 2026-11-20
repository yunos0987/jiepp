#include "path.hpp"

#include <cctype>
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

namespace {

// The path text with '/' as the only separator: a backslash becomes '/' on
// Windows (where both separate), and nothing changes elsewhere (a backslash
// is an ordinary file-name character). Nothing else is touched: a run of '/',
// "." and ".." stay as written, which std::filesystem::path would not keep
// (libstdc++ joins a run of '/' into one).
std::string with_slashes(std::string_view text) {
    std::string s(text);
#ifdef _WIN32
    for (char& c : s)
        if (c == '\\')
            c = '/';
#endif
    return s;
}

}

// e.g. ("tests/in", "./a.iec") -> "tests/in/./a.iec"
std::string Util::reached_path(std::string_view dir, std::string_view name)
{
    std::string n = with_slashes(name);
    // A name with a root (an absolute path; on Windows also "/x" and "C:x")
    // stands alone, as it does for path::operator/.
    const fs::path np{std::string(name)};
    if (dir.empty() || np.is_absolute() || np.has_root_name() || np.has_root_directory())
        return n;
    std::string d = with_slashes(dir);
    // Join with one '/', unless dir already ends with one ("lib/" and
    // "lib//" are kept as they are written).
    if (!d.ends_with('/'))
        d += '/';
    return d + n;
}

// e.g. "././x.iec" -> "x.iec"
std::string Util::dependency_path(std::string_view reached)
{
    while (reached.starts_with("./")) {
        reached.remove_prefix(2);
        while (reached.starts_with('/'))
            reached.remove_prefix(1);
    }
    return std::string(reached);
}

// e.g. "tests/in/main.iec" -> "tests/in"
std::string Util::reached_dir(std::string_view path)
{
    // Everything before the last '/', as written. A run of '/' is kept whole
    // ("lib//a.iec" gives "lib//"): reached_path() adds no '/' after a
    // directory that ends with one, so a file next to it is named "lib//c.iec",
    // as gcc/clang name it.
    const std::string p = with_slashes(path);
    const std::size_t slash = p.rfind('/');
    if (slash == std::string::npos) {
#ifdef _WIN32
        // "C:a.iec" names a file relative to drive C's directory.
        if (p.size() >= 2 && p[1] == ':' && std::isalpha(static_cast<unsigned char>(p[0])))
            return p.substr(0, 2);
#endif
        return {};
    }
    // "/a.iec" is in the root, which is "/" and not "".
    if (slash == 0)
        return "/";
    return p.substr(0, p[slash - 1] == '/' ? slash + 1 : slash);
}
