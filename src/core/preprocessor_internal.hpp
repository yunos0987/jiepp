#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "../env/env.hpp"
#include "../loader/token.hpp"
#include "../env/issue.hpp"

namespace jiepp::preprocessor_detail {

// Returns true iff a macro was actually (re)defined; false if the
// {#define} was rejected (e.g. a duplicate parameter name) and defined
// nothing. See handle_define()'s definition (directive_handlers.cpp) for
// details.
bool handle_define(const std::string& raw_arg, Env& env);
void handle_undef(const std::string& raw_arg, Env& env);
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
