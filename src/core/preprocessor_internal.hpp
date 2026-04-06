#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "../env/env.hpp"
#include "../loader/token.hpp"
#include "../env/issue.hpp"

namespace jiepp::preprocessor_detail {

void handle_define(const std::string& raw_arg, Env& env);
void handle_undef(const std::string& raw_arg, Env& env);
void handle_tokenize(const std::string& raw_arg, Env& env, std::vector<Token>& ots);
void handle_stringize(const std::string& raw_arg,
                      Env& env,
                      std::vector<Token>& ots,
                      bool wide);
void handle_setline(const std::string& raw_arg, Env& env, std::vector<Token>& ots);
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
void handle_pragma_style(const std::string& raw_arg, Env& env);

} // namespace jiepp::preprocessor_detail
