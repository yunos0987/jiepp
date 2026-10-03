#include "constfold.hpp"
#include "constfold_internal.hpp"
#include "constfold_parser.hpp"   // bison generated
#include "../env/issue.hpp"
#include "../util/text.hpp"

#include <cctype>
#include <cstdint>
#include <string>

// flex-generated scanner API (forward declarations)
struct yy_buffer_state;
typedef struct yy_buffer_state *YY_BUFFER_STATE;
extern YY_BUFFER_STATE cf_scan_string(const char*);
extern void cf_delete_buffer(YY_BUFFER_STATE);
extern bool cf_expr_complete;  // set by bison grammar when expr is fully reduced
// C1: code chosen by cf::CfParser::error() (constfold.y) from cf_expr_complete
// at the moment of the parse error; used below instead of unconditionally
// raising MISSING_EXPRESSION, so a case like "1 x" (a complete expression
// followed by more) is reported as INVALID_EXPRESSION, not MISSING_EXPRESSION.
extern Issue::Code cf_error_code;

int64_t eval_const_expr(const std::string& expr) {
    bool all_ws = true;
    for (char ch : expr) {
        if (!std::isspace(static_cast<unsigned char>(ch))) { all_ws = false; break; }
    }
    if (all_ws) {
        // expr comes from a directive operand (e.g. {#if}/{#elif}), which is
        // decoded, so a $n/$r escape in the source is a real line break
        // here; re-escape it so the diagnostic stays on one line, like the
        // directive-operand diagnostics in directive_handlers.cpp.
        ISSUE(MISSING_EXPRESSION, Util::escape_line_breaks(expr));
        return 0;
    }

    YY_BUFFER_STATE buf = cf_scan_string(expr.c_str());
    int rc = 0;
    int64_t result = 0;
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
        return 0;
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
        Issue::happen(cf_error_code, Util::escape_line_breaks(expr));
        return 0;
    }
    return result;
}
