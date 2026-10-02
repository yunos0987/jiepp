#include "file_context.hpp"

#include "../util/path.hpp"

#include <utility>

// ---------------------------------------------------------------------------
// Include stack
// ---------------------------------------------------------------------------

void FileContext::push_file(std::string filepath, bool from_disk) {
    lineno_stack_.push_back(lineno_);
    const std::string p(Util::absolute_path(filepath));
    file_stack_.push_back(p);
    from_disk_stack_.push_back(from_disk ? 1 : 0);
    lineno_ = 1;
}

void FileContext::pop_file() {
    if (!file_stack_.empty() && !lineno_stack_.empty()) {
        file_stack_.pop_back();
        from_disk_stack_.pop_back();
        lineno_ = lineno_stack_.back();
        lineno_stack_.pop_back();
    }
}

std::string FileContext::current_file() const {
    if (!file_stack_.empty())
        return file_stack_.back();
    return "";
}

bool FileContext::current_file_from_disk() const {
    return !from_disk_stack_.empty() && from_disk_stack_.back() != 0;
}

int FileContext::num_of_files() const {
    return static_cast<int>(file_stack_.size());
}

int FileContext::include_level() const {
    int n = static_cast<int>(file_stack_.size());
    return n > 0 ? n - 1 : 0;
}

// ---------------------------------------------------------------------------
// Syspaths
// ---------------------------------------------------------------------------

void FileContext::add_syspath(std::string syspath) {
    const std::string p(Util::absolute_path(syspath));
    syspaths_.push_back(p);
}

const std::vector<std::string>& FileContext::syspaths() const {
    return syspaths_;
}

// ---------------------------------------------------------------------------
// Dependency tracking
// ---------------------------------------------------------------------------

void FileContext::add_dependency(std::string resolved_path, std::string display_path, bool is_system) {
    deps_.emplace_back(Dependency{std::move(display_path), std::move(resolved_path), is_system});
}

// ---------------------------------------------------------------------------
// pragma once
// ---------------------------------------------------------------------------

// A file is identified by what it is on disk (Util::file_id), not by how its
// path is spelled: the current file is pushed with a lexically normalised
// path, while an {#include} resolves to a canonical one, and the two differ
// for a different letter case, an 8.3 short name, a symlink or a hard link.
// The path text stays as the fallback for a file whose identity cannot be
// read. The identity is read once per recorded file, and once per include
// only while some file carries {#pragma once}.
void FileContext::record_pragma_once(const std::string& absolute_path, bool from_disk) {
    // A pseudo name such as "<stdin>" is only a label, and nothing can
    // {#include} it. Recording it would let an unrelated file of that name in
    // the working directory match, by its identity or just by its path text,
    // so it is not recorded at all.
    if (!from_disk)
        return;
    once_files_.insert(absolute_path);
    Util::FileId id;
    if (Util::file_id(absolute_path, id))
        once_ids_.insert({id.volume, id.index});
}

bool FileContext::is_pragma_once_seen(const std::string& absolute_path) const {
    if (once_files_.count(absolute_path) > 0)
        return true;
    if (once_ids_.empty())
        return false;
    Util::FileId id;
    return Util::file_id(absolute_path, id) && once_ids_.count({id.volume, id.index}) > 0;
}
