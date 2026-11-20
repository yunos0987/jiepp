#pragma once
#include <iosfwd>
#include <optional>
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

// -D NAME[=VALUE] as setup() processes one (`name_value` is define_macro_option()'s
// result): {#define NAME VALUE}, so an invalid name or parameter list is the
// same diagnostic as in a {#define}. Where Issue's continue mode is active
// (the CLI) a rejected definition is reported and counted, and processing
// goes on; elsewhere the ERROR throws.
void apply_define_option(const std::pair<std::string, std::string>& name_value, Env& env);

// -U NAME: same as {#undef NAME} (see handle_undef()'s comment in
// directive_handlers.cpp for the name validation and 'defined' rejection).
void apply_undef_option(const std::string& name, Env& env);

std::vector<Token>& expand(const std::vector<Token>& its,
                           std::vector<Token>& ots,
                           Env& env);

// Includes a file: resolves filepath (Loader::fullpath()), then expands its
// tokens into ots. The file is named, in its line marker, __FILE__,
// diagnostics and the dependency output, by the path it was reached by
// (Util::reached_path()); shown_as names it differently (the CLI's
// --disppath for the main input). A file found under an -isystem directory
// (Env::add_system_path()) or included from such a file is a system header,
// left out of -MM/-MMD; main_input (the CLI's main input file) never is one.
// origin says where the file is included from, for the "In file included
// from" lines before a diagnostic in it (SPEC 16): an {#include}/
// {#sinclude} (directive, with the line of the path) or -include
// (command_line). The default, none, is a top-level file -- the main input,
// or a file a library caller reads directly -- which gets no such line; the
// files it includes are shown as included from it ("In file included from
// top.iec:2:"). A file that the string input of preprocess()/
// preprocess_text() includes is shown as included from that input, by the
// name its own diagnostics have ("In file included from <unknown
// location>:2:").
std::vector<Token>& expand(const std::string& filepath,
                           Loader::LoadType load_type,
                           std::vector<Token>& ots,
                           Env& env,
                           const std::optional<std::string>& shown_as = std::nullopt,
                           bool main_input = false,
                           const Issue::IncludeOrigin& origin = {});

void preprocess(std::istream& input, std::ostream& output, Env& env);

std::string preprocess_text(const std::string& input, Env& env);

void dump_macros(Env& env, std::ostream& output);
