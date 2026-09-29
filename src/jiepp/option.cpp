#include "option.hpp"
#include "../env/issue.hpp"
#include "../env/param_constants.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

// Both helpers below keep the actual conversion (std::stoi) as the *only*
// statement inside the try block. INVALID_OPTION_VALUE is a throwing
// (ERROR-severity) code, so if the range check were inside the same try (as
// it used to be), the resulting Issue::Exception would be re-caught by this
// function's own catch clause and re-issued as a second, contradictory
// diagnostic ("must be a non-negative integer" immediately followed by
// "requires a valid integer" for input that in fact parsed fine). Moving the
// range check outside the try means it throws straight to the caller, so
// exactly one diagnostic is ever printed per invocation. `pos` additionally
// guards against trailing garbage (e.g. "3abc"), which std::stoi alone
// silently accepts by parsing only the leading numeric prefix.

int parse_positive_int(const std::string& arg, const std::string& opt_name) {
    int val;
    std::size_t pos = 0;
    try {
        val = std::stoi(arg, &pos);
    } catch (const std::exception&) {
        ISSUE(INVALID_OPTION_VALUE, opt_name + ": requires a valid integer");
        return -1;
    }
    if (pos != arg.size())
        ISSUE(INVALID_OPTION_VALUE, opt_name + ": requires a valid integer");
    if (val <= 0)
        ISSUE(INVALID_OPTION_VALUE, opt_name + ": must be a positive integer");
    return val;
}

int parse_nonnegative_int(const std::string& arg, const std::string& opt_name) {
    int val;
    std::size_t pos = 0;
    try {
        val = std::stoi(arg, &pos);
    } catch (const std::exception&) {
        ISSUE(INVALID_OPTION_VALUE, opt_name + ": requires a valid integer");
        return -1;
    }
    if (pos != arg.size())
        ISSUE(INVALID_OPTION_VALUE, opt_name + ": requires a valid integer");
    if (val < 0)
        ISSUE(INVALID_OPTION_VALUE, opt_name + ": must be a non-negative integer");
    return val;
}

std::string parse_pragma_style(const std::string& arg, const std::string& opt_name) {
    if (arg != VAL_PRAGMA_STANDARD && arg != VAL_PRAGMA_ANNOTATED)
        ISSUE(INVALID_OPTION_VALUE, opt_name + ": must be annotated or standard");
    return arg;
}

void require_value(int i, int argc, const std::string& opt_name) {
    if (i + 1 >= argc)
        ISSUE(MISSING_OPTION_VALUE, opt_name);
}

void display_help_and_exit(int exit_code = 0) {
    std::cout <<
        "Usage: jiepp [filepath] [options]\n"
        "Options:\n"
        "  -o PATH                  Output file\n"
        "  -D NAME[=VALUE]          Define macro\n"
        "  -U NAME                  Undefine macro\n"
        "  -I PATH                  Add syspath\n"
        "  -include FILE            Force-include file before input\n"
        "  -w                       Suppress warnings\n"
        "  -Werror                  Promote warnings to errors\n"
        "  -M                       Output Makefile dependency rules\n"
        "  -MM                      Like -M but exclude system includes\n"
        "  -MD                      Write dependency rules to .d file (keep normal output)\n"
        "  -MMD                     Like -MD but exclude system includes\n"
        "  -MF FILE                 Write dependency rules to file\n"
        "  -MT TARGET               Set dependency target name\n"
        "  --max-include-depth N    Maximum include depth (default: 100)\n"
        "  --max-expansion-depth N  Maximum expansion depth (default: 256)\n"
        "  --max-if-nesting N       Maximum if/elif nesting depth (default: 256)\n"
        "  --max-blank-lines N      Max consecutive blank lines before compaction (default: 7; 0 disables)\n"
        "  --recursion-limit N      Stack size: N x 8 KiB, at least 1 MiB (default: 8 MiB)\n"
        "  --pp-output-pragma-style STYLE\n"
        "                           Pragma output style: annotated or standard (default: annotated)\n"
        "  -P                       Suppress line markers in output\n"
        "  --remove-comments / -nC  Remove comments\n"
        "  -dM                      Dump macro definitions\n"
        "  -dD                      Emit {#define}/{#undef} lines inline\n"
        "  --silent                 Suppress all diagnostic output\n"
        "  --                       End of options\n"
        "  --help                   Show this help\n"
        "  --version                Show version\n";
    std::exit(exit_code);
}

} // namespace

// Splits -D ARG at its first '=' into {name, value} (value "1" if none).
// `name` is not validated here -- it may be a function-like head such as
// "F(x)"; setup() (C4) processes {name, value} as "{#define name value}",
// which validates it the same as any {#define} name.
std::pair<std::string, std::string> define_macro_option(const std::string& arg) {
    // Not tied to a source file; see Issue::CLI_LOCATION. Guarded here too
    // (not only by jiepp_command(), its only non-test caller) so this
    // function is independently correct when called directly, e.g. by tests.
    Issue::CliMode cli_mode_guard;

    auto eq = arg.find('=');
    if (eq != std::string::npos) {
        std::string name = arg.substr(0, eq);
        if (name.empty())
            ISSUE(INVALID_MACRO_DEF, "-D requires a non-empty macro name");
        return {name, arg.substr(eq + 1)};
    }
    if (arg.empty())
        ISSUE(INVALID_MACRO_DEF, "-D requires a non-empty macro name");
    return {arg, "1"};
}

JieppOptions parse_args(int argc, char* argv[]) {
#ifndef JIEPP_VERSION
#define JIEPP_VERSION "0.0.0"
#endif
    // Diagnostics raised in this function (UNKNOWN_OPTION, INVALID_OPTION_VALUE,
    // MISSING_OPTION_VALUE, ...) are not tied to a source file; see
    // Issue::CLI_LOCATION. Guarded here too (not only by main()) so
    // parse_args() is independently correct when called directly, e.g. by
    // tests.
    Issue::CliMode cli_mode_guard;

    JieppOptions opts;
    bool end_of_options = false;

    for (int i = 1; i < argc; ) {
        std::string arg = argv[i];

        // After --, everything is an input filepath
        if (end_of_options) {
            opts.input_filepaths.push_back(arg);
            ++i; continue;
        }

        if (arg == "--") {
            end_of_options = true;
            ++i; continue;
        }

        if (arg == "--help" || arg == "-h")
            display_help_and_exit(0);

        if (arg == "--version") {
            std::cout << "jiepp " << JIEPP_VERSION << "\n";
            std::exit(0);
        }

        if (arg == "-o") {
            require_value(i, argc, "-o");
            opts.output_filepath = argv[++i];
            ++i; continue;
        }

        if (arg == "-D" && i + 1 < argc) {
            std::string spec = argv[++i];
            if (spec.empty())
                ISSUE(MISSING_OPTION_VALUE, "-D");
            opts.define_macros.push_back(spec);
            ++i; continue;
        }

        if (arg.size() >= 2 && arg.compare(0, 2, "-D") == 0) {
            std::string spec = arg.substr(2);
            if (spec.empty())
                ISSUE(MISSING_OPTION_VALUE, "-D");
            opts.define_macros.push_back(spec);
            ++i; continue;
        }

        if (arg == "-U" && i + 1 < argc) {
            std::string name = argv[++i];
            if (name.empty())
                ISSUE(MISSING_OPTION_VALUE, "-U");
            opts.undef_macros.push_back(name);
            ++i; continue;
        }

        if (arg.size() >= 2 && arg.compare(0, 2, "-U") == 0) {
            std::string name = arg.substr(2);
            if (name.empty())
                ISSUE(MISSING_OPTION_VALUE, "-U");
            opts.undef_macros.push_back(name);
            ++i; continue;
        }

        if (arg == "-I") {
            require_value(i, argc, "-I");
            opts.syspaths.push_back(argv[++i]);
            ++i; continue;
        }

        if (arg.size() >= 2 && arg.compare(0, 2, "-I") == 0) {
            opts.syspaths.push_back(arg.substr(2));
            ++i; continue;
        }

        if (arg == "-include") {
            require_value(i, argc, "-include");
            opts.include_filepaths.push_back(argv[++i]);
            ++i; continue;
        }

        if (arg == "-w") {
            opts.suppress_warnings = true;
            ++i; continue;
        }

        if (arg == "-Werror") {
            opts.werror = true;
            ++i; continue;
        }

        if (arg == "-M") {
            opts.dep_mode = DepMode::ALL;
            ++i; continue;
        }

        if (arg == "-MM") {
            opts.dep_mode = DepMode::USER;
            ++i; continue;
        }

        if (arg == "-MD") {
            opts.MD = true;
            opts.dep_mode = DepMode::ALL;
            ++i; continue;
        }

        if (arg == "-MMD") {
            opts.MMD = true;
            opts.dep_mode = DepMode::USER;
            ++i; continue;
        }

        if (arg == "-MF") {
            require_value(i, argc, "-MF");
            opts.dep_file = argv[++i];
            ++i; continue;
        }

        if (arg == "-MT") {
            require_value(i, argc, "-MT");
            opts.dep_target = argv[++i];
            ++i; continue;
        }

        if (arg == "--max-include-depth" || arg == "--max_include_depth") {
            require_value(i, argc, arg);
            opts.max_include_depth = parse_positive_int(argv[++i], arg);
            ++i; continue;
        }

        if (arg == "--max-expansion-depth" || arg == "--max_expansion_depth") {
            require_value(i, argc, arg);
            opts.max_expansion_depth = parse_positive_int(argv[++i], arg);
            ++i; continue;
        }

        if (arg == "--max-if-nesting" || arg == "--max_if_nesting") {
            require_value(i, argc, arg);
            opts.max_if_nesting = parse_positive_int(argv[++i], arg);
            ++i; continue;
        }

        if (arg == "--max-blank-lines" || arg == "--max_blank_lines") {
            require_value(i, argc, arg);
            opts.max_blank_lines = parse_nonnegative_int(argv[++i], arg);
            ++i; continue;
        }

        if (arg == "--pp-output-pragma-style" || arg == "--pp_output_pragma_style") {
            require_value(i, argc, arg);
            opts.pp_output_pragma_style = parse_pragma_style(argv[++i], arg);
            ++i; continue;
        }

        if (arg == "--recursion-limit" || arg == "--recursion_limit") {
            require_value(i, argc, arg);
            opts.recursion_limit = parse_positive_int(argv[++i], arg);
            ++i; continue;
        }

        if (arg == "--remove-comments" || arg == "-nC") {
            opts.remove_comments = true;
            ++i; continue;
        }

        if (arg == "-P") {
            opts.no_line_markers = true;
            ++i; continue;
        }

        if (arg == "-dM") {
            opts.dM = true;
            ++i; continue;
        }

        if (arg == "-dD") {
            opts.dD = true;
            ++i; continue;
        }

        if (arg == "--silent") {
            opts.silent = true;
            ++i; continue;
        }

        if (arg == "--disppath") {
            require_value(i, argc, arg);
            opts.disppath = argv[++i];
            ++i; continue;
        }

        if (!arg.empty() && arg[0] != '-') {
            opts.input_filepaths.push_back(arg);
            ++i; continue;
        }

        if (arg == "-") {
            opts.input_filepaths.push_back("-");
            ++i; continue;
        }

        // Unknown option: exactly one diagnostic line on stderr and exit 1
        // -- no usage dump (matches gcc/clang, e.g. clang's "unknown
        // argument" error, neither of which print --help on this path).
        // ISSUE() throws Issue::Exception; it is deliberately left
        // unhandled here so it propagates to main()'s
        // catch (const Issue::Exception&), which returns 1 without
        // re-printing anything (happen() already emitted the PP70 line).
        ISSUE(UNKNOWN_OPTION, arg);
    }
    return opts;
}
