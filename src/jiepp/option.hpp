#pragma once
#include "../env/issue.hpp"

#include <optional>
#include <string>
#include <vector>

enum class DepMode { NONE, ALL, USER };

// One per-code warning option (SPEC 13), kept in command-line order in
// JieppOptions::warnings. jiepp_command() folds the list front to back, so
// for the same code a later option wins.
enum class WarningAction {
    ENABLE,    // -W<name>: turn the warning on (no effect on a default-on code by itself)
    DISABLE,   // -Wno-<name>: do not report it ({#ignore <code>} from the start)
    PROMOTE,   // -Werror=<name>: turn it on and report it as an error
    DEMOTE,    // -Wno-error=<name>: keep it a warning under -Werror / -Werror=<name>
};

struct WarningOption {
    WarningAction action;
    Issue::Code code;   // a WARNING-severity code; the alias "undef" is PP54
};

struct JieppOptions {
    std::vector<std::string> input_filepaths;
    std::optional<std::string> output_filepath;
    std::vector<std::string> define_macros;   // e.g. "DEBUG" or "VERSION=10"
    std::vector<std::string> undef_macros;    // -U: macro names to undefine
    std::vector<std::string> include_filepaths;   // -include: files to force-include
    std::vector<std::string> syspaths;
    std::optional<int> max_include_depth;
    std::optional<int> max_expansion_depth;
    std::optional<int> max_expansion_steps;   // --max-expansion-steps: 0 = no limit
    std::optional<int> error_limit;           // --error-limit: 0 = no limit
    std::optional<int> max_include_count;     // --max-include-count: 0 = no limit
    std::optional<int> max_input_bytes;       // --max-input-bytes: 0 = no limit
    std::optional<int> max_if_nesting;
    std::optional<int> max_blank_lines;   // --max-blank-lines: 0 disables compaction
    std::optional<std::string> pp_output_pragma_style;
    std::optional<int> recursion_limit;
    bool remove_comments = false;
    bool no_line_markers = false;   // -P: suppress line markers in output
    bool dM = false;
    bool dD = false;                // -dD: emit {#define}/{#undef} lines inline
    bool MD = false;                // -MD:  dep side-effect + normal output (ALL deps)
    bool MMD = false;               // -MMD: dep side-effect + normal output (USER deps only)
    bool MP = false;                // -MP: an empty rule per prerequisite except the main input
    bool silent = false;
    bool suppress_warnings = false;  // -w: suppress warning output
    bool werror = false;             // -Werror / -Wno-error: promote warnings to errors (last one wins)
    std::vector<WarningOption> warnings;   // -W<name> / -Wno-<name> / -Werror=<name> / -Wno-error=<name>, in order
    DepMode dep_mode = DepMode::NONE;           // -M / -MM
    std::optional<std::string> dep_file;        // -MF: dependency output file
    std::optional<std::string> dep_target;      // -MT: dependency target name
    std::optional<std::string> disppath;
};

// Parse "-DNAME=value" or "-D NAME=value" → {name, value}
// "-DNAME" → {name, "1"}
std::pair<std::string, std::string> define_macro_option(const std::string& arg);

JieppOptions parse_args(int argc, char* argv[]);
