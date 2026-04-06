#pragma once
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

    using HideSet    = std::set<std::string>;
    using HideSetPtr = std::shared_ptr<const HideSet>;

    int         type   = ANY;
    std::string text;
    int         num_of_lines = 0; // number of newlines this token contributes
    HideSetPtr  hs;          // hide-set (shared, copy-on-write)

    // Factories
    static Token create(int type, std::string text, int num_of_lines = 0);
    static Token newline(int num_of_lines = 1);
    static Token pragma(std::string_view key,
                        std::optional<std::string_view> value = std::nullopt,
                        bool standard = false,
                        int num_of_lines = 0);
    static Token line_pragma(int lineno,
                             std::optional<std::string_view> filepath = std::nullopt,
                             bool standard = false,
                             int num_of_lines = 0);

    Token clone() const;

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

#define JIEPP_DIRECTIVE_TOKEN(name, val) static constexpr int name = val;
#include "directive_token.def"
#undef JIEPP_DIRECTIVE_TOKEN

    int         directive_kind = NOP;
    std::string arg;

    static DirectiveToken create_empty(std::string text);
    // Parse text (e.g. "{#define N 1}") to set directive_kind and arg.
    void ready();

    static int         name_to_kind(std::string_view name);
    static std::string kind_to_name(int kind);
};

// Token list helpers
std::vector<Token> ts_trim(std::vector<Token> ts);
std::vector<Token> ts_ltrim(std::vector<Token> ts);
std::vector<Token> ts_rtrim(std::vector<Token> ts);
std::vector<Token> ts_flatten(std::vector<Token> ts);

// Hide-set helper functions (defined in token.cpp)
Token::HideSetPtr hs_empty();
Token::HideSetPtr hs_with(const Token::HideSetPtr& base, const std::string& name);
Token::HideSetPtr hs_add_all(const Token::HideSetPtr& base, const Token::HideSet& names);
Token::HideSetPtr hs_intersect(const Token::HideSetPtr& a, const Token::HideSetPtr& b);
