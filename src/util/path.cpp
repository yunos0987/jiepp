#include "path.hpp"

#include <filesystem>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/stat.h>
#endif

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

std::string Util::canonical_path(std::string_view path_text) {
    fs::path p = std::string(path_text);
    std::error_code ec;
    fs::path c = fs::canonical(p, ec);
    if (!ec) {
        return c.generic_string();
    }
    return Util::absolute_path(path_text);
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
    out.volume = info.dwVolumeSerialNumber;
    out.index  = (static_cast<unsigned long long>(info.nFileIndexHigh) << 32) | info.nFileIndexLow;
    return true;
#else
    struct stat st;
    if (::stat(std::string(path_text).c_str(), &st) != 0)
        return false;
    out.volume = static_cast<unsigned long long>(st.st_dev);
    out.index  = static_cast<unsigned long long>(st.st_ino);
    return true;
#endif
}
