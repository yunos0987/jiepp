#pragma once
#include "lineno.hpp"

#include <optional>
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
    // Util::reached_path()); empty means filepath as given. system marks a
    // system header (found under an -isystem directory, or included from a
    // system header): -MM/-MMD leave it out, and what it includes is a
    // system header too.
    void        push_file(std::string filepath, bool from_disk = true, std::string reached_path = {},
                          bool system = false);
    void        pop_file();
    std::string current_file() const;
    // True when the innermost file is a system header (see push_file()).
    bool        current_file_is_system() const;
    // The innermost file's reached-by path ("" when no file is open): the
    // base of the reached-by path of what it includes. Unlike
    // current_file() it is not made absolute.
    const std::string& current_reached_path() const;
    // True when the innermost source was opened from the filesystem.
    bool        current_file_from_disk() const;
    int         num_of_files() const;
    // __INCLUDE_LEVEL__: the nesting depth of the innermost file (0 for the
    // outermost file) plus the base set below.
    int         include_level() const;
    // Files read while this is 1 sit one level deeper: the CLI sets it for
    // the -include files, which gcc/clang treat as included from the command
    // line (level 1, and 2 for what they include). 0 by default.
    void        set_include_level_base(int base) { include_level_base_ = base; }

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
                  std::string reached_path = {}, bool system = false)
            : fc_(fc) {
            fc_.push_file(std::move(filepath), from_disk, std::move(reached_path), system);
        }
        ~FileScope() { fc_.pop_file(); }
        FileScope(const FileScope&) = delete;
        FileScope& operator=(const FileScope&) = delete;
    private:
        FileContext& fc_;
    };

    // ---- Syspaths ----
    // reached_path is the directory as a reached-by path names it (a -I
    // path as given, a {#syspath} under its file's reached-by directory);
    // not given means syspath as given. It may be empty: {#syspath ''} in a
    // file whose own name has no directory names the working directory,
    // and a header found there is named by its operand alone.
    // These are the -I / {#syspath} directories, searched in the order added.
    void                            add_syspath(std::string syspath,
                                                std::optional<std::string> reached_path = std::nullopt);
    const std::vector<std::string>& syspaths() const;
    // Parallel to syspaths().
    const std::vector<std::string>& syspath_reached_paths() const;
    // Parallel to syspaths(): true for a directory that is also a system
    // directory (add_system_path()), whichever was added first. Like gcc/clang
    // (a directory given to both -I and -isystem), it is not searched at its
    // syspaths() position, only at its system one, as a system directory.
    const std::vector<char>&        syspath_shadowed() const;

    // ---- System directories (-isystem) ----
    // Searched after every syspaths() directory, in the order added (gcc/clang
    // search -isystem after -I whatever the order on the command line). A file
    // found under one is a system header (push_file()). The reached-by name of
    // the directory is the path as given, like -I.
    void                            add_system_path(std::string path);
    const std::vector<std::string>& system_paths() const;
    // Parallel to system_paths().
    const std::vector<std::string>& system_path_reached_paths() const;

    // ---- Line number (within current file) ----
    // item a: 64-bit storage, 32-bit unsigned wrap (see lineno.hpp) --
    // matches gcc/clang's line counter (after 4294967295 the next line is
    // 0). set_lineno() applies the wrap for its callers (advance_lineno(),
    // handle_setline()). push_file()/pop_file() assign lineno_ directly:
    // they store 1 and a value saved from lineno_, both already in range.
    LineNo get_lineno() const { return lineno_; }
    void   set_lineno(LineNo n) { lineno_ = wrap_lineno(n); }

    // ---- Dependency tracking (for -M / -MM) ----
    // add_dependency: display_path for output (Util::dependency_path() of
    // the reached-by path, or the caller's name for the file),
    // resolved_path for deduplication. expand() adds one entry per file and
    // is_system, at its first include (an include skipped by {#pragma once}
    // counts), and always one for the main input; a later include with the
    // same pair adds nothing (has_dependency()), which keeps what -M (the
    // first name of each file) and -MM (the first name outside a system
    // header) print.
    void add_dependency(std::string resolved_path, std::string display_path, bool is_system);
    // True when add_dependency() was called with this pair.
    bool has_dependency(const std::string& resolved_path, bool is_system) const;
    // Whether expand() records dependencies at all (default true). The CLI
    // turns it off when no -M/-MM/-MD/-MMD is given, so a run without
    // dependency output pays nothing per include for them.
    void set_record_dependencies(bool on) { record_deps_ = on; }
    bool records_dependencies() const { return record_deps_; }
    struct Dependency {
        std::string display_path;   // path to show in output
        std::string resolved_path;  // canonical/absolute path for deduplication
        bool is_system;             // a system header at this include (left out by -MM)
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
    std::vector<char>        system_stack_;     // parallel to file_stack_
    std::vector<LineNo>      lineno_stack_;
    std::vector<std::string> syspaths_;
    std::vector<std::string> syspath_reached_;  // parallel to syspaths_
    std::vector<char>        syspath_shadowed_; // parallel to syspaths_
    std::vector<std::string> system_paths_;
    std::vector<std::string> system_reached_;   // parallel to system_paths_
    LineNo lineno_ = 1;
    int include_level_base_ = 0;
    std::vector<Dependency> deps_;
    std::unordered_set<std::string> dep_seen_[2];  // resolved paths in deps_, by is_system
    bool record_deps_ = true;
    std::unordered_set<std::string> once_files_;  // files marked with {#pragma once}, by path text
    // The same files by identity (volume, file index), so another spelling of
    // the path (case, 8.3 name, symlink, hard link) is recognised too.
    std::set<std::pair<unsigned long long, unsigned long long>> once_ids_;
};
