// expand_subst.cpp — Prosser's algorithm: subst, glue, hsadd, support functions.
// Corresponds to cpp.algo.md (X3J11/86-196): §subst, §glue, §hsadd, §Support functions.

#include "expand_helpers.hpp"
#include "preprocessor.hpp"
#include "preprocessor_internal.hpp"

#include "../loader/lexer.hpp"
#include "../macro/macro.hpp"
#include "../util/iec_61131-3.hpp"
#include "../util/stack_guard.hpp"
#include "../util/text.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace jiepp::expand_detail {

// ── §hsadd — add to token sequence's hide sets ────────────────────

// Adds HS to every token in ts.
static std::vector<Token>& hsadd(const Token::HideSet& hs, std::vector<Token>& ts) {
    if (!hs.empty()) {
        // O1b: consecutive tokens in `ts` overwhelmingly share the same
        // starting hide set (e.g. every token of a freshly-substituted macro
        // body), so HideSet::unite() would recompute the identical union
        // once per token. Memoize the last (base handle -> result) mapping
        // and reuse it while the base handle stays `same()` (pointer
        // identity -- there is deliberately no operator== on HideSet),
        // skipping both the unite() call and, when the handle is already
        // correct, the assignment itself. memo_in/memo_out are held as
        // HideSet values (strong references), not raw pointers: once t.hs
        // is reassigned below, the old base HideSet may lose its last owner
        // and be freed, and a later allocation could reuse that address,
        // making a raw-pointer memo key alias an unrelated hide set and
        // silently mis-memoize a later token.
        Token::HideSet memo_in;
        Token::HideSet memo_out;
        for (auto& t : ts) {
            // Per Prosser's algorithm, hide-sets propagate to all tokens (including
            // LP/RP/SEP) so that the closing ')' correctly carries context hs for
            // the hs(t) ∩ hs(rp) ∪ {M} intersection in function macro expansion.
            switch (t.type) {
            case Token::ANY:
            case Token::LP:
            case Token::RP:
            case Token::SEP:
                if (t.hs.empty()) {
                    // Share the call's set directly (O(1) handle copy); no
                    // allocation, unlike the old shared_ptr<const set> copy.
                    t.hs = hs;
                } else if (t.hs.same(memo_in)) {
                    if (!t.hs.same(memo_out))
                        t.hs = memo_out;
                } else {
                    memo_in = t.hs;
                    t.hs = Token::HideSet::unite(t.hs, hs);
                    memo_out = t.hs;
                }
                break;
            default:
                    continue;
            }
        }
    }
    return ts;
}

// ── the end of an argument's expansion (SPEC §3.7) ────────────────

void end_argument_expansion(std::vector<Token>& ts, std::size_t from,
                            const std::vector<Token>& input) {
    // The macros active in the call: the union of the input's hide sets
    // (consecutive tokens overwhelmingly share one handle).
    Token::HideSet context;
    {
        Token::HideSet last;
        for (const auto& t : input) {
            if (t.hs.empty() || t.hs.same(last))
                continue;
            last = t.hs;
            context = Token::HideSet::unite(context, t.hs);
        }
    }
    // Memo of the last (hide set -> its part in the context), like hsadd().
    Token::HideSet memo_in;
    Token::HideSet memo_out;
    for (std::size_t i = from; i < ts.size(); ++i) {
        Token& t = ts[i];
        if (t.hs.empty())
            continue;
        // Only an identifier can be a macro name (the cheap test first).
        if (t.type == Token::ANY && !t.text.empty() &&
            (std::isalpha(static_cast<unsigned char>(t.text[0])) || t.text[0] == '_') &&
            t.hs.contains(t.text)) {
            // Left unexpanded because its macro was being expanded:
            // painted, never expanded again (C17 6.10.3.4p2).
            t.hs = (context.empty() ? Token::HideSet() : Token::HideSet::intersect(t.hs, context))
                       .with(t.text);
            continue;
        }
        if (context.empty()) {
            t.hs = Token::HideSet();
            continue;
        }
        if (!t.hs.same(memo_in)) {
            memo_in = t.hs;
            memo_out = Token::HideSet::intersect(t.hs, context);
        }
        t.hs = memo_out;
    }
}

// A macro argument's pre-expansion (C17 6.10.3.1): `input` expanded as a
// whole, appended to `out`, then end_argument_expansion(). Out of line and
// never inlined so its locals do not enlarge subst()'s frame.
[[gnu::noinline]] static void expand_argument_into(const std::vector<Token>& input,
                                                   std::vector<Token>& out, Env& env) {
    const std::size_t from = out.size();
    expand(input, out, env);
    end_argument_expansion(out, from, input);
}

// ── §glue — paste last of left side with first of right side ──────

// A failed paste (PP32) keeps both operands as separate tokens, like gcc and
// clang, so it does not make the left operand grow. A successful paste does:
// repeated good pastes in one body make the left operand large, and a later
// failing paste then echoes it in PP32; echoing it whole in every message
// would be quadratic in the input (clang echoes only the pair formed by each
// paste and stops after -ferror-limit errors; jiepp stops after --error-limit
// errors, default 1000, and also bounds each echo, see below). Bound
// each echoed operand: keep at most kPasteEchoMax bytes and mark the cut
// with "...". The cut is moved to a UTF-8 character boundary so a multibyte
// character is never split. The code and severity of the diagnostic are
// unchanged.
constexpr std::size_t kPasteEchoMax = 64;

static std::string paste_echo_head(const std::string& s) {
    return s.size() <= kPasteEchoMax ? s : std::string(Util::utf8_head(s, kPasteEchoMax)) + "...";
}

static std::string paste_echo_tail(const std::string& s) {
    return s.size() <= kPasteEchoMax ? s : "..." + std::string(Util::utf8_tail(s, kPasteEchoMax));
}

// Work budget (PP64) cost of one token: 1 + len/64 steps.
static inline std::uint64_t step_cost(const Token& t) {
    return 1 + t.text.size() / 64;
}

// Pastes the last token of src with the first token of item. The paste is
// valid when the two written together read as exactly one token
// (iec3_pasted_token_type(), SPEC §4.3), which then takes that token's
// type. The merged token is charged here (unless `item_charged`: the inner
// subst() of __VA_OPT__ content has charged it); every token pushed to src
// instead is charged by the caller's next charge_new().
static void glue_tokens(std::vector<Token>& src, std::vector<Token>& item, Env& env,
                        bool item_charged) {
    if (src.empty()) {
        if (!item.empty())
            item.front().expansion_edge = true;
        for (auto& t : item)
            src.push_back(std::move(t));
        return;
    }
    if (item.empty())
        return;

    auto& prev = src.back();
    auto& next = item.front();

    const std::optional<int> type = iec3_pasted_token_type(prev.text, next.text);
    if (!type) {
        // Report, then keep both operands as separate tokens (gcc/clang).
        // Nothing of prev (text, hide set) changes. ISSUE runs before the
        // push so a thrown Issue::Exception leaves src untouched, and `prev`
        // is not used after the push (it may dangle). The two tokens now
        // meet where no token was written next to the other, so a space
        // goes between them if they would read as other tokens (SPEC §3.9),
        // as gcc/clang print a failed paste.
        ISSUE(INVALID_TOKEN_PASTING,
              paste_echo_tail(prev.text) + " @@ " + paste_echo_head(next.text));
        next.expansion_edge = true;
        for (auto& t : item)
            src.push_back(std::move(t));
        return;
    }

    if (!item_charged)
        env.charge_expansion_steps(step_cost(next));
    prev.type = *type;
    prev.hs = Token::HideSet::intersect(prev.hs, next.hs);
    prev.text += next.text;
    // A pasted token is not the lexer's own text: its pragma body (if it
    // is a PRAGMA) is re-lexed with every diagnostic reported (A2 / S-02).
    prev.relexed_pragma = false;
    prev.pragma_unterminated_reported = false;
    // Nor are its lines those of either operand.
    prev.line_map.reset();

    for (std::size_t k = 1; k < item.size(); ++k)
        src.push_back(std::move(item[k]));
}

// ── §Support functions: select, stringize ─────────────────────────

// The one definition of how the variable arguments from index `idx` are
// joined (S-01): select_arg() collects every token, va_args_empty() stops at
// the first. `emit` is called with each produced token in order (a fresh
// Token as an rvalue, an actual's token as a const lvalue) and returns false
// to stop the join.
template <class Emit>
static void join_va_args(int idx,
                         const std::vector<std::vector<Token>>& actuals,
                         const std::vector<ArgWs>* ws,
                         VaJoin join,
                         Emit&& emit) {
    // Reproduce clang's comma spacing: one space before the comma iff the
    // preceding actual had trailing whitespace/comment in the call, one
    // after iff the following actual had leading whitespace/comment. Which
    // flag pair counts depends on `join` (plain: lead_sp/trail_sp, a newline
    // directly before the token is no space; stringize: lead/trail, R6).
    // Bounds-checked: a missing ws entry (ws == nullptr, or shorter than
    // actuals) counts as false.
    auto flag = [&](int k, bool ArgWs::*field) {
        return ws && k >= 0 && k < static_cast<int>(ws->size()) && (*ws)[k].*field;
    };
    bool any = false; // whether a token has been produced yet
    // D3: when building the one shared spelling for a memoised multi-use
    // variadic parameter, tag each inserted WS token that plain use would
    // not emit so append_expanded() can filter it back out for the plain
    // (non-stringize) consumer. Returns false iff `emit` asked to stop.
    auto put_ws = [&](int k, bool ArgWs::*any_ws, bool ArgWs::*sp) {
        const bool plain_sp = flag(k, sp);
        if (!(join == VaJoin::plain ? plain_sp : flag(k, any_ws)))
            return true;
        Token t = Token::create(Token::WS, " ");
        t.va_sep = (join == VaJoin::shared) && !plain_sp;
        any = true;
        return static_cast<bool>(emit(std::move(t)));
    };

    for (int k = idx; k < static_cast<int>(actuals.size()); ++k) {
        if (k > idx) {
            // The first token produced takes the parameter's own spacing in
            // the body, like clang, so no space before a leading comma.
            if (any && !put_ws(k - 1, &ArgWs::trail, &ArgWs::trail_sp))
                return;
            any = true;
            if (!emit(Token::create(Token::SEP, ",")))
                return;
        }
        // An empty actual adds nothing: its whitespace is the run before the
        // next comma (trail), or nothing at all if it is the last actual.
        if (actuals[k].empty())
            continue;
        if (k > idx && !put_ws(k, &ArgWs::lead, &ArgWs::lead_sp))
            return;
        for (const auto& t : actuals[k]) {
            any = true;
            if (!emit(t))
                return;
        }
    }
}

std::vector<Token> select_arg(int idx,
                              const std::vector<std::vector<Token>>& actuals,
                              bool is_va,
                              const std::vector<ArgWs>* ws,
                              VaJoin join) {
    if (!is_va) {
        if (idx < static_cast<int>(actuals.size()))
            return actuals[idx];
        return {};
    }
    std::vector<Token> result;
    join_va_args(idx, actuals, ws, join, [&](auto&& t) {
        result.push_back(std::forward<decltype(t)>(t));
        return true;
    });
    return result;
}

bool va_args_empty(int idx, const std::vector<std::vector<Token>>& actuals, bool is_va) {
    if (!is_va)
        return idx >= static_cast<int>(actuals.size()) || actuals[idx].empty();
    // At most two actuals are visited: the second one always produces a SEP.
    bool any = false;
    join_va_args(idx, actuals, nullptr, VaJoin::plain, [&](auto&&) {
        any = true;
        return false;
    });
    return !any;
}

static int argc_from(int idx, const std::vector<std::vector<Token>>& actuals) {
    return std::max(0, static_cast<int>(actuals.size()) - idx);
}

// Shared by every site that resolves __VA_ARGC__ as a formal parameter:
// the normal substitution path, the `@@` (paste) raw-operand path, and the
// `@` (stringize) raw-operand path (F16). Kept as a single helper so the
// three sites cannot drift out of sync again.
static Token va_argc_token(int idx, const std::vector<std::vector<Token>>& actuals) {
    return Token::create(Token::ANY, std::to_string(argc_from(idx, actuals)));
}

// R1-R5: one pass, no copy through ts_flatten(). A maximal run of
// MASK_WS tokens (WS: spaces/tabs/newlines; C: comments, with or without
// -nC) between two non-WS tokens becomes one space; a run before the first
// or after the last non-WS token is dropped (R1). A run made only of
// line_filler newlines is not a separator (R2). Every other token is
// appended verbatim: a line-comment DOCUMENT token's trailing CR/LF is
// stripped from the text and instead treated as a separator candidate (R4);
// any ANY token whose interior still carries a raw newline is flattened the
// way ts_flatten() used to (R5); STRING/WSTRING/PRAGMA/block-DOCUMENT
// interiors are left untouched (R3).
std::string stringize_text(const std::vector<Token>& ts) {
    std::string text;
    bool any = false, sep = false;
    for (const auto& t : ts) {
        if (t.type & Token::MASK_WS) {
            if (!t.line_filler)
                sep = true;
            continue;
        }
        std::string_view s = t.text;
        bool trailing_nl = false;
        if (t.type == Token::DOCUMENT && s.starts_with("//")) {
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) {
                s.remove_suffix(1);
                trailing_nl = true;
            }
        }
        if (sep && any)
            text += ' ';
        const std::size_t at = text.size();
        text.append(s);
        if (t.type == Token::ANY && t.num_of_lines != 0)
            std::replace(text.begin() + static_cast<std::ptrdiff_t>(at), text.end(), '\n', ' ');
        any = true;
        sep = trailing_nl;
    }
    return text;
}

static Token stringize_tokens(const std::vector<Token>& ts) {
    return Token::create(Token::STRING, Util::encode_iec_string(stringize_text(ts), '\''));
}

// ── §Support functions: __VA_OPT__ helpers ────────────────────────

// va_opt_active() for variable arguments that are not empty as written.
// Arguments with no macro name and no pragma (whose body is expanded) expand
// to themselves with no side effect: not empty, and nothing to expand here
// (a later use of __VA_ARGS__ still expands them once). Otherwise they are
// macro-expanded -- also two or more, which always give a token (the comma
// between them), since gcc/clang expand them too (a __COUNTER__ in them
// moves) -- into the expansion that the uses of their parameter share
// (ArgExpansionMemo: a variadic body with __VA_OPT__ and __VA_ARGS__, or with
// two __VA_OPT__, has a slot for it, see FunctionMacro::compute_arg_slots()),
// or, when this is the only use, into a temporary.
static bool va_opt_active_uncached(int va_idx,
                                   const std::vector<std::vector<Token>>& actual_params,
                                   const std::vector<ArgWs>* actual_ws,
                                   ArgExpansionMemo* memo,
                                   Env& env) {
    bool may_change = false;
    for (std::size_t k = static_cast<std::size_t>(va_idx);
         k < actual_params.size() && !may_change; ++k) {
        for (const auto& t : actual_params[k]) {
            if (t.type == Token::PRAGMA ||
                (t.type == Token::ANY && !t.text.empty() &&
                 (std::isalpha(static_cast<unsigned char>(t.text[0])) || t.text[0] == '_') &&
                 env.lookup(t.text) != nullptr)) {
                may_change = true;
                break;
            }
        }
    }
    if (!may_change)
        return true;
    auto has_token = [](const std::vector<Token>& ts) {
        return std::any_of(ts.begin(), ts.end(),
                           [](const Token& t) { return !(t.type & Token::MASK_WS); });
    };
    const int slot = (memo && va_idx < static_cast<int>(memo->slots->size()))
                         ? (*memo->slots)[static_cast<std::size_t>(va_idx)]
                         : -1;
    if (slot < 0) {
        std::vector<Token> expanded;
        expand(select_arg(va_idx, actual_params, true), expanded, env);
        return has_token(expanded);
    }
    // Filled as the parameter's first use fills it (subst()).
    auto& cached = memo->expanded[static_cast<std::size_t>(slot)];
    if (!cached) {
        cached.emplace();
        expand_argument_into(actual_ws ? select_arg(va_idx, actual_params, true, actual_ws,
                                                    VaJoin::shared)
                                       : select_arg(va_idx, actual_params, true),
                             *cached, env);
    }
    return has_token(*cached);
}

// Whether __VA_OPT__(...) takes its content (SPEC §3.4): the macro is
// variadic (either form) and its variable arguments are not empty after
// macro expansion -- `F(EMPTY)` with an empty EMPTY leaves the content out,
// like gcc/clang (C23 6.10.5.1). Empty as written is decided in O(1) (see
// va_args_empty()), the rest by va_opt_active_uncached(), once per
// invocation however many __VA_OPT__ the body has (ArgExpansionMemo).
static bool va_opt_active(
    const std::unordered_map<std::string, std::pair<int, bool>>& formal_params,
    const std::vector<std::vector<Token>>& actual_params,
    const std::vector<ArgWs>* actual_ws,
    ArgExpansionMemo* memo,
    Env& env) {
    auto it = formal_params.find(FunctionMacro::VA_SYM);
    if (it == formal_params.end())
        return false;
    auto [va_idx, is_va] = it->second;
    if (va_args_empty(va_idx, actual_params, is_va))
        return false;
    if (memo && memo->va_opt_active)
        return *memo->va_opt_active;
    const bool active = va_opt_active_uncached(va_idx, actual_params, actual_ws, memo, env);
    if (memo)
        memo->va_opt_active = active;
    return active;
}

// For the __VA_OPT__ token at body[i] (va_opt: the VaOptSpan table of body):
// raises "nested __VA_OPT__ is not allowed" once per __VA_OPT__ token inside
// its parentheses, before the malformed check, exactly as the old content
// scan did, and charges one step per report. Returns the index of the
// matching ')' (the content is body[i + next + 1, that index)), or
// body.size() if no parenthesized argument follows or it is unclosed.
// A function-like macro with a nested, unparenthesized or unclosed
// __VA_OPT__ is rejected when it is defined (handle_define() is the only
// place in src/ that builds a FunctionMacro), and a macro without parameters
// never calls this (there __VA_OPT__ is an ordinary identifier). subst() is
// called from expand.cpp only for object macros and built-in replacements
// (no parameters) and for the FunctionMacros in the symbol table, so none of
// these reports is reached from preprocessing input; they are kept for a
// FunctionMacro built directly in code.
static std::size_t va_opt_close(std::span<const Token> body,
                                std::span<const VaOptSpan> va_opt,
                                std::size_t i,
                                Env& env) {
    const VaOptSpan& s = va_opt[i];
    if (s.nested > 0) {
        env.charge_expansion_steps(s.nested);
        for (std::uint32_t n = 0; n < s.nested; ++n)
            ISSUE(INVALID_VA_OPT, "nested __VA_OPT__ is not allowed");
    }
    if (s.close == 0)
        return body.size();
    return i + s.close;
}

// Index of the next non-MASK_WS token after body[i], or body.size(). The
// table's offset is relative to the whole macro body, so when body is a
// __VA_OPT__ content subrange and no non-blank token follows inside it, the
// offset lands exactly on the content's closing ')' (index body.size() of
// the subrange, since ')' is not blank); the clamp keeps that reading as
// "no further token" and guards the end of the whole body.
static std::size_t next_non_ws(std::span<const VaOptSpan> va_opt,
                               std::size_t i,
                               std::size_t size) {
    return std::min<std::size_t>(i + va_opt[i].next, size);
}

// The branches below are kept out of subst() so their locals do not enlarge
// subst()'s frame, which is on the stack once per macro nesting level
// (subst() -> expand() -> subst() ...; unoptimized builds give every local
// of every branch its own slot).

// Substitutes the content of the __VA_OPT__ at body[i], whose matching ')'
// is body[close], as a subspan of body and of its table (no copy).
static std::vector<Token> subst_va_opt_content(
    std::span<const Token> body,
    std::span<const VaOptSpan> va_opt,
    std::size_t i,
    std::size_t close,
    const std::unordered_map<std::string, std::pair<int, bool>>& formal_params,
    const std::vector<std::vector<Token>>& actual_params,
    const std::vector<ArgWs>* actual_ws,
    Env& env,
    bool va_sep_ws,
    ArgExpansionMemo* memo) {
    // F2: the only subst() -> subst() recursion; expand()'s guard never sees it.
    if (Util::stack_nearly_exhausted())
        raise_stack_exhausted(env);
    const std::size_t lp = next_non_ws(va_opt, i, body.size());
    const std::size_t n = close - lp - 1;
    return subst(body.subspan(lp + 1, n), va_opt.subspan(lp + 1, n), formal_params,
                 actual_params, actual_ws, {}, env, va_sep_ws, memo);
}

// `@param` / `@__VA_ARGS__` / `@args` (never `@__VA_ARGC__`: the caller
// handles it first, since it shares the variable arguments' pidx and memo
// slot -- review M1). The stringize operator applies directly to this
// formal parameter, so R6's comma spacing always applies here regardless of
// va_sep_ws (which only governs the separate @__VA_OPT__(...)
// content-substitution recursion). A parameter with a memo slot (spelled
// >= 2 times) is stringized once per invocation (S-01).
static Token stringize_param(int pidx,
                             bool is_va,
                             const std::vector<std::vector<Token>>& actual_params,
                             const std::vector<ArgWs>* actual_ws,
                             ArgExpansionMemo* memo) {
    const int slot = (memo && pidx < static_cast<int>(memo->slots->size()))
                         ? (*memo->slots)[pidx]
                         : -1;
    const auto s = static_cast<std::size_t>(slot);
    if (slot >= 0 && s < memo->stringized.size() && memo->stringized[s])
        return Token::create(Token::STRING, *memo->stringized[s]);
    std::string text;
    if (is_va) {
        text = stringize_text(
            select_arg(pidx, actual_params, is_va, actual_ws, VaJoin::stringize));
    } else {
        static const std::vector<Token> empty_actual;
        text = stringize_text(pidx < static_cast<int>(actual_params.size())
                                  ? actual_params[static_cast<std::size_t>(pidx)]
                                  : empty_actual);
    }
    text = Util::encode_iec_string(text, '\'');
    if (slot >= 0) {
        if (memo->stringized.empty())
            memo->stringized.resize(memo->expanded.size());
        memo->stringized[s] = text;
    }
    return Token::create(Token::STRING, std::move(text));
}

// The `@` at body[i] applied to its operand (SPEC §4.1): appends the STRING
// token to `out`, sets `end` to the index of the operand's last body token
// and returns true. False when the macro has no parameters (`@` is then an
// ordinary token) or no formal parameter, __VA_ARGC__ or well-formed
// __VA_OPT__(...) follows. `va_opt` is computed into `local_va_opt` if the
// body has no table yet (as subst()'s ensure_va_opt()). Shared by the
// stringize case of subst() and the right operand of `@@` (`a @@ @b`).
static bool stringize_operand(
    std::span<const Token> body,
    std::span<const VaOptSpan>& va_opt,
    std::vector<VaOptSpan>& local_va_opt,
    std::size_t i,
    const std::unordered_map<std::string, std::pair<int, bool>>& formal_params,
    const std::vector<std::vector<Token>>& actual_params,
    const std::vector<ArgWs>* actual_ws,
    Env& env,
    ArgExpansionMemo* memo,
    std::vector<Token>& out,
    std::size_t& end) {
    if (formal_params.empty())
        return false;
    std::size_t j = i + 1;
    if (!va_opt.empty()) {
        j = next_non_ws(va_opt, i, body.size());
    } else {
        // A body without __VA_OPT__ is normalized: no whitespace between `@`
        // and a formal parameter.
        while (j < body.size() && (body[j].type & Token::MASK_WS))
            ++j;
    }
    if (j >= body.size() || body[j].type != Token::ANY)
        return false;
    if (body[j].text == FunctionMacro::VA_OPT) {
        // @ __VA_OPT__(content): the content stringized, or ''.
        if (va_opt.empty()) {
            local_va_opt = compute_va_opt_spans(body);
            va_opt = local_va_opt;
        }
        const std::size_t k = va_opt_close(body, va_opt, j, env);
        if (k >= body.size())
            return false;
        std::vector<Token> inner;
        if (va_opt_active(formal_params, actual_params, actual_ws, memo, env))
            inner = subst_va_opt_content(body, va_opt, j, k, formal_params, actual_params,
                                         actual_ws, env, /*va_sep_ws=*/true, memo);
        out.push_back(stringize_tokens(inner));
        end = k;
        return true;
    }
    auto pit = formal_params.find(body[j].text);
    if (pit == formal_params.end())
        return false;
    auto [pidx, is_va] = pit->second;
    if (body[j].text == FunctionMacro::VA_ARGC) {
        // Checked before the stringize cache: __VA_ARGC__ shares the pidx (and
        // slot) of the variable arguments (review M1).
        out.push_back(stringize_tokens({va_argc_token(pidx, actual_params)}));
    } else {
        // @param / @__VA_ARGS__ (see stringize_param()).
        out.push_back(stringize_param(pidx, is_va, actual_params, actual_ws, memo));
    }
    end = j;
    return true;
}

// task_slug arg-expand-once (D1/D3): append one occurrence's worth of a
// memoised expansion `cached` to `result`. keep_sep selects which of the
// two spellings D3 may have cached (plain vs. the WS-carrying variadic
// spelling, see select_arg()'s VaJoin::shared): a va_sep-marked token is dropped
// unless keep_sep is true, and is always copied out with the marker
// cleared, so it can never leak out of subst() even on the moved-out path.
// When `move` is true (the occurrence that observes the slot's last
// remaining need), tokens are moved out of `cached` instead of copied --
// safe because this is always the last appender.
static void append_expanded(std::vector<Token>& result, std::vector<Token>& cached,
                            bool keep_sep, bool move, Env& env) {
    // Work budget (PP64): charged BEFORE copying, so a huge memoised
    // expansion cannot be duplicated past the limit once per occurrence.
    {
        std::uint64_t n = 0;
        for (const auto& t : cached)
            n += step_cost(t);
        env.charge_expansion_steps(n);
    }
    result.reserve(result.size() + cached.size());
    for (auto& t : cached) {
        if (t.va_sep && !keep_sep)
            continue;
        if (move) {
            t.va_sep = false;
            result.push_back(std::move(t));
        } else {
            Token copy = t;
            copy.va_sep = false;
            result.push_back(std::move(copy));
        }
    }
}

// ── §subst — substitute args, handle stringize and paste ──────────

std::vector<Token> subst(
    std::span<const Token> body,
    std::span<const VaOptSpan> va_opt,
    const std::unordered_map<std::string, std::pair<int, bool>>& formal_params,
    const std::vector<std::vector<Token>>& actual_params,
    const std::vector<ArgWs>* actual_ws,
    const Token::HideSet& hs,
    Env& env,
    bool va_sep_ws,
    ArgExpansionMemo* memo) {

    assert(va_opt.empty() || va_opt.size() == body.size());
    std::vector<Token> result;
    // With a __VA_OPT__ table the body may hold large skipped contents, so
    // reserving body.size() per call would allocate for tokens never
    // emitted (S-01); let the vector grow with what is actually produced.
    if (va_opt.empty())
        result.reserve(body.size());
    // A caller without a table (a built-in replacement) gets one computed
    // here the first time a __VA_OPT__ token is met.
    std::vector<VaOptSpan> local_va_opt;
    auto ensure_va_opt = [&]() {
        if (va_opt.empty()) {
            local_va_opt = compute_va_opt_spans(body);
            va_opt = local_va_opt;
        }
    };
    // Tracks whether the token most recently produced for `result` (or, before any
    // token has been produced, the position just before the first GLUE) is a
    // placemarker (C17 6.10.3.3p2): an empty token sequence that participates in
    // `@@` pasting as if it were an actual, invisible token. Set when a formal
    // parameter adjacent to `@@` substitutes to zero tokens, or a __VA_OPT__(...)
    // produces no token other than blanks; cleared whenever any other token is
    // pushed to `result`.
    bool pending_placemarker = false;
    // A formal parameter is an `@@` operand iff a direct neighbour within
    // this body (or __VA_OPT__ content) is GLUE: normalize_glue() leaves no
    // MASK_WS token next to a GLUE in either kind of macro body (asserted at
    // definition in Debug), and built-in replacements have no GLUE. No
    // per-call scan of the body (S-01).
    auto glue_adjacent = [&](std::size_t i) {
        return (i > 0 && body[i - 1].type == Token::GLUE) ||
               (i + 1 < body.size() && body[i + 1].type == Token::GLUE);
    };

    // Work budget (PP64): result[0, charged) has been charged already.
    // charge_new() runs once per body element (and once after the loop), and
    // clamps `charged` because the @@ path can pop_back() below it.
    std::size_t charged = 0;
    auto charge_new = [&]() {
        if (charged > result.size())
            charged = result.size();
        std::uint64_t n = 0;
        for (; charged < result.size(); ++charged)
            n += step_cost(result[charged]);
        env.charge_expansion_steps(n);
    };

    // SPEC §3.9: the tokens of a substituted argument (or __VA_OPT__
    // content) meet the body tokens around it where nothing was written
    // next to them. The first is marked Token::expansion_edge here, and
    // `edge_at` is the index in `result` of the token that will follow the
    // substitution, marked when it is pushed (npos: none pending). A `@@`
    // right after the substitution pastes there instead and clears it.
    std::size_t edge_at = std::string::npos;
    auto substituted_from = [&](std::size_t start) {
        if (start < result.size())
            result[start].expansion_edge = true;
        edge_at = result.size();
    };
    auto mark_edge_at = [&]() {
        if (edge_at < result.size()) {
            result[edge_at].expansion_edge = true;
            edge_at = std::string::npos;
        }
    };

    for (std::size_t i = 0; i < body.size(); ++i) {
        charge_new();
        mark_edge_at();
        const Token& t = body[i];

        // §subst case: IS is ## • T • IS' (paste)
        if (t.type == Token::GLUE) {
            edge_at = std::string::npos;
            // The left operand is a placemarker (an empty formal-param substitution
            // or an empty __VA_OPT__ adjacent to this @@) when `pending_placemarker`
            // is set. In that case the whitespace preceding it belongs to the
            // surrounding text, not to the (nonexistent) left operand, so it must
            // not be stripped.
            bool left_is_placemarker = pending_placemarker;
            if (!left_is_placemarker) {
                while (!result.empty() && (result.back().type & Token::MASK_WS))
                    result.pop_back();
            }

            std::size_t j = i + 1;
            while (j < body.size() && (body[j].type & Token::MASK_WS)) ++j;
            if (j >= body.size())
                continue;

            std::vector<Token> item;
            bool nb_is_param = false;
            // True when item is __VA_OPT__ content, which the inner subst()
            // has charged already.
            bool item_charged = false;
            // GNU comma swallowing: the index of the variable arguments when
            // this is `, @@ __VA_ARGS__` (or `, @@ args` for a named
            // variadic parameter), -1 otherwise. normalize_glue() leaves no
            // whitespace next to a GLUE, so the left operand is body[i - 1].
            int gnu_comma_va_idx = -1;
            const Token& nb = body[j];
            if (nb.type == Token::ANY && nb.text == FunctionMacro::VA_OPT &&
                !formal_params.empty()) {
                // Handle:  X @@ __VA_OPT__(content)
                // (A macro without parameters has no variable arguments to
                // test, so there __VA_OPT__ is an ordinary identifier: it
                // takes the else branch and is pasted like one.)
                ensure_va_opt();
                std::size_t k = va_opt_close(body, va_opt, j, env);
                if (k < body.size()) {
                    if (va_opt_active(formal_params, actual_params, actual_ws, memo, env)) {
                        item = ts_flatten(subst_va_opt_content(body, va_opt, j, k, formal_params,
                                                               actual_params, actual_ws, env,
                                                               va_sep_ws, memo));
                        item_charged = true;
                    }
                    // Empty item → glue_tokens is a no-op → left side preserved unchanged.
                    i = k;
                } else {
                    item = {nb.clone()};
                    i = j;
                }
            } else if (std::size_t end = 0;
                       nb.type == Token::STRINGIZE &&
                       stringize_operand(body, va_opt, local_va_opt, j, formal_params,
                                         actual_params, actual_ws, env, memo, item, end)) {
                // `a @@ @b`: the operand of `@` is stringized first and the
                // string is pasted (the C order, gcc/clang). A string is
                // never a placemarker, even an empty one.
                nb_is_param = true;
                i = end;
            } else {
                if (nb.type == Token::ANY) {
                    auto pit = formal_params.find(nb.text);
                    if (pit != formal_params.end()) {
                        nb_is_param = true;
                        auto [pidx, is_va] = pit->second;
                        if (nb.text == FunctionMacro::VA_ARGC) {
                            item = {va_argc_token(pidx, actual_params)};
                        } else {
                            item = ts_flatten(select_arg(pidx, actual_params, is_va, actual_ws,
                                                         va_sep_ws ? VaJoin::stringize : VaJoin::plain));
                            if (is_va && !left_is_placemarker && i > 0 &&
                                body[i - 1].type == Token::SEP && !result.empty() &&
                                result.back().type == Token::SEP)
                                gnu_comma_va_idx = pidx;
                        }
                    }
                }
                // Only fall back to the literal spelling of `nb` when it was NOT a
                // formal parameter. A formal parameter that substitutes to zero
                // tokens is a placemarker (C17 6.10.3.3p2), not a request to paste
                // the parameter's own name.
                if (item.empty() && !nb_is_param)
                    item = {nb.clone()};
                i = j;
            }

            if (gnu_comma_va_idx >= 0) {
                // `, @@ __VA_ARGS__` (gcc/clang's GNU extension, measured
                // with gcc 11 and clang 22 in their default gnu modes): the
                // comma is never pasted with the variable arguments.
                if (static_cast<int>(actual_params.size()) <= gnu_comma_va_idx) {
                    // Variable arguments omitted (`LOG(x)`, or `F()` for a
                    // macro with only `...`). When they are the left operand
                    // of further @@, gcc/clang still drop the comma if every
                    // operand of that chain is the variable arguments again
                    // (`, @@ __VA_ARGS__ @@ __VA_ARGS__`); any other operand
                    // (`@@ 1`, `@@ f`, `@@ __VA_OPT__(a)`) keeps the comma,
                    // which the chain then pastes with as usual (`, @@ 1` is
                    // PP32). normalize_glue() leaves no blank next to a GLUE.
                    std::size_t k = j + 1;
                    while (k + 1 < body.size() && body[k].type == Token::GLUE &&
                           body[k + 1].type == Token::ANY &&
                           body[k + 1].text != FunctionMacro::VA_ARGC) {
                        auto cit = formal_params.find(body[k + 1].text);
                        if (cit == formal_params.end() || cit->second.first != gnu_comma_va_idx)
                            break;
                        k += 2;
                    }
                    if (k < body.size() && body[k].type == Token::GLUE) {
                        pending_placemarker = false;
                        continue;
                    }
                    // Drop the comma together with the blanks before it, as
                    // gcc/clang do, and the (empty) rest of the chain.
                    result.pop_back();
                    while (!result.empty() && (result.back().type & Token::MASK_WS))
                        result.pop_back();
                    i = k - 1;
                    // What was before the comma meets what follows it.
                    edge_at = result.size();
                } else {
                    // Present (possibly empty: `LOG(x,)` keeps `g(x,)`): the
                    // comma stays and the arguments follow it unpasted, with
                    // one space iff the call had one before them.
                    const bool lead_ws = actual_ws &&
                        gnu_comma_va_idx < static_cast<int>(actual_ws->size()) &&
                        (va_sep_ws ? (*actual_ws)[gnu_comma_va_idx].lead
                                   : (*actual_ws)[gnu_comma_va_idx].lead_sp);
                    const std::size_t start = result.size();
                    if (lead_ws && !item.empty())
                        result.push_back(Token::create(Token::WS, " "));
                    for (auto& tk : item)
                        result.push_back(std::move(tk));
                    substituted_from(start);
                }
                pending_placemarker = false;
                continue;
            }

            // An argument or __VA_OPT__ content on the right ends where the
            // body goes on (SPEC §3.9).
            const bool item_substituted = nb_is_param || item_charged;
            if (left_is_placemarker) {
                // Placemarker @@ item == item: append verbatim, no pasting.
                // Its first token meets what was before the placemarker.
                if (!item.empty())
                    item.front().expansion_edge = true;
                for (auto& tk : item)
                    result.push_back(std::move(tk));
                pending_placemarker = item.empty();
                if (item_charged)
                    charged = result.size();
                if (item_substituted)
                    edge_at = result.size();
            } else {
                // Work budget (PP64): the one token merged by @@ (item[0]
                // into result.back()) is charged by glue_tokens() as it is
                // merged (a merge grows result.back() in place, which
                // charge_new() above cannot see). Every token that is pushed
                // to result instead -- item[1..] of a valid paste, the whole
                // of item when the paste fails (PP32) or result is empty --
                // is charged once, by the next charge_new(); charging it
                // there as well would count it twice. __VA_OPT__ content is
                // the exception: the inner subst() has charged all of it, so
                // neither the merge nor the next charge_new() may charge it
                // again.
                glue_tokens(result, item, env, item_charged);
                if (item_charged)
                    charged = result.size();
                pending_placemarker = false;
                if (item_substituted)
                    edge_at = result.size();
            }
            continue;
        }

        // §subst case: IS is # • T • IS' (stringize)
        if (t.type == Token::STRINGIZE) {
            // A stringized result is always a real (possibly empty-text) STRING
            // token, never a placemarker.
            pending_placemarker = false;
            std::size_t end = 0;
            if (stringize_operand(body, va_opt, local_va_opt, i, formal_params, actual_params,
                                  actual_ws, env, memo, result, end)) {
                i = end;
                continue;
            }
            // Without parameters (an object macro, F(), a built-in
            // replacement) `@` is an ordinary token, as `#` is in C (clang:
            // `#define H # x` gives `# x`, `#define F() #__VA_OPT__(b)` gives
            // `#__VA_OPT__(b)`). A function-like macro with parameters whose
            // `@` is not followed by one is not defined ({#define} reports
            // PP31), so the PP31 here is reached only for a FunctionMacro
            // built directly in code.
            if (!formal_params.empty())
                ISSUE(INVALID_STRINGIZING);
            result.push_back(t.clone());
            continue;
        }

        // §subst case: IS is T • IS' and T is FP[i] (formal param → expand or flatten)
        if (t.type == Token::ANY) {
            // Handle standalone __VA_OPT__(content). In a macro without
            // parameters (F(), an object macro) __VA_OPT__ is an ordinary
            // identifier and goes the generic way below. A non-variadic macro
            // with parameters has no variable arguments, so the content is
            // dropped, as for an empty __VA_ARGS__. A __VA_OPT__ that produces
            // no token other than blanks is a placemarker for a following `@@`.
            if (t.text == FunctionMacro::VA_OPT && !formal_params.empty()) {
                pending_placemarker = false;
                bool va_opt_produced = false;
                ensure_va_opt();
                std::size_t j = va_opt_close(body, va_opt, i, env);
                // Not reached from input: a function-like macro with a
                // __VA_OPT__ lacking '(' or ')' is not defined (see
                // va_opt_close()).
                if (j >= body.size()) {
                    ISSUE(INVALID_VA_OPT, "__VA_OPT__ requires parenthesized argument");
                    result.push_back(t.clone());
                    continue;
                }
                const std::size_t start = result.size();
                if (va_opt_active(formal_params, actual_params, actual_ws, memo, env)) {
                    auto inner = subst_va_opt_content(body, va_opt, i, j, formal_params,
                                                      actual_params, actual_ws, env, va_sep_ws,
                                                      memo);
                    for (auto& et : inner) {
                        if (!(et.type & Token::MASK_WS))
                            va_opt_produced = true;
                        result.push_back(std::move(et));
                    }
                    // Already charged by the inner subst().
                    charged = result.size();
                }
                substituted_from(start);
                pending_placemarker = !va_opt_produced;
                i = j; // advance past closing ')'
                continue;
            }

            auto pit = formal_params.find(t.text);
            if (pit != formal_params.end()) {
                auto [pidx, is_va] = pit->second;
                const std::size_t start = result.size();

                if (t.text == FunctionMacro::VA_ARGC) {
                    pending_placemarker = false;
                    result.push_back(va_argc_token(pidx, actual_params));
                    substituted_from(start);
                    continue;
                }

                // task_slug arg-expand-once (D1/D2): a slot >= 0 means this
                // parameter is spelled >= 2 times in the body, so its
                // expansion is memoised; see ArgExpansionMemo. slot == -1
                // (including memo == nullptr, for a macro with no multi-use
                // parameter) keeps the original O3 direct-expand path below
                // untouched -- no allocation, no copy.
                const int slot = (memo && pidx < static_cast<int>(memo->slots->size()))
                                      ? (*memo->slots)[pidx]
                                      : -1;
                // D3: a multi-use variadic parameter of a call that carries
                // ArgWs flags builds its cached expansion from a
                // single WS-marked spelling (see select_arg's VaJoin::shared),
                // computed fresh below, regardless of actual_ws -- so the
                // raw spelling is not needed to seed that cache.
                const bool slot_needs_marked_spelling = slot >= 0 && is_va && actual_ws;
                const bool is_glue_operand = glue_adjacent(i);
                const bool need_raw =
                    is_glue_operand || slot < 0 ||
                    (!memo->expanded[static_cast<std::size_t>(slot)].has_value() &&
                     !slot_needs_marked_spelling);

                // O2b: select_arg() copies the actual's token vector even for
                // the (overwhelmingly common) non-variadic case, where
                // actual_params[pidx] already holds exactly the tokens
                // select_arg() would build. Bind a pointer straight into
                // actual_params there instead; only the variadic path (which
                // must concatenate multiple actuals with inserted SEP
                // tokens) still needs select_arg()'s freshly-built vector,
                // held alive in actual_va.
                std::vector<Token> actual_va;
                static const std::vector<Token> empty_actual;
                const std::vector<Token>* actual = nullptr;
                if (need_raw) {
                    if (is_va) {
                        actual_va = select_arg(pidx, actual_params, is_va, actual_ws,
                                               va_sep_ws ? VaJoin::stringize : VaJoin::plain);
                        actual = &actual_va;
                    } else {
                        actual = (pidx < static_cast<int>(actual_params.size()))
                                     ? &actual_params[static_cast<std::size_t>(pidx)]
                                     : &empty_actual;
                    }
                }

                if (is_glue_operand) {
                    // A formal parameter directly adjacent to @@ that substitutes to
                    // zero tokens is a placemarker (C17 6.10.3.3p2): it must not fall
                    // back to being pasted as its own literal name. Never memoised
                    // (D1): the raw actual is pasted as-is, independent of any other
                    // occurrence's expansion.
                    // Its last token is pasted with the right operand; its
                    // first meets the body before it (SPEC §3.9).
                    auto flat = ts_flatten(*actual);
                    if (flat.empty()) {
                        pending_placemarker = true;
                    } else {
                        flat.front().expansion_edge = true;
                        for (auto& ft : flat)
                            result.push_back(std::move(ft));
                        pending_placemarker = false;
                    }
                } else if (slot < 0) {
                    // O3: expand() appends straight into `result` instead of
                    // into a throwaway `expanded` vector that is then
                    // copied token-by-token -- one fewer O(k) copy per
                    // formal-parameter occurrence.
                    expand_argument_into(*actual, result, env);
                    substituted_from(start);
                    pending_placemarker = false;
                } else {
                    auto& cached = memo->expanded[static_cast<std::size_t>(slot)];
                    if (!cached) {
                        cached.emplace();
                        if (slot_needs_marked_spelling)
                            expand_argument_into(select_arg(pidx, actual_params, true, actual_ws,
                                                            VaJoin::shared),
                                                 *cached, env);
                        else
                            expand_argument_into(*actual, *cached, env);
                    }
                    const bool last =
                        (--memo->remaining[static_cast<std::size_t>(slot)] == 0);
                    append_expanded(result, *cached, /*keep_sep=*/va_sep_ws, /*move=*/last, env);
                    // Already charged (before the copy) by append_expanded().
                    charged = result.size();
                    substituted_from(start);
                    pending_placemarker = false;
                }
                continue;
            }
        }

        // §subst fallthrough: IS must be T_HS' • IS'
        pending_placemarker = false;
        result.push_back(t.clone());
    }

    mark_edge_at();
    charge_new();
    hsadd(hs, result);
    return result;
}

} // namespace jiepp::expand_detail
