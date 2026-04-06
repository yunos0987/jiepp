// expand_ctrl.cpp — Conditional compilation control flow.
// Not part of Prosser's algorithm; jiepp extension for #if/#elif/#else/#endif.

#include "expand_helpers.hpp"
#include "preprocessor.hpp"

#include "../constfold/constfold.hpp"
#include "../loader/loader.hpp"
#include "../macro/macro.hpp"

#include <cctype>
#include <string>
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

// Replace all __has_include("path") and __has_include(<path>) in raw_cond
// with "1" or "0" based on whether the file is found.
// This runs before macro expansion so arguments are NOT expanded.
std::string resolve_has_include(const std::string& raw_cond, Env& env) {
    static constexpr std::string_view KW = "__has_include";
    std::string result;
    result.reserve(raw_cond.size());
    std::size_t pos = 0;
    while (pos < raw_cond.size()) {
        std::size_t found = raw_cond.find(KW, pos);
        if (found == std::string::npos) {
            result.append(raw_cond, pos);
            break;
        }
        result.append(raw_cond, pos, found - pos);

        std::size_t i = found + KW.size();
        // skip whitespace
        while (i < raw_cond.size() && std::isspace(static_cast<unsigned char>(raw_cond[i]))) ++i;
        // expect '('
        if (i >= raw_cond.size() || raw_cond[i] != '(') {
            result.append(KW);
            pos = found + KW.size();
            continue;
        }
        ++i; // skip '('
        // skip whitespace
        while (i < raw_cond.size() && std::isspace(static_cast<unsigned char>(raw_cond[i]))) ++i;

        // determine quote style and extract path
        std::string path;
        Loader::LoadType load_type;
        bool valid = false;

        if (i < raw_cond.size() && raw_cond[i] == '"') {
            // "path" form
            ++i;
            std::size_t end = raw_cond.find('"', i);
            if (end != std::string::npos) {
                path = raw_cond.substr(i, end - i);
                load_type = Loader::LoadType::INCLUDE;
                i = end + 1;
                valid = true;
            }
        } else if (i < raw_cond.size() && raw_cond[i] == '<') {
            // <path> form
            ++i;
            std::size_t end = raw_cond.find('>', i);
            if (end != std::string::npos) {
                path = raw_cond.substr(i, end - i);
                load_type = Loader::LoadType::SINCLUDE;
                i = end + 1;
                valid = true;
            }
        } else if (i < raw_cond.size() && raw_cond[i] == '\'') {
            // 'path' form (IEC string style)
            ++i;
            std::size_t end = raw_cond.find('\'', i);
            if (end != std::string::npos) {
                path = raw_cond.substr(i, end - i);
                load_type = Loader::LoadType::INCLUDE;
                i = end + 1;
                valid = true;
            }
        }

        if (valid) {
            // skip whitespace after path
            while (i < raw_cond.size() && std::isspace(static_cast<unsigned char>(raw_cond[i]))) ++i;
            // expect ')'
            if (i < raw_cond.size() && raw_cond[i] == ')') {
                ++i;
                bool exists = !Loader::fullpath(path, load_type, env).empty();
                result.append(exists ? "1" : "0");
                pos = i;
                continue;
            }
        }

        // malformed — pass through as-is
        result.append(KW);
        pos = found + KW.size();
    }
    return result;
}

} // namespace

std::string eval_cond_str(const std::string& raw_cond, Env& env) {
#ifdef JIEPP_SANDBOX
    // In sandbox mode, __has_include is not allowed (filesystem probe)
    static constexpr std::string_view KW_HI = "__has_include";
    if (raw_cond.find(KW_HI) != std::string::npos)
        ISSUE(SANDBOX_RESTRICTED_DIRECTIVE, "__has_include");
    std::string cond = raw_cond;
#else
    std::string cond = resolve_has_include(raw_cond, env);
#endif
    bool had_defined = env.exist("defined");
    if (!had_defined)
        env.define("defined", std::make_unique<DefinedOperator>());
    std::string result = preprocess_text(cond, env);
    if (!had_defined)
        env.undef("defined");
    return result;
}

bool eval_cond(const std::string& raw_cond, Env& env) {
    std::string expanded = eval_cond_str(raw_cond, env);
    int64_t val = eval_const_expr(expanded);
    return val != 0;
}

} // namespace jiepp::expand_detail
