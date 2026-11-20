#pragma once
#include <iosfwd>
#include <string>
#include <vector>
#include "../loader/token.hpp"
#include "../env/env.hpp"
#include "../macro/macro.hpp"
#include "../loader/loader.hpp"

// B1: the public library API (setup() + expand()/preprocess()/preprocess_text()
// below) is self-sufficient and does not require the caller to separately call
// Issue::initialize() first -- setup() seeds Issue::loc_stack_'s bottom entry
// itself if it is still empty. Issue::initialize() remains CLI-only (main()
// calls it before parse_args()/jiepp_command() to also reset -Werror/silent/
// ignorings_/blockings_/etc. to their defaults and redirect diagnostic output);
// a library caller that wants that full reset -- e.g. between independent runs
// in the same process -- can still call Issue::initialize() explicitly.
Env setup(const std::vector<std::pair<std::string, std::string>>& predefine_macros = {},
          bool remove_comments = false);

// -U NAME: same as {#undef NAME} (see handle_undef()'s comment in
// directive_handlers.cpp for the name validation and 'defined' rejection).
void apply_undef_option(const std::string& name, Env& env);

std::vector<Token>& expand(const std::vector<Token>& its,
                           std::vector<Token>& ots,
                           Env& env);

// Includes a file: resolves filepath (Loader::fullpath()), then expands its
// tokens into ots. disppath is the name shown in the line marker and in
// diagnostics. The dependency output lists the file by the path it was
// reached by; disppath_names_dependency makes it list disppath instead
// (the CLI's --disppath for the main input).
std::vector<Token>& expand(const std::string& filepath,
                           Loader::LoadType load_type,
                           std::vector<Token>& ots,
                           Env& env,
                           const std::string& disppath,
                           bool disppath_names_dependency = false);

void preprocess(std::istream& input, std::ostream& output, Env& env);

std::string preprocess_text(const std::string& input, Env& env);

void dump_macros(Env& env, std::ostream& output);
