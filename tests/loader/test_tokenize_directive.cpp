#include "test_helper.hpp"

class TokenizeDirectiveTest : public JieppTest {};

TEST_F(TokenizeDirectiveTest, TokenizeDirective) {
    // Fact: {#token} / {###} emit their argument as raw tokens
    EXPECT_EQ("xyz", pp("{#token xyz}"));
    EXPECT_EQ("xyz    ", pp("{#token  xyz  $  }"));
    EXPECT_EQ(" x$y z ", pp("{#token $ x$$y z }"));
    EXPECT_EQ("xyz", pp("{#token:xyz}"));
    EXPECT_EQ("xyz    ", pp("{#token  :  xyz  $  }"));
    EXPECT_EQ(" x$y z ", pp("{#token:$ x$$y z }"));
    EXPECT_EQ("xyz", pp("{### xyz}"));
    EXPECT_EQ("xyz  ", pp("{###  xyz  }"));
    EXPECT_EQ("xyz", pp("{###:xyz}"));
    EXPECT_EQ("xyz  ", pp("{###:  xyz  }"));
    EXPECT_EQ(" x$y z ", pp("{### $ x$$y z }"));
    EXPECT_EQ(" x$y z ", pp("{###:$ x$$y z }"));
    EXPECT_EQ("program Main2 end_program",
              pp("program Main{### 2} end_program"));
    EXPECT_EQ("program Main0 end_program",
              pp("program Main{### __COUNTER__} end_program"));
    EXPECT_EQ("prog ra m Main end_program",
              pp("prog {### ra} m Main end_program"));
    EXPECT_TRUE(empty());
}

