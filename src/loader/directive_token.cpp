// directive_token.cpp — DirectiveToken factories and classification.

#include "token.hpp"
#include "directive_parser.hpp"
#include "../env/issue.hpp"

#include <cctype>
#include <string>
#include <unordered_map>

// ---------------------------------------------------------------------------
// DirectiveToken factories
// ---------------------------------------------------------------------------

DirectiveToken DirectiveToken::create_empty(std::string text) {
    DirectiveToken dt;
    dt.type           = Token::DIRECTIVE;
    dt.text           = std::move(text);
    dt.num_of_lines   = 0;
    dt.directive_kind = DirectiveToken::NOP;
    dt.arg            = "";
    return dt;
}

void DirectiveToken::ready() {
    auto [key, value] = parse_directive(text);

    int kind = name_to_kind(key);
    if (kind == -1) {
        // If key starts with an identifier char but is NOT a pure identifier
        // (e.g. "if;" with trailing semicolon), it looks like a typo → ERROR.
        // Pure identifiers (e.g. "pragma") and non-identifier keys → WARNING only.
        auto is_ident_start = [](unsigned char c) { return std::isalpha(c) || c == '_'; };
        auto is_ident_cont  = [](unsigned char c) { return std::isalnum(c) || c == '_'; };
        bool starts_like_ident = !key.empty() && is_ident_start(static_cast<unsigned char>(key[0]));
        bool is_pure_ident = starts_like_ident;
        if (is_pure_ident) {
            for (size_t i = 1; i < key.size(); ++i) {
                if (!is_ident_cont(static_cast<unsigned char>(key[i]))) {
                    is_pure_ident = false;
                    break;
                }
            }
        }
        if (starts_like_ident && !is_pure_ident) {
            ISSUE(INVALID_DIRECTIVE_NAME, key);
        } else {
            ISSUE(UNKNOWN_DIRECTIVE, key);
        }
        directive_kind = DirectiveToken::NOP;
        arg            = "";
        return;
    }

    if (kind & DirectiveToken::MASK_CTRLEX) {
        directive_kind = DirectiveToken::IF;
        if (kind == DirectiveToken::IFDEF) {
            arg = "defined(" + value + ")";
        } else { // IFNDEF
            arg = "\\not\\ defined(" + value + ")";
        }
    } else {
        directive_kind = kind;
        arg            = value;
    }
}

int DirectiveToken::name_to_kind(std::string_view name) {
    static const std::unordered_map<std::string, int> table = {
#define JIEPP_DIRECTIVE(kind, keyword) {keyword, DirectiveToken::kind},
#include "directive_table.def"
#undef JIEPP_DIRECTIVE
    };

    auto it = table.find(std::string(name));
    if (it == table.end())
        return -1;
    return it->second;
}

std::string DirectiveToken::kind_to_name(int kind) {
    // Build from .def: last entry per kind wins (later entries overwrite)
    static const std::unordered_map<int, std::string> table = []() {
        std::unordered_map<int, std::string> m;
#define JIEPP_DIRECTIVE(k, kw) m[DirectiveToken::k] = kw;
#include "directive_table.def"
#undef JIEPP_DIRECTIVE
        return m;
    }();

    auto it = table.find(kind);
    if (it == table.end())
        return "";
    return it->second;
}
