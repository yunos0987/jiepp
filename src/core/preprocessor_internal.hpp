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
// is_marker_form: true for the nameless marker directive ({#:N}, key == ""),
// false for the named gcc-style directive ({#line N} / {#set_line N} /
// {#set-line N}). See handle_setline()'s definition (directive_handlers.cpp)
// for how this changes the effective line number (unit C, U1(a)).
void handle_setline(const std::string& raw_arg, bool is_marker_form, Env& env, std::vector<Token>& ots);
void handle_syspath(const std::string& raw_arg, Env& env, std::vector<Token>& ots);
void handle_include(const std::string& raw_arg,
                    Env& env,
                    std::vector<Token>& ots,
                    bool syspath_only);
void handle_message(const std::string& raw_arg, Env& env, Issue::Code code);
void handle_ignore(const std::string& raw_arg, Env& env);
void handle_max_include_depth(const std::string& raw_arg, Env& env);
void handle_max_expansion_depth(const std::string& raw_arg, Env& env);
void handle_max_if_nesting(const std::string& raw_arg, Env& env);
void handle_max_blank_lines(const std::string& raw_arg, Env& env);
void handle_pragma_style(const std::string& raw_arg, Env& env);
void handle_pragma_once(const std::string& raw_arg, Env& env);

} // namespace jiepp::preprocessor_detail
