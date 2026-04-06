#include "test_helper.hpp"

class NopDirectiveTest : public JieppTest {};

// Tests for {#nop} — produces no output; any value after the key is ignored.

TEST_F(NopDirectiveTest, NopKeyword) {
    EXPECT_EQ(";;;", pp("{#nop};{# nop };{#  nop  };"));
    EXPECT_TRUE(empty());
}

TEST_F(NopDirectiveTest, NopDollarNL) {
    // $\n or literal NL before the nop keyword produces extra_nl
    EXPECT_EQ(";;;\n;", pp("{#nop};{# nop };{#  nop  };{#  $\nnop    };"));
    EXPECT_EQ(";;;\n;", pp("{#nop};{# nop };{#  nop  };{#  \nnop    };"));
    EXPECT_TRUE(empty());
}

TEST_F(NopDirectiveTest, WithMessage) {
    // text after the key is ignored; no output
    EXPECT_EQ(";", pp("{#nop message};"));
    EXPECT_TRUE(empty());
}

