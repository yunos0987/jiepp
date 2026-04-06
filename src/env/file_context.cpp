#include "file_context.hpp"

#include "../util/path.hpp"

#include <utility>

// ---------------------------------------------------------------------------
// Include stack
// ---------------------------------------------------------------------------

void FileContext::push_file(std::string filepath) {
    lineno_stack_.push_back(lineno_);
    const std::string p(Util::absolute_path(filepath));
    file_stack_.push_back(p);
    lineno_ = 1;
}

void FileContext::pop_file() {
    if (!file_stack_.empty() && !lineno_stack_.empty()) {
        file_stack_.pop_back();
        lineno_ = lineno_stack_.back();
        lineno_stack_.pop_back();
    }
}

std::string FileContext::current_file() const {
    if (!file_stack_.empty())
        return file_stack_.back();
    return "";
}

int FileContext::num_of_files() const {
    return static_cast<int>(file_stack_.size());
}

bool FileContext::contains_file(const std::string& filepath) const {
    const std::string p(Util::absolute_path(filepath));
    for (const auto& f : file_stack_)
        if (f == p)
            return true;
    return false;
}

std::string FileContext::base_file() const {
    if (file_stack_.size() >= 2)
        return file_stack_[1];
    return "";
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
