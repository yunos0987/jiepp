// directive_token.cpp — DirectiveToken classification and kind lookup.

#include "token.hpp"
#include "directive_parser.hpp"
#include "../env/issue.hpp"

#include <cctype>
#include <string>
#include <unordered_map>

// ---------------------------------------------------------------------------
// DirectiveToken classification
// ---------------------------------------------------------------------------

// B1: shared with dispatch_directive() (expand.cpp), which is the sole place
// that reports UNKNOWN_DIRECTIVE (PP45) / INVALID_DIRECTIVE_NAME (PP46) for
// a key DirectiveToken::name_to_kind() couldn't resolve.
Issue::Code classify_unknown_directive(std::string_view key) {
    // If key starts with an identifier char but is NOT a pure identifier
    // (e.g. "if;" with trailing semicolon), it looks like a typo → ERROR
    // (INVALID_DIRECTIVE_NAME). Pure identifiers (e.g. "pragma") and
    // non-identifier keys → ERROR too (UNKNOWN_DIRECTIVE).
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
    if (starts_like_ident && !is_pure_ident)
        return Issue::Code::INVALID_DIRECTIVE_NAME;
    return Issue::Code::UNKNOWN_DIRECTIVE;
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
