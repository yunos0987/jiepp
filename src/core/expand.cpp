// expand.cpp — Prosser's algorithm: §expand (main expansion loop).
// Corresponds to cpp.algo.md (X3J11/86-196): §expand.
// §subst, §glue, §hsadd, §Support functions are in expand_subst.cpp.
// Conditional compilation control (#if/#elif/#else/#endif) is in expand_ctrl.cpp.

#include "expand_helpers.hpp"
#include "preprocessor.hpp"
#include "../loader/loader.hpp"
#include "../loader/directive_parser.hpp"
#include "preprocessor_internal.hpp"

#include "../macro/macro.hpp"

#include <algorithm>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

using namespace jiepp::expand_detail;

namespace {

void err_set_lineno(int ln) {
    std::string fp = Issue::filepath();
    Issue::pop();
    Issue::push({ln, fp});
}

void advance_lineno(int delta, Env& env) {
    if (delta == 0)
        return;
    int nl = env.get_lineno() + delta;
    env.set_lineno(nl);
    err_set_lineno(nl);
}

// ── jiepp extension: directive dispatch (not in Prosser) ──────────

void dispatch_directive(const Token& t,
                        std::vector<CtrlState>& ctrl,
                        Env& env,
                        std::vector<Token>& ots) {
    auto [key, raw_arg] = parse_directive(t.text);
    int kind = DirectiveToken::name_to_kind(key);
    if (kind == -1)
        return;

    if (kind & DirectiveToken::MASK_CTRLEX) {
        if (kind == DirectiveToken::IFDEF) {
            raw_arg = "defined(" + raw_arg + ")";
        } else if (kind == DirectiveToken::IFNDEF) {
            raw_arg = "\\not\\ defined(" + raw_arg + ")";
        }
        kind = DirectiveToken::IF;
    }

    bool active = ctrl_is_active(ctrl);

    if (kind & DirectiveToken::MASK_CTRL) {
        switch (kind) {
        case DirectiveToken::IF:
            if (static_cast<int>(ctrl.size()) > env.get_max_if_nesting()) {
                ISSUE(MAX_IF_NESTING_EXCEEDED);
            }
            if (active) {
                bool cond = eval_cond(raw_arg, env);
                ctrl.push_back({false, cond ? std::optional<bool>(true) : std::nullopt});
            } else {
                ctrl.push_back({false, false});
            }
            break;
        case DirectiveToken::ELIF:
            if (ctrl.size() <= 1) {
                ISSUE(ELIF_ERROR, "elif without matching if");
            }
            {
                auto& last = ctrl.back();
                if (last.seen_else) {
                    ISSUE(ELIF_ERROR, "elif after else");
                }
                if (last.condition.has_value() && last.condition.value()) {
                    last = {true, false};
                } else if (!last.condition.has_value()) {
                    if (ctrl_parent_active(ctrl)) {
                        bool cond = eval_cond(raw_arg, env);
                        last = {true, cond ? std::optional<bool>(true) : std::nullopt};
                    } else {
                        last = {true, false};
                    }
                }
            }
            break;
        case DirectiveToken::ELSE:
            if (ctrl.size() <= 1) {
                ISSUE(ELSE_ERROR, "else without matching if");
            }
            {
                auto& last = ctrl.back();
                if (last.seen_else) {
                    ISSUE(ELSE_ERROR, "else after else");
                }
                last.seen_else = true;
                if (last.condition.has_value() && last.condition.value()) {
                    last.condition = false;
                } else if (!last.condition.has_value()) {
                    last.condition =
                        ctrl_parent_active(ctrl) ? std::optional<bool>(true) : false;
                }
            }
            break;
        case DirectiveToken::ENDIF:
            if (ctrl.size() <= 1) {
                ISSUE(ENDIF_ERROR, "endif without matching if");
            }
            ctrl.pop_back();
            break;
        default:
            break;
        }
        return;
    }

    if (!active)
        return;

    switch (kind) {
    case DirectiveToken::DEFINE:
        jiepp::preprocessor_detail::handle_define(raw_arg, env);
        break;
    case DirectiveToken::UNDEF:
        jiepp::preprocessor_detail::handle_undef(raw_arg, env);
        break;
    case DirectiveToken::TOKENIZE:
        jiepp::preprocessor_detail::handle_tokenize(raw_arg, env, ots);
        break;
    case DirectiveToken::STRING:
        jiepp::preprocessor_detail::handle_stringize(raw_arg, env, ots, false);
        break;
    case DirectiveToken::WSTRING:
        jiepp::preprocessor_detail::handle_stringize(raw_arg, env, ots, true);
        break;
    case DirectiveToken::SETLINE:
        jiepp::preprocessor_detail::handle_setline(raw_arg, env, ots);
        break;
    case DirectiveToken::SYSPATH:
#ifdef JIEPP_SANDBOX
        ISSUE(SANDBOX_RESTRICTED_DIRECTIVE, "syspath");
#else
        jiepp::preprocessor_detail::handle_syspath(raw_arg, env, ots);
#endif
        break;
    case DirectiveToken::INCLUDE:
#ifdef JIEPP_SANDBOX
        ISSUE(SANDBOX_RESTRICTED_DIRECTIVE, "include");
#else
        jiepp::preprocessor_detail::handle_include(raw_arg, env, ots, false);
        ots.push_back(Token::line_pragma(
            env.get_lineno(), Issue::filepath(), env.is_standard_pragma_style()));
#endif
        break;
    case DirectiveToken::SINCLUDE:
#ifdef JIEPP_SANDBOX
        ISSUE(SANDBOX_RESTRICTED_DIRECTIVE, "sinclude");
#else
        jiepp::preprocessor_detail::handle_include(raw_arg, env, ots, true);
        ots.push_back(Token::line_pragma(
            env.get_lineno(), Issue::filepath(), env.is_standard_pragma_style()));
#endif
        break;
    case DirectiveToken::ERROR:
        jiepp::preprocessor_detail::handle_message(raw_arg, env, Issue::Code::ERROR_MESSAGE);
        break;
    case DirectiveToken::WARNING:
        jiepp::preprocessor_detail::handle_message(raw_arg, env, Issue::Code::WARNING_MESSAGE);
        break;
    case DirectiveToken::INFO:
        jiepp::preprocessor_detail::handle_message(raw_arg, env, Issue::Code::INFO_MESSAGE);
        break;
    case DirectiveToken::SEVERE:
        jiepp::preprocessor_detail::handle_message(raw_arg, env, Issue::Code::SEVERE_MESSAGE);
        break;
    case DirectiveToken::IGNORE:
#ifdef JIEPP_SANDBOX
        ISSUE(SANDBOX_RESTRICTED_DIRECTIVE, "ignore");
#else
        jiepp::preprocessor_detail::handle_ignore(raw_arg, env);
#endif
        break;
    case DirectiveToken::MAX_INCLUDE_DEPTH:
#ifdef JIEPP_SANDBOX
        ISSUE(SANDBOX_RESTRICTED_DIRECTIVE, "max_include_depth");
#else
        jiepp::preprocessor_detail::handle_max_include_depth(raw_arg, env);
#endif
        break;
    case DirectiveToken::MAX_EXPANSION_DEPTH:
#ifdef JIEPP_SANDBOX
        ISSUE(SANDBOX_RESTRICTED_DIRECTIVE, "max_expansion_depth");
#else
        jiepp::preprocessor_detail::handle_max_expansion_depth(raw_arg, env);
#endif
        break;
    case DirectiveToken::MAX_IF_NESTING:
#ifdef JIEPP_SANDBOX
        ISSUE(SANDBOX_RESTRICTED_DIRECTIVE, "max_if_nesting");
#else
        jiepp::preprocessor_detail::handle_max_if_nesting(raw_arg, env);
#endif
        break;
    case DirectiveToken::PP_OUTPUT_PRAGMA_STYLE:
        jiepp::preprocessor_detail::handle_pragma_style(raw_arg, env);
        break;
    case DirectiveToken::NOP:
        break;
    default:
        break;
    }
}

// ── jiepp extension: pragma expansion (not in Prosser) ────────────

void expand_pragma_token(const Token& t, Env& env, std::vector<Token>& ots) {
    const auto& ptext = t.text;
    std::string body;
    bool is_annotated = (ptext.size() >= 5 && ptext.substr(0, 3) == "(*{" &&
                         ptext.substr(ptext.size() - 3) == "}*)");
    if (is_annotated) {
        body = ptext.substr(3, ptext.size() - 6);
    } else if (ptext.size() >= 2 && ptext.front() == '{' && ptext.back() == '}') {
        body = ptext.substr(1, ptext.size() - 2);
    } else {
        ots.push_back(t);
        return;
    }

    std::string expanded_body = preprocess_text(body, env);
    Token out = t;
    if (env.is_standard_pragma_style()) {
        out.text = "{" + expanded_body + "}";
    } else {
        out.text = "(*{" + expanded_body + "}*)";
    }
    ots.push_back(out);
}

} // namespace

// ── §expand — main expansion loop (cpp.algo.md) ──────────────────

namespace {

struct ExpansionDepthGuard {
    Env& env;
    explicit ExpansionDepthGuard(Env& e) : env(e) {
        env.inc_expansion_depth();
        if (env.expansion_depth() > env.get_max_expansion_depth()) {
            ISSUE(MAX_EXPANSION_DEPTH_EXCEEDED);
        }
    }
    ~ExpansionDepthGuard() { env.dec_expansion_depth(); }
    ExpansionDepthGuard(const ExpansionDepthGuard&) = delete;
    ExpansionDepthGuard& operator=(const ExpansionDepthGuard&) = delete;
};

} // namespace

std::vector<Token>& expand(const std::vector<Token>& its, std::vector<Token>& ots, Env& env) {
    ExpansionDepthGuard depth_guard(env);

    // Reversed work stack: back() = next token to process.
    // pop_back/push_back are O(1), eliminating the O(N²) erase+insert pattern
    // that previously shifted the entire vector on every macro expansion.
    std::vector<Token> work(its.rbegin(), its.rend());
    std::vector<CtrlState> ctrl = {{false, true}};

    while (!work.empty()) {
        Token t = std::move(work.back());
        work.pop_back();

        if (t.num_of_lines > 0) {
            advance_lineno(t.num_of_lines, env);
        }

        // ── jiepp extension: directive processing ──
        if (t.type == Token::DIRECTIVE) {
            dispatch_directive(t, ctrl, env, ots);
            continue;
        }

        bool active = ctrl_is_active(ctrl);

        if (!active) {
            if (t.num_of_lines > 0)
                ots.push_back(Token::newline(t.num_of_lines));
            continue;
        }

        // ── jiepp extension: pragma expansion ──
        if (t.type == Token::PRAGMA) {
            expand_pragma_token(t, env, ots);
            continue;
        }

        // ── Prosser's algorithm (cpp.algo.md §expand) ──

        if (t.type == Token::ANY && (!t.hs || !t.hs->count(t.text))) {
            Macro* macro = env.lookup(t.text);
            if (macro) {

            // defined operator (jiepp extension)
            if (dynamic_cast<DefinedOperator*>(macro)) {
                while (!work.empty() && (work.back().type & Token::MASK_WS))
                    work.pop_back();

                bool has_paren = false;
                if (!work.empty() && work.back().type == Token::LP) {
                    has_paren = true;
                    work.pop_back();
                    while (!work.empty() && (work.back().type & Token::MASK_WS))
                        work.pop_back();
                }

                std::string operand;
                if (!work.empty() && work.back().type == Token::ANY) {
                    operand = work.back().text;
                    work.pop_back();
                } else {
                    ISSUE(INVALID_DEFINED_OPERAND, t.text);
                    ots.push_back(Token::create(Token::ANY, "0"));
                    continue;
                }

                if (has_paren) {
                    while (!work.empty() && (work.back().type & Token::MASK_WS))
                        work.pop_back();
                    if (!work.empty() && work.back().type == Token::RP) {
                        work.pop_back();
                    } else {
                        ISSUE(INVALID_EXPRESSION, "defined");
                    }
                }

                Macro* m = env.lookup(operand);
                bool is_def = m && !dynamic_cast<DefinedOperator*>(m);
                ots.push_back(Token::create(Token::ANY, is_def ? "1" : "0"));
                continue;
            }

            // Case: T is a "()'d macro" → expand(subst(ts(T), fp(T), actuals, (HS∩HS')∪{T}, {}) • TS'')
            if (auto* fm = dynamic_cast<FunctionMacro*>(macro)) {
                // Find '(' after macro name, skipping whitespace
                std::vector<Token> pre_lp;
                bool found_lp = false;
                while (!work.empty()) {
                    if (work.back().type & Token::MASK_WS) {
                        pre_lp.push_back(std::move(work.back()));
                        work.pop_back();
                    } else if (work.back().type == Token::LP) {
                        pre_lp.push_back(std::move(work.back()));
                        work.pop_back();
                        found_lp = true;
                        break;
                    } else {
                        break;
                    }
                }

                if (found_lp) {
                    // Collect parameters until matching ')'
                    int sum_num_of_lines = 0;
                    for (auto& s : pre_lp)
                        sum_num_of_lines += s.num_of_lines;

                    std::vector<std::vector<Token>> params;
                    std::vector<Token> cur;
                    int depth = 0;
                    bool found_rp = false;
                    Token rp_token;

                    while (!work.empty()) {
                        Token pt = std::move(work.back());
                        work.pop_back();
                        sum_num_of_lines += pt.num_of_lines;

                        if (depth == 0 && pt.type == Token::RP) {
                            rp_token = std::move(pt);
                            params.push_back(std::move(cur));
                            found_rp = true;
                            break;
                        }

                        if (depth == 0 && pt.type == Token::SEP) {
                            params.push_back(std::move(cur));
                            cur.clear();
                            continue;
                        }

                        if (pt.type == Token::LP || pt.type == Token::LB) ++depth;
                        else if ((pt.type == Token::RP || pt.type == Token::RB) && depth > 0) --depth;

                        cur.push_back(std::move(pt));
                    }

                    if (!found_rp) {
                        ISSUE(ARGUMENT_COUNT_MISMATCH, "missing closing parenthesis");
                    }

                    // Zero-args check: M() with min_params == 0
                    if (params.size() == 1) {
                        bool empty = true;
                        for (auto& tok : params[0]) {
                            if (!(tok.type & Token::MASK_WS)) {
                                empty = false;
                                break;
                            }
                        }
                        if (empty && fm->num_of_params_min() == 0) {
                            params.clear();
                        }
                    }

                    int n = static_cast<int>(params.size());
                    if (n < fm->num_of_params_min() || n > fm->num_of_params_max()) {
                        ISSUE(ARGUMENT_COUNT_MISMATCH,
                              "expected " + std::to_string(fm->num_of_params_min())
                              + (fm->num_of_params_min() == fm->num_of_params_max()
                                 ? "" : "-" + std::to_string(fm->num_of_params_max()))
                              + ", got " + std::to_string(n));
                    }

                    for (auto& p : params)
                        p = ts_flatten(std::move(p));

                    Token::HideSet new_hs;
                    if (t.hs && rp_token.hs) {
                        std::set_intersection(
                            t.hs->begin(), t.hs->end(),
                            rp_token.hs->begin(), rp_token.hs->end(),
                            std::inserter(new_hs, new_hs.begin())
                        );
                    }
                    new_hs.insert(t.text);
                    auto replaced = subst(fm->body(), fm->args(), params, new_hs, env);

                    if (sum_num_of_lines > 0) {
                        replaced.push_back(Token::newline(sum_num_of_lines));
                    }

                    // Push replacement in reverse for re-scanning
                    for (auto rit = replaced.rbegin(); rit != replaced.rend(); ++rit) {
                        work.push_back(std::move(*rit));
                    }
                    continue;
                }

                for (auto rit = pre_lp.rbegin(); rit != pre_lp.rend(); ++rit)
                    work.push_back(std::move(*rit));
            }
            // Case: T is a "()-less macro" → expand(subst(ts(T), {}, {}, HS∪{T}, {}) • TS')
            else if (auto* om = dynamic_cast<ObjectMacro*>(macro)) {
                auto repl = om->replacement(env);
                Token::HideSet new_hs = t.hs ? *t.hs : Token::HideSet{};
                new_hs.insert(t.text);
                static const std::unordered_map<std::string, std::pair<int, bool>> no_params;
                static const std::vector<std::vector<Token>> no_actuals;
                repl = subst(repl, no_params, no_actuals, new_hs, env);

                // Push replacement in reverse for re-scanning
                for (auto rit = repl.rbegin(); rit != repl.rend(); ++rit) {
                    work.push_back(std::move(*rit));
                }
                continue;
            }
            }
        }

        // Prosser's fallthrough: T_HS • expand(TS')
        ots.push_back(t);
    }

    if (ctrl.size() != 1)
        ISSUE(UNTERMINATED_CONDITIONAL, "unclosed if");

    return ots;
}

// ── expand — file inclusion wrapper ──────────────────────────

std::vector<Token>& expand(const std::string& filepath,
                           Loader::LoadType load_type,
                           std::vector<Token>& ots,
                           Env& env,
                           const std::string& disppath)
{
    if (env.num_of_files() >= env.get_max_include_depth()) {
        ISSUE(MAX_INCLUDE_DEPTH_EXCEEDED, filepath);
        return ots;
    }

    std::string fullpath = Loader::fullpath(filepath, load_type, env);
    if (fullpath.empty()) {
        ISSUE(FILE_NOT_FOUND, filepath);
        return ots;
    }

    // Record dependency for -M / -MM output
    env.add_dependency(fullpath, disppath, load_type == Loader::LoadType::SINCLUDE);

    env.push_file(fullpath);
    Issue::push({1, disppath});

    ots.push_back(Token::line_pragma(0, disppath, env.is_standard_pragma_style()));
    ots.push_back(Token::newline());
    auto its = Loader::tokens(fullpath, env);
    expand(its, ots, env);

    Issue::pop();
    env.pop_file();
    return ots;
}
