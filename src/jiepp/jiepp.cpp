#include "jiepp.hpp"
#include "option.hpp"
#include "../core/preprocessor.hpp"
#include "../core/line_compaction.hpp"
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
#include <system_error>
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

// Escape a dependency prerequisite/target path for Makefile syntax, mirroring
// gcc/clang's mkdeps munge(): '$' doubles to "$$"; '#' and whitespace (space,
// tab) get a backslash prefix followed by the *original* character itself
// (not a symbolic C-style escape letter), so a tab becomes backslash + an
// actual tab byte, exactly like a space becomes backslash + an actual space
// byte. The colon is deliberately left unescaped (gcc does not escape it
// either, and escaping it would break Windows drive-letter paths such as
// "C:/..."). This function is only ever applied to auto-derived paths
// (prerequisites, and the target when -MT is not given); a user-supplied
// -MT target is passed through verbatim (see dep_target()/format_dep_target()
// below), matching real gcc/clang where only the unimplemented -MQ escapes.
std::string escape_make_path(const std::string& path) {
    std::string out;
    out.reserve(path.size());
    for (char c : path) {
        switch (c) {
        case '$':  out += "$$"; break;
        case '#':  out += "\\#"; break;
        case ' ':  out += "\\ "; break;
        case '\t': out += "\\\t"; break;
        default:   out += c; break;
        }
    }
    return out;
}

// Format the dependency-rule target: a user-supplied -MT value is emitted
// verbatim (gcc/clang convention -- only -MQ, which jiepp does not
// implement, escapes the target), while an auto-derived target (from the
// input/output filename) is Make-escaped like every prerequisite path.
std::string format_dep_target(const JieppOptions& opts) {
    std::string raw = dep_target(opts);
    return opts.dep_target ? raw : escape_make_path(raw);
}

// Write Makefile-style dependency rules. `target_text` must already be in
// its final, rule-ready form (see format_dep_target()): verbatim for a
// user-supplied -MT value, or pre-escaped for an auto-derived target. It is
// written as-is here, never re-escaped.
void write_dep_rules(const std::string& target_text, DepMode dep_mode, std::ostream& output, const Env& env) {
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
            deps.push_back(escape_make_path(dep.display_path));
        }
    }

    // Format output
    output << target_text << ":";
    for (const auto& d : deps)
        output << " \\\n  " << d;
    output << "\n";
}

} // namespace

int jiepp_command(const JieppOptions& opts)
{
    std::ostream* output_stream = &std::cout;
    std::ofstream output_file;
    // Path of a separate dependency file (-MF / -MD / -MMD auto-named) that
    // this run has fully written, if any -- remembered so the catch block
    // below can remove it if a *later* step (typically opening -o) fails:
    // otherwise a correctly-written .d file naming an output that was never
    // produced (or never updated) would be left behind for a Make-based
    // build to trust incorrectly. See the "remove only what this run wrote"
    // rule documented at the catch site.
    std::string dep_file_written;
    try {
        // Deferred until after the whole preprocessed output has been built
        // in memory (see the emit_tokens call below): opening -o eagerly here
        // would truncate the destination before the input file is even read,
        // which is catastrophic when -o names the same path as the input.
        auto open_output = [&]() {
            if (opts.output_filepath) {
                output_file.open(*opts.output_filepath, std::ios::out | std::ios::binary);
                if (!output_file)
                    ISSUE(FILE_ERROR, *opts.output_filepath);
                output_stream = &output_file;
            }
        };

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
        if (opts.max_blank_lines)
            env.fix_max_blank_lines(*opts.max_blank_lines);
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
        // -M/-MM (dependency-only modes) suppress the preprocessed output;
        // -MD/-MMD (and -dM, unrelated to dep mode) keep it. Resolved lazily
        // by emit_tokens below since output_stream is not yet finalized here.
        bool not_out = opts.dM || (dep && !opts.MD && !opts.MMD);

        std::ostringstream virtual_output;

        // Emit tokens to stream, optionally suppressing line markers and their
        // immediately-following WS token (GCC-compatible -P blank-line removal).
        // actual_output is resolved here (not captured earlier) because
        // output_stream may still change when open_output() runs later.
        auto emit_tokens = [&](const std::vector<Token>& tokens) {
            std::ostream* actual_output = not_out ? &virtual_output : output_stream;
            if (opts.no_line_markers) {
                bool skip_next_ws = false;
                for (const auto& t : tokens) {
                    if (jiepp::is_line_marker(t)) {
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
            Issue::pop();
            env.pop_file();
        } else if (opts.input_filepaths.size() == 1) {
            expand(opts.input_filepaths[0], Loader::LoadType::INCLUDE, ots, env, dispath);
        } else {
            ISSUE(INVALID_COMMAND, "multiple input files not supported");
        }

        // The whole preprocessed output is accumulated in ots by this point;
        // only now is it safe to (re)open -o, even if -o names the same path
        // as the input file. Blank-line compaction runs last, as a post-pass
        // over the fully materialised stream: CollapseAll under -P (which
        // already strips line markers, so blank runs must collapse to zero
        // rather than gain a marker of their own), Markers otherwise.
        jiepp::compact_blank_lines(ots, env.get_max_blank_lines(),
            opts.no_line_markers ? jiepp::BlankLineMode::CollapseAll : jiepp::BlankLineMode::Markers,
            env.is_standard_pragma_style());

        bool dep_has_separate_file = dep && effective_dep_file.has_value() && !effective_dep_file->empty();

        // Write the separate dependency file (-MF / -MD / -MMD auto-named)
        // before -o is opened: this write is completely independent of the
        // main output, so if it fails, -o must not be touched at all -- a
        // pre-existing -o from an earlier successful run stays byte-for-byte
        // intact, and a fresh, correct -o is never opened only to be deleted
        // again by the catch block below (see SPECIFICATION.md section 13,
        // "dependency-file Make escaping" subsection).
        if (dep_has_separate_file) {
            const std::string dep_target_text = format_dep_target(opts);
            std::ofstream dep_output;
            dep_output.open(*effective_dep_file, std::ios::out | std::ios::binary);
            if (!dep_output)
                ISSUE(FILE_ERROR, *effective_dep_file);
            write_dep_rules(dep_target_text, opts.dep_mode, dep_output, env);
            dep_output.close();
            dep_file_written = *effective_dep_file;
        }

        // -o (or stdout) is only needed when something will actually be
        // written to it: the main preprocessed content (!not_out), a -dM
        // macro dump, or dependency rules that have no separate file to go
        // to (-M/-MM without -MF). A dependency-only invocation that already
        // has a separate dep file (e.g. "-M -MF x.d -o out.iec") has nothing
        // left to put in -o, so -o is deliberately left unopened/unwritten
        // (matching gcc, whose -M/-MM never touch the compiler's normal
        // output file either).
        bool need_dep_to_stream = dep && !dep_has_separate_file;
        bool need_output_stream = !not_out || opts.dM || need_dep_to_stream;

        if (need_output_stream) {
            open_output();
            emit_tokens(ots);

            if (opts.dM) {
                dump_macros(env, *output_stream);
            }

            if (need_dep_to_stream) {
                // No dep file: write dep rules to main output stream (-M / -MM without -MF)
                write_dep_rules(format_dep_target(opts), opts.dep_mode, *output_stream, env);
            }
        }
        return 0;
    } catch (const Issue::Exception&) {
        // Do not leave a truncated/empty -o target behind on failure: a
        // Make-based build system must reprocess the file on the next run,
        // not treat a 0-byte, freshly-mtime'd file as an up-to-date target.
        if (output_file.is_open()) {
            output_file.close();
            if (opts.output_filepath) {
                std::error_code ec;
                fs::remove(*opts.output_filepath, ec);
            }
        }
        // Likewise, remove only what *this run* wrote: if a separate
        // dependency file was successfully written earlier in this same run
        // but a later step (e.g. opening -o) then failed, the .d file now
        // either references an output that was never (re)produced, or -- if
        // it already existed -- was just truncated with content that was
        // never paired with a matching successful output. Either way it is
        // this run's own write, so it is removed here; a dep file from an
        // *earlier*, unrelated run that this run never touched is left
        // alone, matching gcc's convention of not scrubbing stale files it
        // did not itself just write.
        if (!dep_file_written.empty()) {
            std::error_code ec;
            fs::remove(dep_file_written, ec);
        }
        return 1;
    }
}
