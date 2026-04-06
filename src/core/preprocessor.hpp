#pragma once
#include <iosfwd>
#include <string>
#include <vector>
#include "../loader/token.hpp"
#include "../env/env.hpp"
#include "../macro/macro.hpp"
#include "../loader/loader.hpp"

Env setup(const std::vector<std::pair<std::string, std::string>>& predefine_macros = {});

std::vector<Token>& expand(const std::vector<Token>& its,
                           std::vector<Token>& ots,
                           Env& env);

std::vector<Token>& expand(const std::string& filepath,
                           Loader::LoadType load_type,
                           std::vector<Token>& ots,
                           Env& env,
                           const std::string& disppath);

void preprocess(std::istream& input, std::ostream& output, Env& env);
void preprocess(std::istream& input, const std::vector<std::string>& include_filepaths, std::ostream& output, Env& env);
void preprocess(const std::string& input_filepath, const std::vector<std::string>& include_filepaths, std::ostream& output, Env& env);

std::string preprocess_text(const std::string& input, Env& env);

void dump_macros(Env& env, std::ostream& output);
