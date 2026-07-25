#pragma once
#include <string>
#include <unordered_set>
#include <vector>

class FileContext {
public:
    FileContext() = default;
    virtual ~FileContext() = default;
    FileContext(FileContext&&) noexcept = default;
    FileContext& operator=(FileContext&&) noexcept = default;

    // ---- Include stack ----
    void        push_file(std::string filepath);
    void        pop_file();
    std::string current_file() const;
    int         num_of_files() const;
    bool        contains_file(const std::string& filepath) const; // for cycle detection
    std::string base_file() const;
    int         include_level() const;

    // ---- Syspaths ----
    void                            add_syspath(std::string syspath);
    const std::vector<std::string>& syspaths() const;

    // ---- Line number (within current file) ----
    int  get_lineno() const { return lineno_; }
    void set_lineno(int n) { lineno_ = n; }

    // ---- Dependency tracking (for -M / -MM) ----
    // add_dependency: display_path for output, resolved_path for deduplication
    void add_dependency(std::string resolved_path, std::string display_path, bool is_system);
    struct Dependency {
        std::string display_path;   // path to show in output (original or CLI string)
        std::string resolved_path;  // canonical/absolute path for deduplication
        bool is_system;             // true for sinclude, false for include
    };
    const std::vector<Dependency>& dependencies() const { return deps_; }

    // ---- pragma once (for {#pragma once}) ----
    // Record that a file (by its absolute path) has been seen with {#pragma once}.
    void record_pragma_once(const std::string& absolute_path);
    // Returns true if the file was previously marked with {#pragma once}.
    bool is_pragma_once_seen(const std::string& absolute_path) const;

private:
    std::vector<std::string> file_stack_;
    std::vector<int>         lineno_stack_;
    std::vector<std::string> syspaths_;
    int lineno_ = 1;
    std::vector<Dependency> deps_;
    std::unordered_set<std::string> once_files_;  // files marked with {#pragma once}
};
