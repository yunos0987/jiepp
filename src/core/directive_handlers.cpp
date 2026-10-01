#include "preprocessor.hpp"
#include "preprocessor_internal.hpp"
#include "expand_helpers.hpp"

#include "../env/lineno.hpp"
#include "../env/param_constants.hpp"
#include "../loader/lexer.hpp"
#include "../loader/directive_parser.hpp" //kludge
#include "../macro/macro.hpp"
#include "../util/text.hpp"
#include "../util/iec_61131-3.hpp"

#include <cctype>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

// A path operand is '...' / "..." (a string literal, which the directive
// decoding left with its IEC 61131-3 escapes as written -- SPEC §2 -- so
// they are decoded here by decode_path_literal(), keeping the meaning
// {#include 'a$$b.iec'} always had) or <...> (copied as is; its escapes
// were decoded with the rest of the operand). The literal must be the
// whole operand: 'a$' (the quote is escaped, so the literal never closes)
// is not a path.
bool strip_path(const std::string_view raw_path, std::string& path, bool& syspath_only) {
    auto p = Util::trim_view(raw_path);
    syspath_only = false;
    if (p.size() < 2)
        return false;
    if (p.front() == '\'' || p.front() == '"') {
        if (string_literal_end(p, 0) != p.size())
            return false;
        path = decode_path_literal(p);
        return true;
    }
    if (p.front() == '<' && p.back() == '>') {
        syspath_only = true;
        path = std::string(p.substr(1, p.size() - 2));
        return true;
    }
    return false;
}

// A '//' comment from a directive operand has no newline after it once it
// is spliced into the output ({#token}, a macro body), so it would comment
// out the rest of the output line ("{#token a // c} b;" gave "a // c b;").
// Rewrite it as a block comment, like gcc/clang -CC do for a '//' comment in
// a macro expansion: "//x" -> "/*x*/", "//!x" -> "/*!x*/". The text is kept
// as is, like clang (a "*/" inside x ends the block comment early). A '//'
// comment that already ends with its newline (num_of_lines != 0, from a
// $n/$r escape) cannot swallow anything and is kept.
void line_comments_to_block_comments(std::vector<Token>& ts) {
    for (auto& t : ts) {
        if ((t.type != Token::C && t.type != Token::DOCUMENT) || t.num_of_lines != 0 ||
            t.text.compare(0, 2, "//") != 0)
            continue;
        t.text = "/*" + t.text.substr(2) + "*/";
    }
}

// item a: the shared upper bound for both {#line}/{#set_line}/{#set-line}
// and the marker form {#:N} -- 2^32 - 1, matching gcc/clang's unsigned
// 32-bit line counter. A value above this is PP41 (clang; gcc instead
// warns and wraps -- jiepp follows clang where the two disagree, per the
// coordinator's rev2 decision).
constexpr LineNo kLineNoMax = 4294967295;

// C3, item a (rev2): strict, unsigned-decimal-only line-number operand
// parse. Unlike std::istream >> int (the previous implementation), this
// does not accept leading whitespace beyond a plain trim, a leading
// '+'/'-' sign, or a value above kLineNoMax. On success, returns the
// parsed value and the (unparsed, not yet trimmed) remainder of `s` right
// after the digit run -- e.g. an optional quoted filepath -- for the
// caller to validate. Both the named form and the marker form share this
// same range (0..4294967295); there is no longer a form-specific bound.
std::optional<std::pair<LineNo, std::string_view>> parse_lineno_operand(std::string_view s) {
    s = Util::ltrim_view(s);
    std::size_t i = 0;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9')
        ++i;
    if (i == 0)
        return std::nullopt; // no leading digit: empty, signed ("+5"/"-5"), or non-numeric
    std::uint64_t v = 0;
    for (std::size_t j = 0; j < i; ++j) {
        v = v * 10 + static_cast<std::uint64_t>(s[j] - '0');
        if (v > static_cast<std::uint64_t>(kLineNoMax))
            return std::nullopt; // above 4294967295 (C3, item a)
    }
    return std::make_pair(static_cast<LineNo>(v), s.substr(i));
}

// Resolve a {#syspath} operand relative to the directory of the file
// containing the directive (same rule as {#include}'s relative-path lookup
// in Loader::fullpath: an absolute operand, or a Windows '/x'/'C:x' operand
// that fs::path::operator/ treats as rooted, replaces the base outright).
// When no file is currently open (e.g. library/string input, stdin), falls
// back to the current working directory.
std::string resolve_syspath_base(const std::string& raw_syspath, Env& env) {
    const std::string& cur_file = env.current_file();
    fs::path base_dirpath = cur_file.empty() ? fs::current_path() : fs::path(cur_file).parent_path();
    fs::path candidate = base_dirpath / raw_syspath;
    return candidate.lexically_normal().generic_string();
}

// Number of tokens forming '...' at ts[i], or 0. The lexer emits every '.'
// as its own one-character Token::ANY, so '...' is three adjacent '.'
// tokens; ". . ." (whitespace between) is not an ellipsis, as in C/clang.
std::size_t ellipsis_at(const std::vector<Token>& ts, std::size_t i) {
    for (std::size_t k = i; k < i + 3; ++k) {
        if (k >= ts.size() || ts[k].type != Token::ANY || ts[k].text != ".")
            return 0;
    }
    return 3;
}

} // namespace

namespace jiepp::preprocessor_detail {

// Returns true iff a macro was actually (re)defined. Every rejection path
// (ISSUE + early return in continue mode or under {#ignore}) returns false,
// so the caller does not echo a directive that defined nothing (-dD).
bool handle_define(const std::string& raw_arg, Env& env, std::string* defined_name) {
    // Body comments follow the -nC policy like the rest of the input: kept
    // by default (gcc/clang -CC keep them in macro bodies too), whitespace
    // with -nC (gcc/clang without -C). Either way a comment is whitespace to
    // the checks below (Token::C has MASK_WS), so the definition is the same.
    auto ts = lex_operand(raw_arg, env.get_remove_comments());
    ts = ts_ltrim(std::move(ts));
    // C1: like gcc/clang, the macro name must be an identifier (comments
    // are whitespace, already stripped by ts_ltrim above). A missing name
    // and a non-identifier name are separate diagnoses so the message names
    // the actual operand in the latter case.
    if (ts.empty()) {
        ISSUE(INVALID_DEFINE_SYNTAX, "macro name missing");
        return false;
    }
    if (ts[0].type != Token::ANY || !iec3_is_identifier(ts[0].text)) {
        ISSUE(INVALID_DEFINE_SYNTAX,
              "macro name must be an identifier: " + Util::escape_line_breaks(raw_arg));
        return false;
    }

    std::string name = ts[0].text;
    if (name == "defined") {
        ISSUE(OPERATION_NOT_ALLOWED, "redefine 'defined'");
        return false;
    }

    // Like gcc/clang ("ISO C99 requires whitespace after the macro name"),
    // NAME directly followed by a token other than '(' -- e.g.
    // {#define A.B 1}, {#define X-1 2}, {#define T#1s 1} -- is a warning.
    // The definition proceeds unchanged (NAME = A, body ".B 1"). A comment
    // (Token::C has MASK_WS) and a decoded $n/$l newline are whitespace; a
    // DOCUMENT comment (*! *) is a body token, so it warns. Checked before
    // the body's '@@' check, so {#define A@@B} gives PP38 then PP32, like
    // gcc/clang.
    if (ts.size() > 1 && !(ts[1].type & Token::MASK_WS) && ts[1].type != Token::LP)
        ISSUE(MISSING_WHITESPACE_AFTER_MACRO_NAME,
              Util::escape_line_breaks(Util::trim_view(raw_arg)));

    std::size_t i = 1;
    bool is_function = (i < ts.size() && ts[i].type == Token::LP);
    std::vector<std::string> param_names;
    // O(1) duplicate-parameter lookup (was an O(k^2) nested scan). The views
    // point into ts[i].text: `ts` is not modified until the parameter loop
    // is done, so they stay valid. Views into param_names would dangle when
    // the vector reallocates.
    std::unordered_set<std::string_view> seen_params;
    bool named_variadic = false;

    // Like gcc/clang: the first duplicate parameter is reported once and the
    // whole {#define} is abandoned -- the macro is not defined and any
    // existing definition of `name` is left untouched.
    // A regular parameter named __VA_ARGS__/__VA_ARGC__ collides with the
    // implicit names a trailing '...' introduces (FunctionMacro's args_ map
    // would silently let the variadic entry win), so it is a duplicate too.
    // A GNU named variadic `args...` introduces only __VA_ARGC__ (`args`
    // replaces __VA_ARGS__), so there __VA_ARGS__ is an ordinary name.
    auto variadic_name_clash = [&](bool named) -> bool {
        if (!named && seen_params.count(FunctionMacro::VA_ARGS)) {
            ISSUE(DUPLICATE_MACRO_PARAMETER, FunctionMacro::VA_ARGS);
            return true;
        }
        if (seen_params.count(FunctionMacro::VA_ARGC)) {
            ISSUE(DUPLICATE_MACRO_PARAMETER, FunctionMacro::VA_ARGC);
            return true;
        }
        return false;
    };

    if (is_function) {
        // Parameter list, following C17 6.10.3 / clang:
        //   '(' ')'  |  '(' '...' ')'  |  '(' id (',' id)* [',' '...' | '...'] ')'
        // where id is an identifier as the lexer tokenizes it
        // (iec3_is_identifier). The last id may be followed directly by
        // '...' (GNU named variadic, accepted by gcc/clang). Whitespace,
        // newlines and comments may appear between any two items. The first
        // error found left to right is reported once and the whole
        // {#define} is abandoned, like PP33/PP36:
        // nothing is defined, an existing definition is kept, and -dD does
        // not echo it.
        auto skip_ws = [&]() {
            while (i < ts.size() && (ts[i].type & Token::MASK_WS))
                ++i;
        };
        auto list_error = [&](const std::string& what) {
            ISSUE(INVALID_DEFINE_SYNTAX,
                  what + " in macro parameter list: " + Util::escape_line_breaks(raw_arg));
        };
        ++i; // '('
        skip_ws();
        if (i < ts.size() && ts[i].type == Token::RP) {
            ++i; // "()": no parameters
        } else {
            for (;;) {
                // Expect a parameter name or '...'.
                if (i >= ts.size()) {
                    list_error("missing ')'");
                    return false;
                }
                if (std::size_t n = ellipsis_at(ts, i)) {
                    if (variadic_name_clash(false))
                        return false;
                    param_names.push_back(FunctionMacro::VA_SYM);
                    i += n;
                    skip_ws();
                    if (i >= ts.size()) {
                        list_error("missing ')'");
                        return false;
                    }
                    if (ts[i].type != Token::RP) {
                        ISSUE(INVALID_VARIADIC_PLACEMENT, Util::escape_line_breaks(raw_arg));
                        return false;
                    }
                    ++i; // ')'
                    break;
                }
                if (ts[i].type != Token::ANY || !iec3_is_identifier(ts[i].text)) {
                    list_error("expected parameter name or '...'");
                    return false;
                }
                if (!seen_params.insert(ts[i].text).second) {
                    ISSUE(DUPLICATE_MACRO_PARAMETER, ts[i].text);
                    return false;
                }
                param_names.push_back(ts[i].text);
                ++i;
                // Expect ',' or ')'.
                skip_ws();
                if (i >= ts.size()) {
                    list_error("missing ')'");
                    return false;
                }
                if (ts[i].type == Token::RP) {
                    ++i;
                    break;
                }
                if (ts[i].type != Token::SEP) {
                    if (std::size_t n = ellipsis_at(ts, i)) {
                        // GNU named variadic `args...` (gcc/clang): `args`
                        // takes the variable arguments instead of
                        // __VA_ARGS__. As after a plain '...', only ')' may
                        // follow.
                        if (variadic_name_clash(true))
                            return false;
                        named_variadic = true;
                        i += n;
                        skip_ws();
                        if (i >= ts.size()) {
                            list_error("missing ')'");
                            return false;
                        }
                        if (ts[i].type != Token::RP) {
                            ISSUE(INVALID_VARIADIC_PLACEMENT, Util::escape_line_breaks(raw_arg));
                            return false;
                        }
                        ++i; // ')'
                        break;
                    }
                    list_error("expected ',' or ')'");
                    return false;
                }
                ++i; // ','
                skip_ws();
            }
        }
    }

    while (i < ts.size() && (ts[i].type & Token::MASK_WS))
        ++i;
    std::vector<Token> body(ts.begin() + i, ts.end());
    line_comments_to_block_comments(body);

    // C6/U1: '@@' (GLUE) at either end of the replacement list has nothing
    // to paste with, like gcc/clang ("'##' cannot appear at either end of
    // macro expansion"). body's leading whitespace is already stripped by
    // the skip above; only the trailing side needs a reverse scan past
    // whitespace here to find the true last token.
    if (!body.empty()) {
        std::size_t last = body.size();
        while (last > 0 && (body[last - 1].type & Token::MASK_WS))
            --last;
        if (last > 0 && (body.front().type == Token::GLUE || body[last - 1].type == Token::GLUE)) {
            ISSUE(INVALID_TOKEN_PASTING,
                  "'@@' cannot appear at either end of a macro expansion: " + Util::escape_line_breaks(raw_arg));
            return false;
        }
    }

    auto make_and_define = [&]() {
        if (is_function) {
            auto nm = std::make_unique<FunctionMacro>(param_names, body, named_variadic);
            if (env.exist(name)) {
                Macro* existing = env.lookup(name);
                if (!existing->equal(*nm))
                    ISSUE(MACRO_REDEFINED, name);
            }
            env.define(name, std::move(nm));
        } else {
            auto nm = std::make_unique<UserDefinedObjectMacro>(body);
            if (env.exist(name)) {
                Macro* existing = env.lookup(name);
                if (!existing->equal(*nm))
                    ISSUE(MACRO_REDEFINED, name);
            }
            env.define(name, std::move(nm));
        }
    };
    make_and_define();
    if (defined_name)
        *defined_name = name;
    return true;
}

// Returns true iff the {#undef} operand was accepted (a valid name, even
// if it was never defined); false if it was rejected (missing/non-identifier
// name) and nothing changed. See handle_define()'s matching comment: the
// caller uses this to decide whether to echo the directive under -dD.
bool handle_undef(const std::string& raw_arg, Env& env, std::string* undefined_name) {
    // C1: same name validation as {#define} (comments removed, as whitespace).
    auto ts = ts_trim(lex_operand(raw_arg, /*remove_comments=*/true));
    if (ts.empty()) {
        ISSUE(INVALID_DEFINE_SYNTAX, "macro name missing");
        return false;
    }
    if (ts[0].type != Token::ANY || !iec3_is_identifier(ts[0].text)) {
        ISSUE(INVALID_DEFINE_SYNTAX,
              "macro name must be an identifier: " + Util::escape_line_breaks(raw_arg));
        return false;
    }
    if (ts[0].text == "defined") {
        ISSUE(OPERATION_NOT_ALLOWED, "undef 'defined'");
        return false;
    }
    if (ts.size() > 1) {
        // Tokens after NAME (e.g. "{#undef A B}", "{#undef A(x)}"): like
        // gcc/clang ("extra tokens at end of #undef directive"), a warning,
        // and NAME alone is still undefined. Comments were removed above
        // (whitespace); a DOCUMENT comment (*! *) is a token and counts.
        ISSUE(EXTRA_TOKENS_AT_END_OF_DIRECTIVE,
              "{#undef " + Util::escape_line_breaks(Util::trim_view(raw_arg)) + "}");
    }
    env.undef(ts[0].text);
    if (undefined_name)
        *undefined_name = ts[0].text;
    return true;
}

void handle_tokenize(const std::string& raw_arg, Env& env, std::vector<Token>& ots) {
    auto ts = lex_operand(raw_arg, env.get_remove_comments());
    // Operand newlines are decoded escapes, not source lines (see
    // expand_operand_tokens()).
    ts_mark_output_only(ts);
    line_comments_to_block_comments(ts);
    expand(ts, ots, env);
}

void handle_stringize(const std::string& raw_arg,
                      Env& env,
                      std::vector<Token>& ots,
                      bool wide) {
    // The stringizing whitespace rule (R1-R5, expand_subst.cpp's
    // stringize_text()) applies to the macro-expanded operand, same as the
    // `@` operator: expand first (expand_operand_tokens(), unchanged "expand
    // first" semantics incl. PP29 reporting), then build the text at the
    // token level instead of via expand_operand_text()'s plain concatenation.
    std::string raw_text =
        jiepp::expand_detail::stringize_text(expand_operand_tokens(raw_arg, env));
    if (wide) {
        ots.push_back(Token::create(Token::WSTRING, Util::encode_iec_string(raw_text, '"')));
    } else {
        ots.push_back(Token::create(Token::STRING, Util::encode_iec_string(raw_text, '\'')));
    }
}

void handle_setline(const std::string& raw_arg, bool is_marker_form, Env& env, std::vector<Token>& ots) {
    // C4: a nameless, operand-less directive ({#}, {#:}, and their
    // whitespace-only variants) is gcc's "empty directive" -- a no-op, not
    // an error. A *named* form with an empty operand ({#line}, {#set_line})
    // still reaches the PP41 report below, via parse_lineno_operand()
    // rejecting an operand with no leading digit.
    if (is_marker_form && raw_arg.empty())
        return;

    // Comments around the operand are whitespace, as in C; expand_operand_text()
    // keeps them unless -nC.
    std::string arg = iec3_blank_out_comments(expand_operand_text(raw_arg, env));
    if (auto parsed = parse_lineno_operand(arg)) {
        auto [new_lineno, rest] = *parsed;
        // item a (rev2): both forms now accept the same range,
        // 0..4294967295, already enforced by parse_lineno_operand() above
        // (anything outside it fell through to nullopt, so this branch
        // never sees it). This replaces the two different ranges
        // introduced by commit 2cb1cc6 (U1(a): named form 1..2147483647,
        // marker form 0..2147483646 -- one less than the named form, so a
        // named directive's emitted N-1 marker was itself always a valid
        // marker value). wrap_lineno() below now guarantees that instead,
        // unconditionally, for every effective line number including 0.
        {
            // Read the whole remainder (not just one whitespace-delimited word) so
            // quoted filenames containing spaces are captured intact; then trim
            // and parse as a single path argument. A quoted path followed by
            // trailing garbage, or an unquoted/syspath-only ('<...>') path, is
            // rejected below instead of being silently accepted or truncated.
            std::string_view raw_fp = Util::trim_view(rest);
            std::string new_fp;
            bool syspath_only = false;
            bool ok = raw_fp.empty() ||
                (strip_path(raw_fp, new_fp, syspath_only) && !syspath_only);
            if (ok) {
                // C1/U1(a): a named form sets the line number of the *next*
                // physical line (gcc semantics) to new_lineno, by advancing
                // the running counter to one less than new_lineno -- the
                // nameless marker form's own "counter = new_lineno, next
                // line = new_lineno + 1" semantics, which stays unchanged.
                // item a: the counter wraps mod 2^32 (wrap_lineno(), see
                // lineno.hpp), like gcc/clang, so a named `{#line 0}` is
                // valid: new_lineno - 1 = -1 wraps to 4294967295, and the
                // next physical line is 0.
                const LineNo effective_lineno = wrap_lineno(is_marker_form ? new_lineno : new_lineno - 1);
                env.set_lineno(effective_lineno);
#ifdef JIEPP_SANDBOX
                // Sandbox: ignore filepath argument, keep original filepath
                {
                    std::string old_fp = Issue::filepath();
                    Issue::pop();
                    Issue::push({effective_lineno, old_fp});
                    ots.push_back(Token::line_pragma(effective_lineno, std::nullopt, env.is_standard_pragma_style()));
                    return;
                }
#else
                if (new_fp.empty()) {
                    std::string old_fp = Issue::filepath();
                    Issue::pop();
                    Issue::push({effective_lineno, old_fp});
                    ots.push_back(Token::line_pragma(effective_lineno, std::nullopt, env.is_standard_pragma_style()));
                    return;
                } else {
                    Issue::pop();
                    Issue::push({effective_lineno, std::string(new_fp)});
                    ots.push_back(Token::line_pragma(effective_lineno, new_fp, env.is_standard_pragma_style()));
                    return;
                }
#endif
            }
        }
    }
    // raw_arg is decoded, so re-escape it before it reaches the
    // diagnostic, so a $n/$r escape in the source does not split the
    // message onto multiple lines (same reasoning as {#define} above).
    ISSUE(INVALID_SETLINE_OPERAND, Util::escape_line_breaks(raw_arg));
}

void handle_syspath(const std::string& raw_arg, Env& env, std::vector<Token>& ots) {
    // Comments around the operand are whitespace, as in C; expand_operand_text()
    // keeps them unless -nC.
    std::string raw_path = iec3_blank_out_comments(expand_operand_text(raw_arg, env), /*header_name=*/true);
    std::string syspath;
    bool _;
    if (strip_path(raw_path, syspath, _)) {
        env.add_syspath(resolve_syspath_base(syspath, env));
        ots.push_back(Token::pragma("syspath", Util::encode_iec_string(syspath, '\''), env.is_standard_pragma_style()));
        return;
    }
    // raw_arg is decoded, so re-escape it before it reaches the
    // diagnostic, so a $n/$r escape in the source does not split the
    // message onto multiple lines (same reasoning as {#define} above).
    ISSUE(INVALID_PATH, Util::escape_line_breaks(raw_arg));
}

void handle_include(const std::string& raw_arg,
                    Env& env,
                    std::vector<Token>& ots,
                    bool syspath_only) {
    // Comments around the operand are whitespace, as in C; expand_operand_text()
    // keeps them unless -nC.
    std::string raw_path = iec3_blank_out_comments(expand_operand_text(raw_arg, env), /*header_name=*/true);
    std::string include_path;
    bool syspath_only_path;
    if (strip_path(raw_path, include_path, syspath_only_path)) {
        auto disp_path = fs::path(include_path).generic_string();

        Loader::LoadType loadtype = (syspath_only || syspath_only_path) ? Loader::LoadType::SINCLUDE : Loader::LoadType::INCLUDE;
        expand(include_path, loadtype, ots, env, disp_path);
        return;
    }
    // raw_arg is decoded, so re-escape it before it reaches the
    // diagnostic, so a $n/$r escape in the source does not split the
    // message onto multiple lines (same reasoning as {#define} above).
    ISSUE(INVALID_PATH, Util::escape_line_breaks(raw_arg));
}

void handle_message(const std::string& raw_arg, Env& env, Issue::Code code) {
    // Unlike the operand diagnostics above, this one is *not* re-escaped.
    // {#error}/{#warning}/{#info}/{#severe} print the user's message
    // verbatim. A newline typed directly in the directive has already been
    // folded into one space by the lexer (read_pragma_body()), like in any
    // multi-line directive; a $n/$r escape decodes to a real line break and
    // is printed as one. NewlineInStringAndMessageDirectives checks both.
    // A quote in the message text is not scanned for PP29, like clang's
    // #error/#warning (gcc does scan it).
    std::string msg = expand_operand_text(raw_arg, env, /*report_unterminated=*/false);
#ifdef JIEPP_SANDBOX
    // Sandbox: strip control characters to prevent log injection
    std::erase_if(msg, [](unsigned char c) {
        return (c < 0x20) && (c != '\n') && (c != '\r') && (c != '\t');
    });
#endif
    Issue::happen(code, msg);
}

void handle_ignore(const std::string& raw_arg, Env& env) {
    (void)env;
    // Tokenize the operand (mirroring {#pragma_style}) so a trailing comment
    // is stripped rather than rejected as garbage, and require exactly one
    // non-whitespace token matching "PP" followed by exactly two digits
    // (e.g. PP41). Well-formed but unassigned/retired codes are accepted
    // silently by design (§10); only the format is validated here.
    auto ts = ts_trim(lex_operand(raw_arg, /*remove_comments=*/true));
    if (ts.size() == 1 && ts[0].type == Token::ANY) {
        const std::string& s = ts[0].text;
        if (s.size() == 4 && s[0] == 'P' && s[1] == 'P' &&
            std::isdigit(static_cast<unsigned char>(s[2])) &&
            std::isdigit(static_cast<unsigned char>(s[3]))) {
            unsigned int val = static_cast<unsigned int>((s[2] - '0') * 10 + (s[3] - '0'));
            Issue::add_ignoring(Issue::Code(val));
            return;
        }
    }
    // raw_arg is decoded, so re-escape it before it reaches the
    // diagnostic, so a $n/$r escape in the source does not split the
    // message onto multiple lines (same reasoning as {#define} above).
    ISSUE(INVALID_IGNORE_OPERAND, Util::escape_line_breaks(raw_arg));
}

namespace {

// Parse raw_arg as an integer and call setter.
// Validation (n <= 0, n > 2^24) is handled by the Env setter via ISSUE.
// Parse failure is reported here as INVALID_LIMIT_OPERAND.
void handle_limit_directive(const std::string& raw_arg, Env& env,
                            bool (Env::*setter)(int)) {
    // One decimal integer; comments around it are whitespace (SPEC §11).
    // std::stoi() alone accepted any trailing text ("2x", "2 3", "2.9" all
    // silently meant 2) and rejected a leading comment.
    const std::string arg = iec3_blank_out_comments(raw_arg);
    std::size_t used = 0;
    int n = 0;
    try {
        n = std::stoi(arg, &used);
    } catch (const std::exception&) {
        used = 0;
    }
    if (used == 0 || !Util::trim_view(std::string_view(arg).substr(used)).empty()) {
        // raw_arg is decoded, so re-escape it (see {#define} above).
        ISSUE(INVALID_LIMIT_OPERAND, Util::escape_line_breaks(raw_arg));
        return;
    }
    (env.*setter)(n);
}

} // namespace

void handle_max_include_depth(const std::string& raw_arg, Env& env) {
    handle_limit_directive(raw_arg, env, &Env::set_max_include_depth);
}

void handle_max_expansion_depth(const std::string& raw_arg, Env& env) {
    handle_limit_directive(raw_arg, env, &Env::set_max_expansion_depth);
}

void handle_max_if_nesting(const std::string& raw_arg, Env& env) {
    handle_limit_directive(raw_arg, env, &Env::set_max_if_nesting);
}

void handle_max_blank_lines(const std::string& raw_arg, Env& env) {
    handle_limit_directive(raw_arg, env, &Env::set_max_blank_lines);
}

void handle_pragma_style(const std::string& raw_arg, Env& env) {
    // Tokenize the operand (mirroring the {#max_*} limit directives) so a
    // trailing comment is stripped rather than rejected as garbage, and
    // require exactly one non-whitespace token equal to VAL_PRAGMA_STANDARD
    // or VAL_PRAGMA_ANNOTATED.
    auto ts = ts_trim(lex_operand(raw_arg, /*remove_comments=*/true));
    if (ts.size() != 1 || ts[0].type != Token::ANY ||
        (ts[0].text != VAL_PRAGMA_STANDARD && ts[0].text != VAL_PRAGMA_ANNOTATED)) {
        // raw_arg is decoded, so re-escape it before it reaches the
        // diagnostic, so a $n/$r escape in the source does not split the
        // message onto multiple lines (same reasoning as {#define} above).
        ISSUE(INVALID_PRAGMA_STYLE_OPERAND, Util::escape_line_breaks(raw_arg));
        return;
    }
    env.set_pragma_style(ts[0].text);
}

void handle_pragma_once(const std::string& raw_arg, Env& env) {
    // Only "once" is recognised; other pragma names are silently ignored.
    // Comments are whitespace; tokens after "once" are a warning and the
    // pragma still applies, like clang. A document comment is a token.
    // lex_operand() reports PP29 before the "once" check below, so an
    // unrecognised pragma name still gets it, e.g. {#pragma foo 'x}.
    auto ts = ts_trim(lex_operand(raw_arg, /*remove_comments=*/true));
    if (ts.empty() || ts[0].type != Token::ANY || ts[0].text != "once")
        return;
    if (ts.size() > 1)
        ISSUE(EXTRA_TOKENS_AT_END_OF_DIRECTIVE,
              "{#pragma " + Util::escape_line_breaks(Util::trim_view(raw_arg)) + "}");
    env.record_pragma_once(env.current_file());
}

} // namespace jiepp::preprocessor_detail
