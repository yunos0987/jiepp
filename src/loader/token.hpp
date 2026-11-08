#pragma once
#include "../env/lineno.hpp"
#include "hideset.hpp"

#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <algorithm>
#include <cstddef>
#include <utility>

// Where the source lines of a directive or pragma body break, for the
// diagnostics of its re-lex (task d1). The lexer folds the raw newlines of a
// body into spaces and drops a "$"+newline, so a body's text no longer
// shows its lines; this map keeps them. Positions are offsets into the text
// the map belongs to (a body: the text after its opening '{'). A
// diagnostic at offset `pos` is on line_of(pos) lines below the line the
// text is reported from (the current Issue location while the text is
// re-lexed). Diagnostic-only: never changes the output.
struct LineMap {
    // Lines before the text's first character: a token cut from a longer
    // re-lexed text starts that many lines below the longer text's start.
    std::size_t lines_before = 0;
    // Ascending: a new line starts at the character at each offset (a
    // folded newline's space, if any, is on the new line).
    std::vector<std::size_t> breaks;

    // Binary search, so a text with many lines and many diagnostics is not
    // quadratic.
    std::size_t line_of(std::size_t pos) const {
        return lines_before +
               static_cast<std::size_t>(std::upper_bound(breaks.begin(), breaks.end(), pos) -
                                        breaks.begin());
    }
};

// A shared, immutable LineMap behind a pointer-sized handle with a
// non-atomic reference count (like HideSet: tokens are not shared between
// threads). Pointer-sized rather than std::shared_ptr so Token grows by 8
// bytes, not 16: a Debug build keeps many Token temporaries in each frame
// of a nested expansion, and the nesting the PP63 stack guard allows
// depends on the frame sizes.
class LineMapRef {
public:
    LineMapRef() noexcept = default;
    explicit LineMapRef(LineMap m) : p_(new Node{std::move(m), 1}) {}
    LineMapRef(const LineMapRef& o) noexcept : p_(o.p_) {
        if (p_)
            ++p_->refs;
    }
    LineMapRef(LineMapRef&& o) noexcept : p_(o.p_) { o.p_ = nullptr; }
    LineMapRef& operator=(LineMapRef o) noexcept {
        std::swap(p_, o.p_);
        return *this;
    }
    ~LineMapRef() { release(); }

    const LineMap* get() const noexcept { return p_ ? &p_->map : nullptr; }
    const LineMap* operator->() const noexcept { return &p_->map; }
    explicit operator bool() const noexcept { return p_ != nullptr; }
    void reset() noexcept {
        release();
        p_ = nullptr;
    }

private:
    struct Node {
        LineMap map;
        std::size_t refs;
    };
    void release() noexcept {
        if (p_ && --p_->refs == 0)
            delete p_;
    }
    Node* p_ = nullptr;
};

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
    // True only for a whitespace token that select_arg() inserted around a
    // joining comma of the variable arguments and that only the stringize
    // consumer keeps (a newline-only edge, R6), for the shared memoised
    // expansion of a multi-use variadic parameter (task_slug arg-expand-once,
    // expand_subst.cpp D3). Never leaves subst(): consumers either keep it
    // (stringize context) or skip it (plain use). Placed here, directly after
    // `type`, to reuse the padding before `text` so sizeof(Token) is
    // unchanged; excluded from operator== like the other bookkeeping flags.
    bool        va_sep = false;
    // True only for a PRAGMA token that the lexer produced while re-lexing
    // a pragma body (expand_operand_tokens() with a pragma OperandKind),
    // with its text exactly as lexed. The body of such a token is re-lexed
    // without reporting a raw '{' inside a nested pragma body (PP22): the
    // enclosing pass already reported that same character (task_slug
    // security-impl, A2 / S-02; see read_pragma_body()). Cleared wherever
    // the text is rewritten (expand_pragma_token(), glue_tokens()). Also
    // reuses the padding before `text`; excluded from operator== like the
    // other bookkeeping flags.
    bool        relexed_pragma = false;
    // True for a PRAGMA token whose rewritten text (expand_pragma_token())
    // came from a pass that already reported PP29 for an unterminated literal
    // or PP20 for an unclosed comment in its body: a later rescan of that
    // text (macro argument used in the body) lexes the same literal or
    // comment again and must not report it again.
    // Cleared wherever the text is rewritten again (glue_tokens()). Reuses
    // the padding before `text`; excluded from operator== like the other
    // bookkeeping flags.
    bool        pragma_unterminated_reported = false;
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
    // The source lines of a DIRECTIVE/PRAGMA token's body (LineMap), or of
    // the start of an unterminated literal or a dropped directive lexed
    // from a multi-line text; null when the text is on one line (most
    // tokens). Set by the lexer; read when the text is re-lexed
    // (preprocessor_detail::RelexScope) or the literal reported. Dropped
    // wherever the text is rewritten (expand_pragma_token(), glue_tokens())
    // and once a pragma body's PP20/PP29 are reported
    // (report_pragma_body_issues()). Excluded from operator== like the
    // other bookkeeping fields.
    LineMapRef  line_map;

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
