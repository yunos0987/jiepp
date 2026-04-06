#include "lexer_helpers.hpp"

namespace jiepp::detail {

int consume_one_nl(const std::string& s, std::size_t& pos) {
    if (pos >= s.size()) return 0;
    if (s[pos] == '\r') {
        ++pos;
        if (pos < s.size() && s[pos] == '\n') ++pos;
    } else if (s[pos] == '\n') {
        ++pos;
    } else {
        return 0;
    }
    return 1;
}

} // namespace jiepp::detail
