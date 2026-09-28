#include "preprocessor.hpp"
#include "preprocessor_internal.hpp"

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

bool strip_path(const std::string_view raw_path, std::string& path, bool& syspath_only) {
    auto p = Util::trim_view(raw_path);
    if(p.size() >= 2) {
        auto c0 = p.front(), c1 = p.back();
        syspath_only = (c0 == '<') && (c1 == '>');
        if ((c0 == '\'' && c1 == '\'') || (c0 == '"' && c1 == '"') || syspath_only) {
            path = std::string(p.substr(1, p.size() - 2));
            return true;
        }
    }
    return false;
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

} // namespace

namespace jiepp::preprocessor_detail {

void handle_define(const std::string& raw_arg, Env& env) {
    auto ts = iec3_tokens_from_string(raw_arg, false);
    ts = ts_ltrim(std::move(ts));
    if (ts.empty() || ts[0].type != Token::ANY) {
        ISSUE(INVALID_DEFINE_SYNTAX, raw_arg);
        return;
    }

    std::string name = ts[0].text;
    if (name == "defined") {
        ISSUE(OPERATION_NOT_ALLOWED, "redefine 'defined'");
        return;
    }

    std::size_t i = 1;
    bool is_function = (i < ts.size() && ts[i].type == Token::LP);
    std::vector<std::string> param_names;
    // O(1) duplicate-parameter lookup (was an O(k^2) nested scan). The views
    // point into ts[i].text: `ts` is not modified until the parameter loop
    // is done, so they stay valid. Views into param_names would dangle when
    // the vector reallocates.
    std::unordered_set<std::string_view> seen_params;

    // Like gcc/clang: the first duplicate parameter is reported once and the
    // whole {#define} is abandoned -- the macro is not defined and any
    // existing definition of `name` is left untouched.
    // A regular parameter named __VA_ARGS__/__VA_ARGC__ collides with the
    // implicit names a trailing '...' introduces (FunctionMacro's args_ map
    // would silently let the variadic entry win), so it is a duplicate too.
    auto variadic_name_clash = [&]() -> bool {
        for (const char* va : {FunctionMacro::VA_ARGS, FunctionMacro::VA_ARGC}) {
            if (seen_params.count(va)) {
                ISSUE(DUPLICATE_MACRO_PARAMETER, va);
                return true;
            }
        }
        return false;
    };

    if (is_function) {
        ++i;
        while (i < ts.size() && ts[i].type != Token::RP) {
            if (ts[i].type & Token::MASK_WS) {
                ++i;
                continue;
            }
            if (ts[i].type == Token::SEP) {
                ++i;
                continue;
            }
            if (ts[i].type == Token::ANY) {
                if (ts[i].text == "...") {
                    if (variadic_name_clash())
                        return;
                    param_names.push_back(FunctionMacro::VA_SYM);
                    ++i;
                    while (i < ts.size() && (ts[i].type & Token::MASK_WS)) ++i;
                    if (i < ts.size() && ts[i].type != Token::RP) {
                        ISSUE(INVALID_VARIADIC_PLACEMENT, raw_arg);
                        return;
                    }
                    break;
                }
                if (ts[i].text == "." && i + 2 < ts.size() && ts[i + 1].text == "." &&
                    ts[i + 2].text == ".") {
                    if (variadic_name_clash())
                        return;
                    param_names.push_back(FunctionMacro::VA_SYM);
                    i += 3;
                    while (i < ts.size() && (ts[i].type & Token::MASK_WS)) ++i;
                    if (i < ts.size() && ts[i].type != Token::RP) {
                        ISSUE(INVALID_VARIADIC_PLACEMENT, raw_arg);
                        return;
                    }
                    break;
                }
                if (!seen_params.insert(ts[i].text).second) {
                    ISSUE(DUPLICATE_MACRO_PARAMETER, ts[i].text);
                    return;
                }
                param_names.push_back(ts[i].text);
                ++i;
            } else {
                ISSUE(INVALID_DEFINE_SYNTAX, raw_arg);
                return;
            }
        }
        if (i < ts.size() && ts[i].type == Token::RP)
            ++i;
    }

    while (i < ts.size() && (ts[i].type & Token::MASK_WS))
        ++i;
    std::vector<Token> body(ts.begin() + i, ts.end());

    auto make_and_define = [&]() {
        if (is_function) {
            auto nm = std::make_unique<FunctionMacro>(param_names, body);
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
}

void handle_undef(const std::string& raw_arg, Env& env) {
    auto name = Util::trim_view(raw_arg);
    if (name == "defined") {
        ISSUE(OPERATION_NOT_ALLOWED, "undef 'defined'");
        return;
    }
    env.undef(name);
}

void handle_tokenize(const std::string& raw_arg, Env& env, std::vector<Token>& ots) {
    auto ts = iec3_tokens_from_string(raw_arg, env.get_remove_comments());
    expand(ts, ots, env);
}

void handle_stringize(const std::string& raw_arg,
                      Env& env,
                      std::vector<Token>& ots,
                      bool wide) {
    std::string raw_text = preprocess_text(raw_arg, env);
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

    std::string arg = preprocess_text(raw_arg, env);
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
    ISSUE(INVALID_SETLINE_OPERAND, raw_arg);
}

void handle_syspath(const std::string& raw_arg, Env& env, std::vector<Token>& ots) {
    std::string raw_path = preprocess_text(raw_arg, env);
    std::string syspath;
    bool _;
    if (strip_path(raw_path, syspath, _)) {
        env.add_syspath(resolve_syspath_base(syspath, env));
        ots.push_back(Token::pragma("syspath", Util::encode_iec_string(syspath, '\''), env.is_standard_pragma_style()));
        return;
    }
    ISSUE(INVALID_PATH, raw_arg);
}

void handle_include(const std::string& raw_arg,
                    Env& env,
                    std::vector<Token>& ots,
                    bool syspath_only) {
    std::string raw_path = preprocess_text(raw_arg, env);
    std::string include_path;
    bool syspath_only_path;
    if (strip_path(raw_path, include_path, syspath_only_path)) {
        auto disp_path = fs::path(include_path).generic_string();

        Loader::LoadType loadtype = (syspath_only || syspath_only_path) ? Loader::LoadType::SINCLUDE : Loader::LoadType::INCLUDE;
        expand(include_path, loadtype, ots, env, disp_path);
        return;
    }
    ISSUE(INVALID_PATH, raw_arg);
}

void handle_message(const std::string& raw_arg, Env& env, Issue::Code code) {
    std::string msg = preprocess_text(raw_arg, env);
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
    auto ts = ts_trim(iec3_tokens_from_string(raw_arg, /*remove_comments=*/true));
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
    ISSUE(INVALID_IGNORE_OPERAND, raw_arg);
}

namespace {

// Parse raw_arg as an integer and call setter.
// Validation (n <= 0, n > 2^24) is handled by the Env setter via ISSUE.
// Parse failure is reported here as INVALID_LIMIT_OPERAND.
void handle_limit_directive(const std::string& raw_arg, Env& env,
                            bool (Env::*setter)(int)) {
    try {
        int n = std::stoi(raw_arg);
        (env.*setter)(n);
    } catch (const Issue::Exception&) {
        throw; // re-throw ISSUE errors as-is
    } catch (...) {
        ISSUE(INVALID_LIMIT_OPERAND, raw_arg);
    }
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
    auto ts = ts_trim(iec3_tokens_from_string(raw_arg, /*remove_comments=*/true));
    if (ts.size() != 1 || ts[0].type != Token::ANY ||
        (ts[0].text != VAL_PRAGMA_STANDARD && ts[0].text != VAL_PRAGMA_ANNOTATED)) {
        ISSUE(INVALID_PRAGMA_STYLE_OPERAND, raw_arg);
        return;
    }
    env.set_pragma_style(ts[0].text);
}

void handle_pragma_once(const std::string& raw_arg, Env& env) {
    // Only "once" is recognised; all other pragma names are silently ignored.
    if (Util::trim_view(raw_arg) == "once")
        env.record_pragma_once(env.current_file());
}

} // namespace jiepp::preprocessor_detail
