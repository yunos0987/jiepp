#include "preprocessor.hpp"
#include "../loader/directive_parser.hpp"
#include "../env/param_constants.hpp"

#include "../loader/lexer.hpp"

#include <sstream>
#include <string>
#include <vector>

void preprocess(const std::string& input_filepath, std::ostream& output, Env& env) {
    std::vector<Token> ots;
    expand(input_filepath, Loader::LoadType::INCLUDE, ots, env, input_filepath);
    for (auto& t : ots)
        output << t.text;
}

void preprocess(std::istream& input, std::ostream& output, Env& env) {
    auto its = iec3_tokens(input, env.get_remove_comments(), 1);
    std::vector<Token> ots;
    expand(its, ots, env);
    for (auto& t : ots)
        output << t.text;
}

std::string preprocess_text(const std::string& input, Env& env) {
    std::istringstream input_stream(input);
    std::ostringstream output_stream;
    preprocess(input_stream, output_stream, env);
    return output_stream.str();
}

Env setup(const std::vector<std::pair<std::string, std::string>>& predefine_macros) {
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
    env.set_remove_comments(false);

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

    for (auto& [k, v] : predefine_macros) {
        auto ts = iec3_tokens_from_string(v, false, 0);
        env.define(k, std::make_unique<UserDefinedObjectMacro>(ts));
    }

    return env;
}

// ---------------------------------------------------------------------------
// dump_macros
// ---------------------------------------------------------------------------

void dump_macros(Env& env, std::ostream& output) {
    for (auto& [name, macro] : env.symbols()) {
        if (dynamic_cast<DefinedOperator*>(macro)) continue;

        if (auto* om = dynamic_cast<UserDefinedObjectMacro*>(macro)) {
            std::string kv = encode_directive_text(name);
            std::string vv = encode_directive_text(om->str());
            output << "{#define " << kv << " " << vv << "}\n";
        } else if (auto* fm = dynamic_cast<FunctionMacro*>(macro)) {
            std::string kv = encode_directive_text(name);
            std::string vv = encode_directive_text(fm->str());
            output << "{#define " << kv << vv << "}\n";
        }
    }
}
