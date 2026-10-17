#include "constfold.hpp"
#include "constfold_internal.hpp"
#include "constfold_parser.hpp"   // bison generated
#include "../env/issue.hpp"
#include "../util/text.hpp"

#include <cctype>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <string>

// flex-generated scanner API (forward declarations)
struct yy_buffer_state;
typedef struct yy_buffer_state *YY_BUFFER_STATE;
extern YY_BUFFER_STATE cf_scan_expr(const char*, std::size_t);  // constfold.l
extern void cf_delete_buffer(YY_BUFFER_STATE);
extern bool cf_expr_complete;  // set by bison grammar when expr is fully reduced
// C1: code chosen by cf::CfParser::error() (constfold.y) from cf_expr_complete
// at the moment of the parse error; used below instead of unconditionally
// raising MISSING_EXPRESSION, so a case like "1 x" (a complete expression
// followed by more) is reported as INVALID_EXPRESSION, not MISSING_EXPRESSION.
extern Issue::Code cf_error_code;

namespace {

cf::ConstValue to_const_value(const CfValue& v) {
    cf::ConstValue out;
    out.kind = v.kind;
    out.i = v.ival;
    out.bits = v.bits;
    out.bit_kind = v.bit_kind;
    out.f = v.fval;
    out.b = v.bval;
    return out;
}

// The one evaluation behind eval_const_expr() and eval_const_value(). On an
// empty expression, a parse failure or a type error, value stays Int 0 (the
// old 0 result of eval_const_expr()); a literal error raised inside a scanner
// or grammar action records a failure but keeps the parsed value.
cf::EvalResult evaluate(const std::string& expr) {
    cf::EvalResult r;
    cf_failure_code = Issue::Code::SUCCESS;
    // A parse aborted by an exception (PP50, a throwing diagnostic) never
    // runs the action that lowers the depth; start every evaluation at 0.
    cf_unevaluated_depth = 0;
    bool all_ws = true;
    for (char ch : expr) {
        if (!std::isspace(static_cast<unsigned char>(ch))) { all_ws = false; break; }
    }
    if (all_ws) {
        // expr comes from a directive operand (e.g. {#if}/{#elif}), which is
        // decoded, so a $n/$r escape in the source is a real line break
        // here; re-escape it so the diagnostic stays on one line, like the
        // directive-operand diagnostics in directive_handlers.cpp.
        cf_note_failure(Issue::Code::MISSING_EXPRESSION);
        ISSUE(MISSING_EXPRESSION, Util::escape_line_breaks(expr));
        r.failure = cf_failure_code;
        return r;
    }

    // A7: scan by length. The scanner buffer length is an int (flex adds two
    // terminating NULs), so a longer expression cannot be scanned at all;
    // and a NUL inside the expression is not a valid expression character.
    // As above, cf_note_failure() comes first so __assert / __counter_set
    // see the failure.
    if (expr.size() > static_cast<std::size_t>(INT_MAX) - 2) {
        cf_note_failure(Issue::Code::INVALID_EXPRESSION);
        ISSUE(INVALID_EXPRESSION, "expression too long");
        r.failure = cf_failure_code;
        return r;
    }
    if (expr.find('\0') != std::string::npos) {
        cf_note_failure(Issue::Code::INVALID_EXPRESSION);
        ISSUE(INVALID_EXPRESSION,
              "null character in expression: " + Util::escape_line_breaks(expr));
        r.failure = cf_failure_code;
        return r;
    }

    YY_BUFFER_STATE buf = cf_scan_expr(expr.data(), expr.size());
    int rc = 0;
    CfValue result;
    try {
        cf_expr_complete = false;
        cf_error_code = Issue::Code::MISSING_EXPRESSION;
        cf::CfParser parser(result);
        rc = parser.parse();
        cf_delete_buffer(buf);
    } catch (const CfTypeError&) {
        // A4: EXPR_TYPE_ERROR (PP50) was reported (or suppressed via
        // {#ignore}) inside the grammar action; either way, recover here
        // instead of letting the internal signal escape uncaught.
        cf_delete_buffer(buf);
        r.failure = cf_failure_code;
        return r;
    } catch (...) {
        cf_delete_buffer(buf);
        throw;
    }
    if (rc != 0) {
        // Same reasoning as the all_ws case above: expr is decoded, so
        // re-escape it before it reaches the diagnostic. A syntax error can
        // leave result holding a partial value from before the parser gave
        // up (e.g. "1 x" parses "1" before failing on "x"); treat the whole
        // expression as false, like gcc/clang do for a malformed {#if}.
        // C1: the single diagnostic for this parse failure, using the code
        // cf::CfParser::error() chose (cf_error_code) instead of
        // unconditionally MISSING_EXPRESSION.
        cf_note_failure(cf_error_code);
        Issue::happen(cf_error_code, Util::escape_line_breaks(expr));
        r.failure = cf_failure_code;
        return r;
    }
    r.failure = cf_failure_code;
    r.value = to_const_value(result);
    return r;
}

} // namespace

int64_t eval_const_expr(const std::string& expr) {
    // Unchanged {#if} result: 0 for an empty expression, a parse failure or
    // a type error; otherwise the truth value of the parsed expression, even
    // after a literal error (F7: that leniency is kept for {#if}).
    return evaluate(expr).value.truthy() ? 1 : 0;
}

cf::EvalResult eval_const_value(const std::string& expr) {
    return evaluate(expr);
}
