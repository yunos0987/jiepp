#include "jiepp.hpp"
#include "option.hpp"
#include "../core/preprocessor.hpp"
#include "../env/env.hpp"
#include "../loader/token.hpp"
#include "../loader/lexer.hpp"
#include "../macro/macro.hpp"
#include "../env/issue.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::string dep_target(const JieppOptions& opts) {
    if (opts.dep_target) {
        return *opts.dep_target;
    }
    // Derive from input file for all dep modes (-M/-MM/-MD/-MMD).
    // -MF only controls the destination file; it does not affect the target name.
    if (!opts.input_filepaths.empty() && opts.input_filepaths[0] != "-") {
        fs::path p(opts.input_filepaths[0]);
        p.replace_extension(".output");
        return p.filename().generic_string();
    } else if (opts.output_filepath.has_value()) {
        fs::path p(*opts.output_filepath);
        p.replace_extension(".output");
        return p.filename().generic_string();
    } else {
        return "output.output";
    }
}

// Write Makefile-style dependency rules
void write_dep_rules(const std::string& target, DepMode dep_mode, std::ostream& output, const Env& env) {
    // Collect dependencies with deduplication by resolved_path
    // Track resolved paths we've already seen (only first display_path is output)
    std::unordered_set<std::string> seen_resolved;
    std::vector<std::string> deps;
    
    for (const auto& dep : env.dependencies()) {
        // Skip system includes if -MM mode
        if (dep_mode == DepMode::USER && dep.is_system)
            continue;
        // Deduplicate by resolved_path (only add first occurrence of each resolved path)
        if (seen_resolved.find(dep.resolved_path) == seen_resolved.end()) {
            seen_resolved.insert(dep.resolved_path);
            deps.push_back(dep.display_path);
        }
    }

    // Format output
    output << target << ":";
    for (const auto& d : deps)
        output << " \\\n  " << d;
    output << "\n";
}

} // namespace

int jiepp_command(const JieppOptions& opts)
{
    std::ostream* output_stream = &std::cout;
    std::ofstream output_file;
    try {
        if (opts.output_filepath) {
            output_file.open(*opts.output_filepath, std::ios::out | std::ios::binary);
            if (!output_file)
                ISSUE(FILE_ERROR, *opts.output_filepath);
            output_stream = &output_file;
        }

        std::vector<std::pair<std::string,std::string>> predefine_macros;
        for (const auto& dm : opts.define_macros)
            predefine_macros.push_back(define_macro_option(dm));

        Env env = setup(predefine_macros);

        for (const auto& name : opts.undef_macros) // -U: undefine macros (applied after -D)
            env.undef(name);

        if (opts.remove_comments)
            env.fix_remove_comments(true);
        if (opts.dD)
            env.set_dd_mode(true);
        if (opts.max_include_depth)
            env.fix_max_include_depth(*opts.max_include_depth);
        if (opts.max_expansion_depth)
            env.fix_max_expansion_depth(*opts.max_expansion_depth);
        if (opts.max_if_nesting)
            env.fix_max_if_nesting(*opts.max_if_nesting);
        if (opts.pp_output_pragma_style)
            env.fix_pragma_style(*opts.pp_output_pragma_style);
        if (opts.silent)
            Issue::silent_ = true;
        if (opts.suppress_warnings)
            Issue::suppress_warnings_ = true;
        if (opts.werror)
            Issue::werror_ = true;

        for (const auto& sp : opts.syspaths)
            env.add_syspath(sp);

        // -MD/-MMD: auto-derive dep file if not explicitly set by -MF
        std::optional<std::string> effective_dep_file = opts.dep_file;
        if ((opts.MD || opts.MMD) && !effective_dep_file.has_value()) {
            if (opts.output_filepath.has_value()) {
                effective_dep_file = fs::path(*opts.output_filepath).replace_extension(".d").generic_string();
            } else if (!opts.input_filepaths.empty() && opts.input_filepaths[0] != "-") {
                effective_dep_file = fs::path(opts.input_filepaths[0]).replace_extension(".d").generic_string();
            } else {
                // stdin with no -o and no -MF: cannot write a separate dep file
                ISSUE(INVALID_COMMAND, "-MD/-MMD requires -MF or -o when reading from stdin");
            }
        }

        bool dep = opts.dep_mode != DepMode::NONE;
        bool not_out = opts.dM && (dep && effective_dep_file.has_value() && !effective_dep_file->empty());

        std::ostringstream virtual_output;
        std::ostream* actual_output = not_out ? &virtual_output : output_stream;

        // Returns true for preprocessor-injected line markers: (*{#:N 'file'}*) or {#:N 'file'}.
        // User IEC pragmas with '#' are lexed as DIRECTIVE tokens, so PRAGMA tokens whose
        // body begins with "#:" are exclusively factory-created line markers.
        auto is_line_marker = [](const Token& t) -> bool {
            if (t.type != Token::PRAGMA) return false;
            const auto& s = t.text;
            if (s.size() > 5 && s.compare(0, 5, "(*{#:") == 0) return true;  // annotated
            if (s.size() > 3 && s.compare(0, 3, "{#:") == 0) return true;    // standard
            return false;
        };

        // Emit tokens to stream, optionally suppressing line markers and their
        // immediately-following WS token (GCC-compatible -P blank-line removal).
        auto emit_tokens = [&](const std::vector<Token>& tokens) {
            if (opts.no_line_markers) {
                bool skip_next_ws = false;
                for (const auto& t : tokens) {
                    if (is_line_marker(t)) {
                        skip_next_ws = true;
                        continue;
                    }
                    if (skip_next_ws) {
                        skip_next_ws = false;
                        if (t.type == Token::WS) continue;  // drop blank line only, not comments
                    }
                    *actual_output << t.text;
                }
            } else {
                for (const auto& t : tokens)
                    *actual_output << t.text;
            }
        };

        std::vector<Token> ots;

        // -include: force-include files before main input
        for (const auto& include_filepath : opts.include_filepaths) {
            auto include_disppath = fs::path(include_filepath).generic_string();
            expand(include_filepath, Loader::LoadType::INCLUDE, ots, env, include_disppath);
        }

        std::string dispath;
        if (opts.disppath.has_value())
            dispath = fs::path(*opts.disppath).generic_string();
        else if (!opts.input_filepaths.empty())
            dispath = fs::path(opts.input_filepaths[0]).generic_string();
        else
            dispath = "<stdin>";

        if (opts.input_filepaths.empty() || ((opts.input_filepaths.size() == 1) && (opts.input_filepaths[0] == "-"))) {
            env.push_file("<stdin>");
            Issue::push({1, dispath});
            ots.push_back(Token::line_pragma(0, dispath, env.is_standard_pragma_style()));
            ots.push_back(Token::newline());
            auto its = iec3_tokens(std::cin, env.get_remove_comments(), 1);
            expand(its, ots, env);
            emit_tokens(ots);
            Issue::pop();
            env.pop_file();
        } else if (opts.input_filepaths.size() == 1) {
            expand(opts.input_filepaths[0], Loader::LoadType::INCLUDE, ots, env, dispath);
            emit_tokens(ots);
        } else {
            ISSUE(INVALID_COMMAND, "multiple input files not supported");
        }

        if (opts.dM) {
            dump_macros(env, *output_stream);
        }

        // -M / -MM / -MD / -MMD: write dependency rules
        if (dep) {
            const std::string dep_target_ = dep_target(opts);
            if (effective_dep_file.has_value() && !effective_dep_file->empty()) {
                // Write dependency rules to a separate file (-MF / -MD / -MMD auto-named)
                std::ofstream dep_output;
                dep_output.open(*effective_dep_file, std::ios::out | std::ios::binary);
                if (!dep_output)
                    ISSUE(FILE_ERROR, *effective_dep_file);
                write_dep_rules(dep_target_, opts.dep_mode, dep_output, env);
            } else {
                // No dep file: write dep rules to main output stream (-M / -MM without -MF)
                write_dep_rules(dep_target_, opts.dep_mode, *output_stream, env);
            }
        }
        return 0;
    } catch (const Issue::Exception&) {
        return 1;
    }
}
