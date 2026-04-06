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
                } else {
                    t.hs = hs_add_all(t.hs, hs);
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
                              bool is_va) {
    if (!is_va) {
        if (idx < static_cast<int>(actuals.size()))
            return actuals[idx];
        return {};
    }

    std::vector<Token> result;
    for (int k = idx; k < static_cast<int>(actuals.size()); ++k) {
        if (k > idx)
            result.push_back(Token::create(Token::SEP, ","));
        for (auto& t : actuals[k])
            result.push_back(t);
    }
    return result;
}

int argc_from(int idx, const std::vector<std::vector<Token>>& actuals) {
    return std::max(0, static_cast<int>(actuals.size()) - idx);
}

Token stringize_tokens(const std::vector<Token>& ts) {
    std::string text;
    for (auto& t : ts_flatten(ts))
        text += t.text;
    return Token::create(Token::STRING, Util::encode_iec_string(text, '\''));
}

// ── §subst — substitute args, handle stringize and paste ──────────

std::vector<Token> subst(
    const std::vector<Token>& body,
    const std::unordered_map<std::string, std::pair<int, bool>>& formal_params,
    const std::vector<std::vector<Token>>& actual_params,
    const Token::HideSet& hs,
    Env& env) {

    std::vector<Token> result;
    result.reserve(body.size());
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
            while (!result.empty() && (result.back().type & Token::MASK_WS))
                result.pop_back();

            std::size_t j = i + 1;
            while (j < body.size() && (body[j].type & Token::MASK_WS)) ++j;
            if (j >= body.size())
                continue;

            std::vector<Token> item;
            const Token& nb = body[j];
            if (nb.type == Token::ANY) {
                auto pit = formal_params.find(nb.text);
                if (pit != formal_params.end()) {
                    auto [pidx, is_va] = pit->second;
                    item = ts_flatten(select_arg(pidx, actual_params, is_va));
                }
            }
            if (item.empty())
                item = {nb.clone()};
            i = j;

            glue_tokens(result, item);
            continue;
        }

        // §subst case: IS is # • T • IS' (stringize)
        if (t.type == Token::STRINGIZE) {
            std::size_t j = i + 1;
            while (j < body.size() && (body[j].type & Token::MASK_WS)) ++j;
            if (j < body.size() && body[j].type == Token::ANY) {
                auto pit = formal_params.find(body[j].text);
                if (pit != formal_params.end()) {
                    auto [pidx, is_va] = pit->second;
                    auto actual = select_arg(pidx, actual_params, is_va);
                    result.push_back(stringize_tokens(actual));
                    i = j;
                    continue;
                }
            }
            ISSUE(INVALID_STRINGIZING);
            result.push_back(t.clone());
            continue;
        }

        // §subst case: IS is T • IS' and T is FP[i] (formal param → expand or flatten)
        if (t.type == Token::ANY) {
            auto pit = formal_params.find(t.text);
            if (pit != formal_params.end()) {
                auto [pidx, is_va] = pit->second;

                if (t.text == FunctionMacro::VA_ARGC) {
                    int cnt = argc_from(pidx, actual_params);
                    result.push_back(Token::create(Token::ANY, std::to_string(cnt)));
                    continue;
                }

                auto actual = select_arg(pidx, actual_params, is_va);

                if (glue_adjacent[i]) {
                    auto flat = ts_flatten(actual);
                    for (auto& ft : flat)
                        result.push_back(ft);
                } else {
                    std::vector<Token> expanded;
                    expand(actual, expanded, env);
                    for (auto& et : expanded)
                        result.push_back(et);
                }
                continue;
            }
        }

        // §subst fallthrough: IS must be T_HS' • IS'
        result.push_back(t.clone());
    }

    hsadd(hs, result);
    return result;
}

} // namespace jiepp::expand_detail
