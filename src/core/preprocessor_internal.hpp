#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "../env/env.hpp"
#include "../loader/token.hpp"
#include "../env/issue.hpp"
#include "../macro/macro.hpp"

namespace jiepp::preprocessor_detail {

// Returns true iff a macro was actually (re)defined; false if the
// {#define} was rejected (e.g. a duplicate parameter name) and defined
// nothing. See handle_define()'s definition (directive_handlers.cpp) for
// details. On success (only), *defined_name (when non-null) is set to the
// macro's name, so the caller can look the just-defined macro back up
// (e.g. for a -dD echo under -nC, see define_directive_text() below).
bool handle_define(const std::string& raw_arg, Env& env, std::string* defined_name = nullptr);
// Returns true iff the {#undef} operand was accepted (a valid name, even
// if it was never defined); see handle_undef()'s definition
// (directive_handlers.cpp) for the rejection cases (C1). On success (only),
// *undefined_name (when non-null) is set to the name.
bool handle_undef(const std::string& raw_arg, Env& env, std::string* undefined_name = nullptr);

// Renders "{#define NAME body}" / "{#define NAME(params) body}" the way
// -dM does, from the macro's current, already-tokenized definition rather
// than from the directive's original source text: used for the -dD echo of
// a {#define} under -nC, where the source text may still carry a comment
// the definition itself dropped (U5). Returns "" for a macro that is not a
// UserDefinedObjectMacro/FunctionMacro (a builtin like __LINE__), which
// dump_macros() (preprocessor.cpp) never echoes either.
std::string define_directive_text(const std::string& name, const Macro& macro);
void handle_tokenize(const std::string& raw_arg, Env& env, std::vector<Token>& ots);
void handle_stringize(const std::string& raw_arg,
                      Env& env,
                      std::vector<Token>& ots,
                      bool wide);
// key: the directive name as written -- "" for the nameless marker directive
// ({#:N}), "line"/"set_line"/"set-line" for the named gcc-style directive.
// See handle_setline()'s definition (directive_handlers.cpp) for how the two
// forms differ in the effective line number (unit C, U1(a)); the name is
// also the context of a PP49 for tokens after the operand.
void handle_setline(const std::string& raw_arg, const std::string& key, Env& env, std::vector<Token>& ots);
void handle_syspath(const std::string& raw_arg, Env& env, std::vector<Token>& ots);
void handle_include(const std::string& raw_arg,
                    Env& env,
                    std::vector<Token>& ots,
                    bool syspath_only);
void handle_message(const std::string& raw_arg, Env& env, Issue::Code code);
// Raises a message-passthrough diagnostic (PP90-PP93) with msg as its
// context. Shared by handle_message() and the built-ins __error/__assert.
// The sandbox control-character strip is applied by Issue::happen(), as for
// every other diagnostic.
void report_message(std::string msg, Issue::Code code);
void handle_ignore(const std::string& raw_arg, Env& env);
void handle_max_include_depth(const std::string& raw_arg, Env& env);
void handle_max_expansion_depth(const std::string& raw_arg, Env& env);
void handle_max_if_nesting(const std::string& raw_arg, Env& env);
void handle_max_blank_lines(const std::string& raw_arg, Env& env);
void handle_pragma_style(const std::string& raw_arg, Env& env);
// {#pragma once}, {#pragma push_macro('NAME')} and {#pragma pop_macro('NAME')}
// (SPEC 7.6, 3.8); other names are ignored. Returns true iff a pop_macro
// changed NAME's definition, and then stores NAME in *restored_name (for the
// -dD echo).
bool handle_pragma(const std::string& raw_arg, Env& env, std::string* restored_name);

// PP29 (UNTERMINATED_STRING_LITERAL) at the current Issue location if the
// lexer marked t (Token::unterminated); clears the mark first, so neither
// t nor a later copy (macro body, argument) is reported again.
void report_unterminated_literal(Token& t);

// Lexes a decoded directive operand and reports PP29 for each unterminated
// literal in it; with report_unterminated == false (message text, like
// clang's #error/#warning) only clears the marks. Lexes with iec3_relex(),
// so its own diagnostics (PP20/PP22/PP23) are also reported at the current
// Issue location; report_nested_brace is passed to it.
// found_unterminated, if not null, is set to true when the text has an
// unterminated literal or an unclosed comment (whether or not it was
// reported); never set to false. report_unclosed_comment == false lexes an
// unclosed comment without PP20 (the rescan of an already reported pragma).
// found_directive, if not null, is set to true when a DIRECTIVE token was
// dropped (and PP23 raised for it); never set to false.
std::vector<Token> lex_operand(const std::string& text, bool remove_comments,
                               bool report_unterminated = true,
                               bool report_nested_brace = true,
                               bool* found_unterminated = nullptr,
                               bool report_unclosed_comment = true,
                               bool* found_directive = nullptr);

// ---- Path operands (SPEC §7, §7.5, §9): {#include}, {#sinclude},
// {#syspath}, {#line}'s file name and __has_include's operand. Judged on
// tokens, as gcc/clang do; see ARCHITECTURE.md. ----

// Whether t is a '...' or "..." literal closed by its own quote. Reads the
// one token's text (string_literal_end()): an unterminated literal (`'a$'`)
// is not a path.
bool is_terminated_literal(const Token& t);

// Index of the first token at or after i that is not whitespace or a
// comment (Token::MASK_WS), or ts.size().
std::size_t skip_blanks(const std::vector<Token>& ts, std::size_t i);

// Reads a header name written directly: the '<' has just been taken from
// `work`, a reversed token stack (back() is the next token). Like gcc/clang,
// which read a header name from the source characters, the name is the
// spellings of the next tokens up to the first '>' character, so a comment
// opener or a quote inside <...> is part of the name and nothing in it is
// macro-expanded. The lexer may have read that '>' inside a longer token (a
// comment, a literal): the token is cut after the '>' and the rest of its
// text is lexed again (without diagnostics: the source lex reported them)
// and pushed back onto `work`. The tokens read move to `read` in order, the
// cut token holding its text up to the '>'. Returns false, with every token
// moved to `read`, when no token has a '>'. Each token is read once.
bool read_header_name(std::vector<Token>& work, std::string& name, std::vector<Token>& read);

// The path at the start of macro-expanded tokens, as gcc/clang read the
// result of expanding an #include operand: after blanks, one terminated
// string literal (`text` is the literal as written, escapes not decoded), or
// a '<' token and the tokens up to the next '>' token (`text` is their
// spellings joined, a comment as one blank). `end` is the index just past
// the path, for the caller's check of the tokens after it.
struct ExpandedPath {
    enum class Kind { none, quoted, angle };
    Kind kind = Kind::none;
    std::string text;
    std::size_t end = 0;
};
ExpandedPath expanded_path(const std::vector<Token>& ts);

// What expand_operand_tokens() lexes (A2 / S-02):
// - operand: a directive operand or message text.
// - pragma_body: the body of a pragma token that the lexer did not produce
//   while re-lexing a pragma body (a source pragma, a pragma from a macro
//   replacement, or one whose text was rewritten).
// - nested_pragma_body: the body of a Token::relexed_pragma token; a raw
//   '{' inside a nested pragma body is not reported again (see
//   read_pragma_body()).
// For both pragma kinds every PRAGMA token lexed from the body is marked
// Token::relexed_pragma.
enum class OperandKind { operand, pragma_body, nested_pragma_body };

// Lexes text -- a decoded directive operand ({#if}/{#elif} condition,
// {#string}/{#wstring}/{#line}/{#include}/{#sinclude}/{#syspath} operand,
// message text) or an ordinary pragma body -- and macro-expands it with
// env. Unlike preprocess()/preprocess_text(), no blank-line compaction
// runs: compaction is a post-pass over the final output stream, and running
// it here spliced a line marker into the operand text itself.
std::vector<Token> expand_operand_tokens(const std::string& text, Env& env,
                                         bool report_unterminated = true,
                                         OperandKind kind = OperandKind::operand,
                                         bool* found_unterminated = nullptr,
                                         bool report_unclosed_comment = true);
// Concatenated text of expand_operand_tokens().
std::string expand_operand_text(const std::string& text, Env& env,
                                bool report_unterminated = true);

} // namespace jiepp::preprocessor_detail
