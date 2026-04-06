#include "text.hpp"

#include <algorithm>
#include <cctype>

namespace {

bool _not_space(unsigned char ch) {
	return !std::isspace(ch);
}

}

std::string_view Util::ltrim_view(std::string_view s) {
    auto itr = std::find_if(s.begin(), s.end(), _not_space);
    return std::string_view(itr, s.end());
}

std::string_view Util::rtrim_view(std::string_view s) {
    auto itr = std::find_if(s.rbegin(), s.rend(), _not_space);
    return std::string_view(s.begin(), itr.base());
}

std::string_view Util::trim_view(std::string_view s) {
    return ltrim_view(rtrim_view(s));
}
