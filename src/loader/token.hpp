#pragma once
#include "../env/lineno.hpp"
#include "hideset.hpp"

#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include <optional>

struct Token {
    static constexpr int MASK_WS    = 0b1000'0000;

#define JIEPP_TOKEN(name, val) static constexpr int name = val;
#include "token.def"
#undef JIEPP_TOKEN

    // Prosser hide set (task_slug hideset-linear): a value handle to an
    // immutable, structurally shared trie -- see hideset.hpp. Kept as an
    // alias so hsadd()/subst()'s `const Token::HideSet&` signatures stay
    // unchanged.
    using HideSet = ::HideSet;

    int         type   = ANY;
    // True only for an R6 whitespace token that select_arg() inserted around a
    // joining comma of the variable arguments, for the shared memoised
    // expansion of a multi-use variadic parameter (task_slug arg-expand-once,
    // expand_subst.cpp D3). Never leaves subst(): consumers either keep it
    // (stringize context) or skip it (plain use). Placed here, directly after
    // `type`, to reuse the padding before `text` so sizeof(Token) is
    // unchanged; excluded from operator== like the other bookkeeping flags.
    bool        va_sep = false;
    std::string text;
    int         num_of_lines = 0; // number of newlines this token contributes
    // True once this token's num_of_lines has already been applied to Env's line
    // counter (see expand.cpp's function-macro argument collection). Bookkeeping
    // only: deliberately excluded from operator== so existing token-equality
    // callers (macro redefinition checks, tests) are unaffected.
    bool        lineno_counted = false;
    // True if this token's num_of_lines are printed but are not source
    // lines: the token is part of a macro replacement or of a re-lexed
    // directive operand / pragma body, so its newlines come from a decoded
    // $n/$l/$r/$0A/$0D escape (or, for a function-macro argument, were
    // already re-emitted as a separate newline token by the call; see
    // expand.cpp). Such newlines never advance the Env line counter (like C,
    // where a macro expansion never changes __LINE__) and are not counted
    // as source lines by compact_blank_lines(). Excluded from operator==
    // like lineno_counted, so macro redefinition checks are unaffected.
    bool        output_only_lines = false;
    // True for a STRING/WSTRING token whose literal the lexer ended at a
    // newline or at the end of the input, not at its closing quote.
    // push_string_token() never merges the next literal into such a token
    // (it has no closing quote to drop). Excluded from operator== like
    // lineno_counted. core reports it as PP29 and clears the mark.
    bool        unterminated = false;
    // True for a newline token re-emitted after a function-macro replacement
    // for line-count fidelity (see expand.cpp's function-macro argument
    // collection): it is not whitespace that ever separated two tokens of
    // the replacement, so stringizing (R2) skips it as a separator
    // candidate. Excluded from operator== like lineno_counted.
    bool        line_filler = false;
    HideSet     hs;          // hide-set (persistent trie handle, O(1) copy)

    // Factories
    static Token create(int type, std::string text, int num_of_lines = 0);
    static Token newline(int num_of_lines = 1);
    static Token pragma(std::string_view key,
                        std::optional<std::string_view> value = std::nullopt,
                        bool standard = false,
                        int num_of_lines = 0);
    static Token line_pragma(LineNo lineno,
                             std::optional<std::string_view> filepath = std::nullopt,
                             bool standard = false,
                             int num_of_lines = 0);

    Token clone() const;

    // Sets output_only_lines, and lineno_counted so expand() never applies
    // this token's num_of_lines to the Env line counter.
    void mark_output_only() noexcept {
        output_only_lines = true;
        lineno_counted = true;
    }

    // Convert newlines inside whitespace/any tokens to spaces (for macro arg flattening)
    Token& flatten();

    bool operator==(const Token& o) const noexcept {
        return type == o.type && text == o.text && num_of_lines == o.num_of_lines;
    }
};

struct DirectiveToken : Token {
    static constexpr int MASK_CTRL      = 0b1000'0000'0000'0000;
    static constexpr int MASK_CTRLEX    = 0b0100'0000'0000'0000;
    static constexpr int MASK_INCLUDE   = 0b0010'0000'0000'0000;
    static constexpr int MASK_MESSAGE   = 0b0001'0000'0000'0000;
    static constexpr int MASK_STRINGIZE = 0b0000'1000'0000'0000;
    // Directives whose handler pushes token(s) directly to the output stream
    // (`ots`) rather than only mutating Env's state. Such a directive found
    // while collecting a macro call's argument list cannot be executed
    // safely there: its output would be emitted before the enclosing macro
    // call's own expansion, out of order (see expand.cpp's macro-argument
    // collection loop, F3).
    static constexpr int MASK_OUTPUT    = 0b0000'0100'0000'0000;

#define JIEPP_DIRECTIVE_TOKEN(name, val) static constexpr int name = val;
#include "directive_token.def"
#undef JIEPP_DIRECTIVE_TOKEN

    static int         name_to_kind(std::string_view name);
    static std::string kind_to_name(int kind);
};

// Token list helpers
std::vector<Token> ts_trim(std::vector<Token> ts);
std::vector<Token> ts_ltrim(std::vector<Token> ts);
std::vector<Token> ts_rtrim(std::vector<Token> ts);
std::vector<Token> ts_flatten(std::vector<Token> ts);
// Token::mark_output_only() on every token of ts.
void ts_mark_output_only(std::vector<Token>& ts) noexcept;
