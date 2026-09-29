#include "preprocessor.hpp"
#include "../loader/directive_parser.hpp"
#include "../env/param_constants.hpp"
#include "line_compaction.hpp"
#include "preprocessor_internal.hpp"

#include "../loader/lexer.hpp"

#include <sstream>
#include <string>
#include <vector>

void preprocess(std::istream& input, std::ostream& output, Env& env) {
    auto its = iec3_tokens(input, env.get_remove_comments(), 1);
    std::vector<Token> ots;
    // No leading line marker here (unlike the jiepp command line), so tell
    // the compaction which line the stream starts on.
    const LineNo first_lineno = env.get_lineno();
    expand(its, ots, env);
    jiepp::compact_blank_lines(ots, env.get_max_blank_lines(),
                               jiepp::BlankLineMode::Markers, env.is_standard_pragma_style(),
                               first_lineno);
    for (auto& t : ots)
        output << t.text;
}

namespace jiepp::preprocessor_detail {

std::vector<Token> expand_operand_tokens(const std::string& text, Env& env) {
    auto its = iec3_tokens_from_string(text, env.get_remove_comments(), 1);
    std::vector<Token> ots;
    expand(its, ots, env);
    return ots;
}

std::string expand_operand_text(const std::string& text, Env& env) {
    std::string r;
    for (const auto& t : expand_operand_tokens(text, env))
        r += t.text;
    return r;
}

} // namespace jiepp::preprocessor_detail

std::string preprocess_text(const std::string& input, Env& env) {
    std::istringstream input_stream(input);
    std::ostringstream output_stream;
    preprocess(input_stream, output_stream, env);
    return output_stream.str();
}

Env setup(const std::vector<std::pair<std::string, std::string>>& predefine_macros,
          bool remove_comments) {
#ifndef JIEPP_VERSION_MAJOR
#define JIEPP_VERSION_MAJOR 0
#endif
#ifndef JIEPP_VERSION_MINOR
#define JIEPP_VERSION_MINOR 0
#endif
#ifndef JIEPP_VERSION_PATCH
#define JIEPP_VERSION_PATCH 0
#endif
#ifndef JIEPP_VERSION
#define JIEPP_VERSION "0.0.0"
#endif
    Env env;
    env.set_max_include_depth(DEFAULT_MAX_INCLUDE_DEPTH);
    env.set_pragma_style("annotated");
    // Set before -D processing below (U5): -D's body follows the same
    // comment policy as {#define} (handle_define() reads it via
    // env.get_remove_comments()).
    env.set_remove_comments(remove_comments);

    env.define("__COUNTER__",           std::make_unique<CounterMacro>());
    env.define("__LINE__",              std::make_unique<LineMacro>());
    env.define("__FILE__",              std::make_unique<FileMacro>());
    env.define("__DATE__",              std::make_unique<DateMacro>());
    env.define("__TIME__",              std::make_unique<TimeMacro>());
    env.define("__TIMESTAMP__",         std::make_unique<TimeStampMacro>());
    env.define("__INCLUDE_LEVEL__",     std::make_unique<IncludeLevelMacro>());
    env.define("__BASE_FILE__",         std::make_unique<BaseFileMacro>());
    env.define("__FILE_NAME__",         std::make_unique<FileNameMacro>());
    // NOTE: "defined" is NOT installed permanently; it is installed temporarily
    // by eval_cond_str during #if/#elif condition evaluation only.

    // Predefined IEC 61131-3 type range/mask macros (matching jieccc_config.py)
    static const std::vector<std::pair<std::string, std::string>> builtin_macros = {
#define JIEPP_BUILTIN_STR(name, val) {name, val},
#include "builtin_macros.def"
#undef JIEPP_BUILTIN_STR
    };
    for (auto& [k, v] : builtin_macros) {
        auto ts = iec3_tokens_from_string(v, false, 0);
        env.define(k, std::make_unique<UserDefinedObjectMacro>(ts));
    }

    // Version macros derived from CMake PROJECT_VERSION
    {
        constexpr int full_ver = JIEPP_VERSION_MAJOR * 10000
                               + JIEPP_VERSION_MINOR * 100
                               + JIEPP_VERSION_PATCH;
        constexpr int ver      = JIEPP_VERSION_MAJOR * 100
                               + JIEPP_VERSION_MINOR;
        auto define_str = [&](const std::string& name, const std::string& val) {
            auto ts = iec3_tokens_from_string(val, false, 0);
            env.define(name, std::make_unique<UserDefinedObjectMacro>(ts));
        };
        define_str("_JIEPP_FULL_VER", std::to_string(full_ver));
        define_str("_JIEPP_VER",      std::to_string(ver));
        define_str("_JIEPP_VERSION",  "'" JIEPP_VERSION "'");
    }

    // C4: -D NAME[=VALUE] is {k, v} split at its first '=' (v == "1" if
    // none, see define_macro_option()). Like gcc's cpp_define()/clang's
    // DefineBuiltinMacro(), process it as "{#define k v}" instead of
    // defining an object macro unconditionally: k may be a function-like
    // head such as "F(x)", is validated the same as any {#define} name, and
    // redefining an existing macro (including a builtin one already
    // installed above) with a different body is MACRO_REDEFINED (PP35).
    for (auto& [k, v] : predefine_macros)
        jiepp::preprocessor_detail::handle_define(k + " " + v, env);

    return env;
}

void apply_undef_option(const std::string& name, Env& env) {
    // -U NAME is {#undef NAME}: same name validation, 'defined' guard, and
    // extra-token warning (PP49, reported as "{#undef ...}", NAME alone is
    // undefined), like gcc/clang.
    (void)jiepp::preprocessor_detail::handle_undef(name, env);
}

// ---------------------------------------------------------------------------
// dump_macros
// ---------------------------------------------------------------------------

namespace jiepp::preprocessor_detail {

std::string define_directive_text(const std::string& name, const Macro& macro) {
    if (auto* om = dynamic_cast<const UserDefinedObjectMacro*>(&macro)) {
        std::string kv = encode_directive_text(name);
        std::string vv = encode_directive_text(om->str());
        return "{#define " + kv + " " + vv + "}";
    }
    if (auto* fm = dynamic_cast<const FunctionMacro*>(&macro)) {
        std::string kv = encode_directive_text(name);
        std::string vv = encode_directive_text(fm->str());
        return "{#define " + kv + vv + "}";
    }
    return "";
}

} // namespace jiepp::preprocessor_detail

void dump_macros(Env& env, std::ostream& output) {
    for (auto& [name, macro] : env.symbols()) {
        if (dynamic_cast<DefinedOperator*>(macro)) continue;
        std::string line = jiepp::preprocessor_detail::define_directive_text(name, *macro);
        if (!line.empty())
            output << line << "\n";
    }
}
