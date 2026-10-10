// builtin_function_macros.cpp -- invoke() of the built-in function-like
// macros __error(msg...) and __assert(expr, msg...) (macro.hpp). They are
// called from expand()'s function-macro call branch (expand.cpp) with the
// collected, flattened, NOT pre-expanded arguments.

#include "expand_helpers.hpp"
#include "preprocessor.hpp"
#include "preprocessor_internal.hpp"

#include "../macro/macro.hpp"

#include <cctype>
#include <string>
#include <vector>

using namespace jiepp::expand_detail;

namespace {

// While a message is expanded inside an {#if}/{#elif} condition, the
// transient 'defined' operator is suspended, so the word 'defined' in a
// message is plain text instead of an operator without an operand (PP40,
// which would make the whole condition false). Only applies when the
// operator is installed; a nested expression evaluation (an __assert in the
// message) re-enables it for its own expression (DefinedScope,
// expand_ctrl.cpp) and restores the suspension afterwards.
class SuspendDefined {
public:
    explicit SuspendDefined(Env& env)
        : op_(dynamic_cast<DefinedOperator*>(env.lookup("defined"))) {
        if (op_) {
            saved_ = op_->suspended;
            op_->suspended = true;
        }
    }
    ~SuspendDefined() {
        if (op_)
            op_->suspended = saved_;
    }
    SuspendDefined(const SuspendDefined&) = delete;
    SuspendDefined& operator=(const SuspendDefined&) = delete;

private:
    DefinedOperator* op_;
    bool saved_ = false;
};

// The message of __error/__assert: the variable-argument part from
// args[first] on, joined like __VA_ARGS__ (commas kept, one space on a side
// only where the call had whitespace there), then macro-expanded (hide sets
// kept) and concatenated as text, like the operand of {#error}.
// *only_ws is set to whether the expanded text is empty or whitespace only.
std::string expand_message(std::size_t first,
                           const std::vector<std::vector<Token>>& args,
                           const std::vector<ArgWs>& ws,
                           Env& env,
                           bool* only_ws) {
    std::vector<Token> msg = select_arg(static_cast<int>(first), args, /*is_va=*/true,
                                        &ws, VaJoin::plain);
    std::vector<Token> ots;
    {
        SuspendDefined suspend(env);
        expand(msg, ots, env);
    }
    std::string text;
    for (const auto& t : ots)
        text += t.text;
    if (only_ws) {
        *only_ws = true;
        for (char c : text) {
            if (!std::isspace(static_cast<unsigned char>(c))) {
                *only_ws = false;
                break;
            }
        }
    }
    return text;
}

// PP91 through the shared message path ({#error} uses it too).
void report_error_message(std::string msg) {
    jiepp::preprocessor_detail::report_message(std::move(msg), Issue::Code::ERROR_MESSAGE);
}

} // namespace

std::vector<Token> ErrorBuiltin::invoke(std::vector<std::vector<Token>>& args,
                                        const std::vector<ArgWs>& ws,
                                        Env& env) const {
    report_error_message(expand_message(0, args, ws, env, nullptr));
    return {};
}

std::vector<Token> AssertBuiltin::invoke(std::vector<std::vector<Token>>& args,
                                         const std::vector<ArgWs>& ws,
                                         Env& env) const {
    // args has at least one entry (min_args() == 1; an empty call is one
    // empty argument).
    CondValue r = eval_cond_value_tokens(args[0], env);
    if (r.failure != Issue::Code::SUCCESS) {
        // The evaluator diagnostic was reported: like clang static_assert,
        // nothing is stacked on top. If {#ignore} suppressed it, the
        // assertion must still not pass silently, so PP91 is reported.
        if (!Issue::is_ignored(r.failure))
            return {};
    } else if (r.value.truthy()) {
        // True: silent, and the message is not expanded at all.
        return {};
    }

    std::string msg;
    bool only_ws = true;
    if (args.size() >= 2)
        msg = expand_message(1, args, ws, env, &only_ws);
    if (only_ws) {
        // clang's static_assert wording; EXPR is the expression as written,
        // whitespace collapsed like stringizing (SPEC 4.1).
        msg = "static assertion failed due to requirement '" + stringize_text(args[0]) + "'";
    }
    report_error_message(std::move(msg));
    return {};
}
