// expand_subst.cpp — Prosser's algorithm: subst, glue, hsadd, support functions.
// Corresponds to cpp.algo.md (X3J11/86-196): §subst, §glue, §hsadd, §Support functions.

#include "expand_helpers.hpp"
#include "preprocessor.hpp"
#include "preprocessor_internal.hpp"

#include "../macro/macro.hpp"
#include "../util/iec_61131-3.hpp"

#include <algorithm>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace jiepp::expand_detail {

// ── §hsadd — add to token sequence's hide sets ────────────────────

std::vector<Token>& hsadd(const Token::HideSet& hs, std::vector<Token>& ts) {
    if (!hs.empty()) {
        Token::HideSetPtr shared_hs;
        // O1b: consecutive tokens in `ts` overwhelmingly share the same
        // starting hide set (e.g. every token of a freshly-substituted macro
        // body), so hs_add_all() would recompute the identical union once
        // per token. Memoize the last (base pointer -> result) mapping and
        // reuse it while the base pointer stays the same, skipping both the
        // hs_add_all() call and, when the pointer is already correct, the
        // assignment itself. memo_in is held as a strong reference (not a raw
        // pointer): once t.hs is reassigned below, the old base HideSet may
        // lose its last owner and be freed, and a later make_shared<HideSet>
        // could reuse that address, making a raw-pointer memo key alias an
        // unrelated hide set and silently mis-memoize a later token.
        Token::HideSetPtr memo_in;
        Token::HideSetPtr memo_out;
        for (auto& t : ts) {
            // Per Prosser's algorithm, hide-sets propagate to all tokens (including
            // LP/RP/SEP) so that the closing ')' correctly carries context hs for
            // the hs(t) ∩ hs(rp) ∪ {M} intersection in function macro expansion.
            switch (t.type) {
            case Token::ANY:
            case Token::LP:
            case Token::RP:
            case Token::SEP:
                if (!t.hs || t.hs->empty()) {
                    if (!shared_hs)
                        shared_hs = std::make_shared<Token::HideSet>(hs);
                    t.hs = shared_hs;
                } else if (t.hs == memo_in) {
                    if (t.hs != memo_out)
                        t.hs = memo_out;
                } else {
                    memo_in = t.hs;
                    t.hs = hs_add_all(t.hs, hs);
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

void glue_tokens(std::vector<Token>& src, std::vector<Token>& item) {
    if (src.empty()) {
        for (auto& t : item)
            src.push_back(t);
        return;
    }
    if (item.empty())
        return;

    auto& prev = src.back();
    auto& next = item.front();

    if (prev.type != Token::ANY || next.type != Token::ANY)
        if (prev.type != next.type)
            ISSUE(INVALID_TOKEN_PASTING, prev.text + " @@ " + next.text);

    prev.hs = hs_intersect(prev.hs, next.hs);
    prev.text += next.text;

    for (std::size_t k = 1; k < item.size(); ++k)
        src.push_back(item[k]);
}

// ── §Support functions: select, stringize ─────────────────────────

std::vector<Token> select_arg(int idx,
                              const std::vector<std::vector<Token>>& actuals,
                              bool is_va,
                              const std::vector<ArgWs>* ws) {
    if (!is_va) {
        if (idx < static_cast<int>(actuals.size()))
            return actuals[idx];
        return {};
    }

    // R6: reproduce clang's comma spacing for @__VA_ARGS__/@__VA_OPT__ --
    // one space before the comma iff the preceding actual had trailing
    // whitespace/comment in the call, one after iff the following actual
    // had leading whitespace/comment. Bounds-checked: a missing ws entry
    // (ws == nullptr, or shorter than actuals) counts as false.
    auto flag = [&](int k, bool ArgWs::*field) {
        return ws && k >= 0 && k < static_cast<int>(ws->size()) && (*ws)[k].*field;
    };

    std::vector<Token> result;
    for (int k = idx; k < static_cast<int>(actuals.size()); ++k) {
        if (k > idx) {
            if (flag(k - 1, &ArgWs::trail))
                result.push_back(Token::create(Token::WS, " "));
            result.push_back(Token::create(Token::SEP, ","));
            if (flag(k, &ArgWs::lead))
                result.push_back(Token::create(Token::WS, " "));
        }
        for (auto& t : actuals[k])
            result.push_back(t);
    }
    return result;
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
// are non-empty.
static bool va_args_non_empty(
    const std::unordered_map<std::string, std::pair<int, bool>>& formal_params,
    const std::vector<std::vector<Token>>& actual_params) {
    auto it = formal_params.find(FunctionMacro::VA_SYM);
    if (it == formal_params.end()) return false;
    auto [va_idx, is_va] = it->second;
    return !select_arg(va_idx, actual_params, is_va).empty();
}

// Collects the token sequence inside __VA_OPT__(...), handling nested parens.
// `from` is the index in `body` just after the __VA_OPT__ token.
// Returns the index of the closing ')' on success, or body.size() on parse error.
static std::size_t collect_va_opt_content(
    const std::vector<Token>& body,
    std::size_t from,
    std::vector<Token>& content) {
    std::size_t j = from;
    while (j < body.size() && (body[j].type & Token::MASK_WS)) ++j;
    if (j >= body.size() || body[j].type != Token::LP) return body.size();
    ++j; // skip opening '('
    int depth = 1;
    while (j < body.size()) {
        const auto& bt = body[j];
        if (bt.type == Token::LP) {
            ++depth;
        } else if (bt.type == Token::RP) {
            if (--depth == 0) break;
        } else if (bt.type == Token::ANY && bt.text == FunctionMacro::VA_OPT) {
            ISSUE(INVALID_VA_OPT, "nested __VA_OPT__ is not allowed");
        }
        content.push_back(bt);
        ++j;
    }
    if (depth != 0) return body.size(); // unclosed parenthesis
    return j; // index of matching closing ')'
}

// ── §subst — substitute args, handle stringize and paste ──────────

std::vector<Token> subst(
    const std::vector<Token>& body,
    const std::unordered_map<std::string, std::pair<int, bool>>& formal_params,
    const std::vector<std::vector<Token>>& actual_params,
    const std::vector<ArgWs>* actual_ws,
    const Token::HideSet& hs,
    Env& env,
    bool va_sep_ws) {

    std::vector<Token> result;
    result.reserve(body.size());
    // Tracks whether the token most recently produced for `result` (or, before any
    // token has been produced, the position just before the first GLUE) is a
    // placemarker (C17 6.10.3.3p2): an empty token sequence that participates in
    // `@@` pasting as if it were an actual, invisible token. Set when a formal
    // parameter adjacent to `@@` substitutes to zero tokens; cleared whenever any
    // other token is pushed to `result`.
    bool pending_placemarker = false;
    std::vector<bool> glue_adjacent(body.size(), false);
    for (std::size_t i = 0; i < body.size(); ++i) {
        if (body[i].type != Token::GLUE)
            continue;
        for (int j = static_cast<int>(i) - 1; j >= 0; --j) {
            if (body[j].type & Token::MASK_WS)
                continue;
            glue_adjacent[j] = true;
            break;
        }
        for (std::size_t j = i + 1; j < body.size(); ++j) {
            if (body[j].type & Token::MASK_WS)
                continue;
            glue_adjacent[j] = true;
            break;
        }
    }

    for (std::size_t i = 0; i < body.size(); ++i) {
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
                std::vector<Token> content;
                std::size_t k = collect_va_opt_content(body, j + 1, content);
                if (k < body.size()) {
                    if (va_args_non_empty(formal_params, actual_params))
                        item = ts_flatten(subst(content, formal_params, actual_params,
                                                actual_ws, {}, env, va_sep_ws));
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
                            item = ts_flatten(select_arg(pidx, actual_params, is_va,
                                                         va_sep_ws ? actual_ws : nullptr));
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
            while (j < body.size() && (body[j].type & Token::MASK_WS)) ++j;
            if (j < body.size() && body[j].type == Token::ANY) {
                if (body[j].text == FunctionMacro::VA_OPT) {
                    // Handle: @ __VA_OPT__(content) → stringize VA_OPT content or ''
                    std::vector<Token> content;
                    std::size_t k = collect_va_opt_content(body, j + 1, content);
                    if (k < body.size()) {
                        std::vector<Token> inner;
                        if (va_args_non_empty(formal_params, actual_params))
                            inner = subst(content, formal_params, actual_params,
                                         actual_ws, {}, env, /*va_sep_ws=*/true);
                        result.push_back(stringize_tokens(inner));
                        i = k;
                        continue;
                    }
                } else {
                    auto pit = formal_params.find(body[j].text);
                    if (pit != formal_params.end()) {
                        auto [pidx, is_va] = pit->second;
                        if (body[j].text == FunctionMacro::VA_ARGC) {
                            result.push_back(stringize_tokens({va_argc_token(pidx, actual_params)}));
                        } else {
                            // @param / @__VA_ARGS__: the stringize operator applies
                            // directly to this formal parameter, so R6's comma
                            // spacing always applies here regardless of va_sep_ws
                            // (which only governs the separate @__VA_OPT__(...)
                            // content-substitution recursion above).
                            auto actual = select_arg(pidx, actual_params, is_va, actual_ws);
                            result.push_back(stringize_tokens(actual));
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
                std::vector<Token> content;
                std::size_t j = collect_va_opt_content(body, i + 1, content);
                if (j >= body.size()) {
                    ISSUE(INVALID_VA_OPT, "__VA_OPT__ requires parenthesized argument");
                    result.push_back(t.clone());
                    continue;
                }
                if (va_args_non_empty(formal_params, actual_params)) {
                    auto inner = subst(content, formal_params, actual_params,
                                       actual_ws, {}, env, va_sep_ws);
                    for (auto& et : inner) result.push_back(et);
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

                // O2b: select_arg() copies the actual's token vector even for
                // the (overwhelmingly common) non-variadic case, where
                // actual_params[pidx] already holds exactly the tokens
                // select_arg() would build. Bind a const& straight into
                // actual_params there instead; only the variadic path (which
                // must concatenate multiple actuals with inserted SEP
                // tokens) still needs select_arg()'s freshly-built vector,
                // held alive in actual_va.
                std::vector<Token> actual_va;
                static const std::vector<Token> empty_actual;
                const std::vector<Token>& actual =
                    is_va
                        ? (actual_va = select_arg(pidx, actual_params, is_va,
                                                  va_sep_ws ? actual_ws : nullptr))
                        : (pidx < static_cast<int>(actual_params.size())
                               ? actual_params[pidx]
                               : empty_actual);

                if (glue_adjacent[i]) {
                    // A formal parameter directly adjacent to @@ that substitutes to
                    // zero tokens is a placemarker (C17 6.10.3.3p2): it must not fall
                    // back to being pasted as its own literal name.
                    auto flat = ts_flatten(actual);
                    if (flat.empty()) {
                        pending_placemarker = true;
                    } else {
                        for (auto& ft : flat)
                            result.push_back(ft);
                        pending_placemarker = false;
                    }
                } else {
                    // O3: expand() appends straight into `result` instead of
                    // into a throwaway `expanded` vector that is then
                    // copied token-by-token -- one fewer O(k) copy per
                    // formal-parameter occurrence.
                    expand(actual, result, env);
                    pending_placemarker = false;
                }
                continue;
            }
        }

        // §subst fallthrough: IS must be T_HS' • IS'
        pending_placemarker = false;
        result.push_back(t.clone());
    }

    hsadd(hs, result);
    return result;
}

} // namespace jiepp::expand_detail
