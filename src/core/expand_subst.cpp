// expand_subst.cpp — Prosser's algorithm: subst, glue, hsadd, support functions.
// Corresponds to cpp.algo.md (X3J11/86-196): §subst, §glue, §hsadd, §Support functions.

#include "expand_helpers.hpp"
#include "preprocessor.hpp"
#include "preprocessor_internal.hpp"

#include "../macro/macro.hpp"
#include "../util/iec_61131-3.hpp"
#include "../util/stack_guard.hpp"
#include "../util/text.hpp"

#include <algorithm>
#include <cassert>
#include <set>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace jiepp::expand_detail {

// ── §hsadd — add to token sequence's hide sets ────────────────────

std::vector<Token>& hsadd(const Token::HideSet& hs, std::vector<Token>& ts) {
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

// ── §glue — paste last of left side with first of right side ──────

// A failed paste (PP32) keeps both operands as separate tokens, like gcc and
// clang, so it does not make the left operand grow. A successful paste does:
// repeated good pastes in one body make the left operand large, and a later
// failing paste then echoes it in PP32; echoing it whole in every message
// would be quadratic in the input (clang echoes only the pair formed by each
// paste and stops after -ferror-limit errors; jiepp has no such limit). Bound
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

// The structural tokens ( ) , [ ]: pasting to one never forms a valid token.
static bool is_structural_token(int type) {
    return type == Token::LP || type == Token::RP || type == Token::SEP ||
           type == Token::LB || type == Token::RB;
}

// True iff `prev @@ next` forms one token: both have the same type and the
// type is not a structural token.
static bool paste_is_valid(const Token& prev, const Token& next) {
    return prev.type == next.type && !is_structural_token(prev.type);
}

void glue_tokens(std::vector<Token>& src, std::vector<Token>& item) {
    if (src.empty()) {
        for (auto& t : item)
            src.push_back(std::move(t));
        return;
    }
    if (item.empty())
        return;

    auto& prev = src.back();
    auto& next = item.front();

    if (!paste_is_valid(prev, next)) {
        // Report, then keep both operands as separate tokens (gcc/clang).
        // Nothing of prev (text, hide set) changes. ISSUE runs before the
        // push so a thrown Issue::Exception leaves src untouched, and `prev`
        // is not used after the push (it may dangle).
        ISSUE(INVALID_TOKEN_PASTING,
              paste_echo_tail(prev.text) + " @@ " + paste_echo_head(next.text));
        for (auto& t : item)
            src.push_back(std::move(t));
        return;
    }

    prev.hs = Token::HideSet::intersect(prev.hs, next.hs);
    prev.text += next.text;
    // A pasted token is not the lexer's own text: its pragma body (if it
    // is a PRAGMA) is re-lexed with every diagnostic reported (A2 / S-02).
    prev.relexed_pragma = false;

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

int argc_from(int idx, const std::vector<std::vector<Token>>& actuals) {
    return std::max(0, static_cast<int>(actuals.size()) - idx);
}

// Shared by every site that resolves __VA_ARGC__ as a formal parameter:
// the normal substitution path, the `@@` (paste) raw-operand path, and the
// `@` (stringize) raw-operand path (F16). Kept as a single helper so the
// three sites cannot drift out of sync again.
Token va_argc_token(int idx, const std::vector<std::vector<Token>>& actuals) {
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

Token stringize_tokens(const std::vector<Token>& ts) {
    return Token::create(Token::STRING, Util::encode_iec_string(stringize_text(ts), '\''));
}

// ── §Support functions: __VA_OPT__ helpers ────────────────────────

// True when the macro is variadic (either form) and its variable arguments
// are non-empty. O(1): see va_args_empty().
static bool va_args_non_empty(
    const std::unordered_map<std::string, std::pair<int, bool>>& formal_params,
    const std::vector<std::vector<Token>>& actual_params) {
    auto it = formal_params.find(FunctionMacro::VA_SYM);
    if (it == formal_params.end()) return false;
    auto [va_idx, is_va] = it->second;
    return !va_args_empty(va_idx, actual_params, is_va);
}

// For the __VA_OPT__ token at body[i] (va_opt: the VaOptSpan table of body):
// raises "nested __VA_OPT__ is not allowed" once per __VA_OPT__ token inside
// its parentheses, before the malformed check, exactly as the old content
// scan did, and charges one step per report. Returns the index of the
// matching ')' (the content is body[i + next + 1, that index)), or
// body.size() if no parenthesized argument follows or it is unclosed.
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

// Work budget (PP64) cost of one token: 1 + len/64 steps.
static inline std::uint64_t step_cost(const Token& t) {
    return 1 + t.text.size() / 64;
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
    // parameter adjacent to `@@` substitutes to zero tokens; cleared whenever any
    // other token is pushed to `result`.
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

    for (std::size_t i = 0; i < body.size(); ++i) {
        charge_new();
        const Token& t = body[i];

        // §subst case: IS is ## • T • IS' (paste)
        if (t.type == Token::GLUE) {
            // The left operand is a placemarker (an empty formal-param substitution
            // adjacent to this @@) when `pending_placemarker` is set. In that case the
            // whitespace preceding it belongs to the surrounding text, not to the
            // (nonexistent) left operand, so it must not be stripped.
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
            const Token& nb = body[j];
            if (nb.type == Token::ANY && nb.text == FunctionMacro::VA_OPT) {
                // Handle:  X @@ __VA_OPT__(content)
                ensure_va_opt();
                std::size_t k = va_opt_close(body, va_opt, j, env);
                if (k < body.size()) {
                    if (va_args_non_empty(formal_params, actual_params))
                        item = ts_flatten(subst_va_opt_content(body, va_opt, j, k, formal_params,
                                                               actual_params, actual_ws, env,
                                                               va_sep_ws, memo));
                    // Empty item → glue_tokens is a no-op → left side preserved unchanged.
                    i = k;
                } else {
                    item = {nb.clone()};
                    i = j;
                }
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

            if (left_is_placemarker) {
                // Placemarker @@ item == item: append verbatim, no pasting.
                for (auto& tk : item)
                    result.push_back(std::move(tk));
                pending_placemarker = item.empty();
            } else {
                // Work budget (PP64): the one token merged by @@ (item[0]
                // into result.back()) is charged before it is merged (a merge
                // grows result.back() in place, which charge_new() above
                // cannot see). Every token that is pushed to result instead
                // -- item[1..] of a valid paste, the whole of item when the
                // paste fails (PP32) or result is empty -- is charged once,
                // by the next charge_new(); charging it here as well would
                // count it twice.
                std::uint64_t merge_cost = 0;
                if (!result.empty() && !item.empty() &&
                    paste_is_valid(result.back(), item.front()))
                    merge_cost = step_cost(item.front());
                env.charge_expansion_steps(merge_cost);
                glue_tokens(result, item);
                pending_placemarker = false;
            }
            continue;
        }

        // §subst case: IS is # • T • IS' (stringize)
        if (t.type == Token::STRINGIZE) {
            // A stringized result is always a real (possibly empty-text) STRING
            // token, never a placemarker.
            pending_placemarker = false;
            std::size_t j = i + 1;
            if (!va_opt.empty()) {
                j = next_non_ws(va_opt, i, body.size());
            } else {
                // A body without __VA_OPT__ is normalized: no whitespace
                // between `@` and a formal parameter, and on the error path
                // below every skipped whitespace token is emitted (and
                // charged) right after.
                while (j < body.size() && (body[j].type & Token::MASK_WS)) ++j;
            }
            if (j < body.size() && body[j].type == Token::ANY) {
                if (body[j].text == FunctionMacro::VA_OPT) {
                    // Handle: @ __VA_OPT__(content) → stringize VA_OPT content or ''
                    ensure_va_opt();
                    std::size_t k = va_opt_close(body, va_opt, j, env);
                    if (k < body.size()) {
                        std::vector<Token> inner;
                        if (va_args_non_empty(formal_params, actual_params))
                            inner = subst_va_opt_content(body, va_opt, j, k, formal_params,
                                                         actual_params, actual_ws, env,
                                                         /*va_sep_ws=*/true, memo);
                        result.push_back(stringize_tokens(inner));
                        i = k;
                        continue;
                    }
                } else {
                    auto pit = formal_params.find(body[j].text);
                    if (pit != formal_params.end()) {
                        auto [pidx, is_va] = pit->second;
                        if (body[j].text == FunctionMacro::VA_ARGC) {
                            // Checked before the stringize cache: __VA_ARGC__
                            // shares the pidx (and slot) of the variable
                            // arguments (review M1).
                            result.push_back(stringize_tokens({va_argc_token(pidx, actual_params)}));
                        } else {
                            // @param / @__VA_ARGS__ (see stringize_param()).
                            result.push_back(
                                stringize_param(pidx, is_va, actual_params, actual_ws, memo));
                        }
                        i = j;
                        continue;
                    }
                }
            }
            ISSUE(INVALID_STRINGIZING);
            result.push_back(t.clone());
            continue;
        }

        // §subst case: IS is T • IS' and T is FP[i] (formal param → expand or flatten)
        if (t.type == Token::ANY) {
            // Handle standalone __VA_OPT__(content)
            if (t.text == FunctionMacro::VA_OPT) {
                pending_placemarker = false;
                if (formal_params.find(FunctionMacro::VA_SYM) == formal_params.end()) {
                    ISSUE(INVALID_VA_OPT, "__VA_OPT__ used outside variadic macro");
                    result.push_back(t.clone());
                    continue;
                }
                ensure_va_opt();
                std::size_t j = va_opt_close(body, va_opt, i, env);
                if (j >= body.size()) {
                    ISSUE(INVALID_VA_OPT, "__VA_OPT__ requires parenthesized argument");
                    result.push_back(t.clone());
                    continue;
                }
                if (va_args_non_empty(formal_params, actual_params)) {
                    auto inner = subst_va_opt_content(body, va_opt, i, j, formal_params,
                                                      actual_params, actual_ws, env, va_sep_ws,
                                                      memo);
                    for (auto& et : inner) result.push_back(std::move(et));
                }
                i = j; // advance past closing ')'
                continue;
            }

            auto pit = formal_params.find(t.text);
            if (pit != formal_params.end()) {
                auto [pidx, is_va] = pit->second;

                if (t.text == FunctionMacro::VA_ARGC) {
                    pending_placemarker = false;
                    result.push_back(va_argc_token(pidx, actual_params));
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
                    auto flat = ts_flatten(*actual);
                    if (flat.empty()) {
                        pending_placemarker = true;
                    } else {
                        for (auto& ft : flat)
                            result.push_back(std::move(ft));
                        pending_placemarker = false;
                    }
                } else if (slot < 0) {
                    // O3: expand() appends straight into `result` instead of
                    // into a throwaway `expanded` vector that is then
                    // copied token-by-token -- one fewer O(k) copy per
                    // formal-parameter occurrence.
                    expand(*actual, result, env);
                    pending_placemarker = false;
                } else {
                    auto& cached = memo->expanded[static_cast<std::size_t>(slot)];
                    if (!cached) {
                        cached.emplace();
                        if (slot_needs_marked_spelling)
                            expand(select_arg(pidx, actual_params, true, actual_ws,
                                              VaJoin::shared),
                                   *cached, env);
                        else
                            expand(*actual, *cached, env);
                    }
                    const bool last =
                        (--memo->remaining[static_cast<std::size_t>(slot)] == 0);
                    append_expanded(result, *cached, /*keep_sep=*/va_sep_ws, /*move=*/last, env);
                    // Already charged (before the copy) by append_expanded().
                    charged = result.size();
                    pending_placemarker = false;
                }
                continue;
            }
        }

        // §subst fallthrough: IS must be T_HS' • IS'
        pending_placemarker = false;
        result.push_back(t.clone());
    }

    charge_new();
    hsadd(hs, result);
    return result;
}

} // namespace jiepp::expand_detail
