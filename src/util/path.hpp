#pragma once

#include <string>
#include <string_view>

namespace Util {

std::string absolute_path(std::string_view path_text);

// True when a narrow (char) path string is read as UTF-8, so a file path in a
// diagnostic can be treated like any other UTF-8 text. Always true off
// Windows. On Windows it is true when the process ANSI code page is UTF-8
// (jiepp.exe with its activeCodePage manifest on Windows 10 1903+) or the
// CRT locale's code page is UTF-8 (MSVC STL converts narrow paths with the
// CRT locale's code page, which a library caller may set with setlocale).
// Evaluated at every call and never cached, because the CRT locale can change
// while the process runs.
bool narrow_paths_are_utf8();

// Returns the canonical (symlink-resolved, OS-normalised) form of path_text.
// path_text must already be known to exist. On failure (e.g. a race where the
// file disappears between the existence check and this call), falls back to
// absolute_path()'s purely lexical normalisation.
std::string canonical_path(std::string_view path_text);

// The path a file was reached by: `name` as written, under the directory
// `dir` it was found in (the including file's own reached-by path minus
// its file name, or a system path as given), like gcc/clang. It is the
// name shown in line markers, __FILE__ and diagnostics, and it opens the
// file from the working directory. The two are joined with '/' and an
// absolute `name` stands alone; "." and ".." components stay, as in
// gcc/clang. The text goes through std::filesystem::path, so on Windows
// a '\\' becomes '/', and with libstdc++ a run of '/' becomes one.
// e.g. ("tests/in", "./a.iec") -> "tests/in/./a.iec"; ("", "a.iec") -> "a.iec"
std::string reached_path(std::string_view dir, std::string_view name);

// A reached-by path as a prerequisite of a dependency rule (-M ...): the
// leading "./" runs are dropped, like gcc/clang; the rest is kept.
// e.g. "././x.iec" -> "x.iec"; "./lib/./t.iec" -> "lib/./t.iec"
std::string dependency_path(std::string_view reached);

// The directory part of a reached-by path ("" when it has none).
// e.g. "tests/in/main.iec" -> "tests/in"; "./main.iec" -> "."; "main.iec" -> ""
std::string reached_dir(std::string_view path);

// Identity of a file on disk, independent of how its path is spelled: (volume
// serial number, file index) on Windows, (st_dev, st_ino) on POSIX. Two paths
// that name the same file (different case, "..", an 8.3 short name, a symlink
// or a hard link) have the same FileId. gcc/clang recognise {#pragma once}
// files by identity for the same reason.
struct FileId {
    unsigned long long volume = 0;
    unsigned long long index  = 0;
    bool operator==(const FileId&) const = default;
};

// Returns false when path_text cannot be opened/stat'ed (then `out` is unset),
// and when the file system reports no usable identity: a zero file index /
// inode (some file systems return 0 for every file, which would make all of
// them look like one file) counts as "identity unavailable", so callers fall
// back to comparing path text.
bool file_id(std::string_view path_text, FileId& out);

// The validity rule of file_id(), split out so it can be tested without a file
// system that reports a zero index: stores the pair in `out` and returns true
// unless `index` is 0.
bool make_file_id(unsigned long long volume, unsigned long long index, FileId& out);

}
