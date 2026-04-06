#include "lineno_test_helper.hpp"
#include <algorithm>

// ---- Include directives: file-level line number / diagnostic path tracking ----
namespace {
void normalize_diags(std::vector<std::string>& diags) {
    constexpr const char* kIncludePrefix = "tests/core/test_lineno/";
    for (auto& diag : diags) {
        std::replace(diag.begin(), diag.end(), '\\', '/');
        const auto prefix_pos = diag.find(kIncludePrefix);
        if (prefix_pos != std::string::npos) {
            diag.erase(0, prefix_pos + std::char_traits<char>::length(kIncludePrefix));
        }
        while (!diag.empty() && diag.back() == ' ') {
            diag.pop_back();
        }
    }
}
} // namespace

TEST_F(LinenoTest, Include) {
    const std::vector<FileErrorCase> cases = {
        {
            "a-b",
            "a-b.iec",
            {
                "a-b.iec:1.0: info: PP93: ''",
                "b.error.iec:1.0: info: PP93: ''",
                "b.error.iec:2.0: error: PP91: 'error'",
            },
        },
        {
            "a-b-error",
            "a-b.error.iec",
            {
                "a-b.error.iec:1.0: info: PP93: ''",
                "b.iec:1.0: info: PP93: ''",
                "b.iec:2.0: info: PP93: ''",
                "a-b.error.iec:3.0: info: PP93: ''",
                "b.iec:1.0: info: PP93: ''",
                "b.iec:2.0: info: PP93: ''",
                "a-b.error.iec:5.0: info: PP93: ''",
                "a-b.error.iec:6.0: error: PP91: 'error'",
            },
        },
        {
            "a-b-c",
            "a-b-c.iec",
            {
                "a-b-c.iec:1.0: info: PP93: ''",
                "b-c.error.iec:1.0: info: PP93: ''",
                "c.iec:1.0: info: PP93: ''",
                "c.iec:2.0: info: PP93: ''",
                "b-c.error.iec:3.0: info: PP93: ''",
                "c.iec:1.0: info: PP93: ''",
                "c.iec:2.0: info: PP93: ''",
                "b-c.error.iec:5.0: info: PP93: ''",
                "b-c.error.iec:6.0: error: PP91: 'error'",
            },
        },
        {
            "a-b-c-error",
            "a-b-c.error.iec",
            {
                "a-b-c.error.iec:1.0: info: PP93: ''",
                "b-c.iec:1.0: info: PP93: ''",
                "c.error.iec:1.0: info: PP93: ''",
                "c.error.iec:2.0: error: PP91: 'error'",
            },
        },
    };

    for (const auto& test_case : cases) {
        SCOPED_TRACE(test_case.name);
        fs::path path = jiepp_test_dir() / "core" / "test_lineno" / test_case.relative_path; 
        Env env = setup();
        env.push_file(path.generic_string());
        Issue::push({1, path.generic_string()});
        std::ifstream ifs(path);
        std::stringstream buffer;
        buffer << ifs.rdbuf();
        EXPECT_THROW(pp(buffer.str(), env), Issue::Exception);
        auto actual_diags = messages();
        normalize_diags(actual_diags);
        auto expected_diags = test_case.expected_diags;
        EXPECT_EQ(expected_diags, actual_diags);
    }
}
