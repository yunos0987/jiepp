#include "preprocessor.hpp"
#include "preprocessor_internal.hpp"

#include "../loader/lexer.hpp"
#include "../loader/directive_parser.hpp" //kludge
#include "../macro/macro.hpp"
#include "../util/text.hpp"
#include "../util/iec_61131-3.hpp"

#include <cctype>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
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

} // namespace

namespace jiepp::preprocessor_detail {

void handle_define(const std::string& raw_arg, Env& env) {
    if (raw_arg == "defined") {
        ISSUE(OPERATION_NOT_ALLOWED, "redefine 'defined'");
        return;
    }

    auto ts = iec3_tokens_from_string(raw_arg, false);
    ts = ts_ltrim(std::move(ts));
    if (ts.empty() || ts[0].type != Token::ANY) {
        ISSUE(INVALID_DEFINE_SYNTAX, raw_arg);
        return;
    }

    std::string name = ts[0].text;
    std::size_t i = 1;
    bool is_function = (i < ts.size() && ts[i].type == Token::LP);
    std::vector<std::string> param_names;

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
                    param_names.push_back(FunctionMacro::VA_SYM);
                    ++i;
                    while (i < ts.size() && (ts[i].type & Token::MASK_WS)) ++i;
                    if (i < ts.size() && ts[i].type != Token::RP) {
                        ISSUE(INVALID_DEFINE_SYNTAX, raw_arg);
                        return;
                    }
                    break;
                }
                if (ts[i].text == "." && i + 2 < ts.size() && ts[i + 1].text == "." &&
                    ts[i + 2].text == ".") {
                    param_names.push_back(FunctionMacro::VA_SYM);
                    i += 3;
                    while (i < ts.size() && (ts[i].type & Token::MASK_WS)) ++i;
                    if (i < ts.size() && ts[i].type != Token::RP) {
                        ISSUE(INVALID_DEFINE_SYNTAX, raw_arg);
                        return;
                    }
                    break;
                }
                param_names.push_back(ts[i].text);
                // Check for duplicate parameter names
                for (std::size_t k = 0; k + 1 < param_names.size(); ++k)
                    if (param_names[k] == param_names.back())
                        ISSUE(DUPLICATE_MACRO_PARAMETER, ts[i].text);
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

void handle_setline(const std::string& raw_arg, Env& env, std::vector<Token>& ots) {
    std::string arg = preprocess_text(raw_arg, env);
    std::istringstream arg_ss(arg);
    int new_lineno;
    if (arg_ss >> new_lineno) {
        std::string raw_fp;
        arg_ss >> raw_fp;
        std::string new_fp = std::string(Util::trim_view(raw_fp));
        bool syspath_only = false;
        if (new_fp.empty() || strip_path(new_fp, new_fp, syspath_only)) {
            env.set_lineno(new_lineno);
#ifdef JIEPP_SANDBOX
            // Sandbox: ignore filepath argument, keep original filepath
            {
                std::string old_fp = Issue::filepath();
                Issue::pop();
                Issue::push({new_lineno, old_fp});
                ots.push_back(Token::line_pragma(new_lineno, std::nullopt, env.is_standard_pragma_style()));
                return;
            }
#else
            if (new_fp.empty()) {
                std::string old_fp = Issue::filepath();
                Issue::pop();
                Issue::push({new_lineno, old_fp});
                ots.push_back(Token::line_pragma(new_lineno, std::nullopt, env.is_standard_pragma_style()));
                return;
            } else {
                Issue::pop();
                Issue::push({new_lineno, std::string(new_fp)});
                ots.push_back(Token::line_pragma(new_lineno, new_fp, env.is_standard_pragma_style()));
                return;
            }
#endif
        }
    }
    ISSUE(INVALID_SETLINE_OPERAND, raw_arg);
}

void handle_syspath(const std::string& raw_arg, Env& env, std::vector<Token>& ots) {
    std::string raw_path = preprocess_text(raw_arg, env);
    std::string syspath;
    bool _;
    if (strip_path(raw_path, syspath, _)) {
        env.add_syspath(syspath);
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
    auto s = Util::trim_view(raw_arg);
    // Accept "PPnn" format (e.g., PP41)
    if ((s.size() >= 3) && s.starts_with("PP")) {
        try {
            unsigned int val = static_cast<unsigned int>(std::stoul(std::string(s.substr(2))));
            Issue::add_ignoring(Issue::Code(val));
            return;
        } catch (...) {}
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

void handle_pragma_style(const std::string& raw_arg, Env& env) {
    env.set_pragma_style(std::string(Util::trim_view(raw_arg)));
}

} // namespace jiepp::preprocessor_detail
