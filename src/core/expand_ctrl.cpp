// expand_ctrl.cpp — Conditional compilation control flow.
// Not part of Prosser's algorithm; jiepp extension for #if/#elif/#else/#endif.

#include "expand_helpers.hpp"
#include "preprocessor.hpp"
#include "preprocessor_internal.hpp"

#include "../constfold/constfold.hpp"
#include "../loader/loader.hpp"
#include "../loader/directive_parser.hpp"
#include "../loader/lexer.hpp"
#include "../macro/macro.hpp"
#include "../util/text.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace jiepp::expand_detail {

bool ctrl_is_active(const std::vector<CtrlState>& stack) {
    for (auto& s : stack) {
        if (!s.condition.has_value() || !s.condition.value()) return false;
    }
    return true;
}

bool ctrl_parent_active(const std::vector<CtrlState>& stack) {
    for (std::size_t i = 0; i + 1 < stack.size(); ++i) {
        if (!stack[i].condition.has_value() || !stack[i].condition.value()) return false;
    }
    return true;
}

namespace {

// The __has_include pre-scan of eval_cond_value_tokens() (both builds) does
// not look inside the arguments of a nested call of __assert, __error,
// __counter, __counter_peek or __counter_set: expand() invokes the inner
// call, which handles its own arguments (a condition runs its own pre-scan, a
// message is expanded only when it is reported, a counter name is never
// expanded). The call is recognised as expand() does: an ANY token that is not
// in its own hide set, whose macro is still the built-in object, followed
// after whitespace by '('. Only names written in the operand count; a call
// made by a macro expansion is resolved like any other text. The operand of
// `defined` / __has_builtin is a name, not a call.
bool is_nested_builtin_name(const Token& t, Env& env) {
    if (t.type != Token::ANY || t.text.size() < 7 || t.text[0] != '_' || t.text[1] != '_')
        return false;
    if (t.text != "__error" && t.text != "__assert" && t.text != "__counter" &&
        t.text != "__counter_peek" && t.text != "__counter_set")
        return false;
    if (t.hs.contains(t.text))
        return false;
    Macro* m = env.lookup(t.text);
    return dynamic_cast<ErrorBuiltin*>(m) || dynamic_cast<AssertBuiltin*>(m) ||
           dynamic_cast<CounterBuiltin*>(m) || dynamic_cast<CounterSetBuiltin*>(m);
}

bool is_defined_or_has_builtin(const Token& t) {
    return t.type == Token::ANY && (t.text == "defined" || t.text == "__has_builtin");
}

// Whether the tokens v[0, n) end with `defined`, `defined (`, `__has_builtin`
// or `__has_builtin (`, whitespace and comments ignored: a name that follows
// is the operand. A walk stops at the first token that is not whitespace, so
// it costs the whitespace run just before the name (and before a '(' in front
// of it), which a later name cannot share: the earlier name sits between.
bool follows_defined_or_has_builtin(const std::vector<Token>& v, std::size_t n) {
    auto skip_ws = [&v](std::size_t k) {
        while (k > 0 && (v[k - 1].type & Token::MASK_WS))
            --k;
        return k;
    };
    std::size_t k = skip_ws(n);
    if (k == 0)
        return false;
    if (is_defined_or_has_builtin(v[k - 1]))
        return true;
    if (v[k - 1].type != Token::LP)
        return false;
    k = skip_ws(k - 1);
    return k > 0 && is_defined_or_has_builtin(v[k - 1]);
}

// When ts[i] starts a call of one of those built-ins, the index one past the
// call, else npos. `before[0, before_n)` are the tokens in front of ts[i]
// (the scan may already have moved them elsewhere). The extent follows the
// argument collector of expand(): '(' and '[' open, ')' and ']' close, a ']'
// at depth 0 is ignored, the first ')' at depth 0 ends the call; a call
// without it extends to the end of the operand (expand() reports PP34).
std::size_t nested_builtin_call_end(const std::vector<Token>& ts, std::size_t i,
                                    const std::vector<Token>& before, std::size_t before_n,
                                    Env& env) {
    if (!is_nested_builtin_name(ts[i], env))
        return std::string::npos;
    std::size_t j = i + 1;
    while (j < ts.size() && (ts[j].type & Token::MASK_WS))
        ++j;
    if (j >= ts.size() || ts[j].type != Token::LP ||
        follows_defined_or_has_builtin(before, before_n))
        return std::string::npos;
    int depth = 0;
    for (++j; j < ts.size(); ++j) {
        const int ty = ts[j].type;
        if (ty == Token::RP && depth == 0)
            return j + 1;
        if (ty == Token::LP || ty == Token::LB)
            ++depth;
        else if ((ty == Token::RP || ty == Token::RB) && depth > 0)
            --depth;
    }
    return ts.size();
}

// Only called from the non-sandbox branches of eval_cond_str() and
// eval_cond_value_tokens() below; in a sandbox build they would otherwise be
// unused functions (__has_include is rejected outright there, see
// find_has_include_call()).
#ifndef JIEPP_SANDBOX

constexpr std::string_view HAS_INCLUDE_KW = "__has_include";

bool is_space(char c) {
    return std::isspace(static_cast<unsigned char>(c)) != 0;
}

// The keyword at s[kw] starts a word: it is not the tail of a longer
// identifier (e.g. a user macro name ending in __has_include).
bool starts_word(std::string_view s, std::size_t kw) {
    if (kw == 0)
        return true;
    unsigned char prev = static_cast<unsigned char>(s[kw - 1]);
    return !(std::isalnum(prev) || prev == '_');
}

// Searches over one text whose start positions never decrease from one
// call to the next, so the whole sequence reads each character at most
// once: a call whose answer still lies ahead of its start reuses the
// previous answer. (A smaller start is still answered correctly, only not
// in amortized constant time.)
class HasIncludeScan {
public:
    explicit HasIncludeScan(std::string_view s) : s_(s) {}
    std::string_view text() const { return s_; }

    // Index of the first '>' at or after q, or npos.
    std::size_t find_gt(std::size_t q) {
        if (!(gt_used_ && q >= gt_from_ && (gt_at_ == std::string_view::npos || gt_at_ >= q))) {
            gt_used_ = true;
            gt_from_ = q;
            gt_at_ = s_.find('>', q);
        }
        return gt_at_;
    }

    // Index of the first non-space character at or after q, or s.size().
    std::size_t skip_space(std::size_t q) {
        if (!(sp_used_ && q >= sp_from_ && sp_at_ >= q)) {
            sp_used_ = true;
            sp_from_ = q;
            std::size_t i = q;
            while (i < s_.size() && is_space(s_[i]))
                ++i;
            sp_at_ = i;
        }
        return std::min(sp_at_, s_.size());
    }

private:
    std::string_view s_;
    bool gt_used_ = false;
    std::size_t gt_from_ = 0;
    std::size_t gt_at_ = 0;
    bool sp_used_ = false;
    std::size_t sp_from_ = 0;
    std::size_t sp_at_ = 0;
};

// One __has_include call whose keyword starts at kw, parsed on the text up
// to its end: the keyword, whitespace, '(', whitespace, a path, whitespace,
// ')'. The path is a string literal ("path" / 'path'), which the directive
// decoding left with its IEC escapes as written (SPEC §2), so its end is
// found past "$'"-style escapes (string_literal_end()); or '<' up to the
// first '>' character after it -- a character search, so a '>' inside a
// string literal or a multi-character token ends the path too.
struct HasIncludeCall {
    std::size_t path = std::string_view::npos;     // the quote or '<'; npos: no path
    std::size_t path_end = std::string_view::npos; // past the closing quote, or the '>'
    bool angle = false;
    std::size_t close = std::string_view::npos;    // the ')' after the path; npos: malformed
};

HasIncludeCall parse_has_include_call(HasIncludeScan& sc, std::size_t kw) {
    const std::string_view s = sc.text();
    HasIncludeCall c;
    std::size_t i = kw + HAS_INCLUDE_KW.size();
    while (i < s.size() && is_space(s[i]))
        ++i;
    if (i >= s.size() || s[i] != '(')
        return c;
    ++i;
    while (i < s.size() && is_space(s[i]))
        ++i;
    std::size_t j = 0;
    if (i < s.size() && (s[i] == '"' || s[i] == '\'')) {
        std::size_t end = string_literal_end(s, i);
        if (end == std::string_view::npos)
            return c;
        c.path = i;
        c.path_end = end;
        // Literals reached from different keywords end at different quotes:
        // a later call's opening quote, preceded by '(' or a space, is never
        // escaped, so it ends an earlier literal of the same kind that
        // reaches it. Two such literal scans share at most that one quote,
        // and the space runs after them never overlap: linear overall.
        j = end;
        while (j < s.size() && is_space(s[j]))
            ++j;
    } else if (i < s.size() && s[i] == '<') {
        std::size_t gt = sc.find_gt(i + 1);
        if (gt == std::string_view::npos)
            return c;
        c.path = i;
        c.path_end = gt;
        c.angle = true;
        // Many calls can share one '>': memoized.
        j = sc.skip_space(gt + 1);
    } else {
        return c;
    }
    if (j < s.size() && s[j] == ')')
        c.close = j;
    return c;
}

// The quoted path literal of a call (c.path set, not angle).
std::string_view call_literal(std::string_view s, const HasIncludeCall& c) {
    return s.substr(c.path, c.path_end - c.path);
}

// Whether the file of a well-formed call (c.close set) exists.
bool call_target_exists(std::string_view s, const HasIncludeCall& c, std::string path,
                        Env& env) {
    if (c.angle)
        path = std::string(s.substr(c.path + 1, c.path_end - c.path - 1));
    const auto load_type = c.angle ? Loader::LoadType::SINCLUDE : Loader::LoadType::INCLUDE;
    return !Loader::fullpath(path, load_type, env).empty();
}

// Replace all __has_include("path") and __has_include(<path>) in raw_cond
// with "1" or "0" based on whether the file is found.
// This runs before macro expansion so arguments are NOT expanded.
// The keyword positions only grow, so every search of `sc` is amortized
// constant and the whole pass is linear in the text.
std::string resolve_has_include(const std::string& raw_cond, Env& env) {
    const std::string_view KW = HAS_INCLUDE_KW;
    std::string result;
    result.reserve(raw_cond.size());
    HasIncludeScan sc(raw_cond);
    std::size_t pos = 0;
    while (pos < raw_cond.size()) {
        std::size_t found = raw_cond.find(KW, pos);
        if (found == std::string::npos) {
            result.append(raw_cond, pos);
            break;
        }
        result.append(raw_cond, pos, found - pos);

        if (!starts_word(raw_cond, found)) {
            // Ordinary text; keep scanning after it.
            result.append(KW);
            pos = found + KW.size();
            continue;
        }

        HasIncludeCall c = parse_has_include_call(sc, found);
        // A closed quoted literal is decoded the way {#include} decodes its
        // path (strip_path()), also when the call turns out malformed, so
        // an invalid escape reports PP21 either way.
        std::string path;
        if (c.path != std::string_view::npos && !c.angle)
            path = decode_path_literal(call_literal(raw_cond, c));
        if (c.close != std::string_view::npos) {
            bool exists = call_target_exists(raw_cond, c, std::move(path), env);
            result.append(exists ? "1" : "0");
            pos = c.close + 1;
            continue;
        }

        // malformed — pass through as-is
        result.append(KW);
        pos = found + KW.size();
    }
    return result;
}

// The __has_include pre-scan of eval_cond_value_tokens() (normal build).
//
// Each token "__has_include" followed (after whitespace or comments) by '('
// is a candidate call; its text is the tokens up to the ')' that matches
// that '(' (counting only '(' / ')' tokens), concatenated with comments as
// one space. The candidate resolves -- its tokens become one "1" or "0"
// token and the scan continues after its ')' -- exactly when
// resolve_has_include() on its text returns "1" or "0", that is when the
// call parsed from the start of the text is well formed and its ')' is the
// last character. Otherwise its tokens stay and the scan goes on with the
// next token, so calls nested in a malformed call are still resolved. The
// scan goes left to right and a resolved call consumes its extent, so a
// built-in name inside its path is path text; a nested built-in call met
// outside any resolved call is copied whole and not scanned (see
// nested_builtin_call_end()), which adds work only in proportion to its size.
//
// Earlier versions built every candidate's text and ran
// resolve_has_include() on it, which was quadratic (cubic for nested
// <...> paths). Here the tokens are concatenated once, the matching ')' of
// every '(' comes from one stack pass, and every keyword occurrence in the
// whole text is parsed once (parse_has_include_call(), linear overall).
// A candidate's decision needs only its own parse: a parse within the
// candidate's text reads the same characters as within the whole text
// unless it reaches past the text's end, and then it cannot end at the
// text's last character.
//
// The rest of resolve_has_include() on a candidate's text had one visible
// effect (its Loader::fullpath() calls only query the filesystem), kept
// here: every quoted literal it decoded reported PP21 for an invalid
// escape, so a literal inside n candidate texts was reported up to n
// times. report_pp21_in_call_text() repeats that walk, but only for a
// candidate whose text holds such a literal, and only over the keywords
// that can report PP21 or skip text.
class HasIncludeTokenScan {
public:
    explicit HasIncludeTokenScan(const std::vector<Token>& ts) : off_(ts.size() + 1) {
        for (std::size_t m = 0; m < ts.size(); ++m) {
            off_[m] = s_.size();
            // Comments are whitespace here, as in the expression itself.
            if (ts[m].type == Token::C)
                s_ += ' ';
            else
                s_ += ts[m].text;
        }
        off_[ts.size()] = s_.size();

        match_.assign(ts.size(), std::string_view::npos);
        std::vector<std::size_t> open;
        for (std::size_t m = 0; m < ts.size(); ++m) {
            if (ts[m].type == Token::LP) {
                open.push_back(m);
            } else if (ts[m].type == Token::RP && !open.empty()) {
                match_[open.back()] = m;
                open.pop_back();
            }
        }

        const std::string_view s = s_;
        HasIncludeScan sc(s);
        for (std::size_t p = s.find(HAS_INCLUDE_KW); p != std::string_view::npos;
             p = s.find(HAS_INCLUDE_KW, p + HAS_INCLUDE_KW.size())) {
            Node n;
            n.pos = p;
            n.call = parse_has_include_call(sc, p);
            n.word = starts_word(s, p);
            if (n.call.path != std::string_view::npos && !n.call.angle) {
                std::string_view lit = call_literal(s, n.call);
                decode_directive_text(lit.substr(1, lit.size() - 2), &n.pp21);
            }
            nodes_.push_back(n);
        }
        // next_pp21_[k]: the first node at or after k with an invalid escape;
        // next_live_[k]: the first node at or after k that, when reached
        // inside a candidate text, can report PP21 or skip text.
        next_pp21_.assign(nodes_.size() + 1, nodes_.size());
        next_live_.assign(nodes_.size() + 1, nodes_.size());
        for (std::size_t k = nodes_.size(); k-- > 0;) {
            const Node& n = nodes_[k];
            next_pp21_[k] = n.pp21 ? k : next_pp21_[k + 1];
            bool live = n.word && (n.pp21 || n.call.close != std::string_view::npos);
            next_live_[k] = live ? k : next_live_[k + 1];
        }
    }

    // Resolves the candidate calls of the token vector into `out`.
    void resolve(std::vector<Token>& ts, std::vector<Token>& out, Env& env) {
        const std::size_t n = ts.size();
        std::size_t node = 0;
        for (std::size_t i = 0; i < n; ++i) {
            if (ts[i].type != Token::ANY || ts[i].text != HAS_INCLUDE_KW) {
                // A nested built-in call is copied as a whole, unscanned.
                const std::size_t e = nested_builtin_call_end(ts, i, out, out.size(), env);
                if (e != std::string::npos) {
                    for (; i < e; ++i)
                        out.push_back(std::move(ts[i]));
                    --i;
                    continue;
                }
                out.push_back(std::move(ts[i]));
                continue;
            }
            std::size_t j = i + 1;
            while (j < n && (ts[j].type & Token::MASK_WS))
                ++j;
            if (j >= n || ts[j].type != Token::LP || match_[j] == std::string_view::npos) {
                // not a call, or unclosed: left as is, the evaluator reports it
                out.push_back(std::move(ts[i]));
                continue;
            }
            const std::size_t k = match_[j];
            const std::size_t end = off_[k] + 1; // the candidate text is s_[off_[i], end)
            while (nodes_[node].pos < off_[i])
                ++node;
            if (next_pp21_[node] < nodes_.size() && nodes_[next_pp21_[node]].pos < end)
                report_pp21_in_call_text(node, end);
            const HasIncludeCall& c = nodes_[node].call;
            if (c.close == end - 1) {
                std::string path;
                if (!c.angle) {
                    bool invalid = false; // already reported above
                    std::string_view lit = call_literal(s_, c);
                    path = decode_directive_text(lit.substr(1, lit.size() - 2), &invalid);
                }
                bool exists = call_target_exists(s_, c, std::move(path), env);
                out.push_back(Token::create(Token::ANY, exists ? "1" : "0"));
                i = k;
                continue;
            }
            out.push_back(std::move(ts[i]));
        }
    }

private:
    struct Node {
        std::size_t pos = 0; // of the keyword in s_
        HasIncludeCall call; // parsed on the whole s_
        bool word = false;   // starts_word()
        bool pp21 = false;   // a closed quoted literal with an invalid escape
    };

    // The PP21 reports of resolve_has_include() on the candidate text
    // s_[nodes_[first].pos, end). The keyword at the start of the text has
    // no word check. A parse limited to the text gives the same result as
    // the whole-text parse when every position it uses lies inside the
    // text: a literal closes in the text iff path_end <= end, a call is
    // well formed in the text iff close < end. Keywords that are not live
    // neither report nor skip, so the walk jumps over them.
    void report_pp21_in_call_text(std::size_t first, std::size_t end) {
        auto visit = [&](std::size_t k) {
            const Node& n = nodes_[k];
            if (n.pp21 && n.call.path_end <= end)
                ISSUE(INVALID_ESCAPE_SEQUENCE, std::string(call_literal(s_, n.call)));
            if (n.call.close != std::string_view::npos && n.call.close < end) {
                // Skips the call: continue at the first keyword after its ')'.
                auto it = std::upper_bound(
                    nodes_.begin() + static_cast<std::ptrdiff_t>(k), nodes_.end(), n.call.close,
                    [](std::size_t pos, const Node& m) { return pos < m.pos; });
                return static_cast<std::size_t>(it - nodes_.begin());
            }
            return k + 1;
        };
        std::size_t k = visit(first);
        for (;;) {
            k = next_live_[k];
            if (k >= nodes_.size() || nodes_[k].pos >= end)
                break;
            k = visit(k);
        }
    }

    std::string s_;                  // token texts concatenated, comments as ' '
    std::vector<std::size_t> off_;   // off_[m]: start of token m in s_
    std::vector<std::size_t> match_; // match_[m]: the ')' of the '(' at m, or npos
    std::vector<Node> nodes_;        // every occurrence of the keyword in s_
    std::vector<std::size_t> next_pp21_;
    std::vector<std::size_t> next_live_;
};

void resolve_has_include_tokens(std::vector<Token>& ts, Env& env) {
    bool any = false;
    for (const auto& t : ts) {
        if (t.type == Token::ANY && t.text == HAS_INCLUDE_KW) {
            any = true;
            break;
        }
    }
    if (!any)
        return;
    HasIncludeTokenScan scan(ts);
    std::vector<Token> out;
    out.reserve(ts.size());
    scan.resolve(ts, out, env);
    ts = std::move(out);
}

#endif  // !JIEPP_SANDBOX

#ifdef JIEPP_SANDBOX
// Index of the first actual __has_include operator use in s -- the keyword
// not preceded by an identifier character (so a longer name such as
// weird__has_include does not match) and followed, after optional
// whitespace, by '(' -- or npos. `defined(__has_include)` and
// `defined __has_include` are feature tests, not uses.
std::size_t find_has_include_call(const std::string& s) {
    static constexpr std::string_view KW = "__has_include";
    std::size_t pos = 0;
    while ((pos = s.find(KW, pos)) != std::string::npos) {
        std::size_t kw = pos;
        pos += KW.size();
        if (kw > 0) {
            unsigned char prev = static_cast<unsigned char>(s[kw - 1]);
            if (std::isalnum(prev) || prev == '_')
                continue;
        }
        std::size_t i = pos;
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
        if (i < s.size() && s[i] == '(')
            return kw;
    }
    return std::string::npos;
}
#endif

// Installs the transient 'defined' operator for one condition evaluation and
// removes it again on scope exit (also when the evaluation throws). When it
// is already installed -- a built-in such as __assert evaluated inside an
// {#if}/{#elif} condition -- the scope reuses it for this nested evaluation:
// the outer condition's operand-error state is saved and reset, the operator
// is re-enabled if a message expansion had suspended it, and everything is
// restored on scope exit, so the nested evaluation can neither clear nor
// inherit the outer condition's PP40 short-circuit.
class DefinedScope {
public:
    explicit DefinedScope(Env& env) : env_(env) {
        if (auto* d = dynamic_cast<DefinedOperator*>(env.lookup("defined"))) {
            op_ = d;
            owns_ = false;
            saved_error_ = d->operand_error;
            saved_code_ = d->operand_error_code;
            saved_suspended_ = d->suspended;
            d->operand_error = false;
            d->operand_error_code = Issue::Code::SUCCESS;
            d->suspended = false;
        } else {
            auto owned = std::make_unique<DefinedOperator>();
            op_ = owned.get();
            env.define("defined", std::move(owned));
            owns_ = true;
        }
    }
    ~DefinedScope() {
        if (owns_) {
            env_.undef("defined");
        } else {
            op_->operand_error = saved_error_;
            op_->operand_error_code = saved_code_;
            op_->suspended = saved_suspended_;
        }
    }
    DefinedScope(const DefinedScope&) = delete;
    DefinedScope& operator=(const DefinedScope&) = delete;
    // Valid for the lifetime of the scope (a superseded or undefined macro
    // is retired, never destroyed: symtab.hpp lifetime invariant).
    DefinedOperator* op() const { return op_; }

private:
    Env& env_;
    DefinedOperator* op_ = nullptr;
    bool owns_ = false;
    bool saved_error_ = false;
    Issue::Code saved_code_ = Issue::Code::SUCCESS;
    bool saved_suspended_ = false;
};

} // namespace

std::string eval_cond_str(const std::string& raw_cond, Env& env, bool* operand_error) {
#ifdef JIEPP_SANDBOX
    // In sandbox mode the __has_include operator is not allowed (filesystem
    // probe); only an actual use is PP62, see find_has_include_call().
    if (find_has_include_call(raw_cond) != std::string::npos)
        ISSUE(SANDBOX_RESTRICTED_DIRECTIVE, "__has_include");
    std::string cond = raw_cond;
#else
    std::string cond = resolve_has_include(raw_cond, env);
#endif
    // C3/U5: the transient 'defined' operator is installed by an RAII
    // scope, so it is removed on scope exit even when expand_operand_text()
    // below throws.
    DefinedScope defined_scope(env);
    std::string result = jiepp::preprocessor_detail::expand_operand_text(cond, env);
    if (operand_error)
        *operand_error = defined_scope.op()->operand_error;
    return result;
}

bool eval_cond(const std::string& raw_cond, Env& env) {
    bool operand_error = false;
    std::string expanded = eval_cond_str(raw_cond, env, &operand_error);
    // C3: a 'defined' operand/paren error already reported the diagnostic;
    // the whole condition is false and the rest of it is not evaluated,
    // like clang.
    if (operand_error)
        return false;
    // Comments are whitespace in the expression, as in C: those written in
    // the operand and those a macro body brought in (bodies keep comments
    // unless -nC) both survive expand_operand_text() as text, and constfold's
    // scanner knows no comment syntax. Blanked only after
    // resolve_has_include() ran on the raw text, so a
    // __has_include(<a//b>) path is not mistaken for a comment.
    int64_t val = eval_const_expr(iec3_blank_out_comments(expanded));
    return val != 0;
}

// See expand_helpers.hpp for the contract. Mirrors eval_cond_str() +
// eval_cond() on tokens instead of re-lexed text.
CondValue eval_cond_value_tokens(std::vector<Token> ts, Env& env) {
    CondValue r;
    auto note = [&r](Issue::Code code) {
        if (r.failure == Issue::Code::SUCCESS)
            r.failure = code;
    };

    // Work budget (PP64): one step per byte of the operand, like the
    // operand charge of expand_operand_tokens() for an {#if} condition.
    std::size_t bytes = 0;
    for (const auto& t : ts)
        bytes += t.text.size();
    env.charge_expansion_steps(bytes);

    // __has_include(...) written in the operand: the same rule as
    // find_has_include_call() / resolve_has_include() on an {#if} text --
    // the keyword followed (after whitespace or comments) by '(' -- before
    // macro expansion, so its argument is not expanded. Not looked for in
    // the arguments of a nested built-in call (nested_builtin_call_end()).
#ifdef JIEPP_SANDBOX
    for (std::size_t i = 0; i < ts.size(); ++i) {
        if (ts[i].type != Token::ANY || ts[i].text != "__has_include") {
            // A nested built-in call is skipped as a whole.
            const std::size_t e = nested_builtin_call_end(ts, i, ts, i, env);
            if (e != std::string::npos)
                i = e - 1;
            continue;
        }
        std::size_t j = i + 1;
        while (j < ts.size() && (ts[j].type & Token::MASK_WS))
            ++j;
        if (j >= ts.size() || ts[j].type != Token::LP)
            continue;
        // Sandbox: the operator is a filesystem probe (PP62), reported once
        // like eval_cond_str(); the call text stays in the expression, so
        // the evaluator below reports it as well (same pair as {#if}).
        note(Issue::Code::SANDBOX_RESTRICTED_DIRECTIVE);
        ISSUE(SANDBOX_RESTRICTED_DIRECTIVE, "__has_include");
        break;
    }
#else
    // See HasIncludeTokenScan for the rule and why it is linear.
    resolve_has_include_tokens(ts, env);
#endif

    std::string text;
    {
        DefinedScope defined_scope(env);
        std::vector<Token> ots;
        expand(ts, ots, env);
        // C3: a 'defined' operand/paren error already reported its
        // diagnostic; the rest of the expression is not evaluated, like
        // eval_cond().
        if (defined_scope.op()->operand_error) {
            note(defined_scope.op()->operand_error_code);
            return r;
        }
        for (const auto& t : ots)
            text += t.text;
    }
    // Comments are whitespace in the expression (see eval_cond()).
    cf::EvalResult ev = eval_const_value(iec3_blank_out_comments(text));
    if (ev.failure != Issue::Code::SUCCESS)
        note(ev.failure);
    if (r.failure == Issue::Code::SUCCESS)
        r.value = ev.value;
    return r;
}

// C2: {#ifdef NAME}/{#ifndef NAME}. See expand_helpers.hpp for the contract.
bool eval_ifdef(const std::string& raw_arg, bool is_ifndef, Env& env) {
    auto ts = ts_trim(jiepp::preprocessor_detail::lex_operand(raw_arg, /*remove_comments=*/true));
    if (ts.empty()) {
        ISSUE(INVALID_DEFINED_OPERAND, "macro name missing");
        return false;
    }
    if (ts[0].type != Token::ANY || !iec3_is_identifier(ts[0].text)) {
        ISSUE(INVALID_DEFINED_OPERAND,
              "macro name must be an identifier: " + Util::escape_line_breaks(raw_arg));
        return false;
    }
    if (ts.size() > 1) {
        // Like gcc/clang ("extra tokens at end of #ifdef directive"), a
        // warning; only NAME is tested. The extra tokens are never evaluated
        // as an expression (formerly "{#ifdef X) \or\ (1}" was spliced into a
        // defined(...) string and evaluated). Not reached in an inactive
        // group (the caller only evaluates active {#ifdef}s).
        ISSUE(EXTRA_TOKENS_AT_END_OF_DIRECTIVE,
              std::string(is_ifndef ? "{#ifndef " : "{#ifdef ") +
                  Util::escape_line_breaks(Util::trim_view(raw_arg)) + "}");
    }
    bool is_def = macro_name_is_defined(ts[0].text, env);
    return is_ifndef ? !is_def : is_def;
}

// Whether NAME counts as defined for {#ifdef}/{#ifndef}/`defined`.
// A macro in env counts (the transient 'defined' operator itself does not).
// Like gcc/clang, `__has_include` also counts in a normal build so code can
// feature-test it; jiepp's __has_include(...) operator cannot be disabled
// by {#undef}/{#define}, so it stays defined even after
// {#undef __has_include}. Not in a JIEPP_SANDBOX build, where the operator
// itself is restricted (PP62): there it reads as undefined. The
// __has_builtin(NAME) operator counts in both builds (it needs no filesystem
// access) and likewise survives {#undef __has_builtin}.
bool macro_name_is_defined(std::string_view name, Env& env) {
#ifndef JIEPP_SANDBOX
    if (name == "__has_include")
        return true;
#endif
    if (name == "__has_builtin")
        return true;
    Macro* m = env.lookup(name);
    return m && !dynamic_cast<DefinedOperator*>(m);
}

// The value of __has_builtin(NAME), SPEC 5.2: 1 for the two operators and for
// a symbol-table entry that is still a built-in kind (is_builtin_macro()); 0
// for value macros, user macros, undefined/redefined names and unknown names.
bool has_builtin(std::string_view name, Env& env) {
    if (name == "__has_builtin" || name == "defined")
        return true;
#ifndef JIEPP_SANDBOX
    if (name == "__has_include")
        return true;
#endif
    return is_builtin_macro(env.lookup(name));
}

// The __has_builtin operator branch of expand(): `work` has the operator token
// already popped. Errors follow the 'defined' branch (first error per
// condition only, state kept in the DefinedOperator, remaining text not
// evaluated by the caller); the result token is "1"/"0".
[[gnu::noinline]] void expand_has_builtin(std::vector<Token>& work,
                                          std::vector<Token>& ots,
                                          Env& env,
                                          DefinedOperator& defop) {
    auto skip_ws = [&work]() {
        while (!work.empty() && (work.back().type & Token::MASK_WS))
            work.pop_back();
    };
    auto fail = [&](Issue::Code code, const std::string& what) {
        if (!defop.operand_error) {
            defop.operand_error = true;
            defop.operand_error_code = code;
            if (code == Issue::Code::INVALID_BUILTIN_OPERAND)
                ISSUE(INVALID_BUILTIN_OPERAND, what);
            else
                ISSUE(INVALID_EXPRESSION, what);
        }
        ots.push_back(Token::create(Token::ANY, "0"));
    };

    skip_ws();
    if (work.empty() || work.back().type != Token::LP) {
        fail(Issue::Code::INVALID_EXPRESSION, "missing '(' after '__has_builtin'");
        return;
    }
    work.pop_back();
    skip_ws();

    if (work.empty() || work.back().type == Token::RP) {
        fail(Issue::Code::INVALID_BUILTIN_OPERAND, "__has_builtin: name missing");
        return;
    }
    if (work.back().type != Token::ANY || !iec3_is_identifier(work.back().text)) {
        fail(Issue::Code::INVALID_BUILTIN_OPERAND,
             "__has_builtin: name must be an identifier: " +
                 Util::escape_line_breaks(work.back().text));
        return;
    }
    std::string operand = std::move(work.back().text);
    work.pop_back();
    skip_ws();
    if (work.empty() || work.back().type != Token::RP) {
        fail(Issue::Code::INVALID_EXPRESSION, "missing ')' after '__has_builtin'");
        return;
    }
    work.pop_back();
    ots.push_back(Token::create(Token::ANY, has_builtin(operand, env) ? "1" : "0"));
}

} // namespace jiepp::expand_detail
