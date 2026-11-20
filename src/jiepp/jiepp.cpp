#include "jiepp.hpp"
#include "option.hpp"
#include "../core/preprocessor.hpp"
#include "../core/line_compaction.hpp"
#include "../env/env.hpp"
#include "../loader/token.hpp"
#include "../loader/lexer.hpp"
#include "../loader/loader.hpp"
#include "../macro/macro.hpp"
#include "../env/issue.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <set>
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

// Escape a dependency prerequisite/target path for Makefile syntax, mirroring
// gcc/clang's mkdeps munge(): '$' doubles to "$$"; '#' and whitespace (space,
// tab) get a backslash prefix followed by the *original* character itself
// (not a symbolic C-style escape letter), so a tab becomes backslash + an
// actual tab byte, exactly like a space becomes backslash + an actual space
// byte. The colon is deliberately left unescaped (gcc does not escape it
// either, and escaping it would break Windows drive-letter paths such as
// "C:/..."). It is applied to prerequisites, to a target derived from the
// input/-o name and to -MQ values; an -MT target is passed through verbatim
// (see format_dep_target() below), matching real gcc/clang.
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

// Format the dependency-rule target, the text before the ':'. -MT and -MQ
// values come in the order written, separated by a space: an -MT value
// verbatim, an -MQ value Make-escaped like a prerequisite (gcc/clang). Without
// either, the target is derived and Make-escaped:
//   - -MD/-MMD with -o FILE: FILE as written (clang; gcc uses the input name).
//     "-o -" is stdout, not a file, so it is not used;
//   - otherwise the input file's name without its directory and with its
//     extension replaced by ".output" (gcc/clang use ".o": a deliberate
//     difference, SPEC 17), or "-.output" for stdin (clang: "-.o").
// -MF only controls the destination file; it never affects the target.
std::string format_dep_target(const JieppOptions& opts) {
    if (!opts.dep_targets.empty()) {
        std::string text;
        for (const auto& t : opts.dep_targets) {
            if (!text.empty())
                text += ' ';
            text += t.escape ? escape_make_path(t.text) : t.text;
        }
        return text;
    }
    if ((opts.MD || opts.MMD) && opts.output_filepath.has_value() && !output_is_stdout(opts))
        return escape_make_path(*opts.output_filepath);
    if (!opts.input_filepaths.empty() && opts.input_filepaths[0] != "-") {
        fs::path p(opts.input_filepaths[0]);
        p.replace_extension(".output");
        return escape_make_path(p.filename().generic_string());
    }
    return "-.output";
}

// Write Makefile-style dependency rules. `target_text` must already be in
// its final, rule-ready form (see format_dep_target()); it is written as-is
// here, never re-escaped. The main input comes first among the prerequisites
// (gcc/clang: `main.c pre.h` for -include pre.h), the others in the order they
// were read; `main_index` is its index in env.dependencies() (out of range
// for stdin, which has none). With `phony` (-MP), an empty rule follows for
// each prerequisite except the main input, like gcc/clang -MP.
void write_dep_rules(const std::string& target_text, DepMode dep_mode, std::ostream& output, const Env& env,
                     bool phony, std::size_t main_index) {
    // Collect dependencies with deduplication by resolved_path
    // Track resolved paths we've already seen (only first display_path is output)
    std::unordered_set<std::string> seen_resolved;
    std::vector<std::string> deps;
    std::vector<std::string> phony_targets;
    const auto& all = env.dependencies();
    const bool has_main = main_index < all.size();
    const std::string main_resolved = has_main ? all[main_index].resolved_path : std::string();

    auto add = [&](const FileContext::Dependency& dep) {
        // Skip system includes if -MM mode
        if (dep_mode == DepMode::USER && dep.is_system)
            return;
        // Deduplicate by resolved_path (only add first occurrence of each resolved path)
        if (seen_resolved.insert(dep.resolved_path).second) {
            deps.push_back(escape_make_path(dep.display_path));
            if (phony && !(has_main && dep.resolved_path == main_resolved))
                phony_targets.push_back(deps.back());
        }
    };
    if (has_main)
        add(all[main_index]);
    for (std::size_t i = 0; i < all.size(); ++i)
        if (!(has_main && i == main_index))
            add(all[i]);

    // Format output
    output << target_text << ":";
    for (const auto& d : deps)
        output << " \\\n  " << d;
    output << "\n";
    for (const auto& p : phony_targets)
        output << p << ":\n";
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
// SEVERE + {10,11,12,13,14,60,61}. PP64 (MAX_EXPANSION_STEPS_EXCEEDED) is
// SEVERE, so it stops processing without being listed here.
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

// Context of a PP10 for a failed write to stdout (also for "-o -").
const char* const STDOUT_CONTEXT = "<stdout>";

// A8: an output (-o, the dependency file or stdout) could not be written,
// flushed or closed: PP10 with the path (or STDOUT_CONTEXT) at the CLI
// location. PP10 is blocked by default and aborts in continue mode, so
// ISSUE throws; the explicit throw covers a caller that unblocked it.
[[noreturn]] void output_failure(const std::string& context) {
    ISSUE(FILE_ERROR, context);
    throw Issue::Exception(Issue::Code::FILE_ERROR);
}

// output_failure() for the paths that are already failing (E3, E4, the E4b
// catch): the diagnostic is printed and the exception is swallowed, since
// the caller returns 1 anyway.
void report_output_failure(const std::string& context) {
    try {
        output_failure(context);
    } catch (const Issue::Exception&) {
    }
}

// Removes an output file this run opened for writing, after a failure. The
// stream is closed first (its result is ignored: Windows cannot delete an
// open file, and the failure is already being reported). Only a regular
// file is removed: is_regular_file() follows symlinks (stat), like gcc's
// delete_if_ordinary, so "-o /dev/full", a FIFO, a symlink to either and a
// dangling symlink are all kept, while a symlink to a regular file is
// removed as a link. Only the error_code overloads are used: this runs
// inside catch blocks, where a filesystem_error would escape as PP01.
void remove_if_regular(std::ofstream& stream, const std::string& path) {
    stream.close();
    std::error_code ec;
    if (fs::is_regular_file(path, ec))
        fs::remove(path, ec);
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
    // A8: true from the moment -o was opened successfully. output_file's own
    // is_open() cannot be used for the cleanup below: it is false again
    // after a close() that failed.
    bool output_file_opened = false;
    // Path of a separate dependency file (-MF / -MD / -MMD auto-named) that
    // this run has opened for writing, if any (set as soon as the open
    // succeeds) -- remembered so the E4b catch below can remove it if its
    // own write/close fails or a *later* step (opening or writing -o,
    // writing stdout) fails: otherwise a truncated .d file, or a
    // correctly-written one naming an output that was never produced (or
    // never updated), would be left behind for a Make-based build to trust
    // incorrectly. See the "remove only what this run wrote" rule documented
    // at the catch site. Declared here, not inside the try, so the catch
    // can close it before removing the file (remove_if_regular()).
    std::ofstream dep_output;
    std::string dep_file_opened;
    // A8: set before anything is written to std::cout, so an error path
    // never sends the output a second time.
    bool stdout_attempted = false;

    // E4a: hoisted above the outer try (along with `ots` and `emit_tokens`
    // below) so a partial-output path reached via a nested catch --
    // "aborted mid-expand" (E4a) or "-o/dep-file open, write or close
    // failed after the output was already built" (E4b) -- can still see
    // and print whatever of `ots` was produced. Both depend only on `opts`, not on anything
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
        // Accumulate into one buffer and write once: each operator<< on
        // cout/ofstream takes the C runtime's per-call FILE lock (and stdio
        // sync), so a write per token is measurably slower on large outputs.
        std::string buf;
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
                buf += t.text;
            }
        } else {
            for (const auto& t : tokens)
                buf += t.text;
        }
        actual_output->write(buf.data(), static_cast<std::streamsize>(buf.size()));
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
                output_file_opened = output_file.is_open();
                output_stream = &output_file;
            }
        };

        // U4: --silent/-w/-Werror must be in effect before -D/-U are
        // processed below (apply_define_option() and apply_undef_option()
        // can both raise diagnostics -- an invalid -D/-U name, a
        // redefinition warning, ...), not only for the main
        // input's own expansion. Previously these three were set after the
        // -D/-U block, so e.g. `--silent -D=1` still printed its PP73.
        if (opts.silent)
            Issue::silent_ = true;
        if (opts.suppress_warnings)
            Issue::suppress_warnings_ = true;
        if (opts.werror)
            Issue::werror_ = true;
        // --error-limit: the CLI's default (1000, or 100 in a sandbox build)
        // applies when the option is absent; the library's default is 0.
        Issue::error_limit_ = opts.error_limit.value_or(DEFAULT_ERROR_LIMIT);
        // -W<name> / -Wno-<name> / -Werror=<name> / -Wno-error=<name>
        // (SPEC 13), applied here, before -D/-U, like the options above.
        // opts.warnings is in command-line order; folding it front to back
        // gives each code its last on/off option (-W, -Wno-, -Werror=) and,
        // separately, its last error option (-Werror=, -Wno-error=), as in
        // gcc/clang. -Wno-error=<name> also exempts the code from -Werror,
        // whichever comes first (also gcc/clang). A winning -Wno-<name> is
        // {#ignore <code>}, which happen() checks before promotion, so it
        // hides a -Werror=<name> that came earlier.
        struct WarningState {
            std::optional<bool> on;
            std::optional<bool> error;
        };
        std::map<Issue::Code, WarningState> warning_states;
        for (const auto& w : opts.warnings) {
            WarningState& st = warning_states[w.code];
            switch (w.action) {
            case WarningAction::ENABLE:  st.on = true; break;
            case WarningAction::DISABLE: st.on = false; break;
            case WarningAction::PROMOTE: st.on = true; st.error = true; break;
            case WarningAction::DEMOTE:  st.error = false; break;
            }
        }
        for (const auto& [code, st] : warning_states) {
            if (st.on == true)
                Issue::enable(code);
            else if (st.on == false)
                Issue::add_ignoring(code);
            if (st.error == true)
                Issue::add_werror(code);
            else if (st.error == false)
                Issue::remove_werror(code);
        }

        // A -D whose NAME is empty (-D=1) is a malformed option, not a bad
        // definition: PP73 stops the run before any -D/-U is applied, like a
        // missing value (PP72) does in parse_args().
        std::vector<std::pair<std::string, std::string>> defines(opts.macro_options.size());
        for (std::size_t k = 0; k < opts.macro_options.size(); ++k)
            if (opts.macro_options[k].kind == MacroOption::Kind::DEFINE)
                defines[k] = define_macro_option(opts.macro_options[k].arg);

        // U5: -D's macro bodies follow the same -nC policy as {#define}
        // (setup() sets Env's remove_comments before processing -D below).
        Env env = setup({}, opts.remove_comments);

        // -D and -U apply in the order written (-UX -DX=1 defines X; -DX=1
        // -UX does not). Like a {#define} / {#undef} in the source, one that
        // is rejected (an invalid name, a duplicate parameter, 'defined', ...)
        // is reported and counted, then the rest of the options and the input
        // are processed: the run ends with exit code 1 (gcc/clang). Only a
        // code that stops processing outright aborts here.
        {
            Issue::ContinueMode continue_guard(jiepp_continue_abort_codes());
            for (std::size_t k = 0; k < opts.macro_options.size(); ++k) {
                if (opts.macro_options[k].kind == MacroOption::Kind::DEFINE)
                    apply_define_option(defines[k], env);
                else
                    apply_undef_option(opts.macro_options[k].arg, env);
            }
        }

        if (opts.remove_comments)
            env.fix_remove_comments(true);
        if (opts.dD)
            env.set_dd_mode(true);
        if (opts.max_include_depth)
            env.fix_max_include_depth(*opts.max_include_depth);
        if (opts.max_expansion_depth)
            env.fix_max_expansion_depth(*opts.max_expansion_depth);
        if (opts.max_expansion_steps)
            env.set_max_expansion_steps(static_cast<std::uint64_t>(*opts.max_expansion_steps));
        if (opts.max_include_count)
            env.set_max_include_count(*opts.max_include_count);
        // C11: before the -include files are read; 0 (the default in every
        // build) is no limit.
        if (opts.max_input_bytes)
            env.set_max_input_bytes(*opts.max_input_bytes);
        if (opts.max_if_nesting)
            env.fix_max_if_nesting(*opts.max_if_nesting);
        if (opts.max_blank_lines)
            env.fix_max_blank_lines(*opts.max_blank_lines);
        if (opts.pp_output_pragma_style)
            env.fix_pragma_style(*opts.pp_output_pragma_style);

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
        // check just above) is CLI-shaped misuse that still aborts (except
        // -D/-U, which run in their own continue mode above: only a code in
        // the abort set or SEVERE stops them); everything after it (E4b's
        // -o/dep-file open, write and close, the stdout checks) raises only
        // PP10, already in the abort set regardless of continue_mode_, so
        // where this guard's scope ends
        // does not change behavior there either.
        bool aborted = false;
        // Index in env.dependencies() of the main input file's entry (none
        // for stdin); see write_dep_rules().
        std::size_t main_dep_index = static_cast<std::size_t>(-1);
        {
            Issue::ContinueMode continue_guard(jiepp_continue_abort_codes());
            try {
                // -include: force-include files before main input. They are
                // included from the command line, one level below the main
                // input's own (__INCLUDE_LEVEL__ is 1 in such a file, 2 in
                // what it includes; gcc/clang).
                env.set_include_level_base(1);
                for (const auto& include_filepath : opts.include_filepaths) {
                    expand(include_filepath, Loader::LoadType::INCLUDE, ots, env);
                }
                env.set_include_level_base(0);

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
                    FileContext::FileScope file_guard(env, "<stdin>", /*from_disk=*/false);
                    Issue::LineGuard line_guard(1, dispath);
                    ots.push_back(Token::line_pragma(0, dispath, env.is_standard_pragma_style()));
                    ots.push_back(Token::newline());
                    // C11: bounded read (PP67); a read error, including
                    // std::ferror(stdin), is PP10 '<stdin>' at this location.
                    const std::optional<std::string> text =
                        Loader::read_stream(std::cin, env, "<stdin>", /*check_stdio_stdin=*/true);
                    auto its = iec3_tokens_from_string(text ? *text : std::string(),
                                                       env.get_remove_comments(), 1);
                    expand(its, ots, env);
                } else {
                    // Exactly one input file here: more than one was already
                    // rejected (PP13) before this point.
                    // -MP leaves the main input out of the empty rules: it is
                    // the dependency expand() records first for this file.
                    main_dep_index = env.dependencies().size();
                    // Only --disppath names the file; otherwise it is shown by
                    // the path it is reached by, which is dispath unless the
                    // file is found under a system path.
                    expand(opts.input_filepaths[0], Loader::LoadType::INCLUDE, ots, env,
                           opts.disppath.has_value() ? std::optional<std::string>(dispath)
                                                     : std::nullopt);
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
            // path is touched. A8: a failed write of that partial output adds
            // PP10 '<stdout>' after the diagnostic that stopped processing.
            if (pp_output_to_stdout && !not_out) {
                jiepp::compact_blank_lines(ots, env.get_max_blank_lines(),
                    opts.no_line_markers ? jiepp::BlankLineMode::CollapseAll : jiepp::BlankLineMode::Markers,
                    env.is_standard_pragma_style());
                stdout_attempted = true;
                emit_tokens(ots);
                if (!flush_stdout())
                    report_output_failure(STDOUT_CONTEXT);
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
            // prerequisite when it does not. A8: a failed write of what was
            // printed adds PP10 '<stdout>' after the counted errors.
            if (pp_output_to_stdout) {
                if (opts.dM) {
                    stdout_attempted = true;
                    dump_macros(env, std::cout);
                } else if (!not_out) {
                    stdout_attempted = true;
                    emit_tokens(ots);
                }
                if (stdout_attempted && !flush_stdout())
                    report_output_failure(STDOUT_CONTEXT);
            }
            return 1;
        }

        // -MF - writes the rules to stdout (like -o -), not to a file named
        // "-" (gcc/clang).
        const bool dep_to_stdout = dep && effective_dep_file.has_value() && *effective_dep_file == "-";
        bool dep_has_separate_file = dep && effective_dep_file.has_value() && !effective_dep_file->empty()
                                     && !dep_to_stdout;

        // E4b: the dependency-file write and the -o open and write are
        // wrapped in their own try -- gcc has already flushed its
        // preprocessed output before any of these can fail, so a PP10 here
        // gets the same partial-output treatment as E4/E4a, plus the "remove
        // only what this run wrote" cleanup (see the catch). -o and the
        // dependency file are opened only inside this try, so this catch is
        // the only place that removes them. A8: besides open failures this
        // covers a failed write/close of the dependency file or -o and a
        // failed write/flush of stdout.
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
                dep_output.open(*effective_dep_file, std::ios::out | std::ios::binary);
                if (!dep_output)
                    ISSUE(FILE_ERROR, *effective_dep_file);
                if (dep_output.is_open())
                    dep_file_opened = *effective_dep_file;
                write_dep_rules(dep_target_text, opts.dep_mode, dep_output, env, opts.MP, main_dep_index);
                // A8: close() flushes; a failed write leaves badbit and a
                // failed flush or close sets failbit.
                dep_output.close();
                if (dep_output.fail())
                    output_failure(*effective_dep_file);
            }

            // -o (or stdout) is only needed when something will actually be
            // written to it: the main preprocessed content (!not_out), a -dM
            // macro dump, or dependency rules that have no separate file to go
            // to (-M/-MM without -MF). A dependency-only invocation that already
            // has a separate dep file (e.g. "-M -MF x.d -o out.iec") has nothing
            // left to put in -o, so -o is deliberately left unopened/unwritten
            // (matching gcc, whose -M/-MM never touch the compiler's normal
            // output file either).
            bool need_dep_to_stream = dep && !dep_has_separate_file && !dep_to_stdout;
            bool need_output_stream = !not_out || opts.dM || need_dep_to_stream;

            if (need_output_stream) {
                open_output();
                const bool to_stdout = output_stream == &std::cout;
                if (to_stdout)
                    stdout_attempted = true;
                emit_tokens(ots);

                if (opts.dM) {
                    dump_macros(env, *output_stream);
                }

                if (need_dep_to_stream) {
                    // No dep file: write dep rules to main output stream (-M / -MM without -MF)
                    write_dep_rules(format_dep_target(opts), opts.dep_mode, *output_stream, env, opts.MP,
                                    main_dep_index);
                }

                // A8: check that everything got out. -o is closed here (not
                // by its destructor) so a failed flush or close is seen.
                if (to_stdout) {
                    if (!flush_stdout())
                        output_failure(STDOUT_CONTEXT);
                } else {
                    output_file.close();
                    if (output_file.fail())
                        output_failure(*opts.output_filepath);
                }
            }

            // -MF -: the rules go to stdout after whatever the main output
            // put there (-o is not involved: with -M it is not even opened).
            if (dep_to_stdout) {
                stdout_attempted = true;
                write_dep_rules(format_dep_target(opts), opts.dep_mode, std::cout, env, opts.MP,
                                main_dep_index);
                if (!flush_stdout())
                    output_failure(STDOUT_CONTEXT);
            }
        } catch (const Issue::Exception&) {
            // E4b: -o or the dependency file could not be opened, written or
            // closed, or stdout could not be written (PP10). Remove what this
            // run opened for writing (regular files only, see
            // remove_if_regular()). Do not leave a truncated/empty -o target
            // behind: a Make-based build system must reprocess the file on
            // the next run, not treat a 0-byte, freshly-mtime'd file as an
            // up-to-date target.
            if (output_file_opened)
                remove_if_regular(output_file, *opts.output_filepath);
            // Likewise the separate dependency file: if it was written
            // earlier in this same run but a later step (e.g. opening -o)
            // then failed, the .d file either references an output that was
            // never (re)produced, or -- if it already existed -- was just
            // truncated with content that was never paired with a matching
            // successful output. Either way it is this run's own write, so
            // it is removed; a dep file from an *earlier*, unrelated run
            // that this run never touched is left alone, matching gcc's
            // convention of not scrubbing stale files it did not itself
            // just write.
            if (!dep_file_opened.empty())
                remove_if_regular(dep_output, dep_file_opened);
            // A stdout destination still gets the full output when the
            // failure was elsewhere (a dependency-file failure) -- but never
            // a second copy when stdout itself was already written.
            if (pp_output_to_stdout && !not_out && !stdout_attempted) {
                stdout_attempted = true;
                emit_tokens(ots);
                if (!flush_stdout())
                    report_output_failure(STDOUT_CONTEXT);
            }
            return 1;
        }
        return 0;
    } catch (const Issue::Exception&) {
        // Reached only by exceptions from *before* the E4a -include/expand
        // guard above (e.g. PP73 for -D=1, a SEVERE or abort-set code raised
        // while -D/-U are applied, an env-fixup failure, or the
        // stdin+-MD/-MMD-without--MF check): the E4a and E4b catches above
        // handle every Issue::Exception raised inside their own try blocks.
        // `ots` is still empty, so there is nothing to print, and neither
        // -o nor a dependency file has been opened (both are opened only
        // inside the E4b try), so there is nothing to remove.
        return 1;
    }
}
