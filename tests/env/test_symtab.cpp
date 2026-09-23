#include "test_helper.hpp"
#include "env/env.hpp"
#include "macro/macro.hpp"

class SymtabTest : public JieppTest {};

TEST_F(SymtabTest, DefaultConstruct) {
    Env env;
    EXPECT_TRUE(env.symbols().empty());
    EXPECT_FALSE(env.exist("ANY"));
    EXPECT_EQ(nullptr, env.lookup("ANY"));
}

TEST_F(SymtabTest, Define) {
    Env env;
    auto macro = std::make_unique<UserDefinedObjectMacro>(std::vector<Token>{});
    EXPECT_TRUE(env.define("FOO", std::move(macro)));
    EXPECT_TRUE(env.exist("FOO"));
    EXPECT_FALSE(env.exist("BAR"));
}

TEST_F(SymtabTest, Lookup) {
    Env env;
    auto macro = std::make_unique<UserDefinedObjectMacro>(std::vector<Token>{});
    Macro* raw = macro.get();
    env.define("FOO", std::move(macro));
    EXPECT_EQ(raw, env.lookup("FOO"));
    EXPECT_EQ(nullptr, env.lookup("BAR"));
}

TEST_F(SymtabTest, Undef) {
    Env env;
    env.define("FOO", std::make_unique<UserDefinedObjectMacro>(std::vector<Token>{}));
    EXPECT_TRUE(env.exist("FOO"));
    // F14: undef() returns void — the superseded Macro is retired inside the
    // Symtab (not handed back to the caller) so that a raw Macro* obtained
    // via an earlier lookup() stays valid for the Symtab's lifetime.
    env.undef("FOO");
    EXPECT_FALSE(env.exist("FOO"));
    EXPECT_EQ(nullptr, env.lookup("FOO"));
}

TEST_F(SymtabTest, RedefineAfterUndefKeepsInsertionOrder) {
    // F11: undef() no longer erases the name from the index, so a later
    // define() of the same name replaces the retained slot in place instead
    // of appending a brand-new entry at the end — insertion order (as seen
    // via symbols()) is preserved across undef-then-redefine.
    Env env;
    env.define("A", std::make_unique<UserDefinedObjectMacro>(std::vector<Token>{}));
    env.define("B", std::make_unique<UserDefinedObjectMacro>(std::vector<Token>{}));
    env.define("C", std::make_unique<UserDefinedObjectMacro>(std::vector<Token>{}));
    env.undef("B");
    env.define("B", std::make_unique<UserDefinedObjectMacro>(std::vector<Token>{}));
    auto syms = env.symbols();
    ASSERT_EQ(3u, syms.size());
    EXPECT_EQ("A", syms[0].first);
    EXPECT_EQ("B", syms[1].first);
    EXPECT_EQ("C", syms[2].first);
}

TEST_F(SymtabTest, Symbols) {
    Env env;
    env.define("A", std::make_unique<UserDefinedObjectMacro>(std::vector<Token>{}));
    env.define("B", std::make_unique<UserDefinedObjectMacro>(std::vector<Token>{}));
    auto syms = env.symbols();
    ASSERT_EQ(2u, syms.size());
    EXPECT_EQ("A", syms[0].first);
    EXPECT_EQ("B", syms[1].first);
}

TEST_F(SymtabTest, SymbolsSkipsUndef) {
    Env env;
    env.define("A", std::make_unique<UserDefinedObjectMacro>(std::vector<Token>{}));
    env.define("B", std::make_unique<UserDefinedObjectMacro>(std::vector<Token>{}));
    env.undef("A");
    auto syms = env.symbols();
    ASSERT_EQ(1u, syms.size());
    EXPECT_EQ("B", syms[0].first);
}
