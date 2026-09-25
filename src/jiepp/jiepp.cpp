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

// -o - means "write the preprocessed output to stdout" (gcc/clang
// convention), not a literal file named "-". This helper is used everywhere
// opts.output_filepath would otherwise be treated as a real, creatable path
// (opening the file, deriving a dependency/target name from it, or removing
// it on failure).
bool output_is_stdout(const JieppOptions& opts) {
    return opts.output_filepath.has_value() && *opts.output_filepath == "-";
}

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
    } else if (opts.output_filepath.has_value() && !output_is_stdout(opts)) {
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

// E0/E1: codes that still stop processing outright in continue mode --
// SEVERE (forced in by ContinueMode regardless) plus the File/IO and
// Runtime/Limit codes plan.md unit E calls out by number: PP10-14
// (FILE_ERROR, FILE_NOT_FOUND, MAX_INCLUDE_DEPTH_EXCEEDED, INVALID_COMMAND,
// INCLUDE_TARGET_IS_DIRECTORY) and PP60-61 (MAX_EXPANSION_DEPTH_EXCEEDED,
// MAX_IF_NESTING_EXCEEDED). Every other ERROR code (expression/macro/
// directive/lexical errors, and a -Werror-promoted WARNING) is counted via
// Issue::error_count_ instead of aborting. Deliberately excludes PP62
// (SANDBOX_RESTRICTED_DIRECTIVE): plan.md unit E's abort set is exactly
// SEVERE + {10,11,12,13,14,60,61}.
std::set<Issue::Code> jiepp_continue_abort_codes() {
    return {
        Issue::Code::FILE_ERROR,
        Issue::Code::FILE_NOT_FOUND,
        Issue::Code::MAX_INCLUDE_DEPTH_EXCEEDED,
        Issue::Code::INVALID_COMMAND,
        Issue::Code::INCLUDE_TARGET_IS_DIRECTORY,
        Issue::Code::MAX_EXPANSION_DEPTH_EXCEEDED,
        Issue::Code::MAX_IF_NESTING_EXCEEDED,
    };
}

} // namespace

int jiepp_command(const JieppOptions& opts)
{
    // Diagnostics raised before any real input/include file has been pushed
    // (-D/-o/-MF failures, INVALID_COMMAND, and the top-level input's own
    // FILE_NOT_FOUND) are not tied to a source file; see Issue::CLI_LOCATION.
    // Pushed here (not only by main()) so jiepp_command() is independently
    // correct when called directly, e.g. by tests.
    Issue::CliMode cli_mode_guard;

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

    // E4a: hoisted above the outer try (along with `ots` and `emit_tokens`
    // below) so a partial-output path reached via a nested catch --
    // "aborted mid-expand" (E4a) or "-o/dep-file open failed after the
    // output was already built" (E4b) -- can still see and print whatever
    // of `ots` was produced. Both depend only on `opts`, not on anything
    // computed inside the try (in particular, not on `env`), so they can be
    // decided this early.
    bool not_out = opts.dM || (opts.dep_mode != DepMode::NONE && !opts.MD && !opts.MMD);
    // Destination-table (E3) sense of "stdout": true for *both* "-o omitted"
    // and the explicit "-o -" gcc/clang convention (output_is_stdout() above
    // only recognizes the latter; -o/-MF open failures below need the
    // narrower, literal sense instead, so that function is left as-is).
    bool pp_output_to_stdout = !opts.output_filepath.has_value() || output_is_stdout(opts);
    bool no_line_markers = opts.no_line_markers;

    std::ostringstream virtual_output;
    // Emit tokens to stream, optionally suppressing line markers and their
    // immediately-following WS token (GCC-compatible -P blank-line removal).
    auto emit_tokens = [&](const std::vector<Token>& tokens) {
        std::ostream* actual_output = not_out ? &virtual_output : output_stream;
        if (no_line_markers) {
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

    try {
        // U2/E4a: reject a fundamentally malformed invocation before doing
        // any other work (predefining macros, expanding -include files,
        // ...) -- matches gcc/clang validating argument shape before
        // touching any input. Also sidesteps an ambiguity in E4's
        // "CLI-argument-stage abort (PP13) prints nothing" rule: if this
        // check instead ran after -include expansion (as in the pre-E4a
        // code), -include files could already have produced partial output
        // by the time it fired, muddying "CLI-stage abort" with "aborted
        // mid-preprocessing" (E4/E4a, which *does* print partial output to
        // stdout).
        if (opts.input_filepaths.size() > 1)
            ISSUE(INVALID_COMMAND, "multiple input files not supported");

        // Deferred until after the whole preprocessed output has been built
        // in memory (see the emit_tokens call below): opening -o eagerly here
        // would truncate the destination before the input file is even read,
        // which is catastrophic when -o names the same path as the input.
        auto open_output = [&]() {
            // -o - leaves output_stream pointing at std::cout (its initial
            // value): no file named "-" is ever created. main.cpp already
            // puts stdout into binary mode unconditionally, so redirected
            // stdout matches a real -o FILE byte-for-byte here too.
            if (opts.output_filepath && !output_is_stdout(opts)) {
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

        // G: -MD/-MMD auto-derive dep file if not explicitly set by -MF,
        // matching the gcc *driver*'s own rule (gcc manual: "If [-o] is
        // given, uses its argument but with a suffix of .d, otherwise it
        // takes the name of the input file, removes any directory
        // components and suffix, and applies a .d suffix"), verified
        // empirically against GNU cpp 5.3.0 (see unit G's report):
        std::optional<std::string> effective_dep_file = opts.dep_file;
        if ((opts.MD || opts.MMD) && !effective_dep_file.has_value()) {
            if (opts.output_filepath.has_value()) {
                // "uses its argument [literally], but with a suffix of
                // .d" -- including the "-o -" quirk: gcc does NOT
                // special-case "-" here the way it does for the main
                // output, so -o - creates a file literally named "-.d" in
                // the current directory (confirmed with GNU cpp 5.3.0).
                // jiepp matches this rather than carving out its own
                // exception, since a stray "-.d" is exactly what a user
                // asking for -MD without -MF and -o - should expect from
                // gcc's own documented rule.
                effective_dep_file = fs::path(*opts.output_filepath).replace_extension(".d").generic_string();
            } else if (!opts.input_filepaths.empty() && opts.input_filepaths[0] != "-") {
                // "takes the name of the input file, removes any
                // directory components and suffix, and applies a .d
                // suffix" -- written to the current directory, *not* the
                // input file's own directory (confirmed with GNU cpp
                // 5.3.0: "cpp -MD subdir/in.c" from a different CWD writes
                // "in.d" there, not "subdir/in.d"). fs::path::stem() already
                // strips both the directory and the last extension.
                effective_dep_file = fs::path(opts.input_filepaths[0]).stem().string() + ".d";
            } else {
                // stdin with no -o and no -MF: cannot write a separate dep
                // file. NOTE (unit G): GNU cpp 5.3.0 does *not* error here
                // -- it applies the same rule as any other input, treating
                // "-" as the "file name" to strip (no directory, no
                // suffix), and so silently writes "-.d" to the current
                // directory instead. jiepp intentionally keeps its own
                // long-standing PP13 diagnostic rather than replicating
                // that quirk; see unit G's report for the rationale.
                ISSUE(INVALID_COMMAND, "-MD/-MMD requires -MF or -o when reading from stdin");
            }
        }

        bool dep = opts.dep_mode != DepMode::NONE;

        // E1/U2: continue past non-abort ERRORs (see jiepp_continue_abort_codes()
        // above for exactly which codes still stop it outright) for exactly
        // the -include/top-level-expansion phase below, not the whole
        // function: everything before this point (the multiple-input-files
        // check above, -D/-U, env fixups, the stdin+-MD/-MMD-without--MF
        // check just above) is CLI-shaped misuse that should keep aborting
        // unconditionally, the way it always has; everything after it (E4b's
        // -o/dep-file open) raises only PP10, already in the abort set
        // regardless of continue_mode_, so where this guard's scope ends
        // does not change behavior there either.
        bool aborted = false;
        {
            Issue::ContinueMode continue_guard(jiepp_continue_abort_codes());
            try {
                // -include: force-include files before main input
                for (const auto& include_filepath : opts.include_filepaths) {
                    auto include_disppath = fs::path(include_filepath).generic_string();
                    expand(include_filepath, Loader::LoadType::INCLUDE, ots, env, include_disppath);
                }

                // H: gcc/clang both display "<stdin>" for the sole input
                // being "-" (no input argument at all is the same case, see
                // the branch below) -- confirmed with GNU cpp 5.3.0 and
                // clang: `echo x | cpp -`/`echo x | clang -E -x c -` both
                // report __FILE__ and line markers as "<stdin>", never the
                // literal "-". Without this check, dispath fell through to
                // fs::path("-").generic_string() == "-", which then leaked
                // into __FILE__, __BASE_FILE__, __FILE_NAME__ (all backed by
                // the same Issue location-stack entry pushed below) and
                // every line marker/diagnostic location for a stdin input.
                // --disppath still overrides this, same as for a real file.
                std::string dispath;
                if (opts.disppath.has_value())
                    dispath = fs::path(*opts.disppath).generic_string();
                else if (opts.input_filepaths.empty() || ((opts.input_filepaths.size() == 1) && (opts.input_filepaths[0] == "-")))
                    dispath = "<stdin>";
                else
                    dispath = fs::path(opts.input_filepaths[0]).generic_string();

                if (opts.input_filepaths.empty() || ((opts.input_filepaths.size() == 1) && (opts.input_filepaths[0] == "-"))) {
                    // item f: RAII-guarded (see expand()'s file-inclusion
                    // wrapper in core/expand.cpp for the declaration-order
                    // rationale), so an ERROR raised while processing stdin
                    // still pops both stacks correctly.
                    FileContext::FileScope file_guard(env, "<stdin>");
                    Issue::LineGuard line_guard(1, dispath);
                    ots.push_back(Token::line_pragma(0, dispath, env.is_standard_pragma_style()));
                    ots.push_back(Token::newline());
                    auto its = iec3_tokens(std::cin, env.get_remove_comments(), 1);
                    expand(its, ots, env);
                } else {
                    // Exactly one input file here: more than one was already
                    // rejected (PP13) before this point.
                    expand(opts.input_filepaths[0], Loader::LoadType::INCLUDE, ots, env, dispath);
                }
            } catch (const Issue::Exception&) {
                // E4a: a code in jiepp_continue_abort_codes() (or SEVERE)
                // stopped processing partway through. Handled below, once
                // continue_guard's scope (and so continue_mode_) has ended.
                aborted = true;
            }
        }

        if (aborted) {
            // E4: gcc-fatal-error-style partial output. Only when the
            // preprocessed result's own destination is stdout and it is not
            // suppressed by -dM/-M/-MM (see the destination table's E4 row:
            // "-o FILE、-dM、-M/-MM: 標準出力には何も出さない" -- unlike the
            // E3 branch below, -dM prints nothing here either, since
            // processing stopped before the macro table could reach its
            // final state). Neither -o nor any dependency file is ever
            // opened/written in this branch, and no existing file at either
            // path is touched.
            if (pp_output_to_stdout && !not_out) {
                jiepp::compact_blank_lines(ots, env.get_max_blank_lines(),
                    opts.no_line_markers ? jiepp::BlankLineMode::CollapseAll : jiepp::BlankLineMode::Markers,
                    env.is_standard_pragma_style());
                emit_tokens(ots);
            }
            return 1;
        }

        // Not aborted: the whole preprocessed output is accumulated in ots
        // by this point. Blank-line compaction runs unconditionally here
        // (needed regardless of error_count_ below: it feeds both the E3
        // stdout-only print and the ordinary success path's own output) --
        // CollapseAll under -P (which already strips line markers, so blank
        // runs must collapse to zero rather than gain a marker of their
        // own), Markers otherwise.
        jiepp::compact_blank_lines(ots, env.get_max_blank_lines(),
            opts.no_line_markers ? jiepp::BlankLineMode::CollapseAll : jiepp::BlankLineMode::Markers,
            env.is_standard_pragma_style());

        if (Issue::error_count_ >= 1) {
            // E3: reached the end, but with one or more non-aborting errors
            // counted along the way (U2). gcc/clang both still exit 1 here;
            // -o and any dependency file are never opened/written, and
            // neither touches an existing file at that path (destination
            // table). Unlike the aborted (E4) branch above, -dM's own row
            // in the destination table differs from the plain-output rows:
            // since processing reached the end, `env`'s macro table is
            // complete, so -dM still prints it in full on a stdout
            // destination ("-dM（-oなし、または-o -）: マクロの一覧を全部
            // 出す") -- only -o FILE (row below it) silently produces
            // nothing. -M/-MM without -MF (not_out, but not opts.dM) prints
            // nothing on either destination: a partial dependency list
            // would mislead a Make-based build into thinking it has every
            // prerequisite when it does not.
            if (pp_output_to_stdout) {
                if (opts.dM)
                    dump_macros(env, std::cout);
                else if (!not_out)
                    emit_tokens(ots);
            }
            return 1;
        }

        bool dep_has_separate_file = dep && effective_dep_file.has_value() && !effective_dep_file->empty();

        // E4b: the dependency-file write and -o open are wrapped in their
        // own try -- gcc has already flushed its preprocessed output before
        // either of these can fail, so a PP10 here gets the same
        // partial-output treatment as E4/E4a (plus the same "remove only
        // what this run wrote" cleanup as the outer catch below, duplicated
        // here rather than shared with it: by the time the outer catch
        // runs, `ots`/`env` are already out of scope, so it cannot print
        // anything itself).
        try {
            // Write the separate dependency file (-MF / -MD / -MMD auto-named)
            // before -o is opened: this write is completely independent of the
            // main output, so if it fails, -o must not be touched at all -- a
            // pre-existing -o from an earlier successful run stays byte-for-byte
            // intact, and a fresh, correct -o is never opened only to be deleted
            // again if it were to fail (see SPECIFICATION.md section 13,
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
        } catch (const Issue::Exception&) {
            // E4b: -o or the dependency file could not be opened (PP10).
            if (output_file.is_open()) {
                output_file.close();
                if (opts.output_filepath) {
                    std::error_code ec;
                    fs::remove(*opts.output_filepath, ec);
                }
            }
            if (!dep_file_written.empty()) {
                std::error_code ec;
                fs::remove(dep_file_written, ec);
            }
            if (pp_output_to_stdout && !not_out)
                emit_tokens(ots);
            return 1;
        }
        return 0;
    } catch (const Issue::Exception&) {
        // Do not leave a truncated/empty -o target behind on failure: a
        // Make-based build system must reprocess the file on the next run,
        // not treat a 0-byte, freshly-mtime'd file as an up-to-date target.
        // Reached only by exceptions from *before* the E4a -include/expand
        // guard above (e.g. a malformed -D/-U, an env-fixup failure, or the
        // stdin+-MD/-MMD-without--MF check): `ots` is always still empty
        // here, unlike the E4a/E4b catches above, so there is nothing to
        // print.
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
