#pragma once
#include "test_helper.hpp"

struct TestCase {
    std::string name;
    std::string input;
    std::string expected_output;
    std::vector<std::string> expected_diags;
    bool verify_plain_newline_equivalence = false;
};

struct FileErrorCase {
    std::string name;
    std::string relative_path;
    std::vector<std::string> expected_diags;
};

class LinenoTest : public JieppTest {};

inline std::string replace_dollar_newline(const std::string& input) {
    std::string output;
    output.reserve(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '$' && i + 1 < input.size() && input[i + 1] == '\n')
            continue;
        output.push_back(input[i]);
    }
    return output;
}

inline std::string normalize_slashes(std::string text) {
    std::replace(text.begin(), text.end(), '\\', '/');
    return text;
}
