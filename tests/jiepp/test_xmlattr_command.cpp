#include "jiepp_command_test_helper.hpp"

// jiepp command-line output of __xmlattr / __xmltext (SPECIFICATION.md 12.12)
// inside {ld}...{end}, the way rungiec writes IEC 61131-10 LD XML, under the
// default (annotated) pragma style, -P and --pp-output-pragma-style standard.

namespace {
class XmlattrCommandTest : public JieppTest {};

std::string run_jiepp_on(const std::string& content, JieppOptions opts,
                         const std::string& name = "xmlattr_cmd.iec") {
    JieppCommandResult r = run_jiepp_command_on(content, std::move(opts), name);
    EXPECT_EQ(0, r.rc);
    return r.out;
}

const std::string kInput =
    "{#if __has_builtin(__xmlattr)}\n"
    "{#define CONTACT(op) <LdObject xsi:type=\"Contact\" operand=__xmlattr(op)/>}\n"
    "{#define LABEL(t) <Text>__xmltext(t)</Text>}\n"
    "{#endif}\n"
    "{ld}CONTACT(Mode = 'RUN' & a < b)\n"
    "LABEL(x := 'y' < 2){end}\n";
} // namespace

TEST_F(XmlattrCommandTest, DefaultStyle) {
    EXPECT_EQ("(*{#:0 'xmlattr_cmd.iec'}*)\n\n\n\n\n"
              "(*{ld}*)<LdObject xsi:type=\"Contact\" operand=\"Mode = &apos;RUN&apos; &amp; a &lt; b\"/>\n"
              "<Text>x := 'y' &lt; 2</Text>(*{end}*)\n",
              run_jiepp_on(kInput, {}));
}

TEST_F(XmlattrCommandTest, NoLineMarkers) {
    JieppOptions opts;
    opts.no_line_markers = true;
    EXPECT_EQ("(*{ld}*)<LdObject xsi:type=\"Contact\" operand=\"Mode = &apos;RUN&apos; &amp; a &lt; b\"/>\n"
              "<Text>x := 'y' &lt; 2</Text>(*{end}*)\n",
              run_jiepp_on(kInput, opts));
}

TEST_F(XmlattrCommandTest, StandardStyle) {
    JieppOptions opts;
    opts.pp_output_pragma_style = "standard";
    EXPECT_EQ("{#:0 'xmlattr_cmd.iec'}\n\n\n\n\n"
              "{ld}<LdObject xsi:type=\"Contact\" operand=\"Mode = &apos;RUN&apos; &amp; a &lt; b\"/>\n"
              "<Text>x := 'y' &lt; 2</Text>{end}\n",
              run_jiepp_on(kInput, opts));
}
