#include "constfold.hpp"
#include "constfold_internal.hpp"
#include "constfold_parser.hpp"   // bison generated
#include "../env/issue.hpp"

#include <cctype>
#include <cstdint>
#include <string>

// flex-generated scanner API (forward declarations)
struct yy_buffer_state;
typedef struct yy_buffer_state *YY_BUFFER_STATE;
extern YY_BUFFER_STATE cf_scan_string(const char*);
extern void cf_delete_buffer(YY_BUFFER_STATE);
extern bool cf_expr_complete;  // set by bison grammar when expr is fully reduced

int64_t eval_const_expr(const std::string& expr) {
    bool all_ws = true;
    for (char ch : expr) {
        if (!std::isspace(static_cast<unsigned char>(ch))) { all_ws = false; break; }
    }
    if (all_ws) {
        ISSUE(MISSING_EXPRESSION, expr);
        return 0;
    }

    YY_BUFFER_STATE buf = cf_scan_string(expr.c_str());
    int rc = 0;
    int64_t result = 0;
    try {
        cf_expr_complete = false;
        cf::CfParser parser(result);
        rc = parser.parse();
        cf_delete_buffer(buf);
    } catch (...) {
        cf_delete_buffer(buf);
        throw;
    }
    if (rc != 0)
        ISSUE(MISSING_EXPRESSION, expr);
    return result;
}
