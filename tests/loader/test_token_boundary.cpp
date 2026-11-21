#include "lexer_test_helper.hpp"

#include "loader/lexer.hpp"

// ---- the token-boundary rules of macro expansion (SPEC §3.9, §4.3) ----
//
// iec3_needs_space(before, after): whether the two texts, written together,
// no longer have a token boundary between them while one space restores it.
// iec3_pasted_token_type(left, right): the type of the one token the two
// read as, or nullopt.

class TokenBoundaryTest : public JieppTest {};

TEST_F(TokenBoundaryTest, NeedsSpaceWhereTheTokensWouldJoin) {
    // One identifier or number.
    EXPECT_TRUE(iec3_needs_space("a", "b"));
    EXPECT_TRUE(iec3_needs_space("1", "2"));
    EXPECT_TRUE(iec3_needs_space("1", "a"));
    EXPECT_TRUE(iec3_needs_space("a", "1"));
    // A number continued across the edge, on either side of it.
    EXPECT_TRUE(iec3_needs_space("1", ".5"));
    EXPECT_TRUE(iec3_needs_space("1.", "5"));
    EXPECT_TRUE(iec3_needs_space("1e", "+5"));
    EXPECT_TRUE(iec3_needs_space("1e+", "5"));
    // The IEC operators of two characters.
    for (const char* op : {":=", "=>", "<=", ">=", "<>", "**", ".."})
        EXPECT_TRUE(iec3_needs_space(std::string(1, op[0]), std::string(1, op[1]))) << op;
    // Comment openers, the @@ token, a direct representation.
    EXPECT_TRUE(iec3_needs_space("/", "/"));
    EXPECT_TRUE(iec3_needs_space("/", "*"));
    EXPECT_TRUE(iec3_needs_space("(", "*"));
    EXPECT_TRUE(iec3_needs_space("/", "/* c */"));
    EXPECT_TRUE(iec3_needs_space("@", "@"));
    EXPECT_TRUE(iec3_needs_space("%", "IX0"));
    EXPECT_TRUE(iec3_needs_space("*", "**"));
    EXPECT_TRUE(iec3_needs_space(".", ".."));
}

TEST_F(TokenBoundaryTest, NoSpaceWhereTheBoundaryStays) {
    // Pairs that are one token in C but not in IEC 61131-3.
    for (const char* op : {"++", "--", "&&", "||", "==", "!=", "->", "<<", ">>", "::", "+-"})
        EXPECT_FALSE(iec3_needs_space(std::string(1, op[0]), std::string(1, op[1]))) << op;
    EXPECT_FALSE(iec3_needs_space("*", ")"));
    EXPECT_FALSE(iec3_needs_space("1", "."));
    EXPECT_FALSE(iec3_needs_space(".", "5"));
    EXPECT_FALSE(iec3_needs_space("a", "("));
    EXPECT_FALSE(iec3_needs_space("$", "$"));
    // The tokens already join the same way on each side.
    EXPECT_FALSE(iec3_needs_space(":=", "="));
    EXPECT_FALSE(iec3_needs_space("..", "."));
    EXPECT_FALSE(iec3_needs_space("<", ":="));
    // Two literals with the same quote are merged with or without a space.
    EXPECT_FALSE(iec3_needs_space("'x'", "'y'"));
    // The '#' of a typed literal: the lexer splits it there anyway.
    EXPECT_FALSE(iec3_needs_space("16", "#FF"));
    EXPECT_FALSE(iec3_needs_space("T", "#1s"));
    EXPECT_FALSE(iec3_needs_space("16#", "FF"));
    // An unclosed comment or unterminated literal takes in everything after
    // it: a space does not help.
    EXPECT_FALSE(iec3_needs_space("(* c", "x"));
    EXPECT_FALSE(iec3_needs_space("'abc", "x"));
    EXPECT_FALSE(iec3_needs_space("", "x"));
    EXPECT_FALSE(iec3_needs_space("x", ""));
    EXPECT_TRUE(empty());
}

TEST_F(TokenBoundaryTest, MayJoinIsANecessaryCondition) {
    // Every pair that needs a space passes the cheap filter, which sees the
    // last two output tokens and the first two characters after the edge.
    struct Pair { const char* before_left; const char* left; const char* after; };
    for (const Pair& p : {Pair{"", "a", "b"}, Pair{"", "1", ".5"}, Pair{"1", ".", "5"},
                          Pair{"1e", "+", "5"}, Pair{"", ":", "="}, Pair{"", "*", "*"},
                          Pair{"", ".", "."}, Pair{"", "/", "/"}, Pair{"", "/", "*"},
                          Pair{"", "(", "*"}, Pair{"", "@", "@"}, Pair{"", "%", "IX0"},
                          Pair{"%IX0", ".", "1"}}) {
        const std::string before = std::string(p.before_left) + p.left;
        ASSERT_TRUE(iec3_needs_space(before, p.after)) << before << " " << p.after;
        EXPECT_TRUE(iec3_may_join(p.before_left, p.left, p.after[0], p.after[1])) << before << " " << p.after;
    }
    // Pairs that never join are told apart without lexing.
    EXPECT_FALSE(iec3_may_join("", "+", '+', '\0'));
    EXPECT_FALSE(iec3_may_join("", "+", '1', '\0'));
    EXPECT_FALSE(iec3_may_join("x", "+", '1', '\0'));
    EXPECT_FALSE(iec3_may_join("", "(", '(', '\0'));
    EXPECT_FALSE(iec3_may_join("a", ".", '5', '\0'));
    EXPECT_FALSE(iec3_may_join("", "a", '(', '\0'));
    // A number and '.' join only with a digit after the '.'.
    EXPECT_FALSE(iec3_may_join("", "1", '.', '.'));
    EXPECT_FALSE(iec3_may_join("", "1", '.', '\0'));
    EXPECT_FALSE(iec3_may_join("1", ".", 'e', '5'));
    EXPECT_FALSE(iec3_may_join("", "x", '.', '5'));
    EXPECT_FALSE(iec3_may_join("", "x", '+', '1'));
    EXPECT_FALSE(iec3_may_join("", "16", '#', 'F'));
    EXPECT_FALSE(iec3_may_join("", "#", 'F', 'F'));
}

// The filter never changes a decision: over the product of representative
// tokens (the output token before the last, the last one, the text after the
// edge), every edge where iec3_needs_space() wants a space -- the decision
// without the filter -- passes iec3_may_join().
TEST_F(TokenBoundaryTest, MayJoinNeverDropsANeededSpace) {
    const std::vector<std::string> tokens = {
        "", "a", "x1", "_", "e", "E", "1", "12", "1e", "1E", "1.5", "1.5e", "16#FF", "16#E",
        "1_000", "%IX0", "%I", "%", "+", "-", "*", "/", ".", ":", "=", "<", ">", "(", ")",
        "[", "]", ",", ";", "&", "|", "#", "@", "@@", "$", "!", "?", "^", "~", "'a'", "\"a\"",
        "**", ":=", "..", "<=", "<>"};
    const std::vector<std::string> afters = {
        "a", "b1", "_", "e", "e5", "E5", "1", "5", "5e", ".", ".5", "..", "+", "+5", "-5", "-",
        "*", "**", "*)", "/", "//", "/* c */", "(* c *)", "(", ")", "=", "==", ">", "<", ":",
        ":=", "#", "#FF", "@", "@@", "%", "%IX0", "$", "'a'", "\"a\"", ",", ";", "[", "!", "IX0"};
    std::size_t needed = 0;
    for (const auto& bl : tokens) {
        for (const auto& l : tokens) {
            // Two output tokens that would read as one were kept apart by an
            // earlier edge (or are spliced as written by {#token}).
            if (l.empty() || (!bl.empty() && iec3_needs_space(bl, l)))
                continue;
            for (const auto& a : afters) {
                if (!iec3_needs_space(bl + l, a))
                    continue;
                ++needed;
                EXPECT_TRUE(iec3_may_join(bl, l, a.front(), a.size() > 1 ? a[1] : '\0'))
                    << "'" << bl << "' '" << l << "' + '" << a << "'";
            }
        }
    }
    EXPECT_GT(needed, 100u);
}

TEST_F(TokenBoundaryTest, PastedTokenType) {
    EXPECT_EQ(Token::ANY, iec3_pasted_token_type("x", "y"));
    EXPECT_EQ(Token::ANY, iec3_pasted_token_type("1", "x"));
    EXPECT_EQ(Token::ANY, iec3_pasted_token_type("1.5", "e3"));
    EXPECT_EQ(Token::ANY, iec3_pasted_token_type("16#2", "5"));
    EXPECT_EQ(Token::ANY, iec3_pasted_token_type(":", "="));
    EXPECT_EQ(Token::ANY, iec3_pasted_token_type(".", "."));
    EXPECT_EQ(Token::GLUE, iec3_pasted_token_type("@", "@"));
    // The '#' of a typed literal, on either side.
    EXPECT_EQ(Token::ANY, iec3_pasted_token_type("#", "FF"));
    EXPECT_EQ(Token::ANY, iec3_pasted_token_type("T", "#"));
    EXPECT_EQ(Token::ANY, iec3_pasted_token_type("COLOR#", "RED"));
    // More than one token, another spelling, or not a token at all.
    EXPECT_EQ(std::nullopt, iec3_pasted_token_type("+", "-"));
    EXPECT_EQ(std::nullopt, iec3_pasted_token_type("=", "="));
    EXPECT_EQ(std::nullopt, iec3_pasted_token_type("x", "+"));
    EXPECT_EQ(std::nullopt, iec3_pasted_token_type("1", "."));
    EXPECT_EQ(std::nullopt, iec3_pasted_token_type(".", "5"));
    EXPECT_EQ(std::nullopt, iec3_pasted_token_type("16#2", "16#5"));
    EXPECT_EQ(std::nullopt, iec3_pasted_token_type("'a'", "'b'"));
    EXPECT_EQ(std::nullopt, iec3_pasted_token_type("/", "/"));
    EXPECT_EQ(std::nullopt, iec3_pasted_token_type("(", "*"));
    EXPECT_EQ(std::nullopt, iec3_pasted_token_type(")", "y"));
    EXPECT_EQ(std::nullopt, iec3_pasted_token_type("'a", "b"));
    EXPECT_EQ(std::nullopt, iec3_pasted_token_type("", "b"));
    // The lexer reports nothing while it reads these.
    EXPECT_EQ(std::nullopt, iec3_pasted_token_type("(*", "x"));
    EXPECT_EQ(std::nullopt, iec3_pasted_token_type("{", "x"));
    EXPECT_TRUE(empty());
}
