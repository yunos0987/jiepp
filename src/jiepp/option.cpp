#include "option.hpp"
#include "../env/issue.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

int parse_positive_int(const std::string& arg, const std::string& opt_name) {
    try {
        int val = std::stoi(arg);
        if (val <= 0)
            ISSUE(INVALID_OPTION_VALUE, opt_name + ": must be a positive integer");
        return val;
    } catch (const std::exception&) {
        ISSUE(INVALID_OPTION_VALUE, opt_name + ": requires a valid integer");
        return -1;
    }
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
        "  -MF FILE                 Write dependency rules to file\n"
        "  -MT TARGET               Set dependency target name\n"
        "  --max-include-depth N    Maximum include depth (default: 100)\n"
        "  --max-expansion-depth N  Maximum expansion depth (default: 256)\n"
        "  --max-if-nesting N       Maximum if/elif nesting depth (default: 256)\n"
        "  --recursion-limit N      Set OS stack size (N * ~8KB frames)\n"
        "  --pp-output-pragma-style STYLE\n"
        "  --remove-comments / -nC  Remove comments\n"
        "  -dM                      Dump macro definitions\n"
        "  --silent                 Suppress all diagnostic output\n"
        "  --                       End of options\n"
        "  --help                   Show this help\n"
        "  --version                Show version\n";
    std::exit(exit_code);
}

} // namespace

std::pair<std::string, std::string> define_macro_option(const std::string& arg) {
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
            opts.define_macros.push_back(argv[++i]);
            ++i; continue;
        }

        if (arg.size() >= 2 && arg.compare(0, 2, "-D") == 0) {
            opts.define_macros.push_back(arg.substr(2));
            ++i; continue;
        }

        if (arg == "-U" && i + 1 < argc) {
            opts.undef_macros.push_back(argv[++i]);
            ++i; continue;
        }

        if (arg.size() >= 2 && arg.compare(0, 2, "-U") == 0) {
            opts.undef_macros.push_back(arg.substr(2));
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

        if (arg == "--pp-output-pragma-style" || arg == "--pp_output_pragma_style") {
            require_value(i, argc, arg);
            opts.pp_output_pragma_style = argv[++i];
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

        if (arg == "-dM") {
            opts.dM = true;
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

        // Unknown option: error and exit (gcc-compatible behavior)
        try {
            ISSUE(UNKNOWN_OPTION, arg);
        } catch (...) {
            display_help_and_exit(1);
        }
    }
    return opts;
}
