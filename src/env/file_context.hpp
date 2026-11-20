#pragma once
#include "lineno.hpp"

#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

class FileContext {
public:
    FileContext() = default;
    virtual ~FileContext() = default;
    FileContext(FileContext&&) noexcept = default;
    FileContext& operator=(FileContext&&) noexcept = default;

    // ---- Include stack ----
    // from_disk is false for a pseudo name such as "<stdin>": the text is
    // only a label, so it must never be looked up in the filesystem.
    // reached_path is the path the file was reached by (see
    // Util::reached_path()); empty means filepath as given.
    void        push_file(std::string filepath, bool from_disk = true, std::string reached_path = {});
    void        pop_file();
    std::string current_file() const;
    // The innermost file's reached-by path ("" when no file is open): the
    // base of the dependency path of what it includes. Unlike
    // current_file() it is not made absolute.
    const std::string& current_reached_path() const;
    // True when the innermost source was opened from the filesystem.
    bool        current_file_from_disk() const;
    int         num_of_files() const;
    int         include_level() const;

    // item f: RAII guard for the include stack -- push_file() on
    // construction, pop_file() on destruction, so a file-processing
    // function's entry is popped even if an exception (e.g. an ERROR
    // raised somewhere inside that file, in library/throw mode) unwinds
    // through it. Used by expand()'s file-inclusion wrapper
    // (core/expand.cpp) and jiepp.cpp's stdin branch, both of which pair
    // it with the matching Issue::LineGuard -- declare the FileScope
    // first, then the LineGuard, so construction/destruction order
    // matches the pre-RAII manual push_file()+Issue::push() /
    // Issue::pop()+pop_file() pairs exactly (file pushed first, popped
    // last).
    struct FileScope {
        FileScope(FileContext& fc, std::string filepath, bool from_disk = true,
                  std::string reached_path = {})
            : fc_(fc) {
            fc_.push_file(std::move(filepath), from_disk, std::move(reached_path));
        }
        ~FileScope() { fc_.pop_file(); }
        FileScope(const FileScope&) = delete;
        FileScope& operator=(const FileScope&) = delete;
    private:
        FileContext& fc_;
    };

    // ---- Syspaths ----
    // reached_path is the directory as a dependency path names it (a -I
    // path as given, a {#syspath} under its file's reached-by directory);
    // empty means syspath as given.
    void                            add_syspath(std::string syspath, std::string reached_path = {});
    const std::vector<std::string>& syspaths() const;
    // Parallel to syspaths().
    const std::vector<std::string>& syspath_reached_paths() const;

    // ---- Line number (within current file) ----
    // item a: 64-bit storage, 32-bit unsigned wrap (see lineno.hpp) --
    // matches gcc/clang's line counter (after 4294967295 the next line is
    // 0). set_lineno() applies the wrap for its callers (advance_lineno(),
    // handle_setline()). push_file()/pop_file() assign lineno_ directly:
    // they store 1 and a value saved from lineno_, both already in range.
    LineNo get_lineno() const { return lineno_; }
    void   set_lineno(LineNo n) { lineno_ = wrap_lineno(n); }

    // ---- Dependency tracking (for -M / -MM) ----
    // add_dependency: display_path for output (the reached-by path, which
    // opens the file from the working directory, or the caller's name for
    // it), resolved_path for deduplication
    void add_dependency(std::string resolved_path, std::string display_path, bool is_system);
    struct Dependency {
        std::string display_path;   // path to show in output
        std::string resolved_path;  // canonical/absolute path for deduplication
        bool is_system;             // true for sinclude, false for include
    };
    const std::vector<Dependency>& dependencies() const { return deps_; }

    // ---- pragma once (for {#pragma once}) ----
    // Record that a file (by its absolute path) has been seen with {#pragma once}.
    // Only a source opened from the filesystem (from_disk) is recorded, by path
    // text and by on-disk identity; a pseudo source (stdin, in-memory) is not,
    // since its name could collide with an unrelated real file.
    void record_pragma_once(const std::string& absolute_path, bool from_disk = true);
    // Returns true if the file was previously marked with {#pragma once}.
    bool is_pragma_once_seen(const std::string& absolute_path) const;

private:
    std::vector<std::string> file_stack_;
    std::vector<char>        from_disk_stack_;  // parallel to file_stack_
    std::vector<std::string> reached_stack_;    // parallel to file_stack_
    std::vector<LineNo>      lineno_stack_;
    std::vector<std::string> syspaths_;
    std::vector<std::string> syspath_reached_;  // parallel to syspaths_
    LineNo lineno_ = 1;
    std::vector<Dependency> deps_;
    std::unordered_set<std::string> once_files_;  // files marked with {#pragma once}, by path text
    // The same files by identity (volume, file index), so another spelling of
    // the path (case, 8.3 name, symlink, hard link) is recognised too.
    std::set<std::pair<unsigned long long, unsigned long long>> once_ids_;
};
