#pragma once
#include <optional>
#include <string>
#include <vector>

enum class DepMode { NONE, ALL, USER };

struct JieppOptions {
    std::vector<std::string> input_filepaths;
    std::optional<std::string> output_filepath;
    std::vector<std::string> define_macros;   // e.g. "DEBUG" or "VERSION=10"
    std::vector<std::string> undef_macros;    // -U: macro names to undefine
    std::vector<std::string> include_filepaths;   // -include: files to force-include
    std::vector<std::string> syspaths;
    std::optional<int> max_include_depth;
    std::optional<int> max_expansion_depth;
    std::optional<int> max_if_nesting;
    std::optional<std::string> pp_output_pragma_style;
    std::optional<int> recursion_limit;
    bool remove_comments = false;
    bool dM = false;
    bool silent = false;
    bool suppress_warnings = false;  // -w: suppress warning output
    bool werror = false;             // -Werror: promote warnings to errors
    DepMode dep_mode = DepMode::NONE;           // -M / -MM
    std::optional<std::string> dep_file;        // -MF: dependency output file
    std::optional<std::string> dep_target;      // -MT: dependency target name
    std::optional<std::string> disppath;
};

// Parse "-DNAME=value" or "-D NAME=value" → {name, value}
// "-DNAME" → {name, "1"}
std::pair<std::string, std::string> define_macro_option(const std::string& arg);

JieppOptions parse_args(int argc, char* argv[]);
