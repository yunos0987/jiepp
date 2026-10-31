#include "test_helper.hpp"

#include <string>
#include <vector>

// A __VA_OPT__(...) that produces no token (the variable arguments are
// empty, or its content substitutes to nothing but blanks and comments) is a
// placemarker (C17 6.10.3.3p2) when it is the left operand of `@@`, exactly
// like an empty parameter written at the same place: the token before it and
// the right operand are not pasted (`a __VA_OPT__(z) @@ y` with empty
// variable arguments is `x y`, as gcc and clang print it). The same holds in
// a non-variadic macro with parameters, where __VA_OPT__ acts as if the
// variable arguments were empty.
//
// The expected outputs were measured with clang 22.1.3 (`clang -E`, `@@` as
// `##`, `@` as `#`); blanks around the placemarker stay as written (SPEC
// 3.4), so some outputs differ from clang's in blanks only. An empty
// parameter at the edge of a non-empty __VA_OPT__ content next to `@@` stays
// unpasted for the `@@` outside the parentheses, as in gcc (clang pastes the
// neighbour); the guards pin that difference.

class VaOptPlacemarkerTest : public JieppTest {
protected:
    // `def` follows `{#define `; `E` is an empty object macro.
    std::string run(const std::string& def, const std::string& calls) {
        return pp("{#define E}{#define " + def + "}" + calls);
    }
    // "PPnn", with "(NAME)" after PP80: the shape of each diagnostic line.
    std::vector<std::string> shapes() {
        std::vector<std::string> r;
        for (const auto& m : messages()) {
            const auto p = m.find(": PP");
            if (p == std::string::npos) {
                r.push_back(m);
                continue;
            }
            std::string s = m.substr(p + 2, 4);
            if (s == "PP80") {
                const auto q = m.rfind("; '");
                if (q != std::string::npos)
                    s += "(" + m.substr(q + 3, m.size() - q - 4) + ")";
            }
            r.push_back(s);
        }
        return r;
    }
};

// Runs in sandbox builds too (the sandbox test presets run ^(Sandbox|RobustnessTest)).
class RobustnessTestVaOptPlacemarker : public VaOptPlacemarkerTest {};

namespace {

struct Form {
    const char* def;   // after `{#define `
    const char* calls; // separated by `|`
    const char* out;
};

} // namespace

#define EXPECT_FORMS_SILENT(forms)                                       \
    do {                                                                 \
        Issue::ContinueMode guard({});                                   \
        for (const auto& f : forms) {                                    \
            SCOPED_TRACE(std::string(f.def) + " " + f.calls);            \
            EXPECT_EQ(f.out, run(f.def, f.calls));                       \
            EXPECT_TRUE(messages().empty());                             \
            EXPECT_EQ(0, Issue::error_count_);                           \
        }                                                                \
    } while (0)

TEST_F(RobustnessTestVaOptPlacemarker, EmptyVaOptLeftOfPasteIsAPlacemarker) {
    const Form forms[] = {
        {"M(a, ...) a __VA_OPT__(z) @@ y", "M(x)|M(x, 1)|M(x, )", "x y|x zy|x y"},
        {"M(a, ...) a __VA_OPT__(z)@@y", "M(x)", "x y"},
        {"M(a, ...) a __VA_OPT__(z) @@ __VA_OPT__(w)y", "M(x)", "x y"},
        {"M(a, ...) a __VA_OPT__(z) @@ a", "M(x)", "x x"},
        {"M(a, ...) a __VA_OPT__(z) @@ __VA_ARGS__", "M(x)|M(x, 1)", "x |x z1"},
        {"M(a, b, ...) a __VA_OPT__(z) @@ b", "M(x, )|M(x, q)", "x |x q"},
        {"M(a, args...) a __VA_OPT__(z) @@ y", "M(x)|M(x, 1)", "x y|x zy"},
        {"M(...) p __VA_OPT__(z) @@ y", "M()|M(1)", "p y|p zy"},
        {"M(a, ...) a __VA_OPT__(z) @@ y @@ a", "M(x)", "x yx"},
    };
    EXPECT_FORMS_SILENT(forms);
}

TEST_F(RobustnessTestVaOptPlacemarker, ChainsWithAnEmptyVaOpt) {
    const Form forms[] = {
        {"M(a, ...) a __VA_OPT__(b) @@ __VA_OPT__(c) @@ d", "M(x)|M(x, 1)", "x d|x bcd"},
        {"M(a, ...) p @@ a __VA_OPT__(b) @@ c", "M(x)|M(, 1)", "px c|p bc"},
        {"M(a, ...) a __VA_OPT__(b) @@ c @@ d", "M(x)", "x cd"},
        {"M(a, ...) a __VA_OPT__(b) @@ __VA_OPT__(c) @@ __VA_OPT__(d) @@ y", "M(x)", "x y"},
        // The right-operand form was already a placemarker (guard).
        {"M(a, ...) a @@ __VA_OPT__(b) @@ c", "M(x)|M(x, 1)", "xc|xbc"},
    };
    EXPECT_FORMS_SILENT(forms);
}

TEST_F(RobustnessTestVaOptPlacemarker, ContentThatSubstitutesToNothing) {
    const Form forms[] = {
        {"M(a, ...) a __VA_OPT__(__VA_ARGS__) @@ y", "M(x)|M(x,)|M(x, E)|M(x, 1)",
         "x y|x y|x y|x 1y"},
        {"M(a, b, ...) a __VA_OPT__(b) @@ y", "M(x, , 1)|M(x, E, 1)|M(x, q, 1)",
         "x y|x y|x qy"},
        {"M(a, ...) a __VA_OPT__() @@ y", "M(x, 1)", "x y"},
        {"M(a, b, c, ...) a __VA_OPT__(b c) @@ y", "M(x, , , 1)", "x  y"},
        {"M(a, b, ...) a __VA_OPT__(b b) @@ y", "M(x, , 1)", "x  y"},
        {"M(a, b, ...) a __VA_OPT__(b @@ b) @@ y", "M(x, , 1)", "x y"},
        // IEC comments are blanks.
        {"M(a, e, ...) a __VA_OPT__((* c *)) @@ y", "M(x, , 1)|M(x, )", "x (* c *)y|x y"},
    };
    EXPECT_FORMS_SILENT(forms);
}

// The token before the empty __VA_OPT__ and the right operand cannot be
// pasted in these: no PP32, since nothing is pasted.
TEST_F(RobustnessTestVaOptPlacemarker, NoSpuriousPasteError) {
    const Form forms[] = {
        {"M(a, ...) ( __VA_OPT__(z) @@ y", "M(x)|M(x, 1)", "( y|( zy"},
        {"M(a, ...) 's' __VA_OPT__(z) @@ y", "M(x)", "'s' y"},
        {"M(a, ...) 1 __VA_OPT__(z) @@ y", "M(x)", "1 y"},
        {"M(a, ...) a __VA_OPT__(z) @@ )", "M(x)", "x )"},
        {"M(a, ...) a __VA_OPT__(z) @@ 's'", "M(x)", "x 's'"},
        {"M(a, ...) a __VA_OPT__(z) @@ y", "M(v[x,y])|M(v[x,y], 1)", "v[x,y] y|v[x,y] zy"},
    };
    EXPECT_FORMS_SILENT(forms);
}

// Without continue mode the spurious PP32 was an exception.
TEST_F(RobustnessTestVaOptPlacemarker, NoThrowWithoutContinueMode) {
    EXPECT_NO_THROW({
        EXPECT_EQ("( y", run("M(a, ...) ( __VA_OPT__(z) @@ y", "M(x)"));
    });
}

TEST_F(RobustnessTestVaOptPlacemarker, NonVariadicWithParameters) {
    Issue::ContinueMode guard({});
    const Form forms[] = {
        {"M(a) a __VA_OPT__(z) @@ y", "M(x)", "x y"},
        {"M(a, b) a __VA_OPT__(z) @@ b", "M(x, y)", "x y"},
        {"M(a) a __VA_OPT__(a) @@ y", "M(x)", "x y"},
        {"M(a) ( __VA_OPT__(z) @@ y", "M(x)", "( y"},
    };
    for (const auto& f : forms) {
        SCOPED_TRACE(std::string(f.def) + " " + f.calls);
        EXPECT_EQ(f.out, run(f.def, f.calls));
        // PP80 at the definition, nothing at the use.
        EXPECT_EQ((std::vector<std::string>{"PP80(M)"}), shapes());
        EXPECT_EQ(0, Issue::error_count_);
    }
}

// Guards (pass before and after the change): shapes whose output must not
// move.
TEST_F(RobustnessTestVaOptPlacemarker, UnchangedShapes_Guard) {
    const Form silent[] = {
        {"M(a, ...) a @@ __VA_OPT__(z) y", "M(x)|M(x, 1)", "x y|xz y"},
        {"M(a, ...) a @@ __VA_OPT__(__VA_ARGS__) y", "M(x, E)|M(x,)", "x y|x y"},
        {"M(a, ...) __VA_OPT__(z) @@ y", "M(x)", "y"},
        // Non-empty content: the blanks and the paste stay as written.
        {"M(a, ...) a __VA_OPT__(z) w @@ y", "M(x)|M(x, 1)", "x  wy|x z wy"},
        {"M(a, b, ...) a __VA_OPT__(z) b @@ y", "M(x, q)|M(x, )", "x  qy|x  y"},
        {"M(a, ...) a __VA_OPT__(z) __VA_ARGS__ @@ y", "M(x)", "x  y"},
        {"M(a, ...) a __VA_OPT__(z)y", "M(x)", "x y"},
        // An empty parameter at the edge of a non-empty content is not a
        // placemarker for the `@@` outside the parentheses (gcc; clang pastes
        // the neighbour: `x z y`). An intended difference, see SPEC 4.3.
        {"M(a, b, ...) a __VA_OPT__(z b) @@ y", "M(x, , 1)|M(x, E, 1)|M(x, q, 1)",
         "x zy|x zy|x z qy"},
        {"M(a, b, ...) a @@ __VA_OPT__(b z) y", "M(x, , 1)|M(x, E, 1)", "xz y|xz y"},
    };
    EXPECT_FORMS_SILENT(silent);

    // Failed pastes that stay failed: `@` always yields a token, and
    // __VA_OPT__ is an identifier in F() and in object-like macros.
    Issue::ContinueMode guard({});
    const struct {
        const char* def;
        const char* calls;
        const char* out;
        std::vector<std::string> expected;
    } failing[] = {
        {"M(a, ...) a @__VA_OPT__(z) @@ y", "M(x)|M(x, 1)", "x ''y|x 'z'y", {"PP32", "PP32"}},
        {"M(a, b, ...) a __VA_OPT__(@b) @@ y", "M(x, , 1)", "x ''y", {"PP32"}},
        {"M() p __VA_OPT__(z) @@ y", "M()", "p __VA_OPT__(z)y", {"PP80(M)", "PP32"}},
        {"M() p @@ __VA_OPT__(z) @@ y", "M()", "p__VA_OPT__(z)y", {"PP80(M)", "PP32"}},
        {"M p __VA_OPT__(z) @@ y", "M", "p __VA_OPT__(z)y", {"PP80(M)", "PP32"}},
        {"M p @@ __VA_OPT__(z) @@ y", "M", "p__VA_OPT__(z)y", {"PP80(M)", "PP32"}},
        {"M(a) a @@ __VA_OPT__(z) @@ y", "M(x)", "xy", {"PP80(M)"}},
    };
    for (const auto& f : failing) {
        SCOPED_TRACE(std::string(f.def) + " " + f.calls);
        EXPECT_EQ(f.out, run(f.def, f.calls));
        EXPECT_EQ(f.expected, shapes());
    }
}

// Guards: an identical redefinition is silent (no PP35) but warns about its
// __VA_OPT__ again; `v[x,y]` stays one argument.
TEST_F(RobustnessTestVaOptPlacemarker, Watchpoints_Guard) {
    Issue::ContinueMode guard({});
    const std::string def = "{#define F(a) a __VA_OPT__(z) @@ y}";
    EXPECT_EQ("", pp(def + def));
    EXPECT_EQ((std::vector<std::string>{"PP80(F)", "PP80(F)"}), shapes());
    const std::string vdef = "{#define G(a, ...) a __VA_OPT__(z) @@ y}";
    EXPECT_EQ("", pp(vdef + vdef));
    EXPECT_TRUE(messages().empty());

    EXPECT_EQ("<v[x,y]>|<v[x,y]>", run("M(a, ...) <a>", "M(v[x,y])|M(v[x,y], 1)"));
    EXPECT_TRUE(messages().empty());
}
