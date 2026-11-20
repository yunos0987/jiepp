// directive_token.cpp — DirectiveToken classification and kind lookup.

#include "token.hpp"
#include "directive_parser.hpp"
#include "lexer.hpp"
#include "lexer_helpers.hpp"
#include "../env/issue.hpp"

#include <string>
#include <string_view>
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
    const bool starts_like_ident = !key.empty() && jiepp::detail::is_ident_start(key[0]);
    if (starts_like_ident && !iec3_is_identifier(key))
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
