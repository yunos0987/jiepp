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
    } else {
        // Default: replace extension with .o
        fs::path p(opts.input_filepaths[0]);
        p.replace_extension(".output");
        return p.filename().generic_string();
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

        bool dep = opts.dep_mode != DepMode::NONE;
        bool not_out = opts.dM && (dep && opts.dep_file.has_value() && !opts.dep_file->empty());

        std::ostringstream virtual_output;
        std::ostream* actual_output = not_out ? &virtual_output : output_stream;

        std::vector<Token> ots;

        // -include: force-include files before main input
        for (const auto& include_filepath : opts.include_filepaths)
            expand(include_filepath, Loader::LoadType::INCLUDE, ots, env, include_filepath);

        std::string dispath = opts.disppath.value_or(opts.input_filepaths.empty() ? "<stdin>" : opts.input_filepaths[0]);

        if (opts.input_filepaths.empty() || ((opts.input_filepaths.size() == 1) && (opts.input_filepaths[0] == "-"))) {
            env.push_file("<stdin>");
            Issue::push({1, dispath});
            ots.push_back(Token::line_pragma(0, dispath, env.is_standard_pragma_style()));
            ots.push_back(Token::newline());
            auto its = iec3_tokens(std::cin, env.get_remove_comments(), 1);
            expand(its, ots, env);
            for (auto& t : ots)
                *actual_output << t.text;
            Issue::pop();
            env.pop_file();
        } else if (opts.input_filepaths.size() == 1) {
            expand(opts.input_filepaths[0], Loader::LoadType::INCLUDE, ots, env, dispath);
            for (auto& t : ots)
                *actual_output << t.text;
        } else {
            ISSUE(INVALID_COMMAND, "multiple input files not supported");
        }

        if (opts.dM) {
            dump_macros(env, *output_stream);
        }

        // -M / -MM: write dependency rules
        if (dep) {
            const std::string dep_target_ = dep_target(opts);
            if (not_out && !opts.dM) {
                // Only write dependency rules, skip writing preprocessed output
                write_dep_rules(dep_target_, opts.dep_mode, *output_stream, env);
            } else if (opts.dep_file.has_value() && !opts.dep_file->empty()) {
                // Write preprocessed output first, then write dependency rules to separate file or stdout
                std::ofstream dep_output;
                dep_output.open(*opts.dep_file, std::ios::out | std::ios::binary);
                if (!dep_output)
                    ISSUE(FILE_ERROR, *opts.dep_file);
                write_dep_rules(dep_target_, opts.dep_mode, dep_output, env);
            }
        }
        return 0;
    } catch (const Issue::Exception&) {
        return 1;
    }
}
