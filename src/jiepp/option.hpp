#pragma once
#include "../env/issue.hpp"

#include <optional>
#include <string>
#include <utility>
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

// One -D or -U, kept in command-line order in JieppOptions::macro_options
// (clang and gcc process them in the order written: -UX -DX=1 defines X).
struct MacroOption {
    enum class Kind { DEFINE, UNDEF };
    Kind kind;
    std::string arg;   // DEFINE: "DEBUG" or "VERSION=10"; UNDEF: the macro name
};

// One -MT or -MQ, kept in command-line order in JieppOptions::dep_targets.
// -MT text is written to the rule as it is; -MQ text is Make-escaped (like a
// prerequisite).
struct DepTarget {
    std::string text;
    bool escape = false;
};

struct JieppOptions {
    std::vector<std::string> input_filepaths;
    std::optional<std::string> output_filepath;
    std::vector<MacroOption> macro_options;   // -D / -U, in command-line order
    std::vector<std::string> include_filepaths;   // -include: files to force-include
    std::vector<std::string> syspaths;        // -I
    std::vector<std::string> system_paths;    // -isystem: searched after -I; left out by -MM/-MMD
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
    bool no_include_stack = false;   // --no-include-stack: no "In file included from" lines
    bool suppress_warnings = false;  // -w: suppress warning output
    bool werror = false;             // -Werror / -Wno-error: promote warnings to errors (last one wins)
    std::vector<WarningOption> warnings;   // -W<name> / -Wno-<name> / -Werror=<name> / -Wno-error=<name>, in order
    DepMode dep_mode = DepMode::NONE;           // -M / -MM
    std::optional<std::string> dep_file;        // -MF: dependency output file ("-" = stdout)
    std::vector<DepTarget> dep_targets;         // -MT / -MQ: dependency target names, in order
    std::optional<std::string> disppath;

    // Appends -D / -U options (library callers and tests; parse_args() fills
    // macro_options itself).
    void add_define(std::string arg) { macro_options.push_back({MacroOption::Kind::DEFINE, std::move(arg)}); }
    void add_undef(std::string name) { macro_options.push_back({MacroOption::Kind::UNDEF, std::move(name)}); }
    void add_defines(const std::vector<std::string>& args) { for (const auto& a : args) add_define(a); }
    void add_undefs(const std::vector<std::string>& names) { for (const auto& n : names) add_undef(n); }
};

// Parse "-DNAME=value" or "-D NAME=value" → {name, value}
// "-DNAME" → {name, "1"}
std::pair<std::string, std::string> define_macro_option(const std::string& arg);

JieppOptions parse_args(int argc, char* argv[]);

// Flushes stdout and reports whether everything written to it so far got
// out: std::cout's state (a failed write or a failed flush through its
// streambuf sets badbit), then the C stdio stream it is synced with
// (fflush(stdout), ferror(stdout)). std::cout is flushed first so its
// buffered bytes reach stdout before stdout itself is flushed. stdout is
// never closed here, so an error that only close(2) at exit reports (NFS)
// is not seen (SPECIFICATION.md section 16).
bool flush_stdout();
