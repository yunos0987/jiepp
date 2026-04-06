#include "test_helper.hpp"
#include <sstream>

class MessageDirectiveTest : public JieppTest {};

TEST_F(MessageDirectiveTest, SevereMessage) {
    // Fact: severe/error throw; warning/info do not throw
    EXPECT_THROW(pp("{#severe severe message}"), Issue::Exception);
    EXPECT_EQ("<unknown location>:1.0: error: PP90: 'severe message'", message());
}

TEST_F(MessageDirectiveTest, ErrorMessage) {
    // Fact: severe/error throw; warning/info do not throw
    EXPECT_THROW(pp("{#error error message}"), Issue::Exception);
    EXPECT_EQ("<unknown location>:1.0: error: PP91: 'error message'", message());
}

TEST_F(MessageDirectiveTest, WarningMessage) {
    EXPECT_NO_THROW(pp("{#warning warning message}"));
    EXPECT_EQ("<unknown location>:1.0: warning: PP92: 'warning message'", message());
}

TEST_F(MessageDirectiveTest, InfoMessage) {
    EXPECT_NO_THROW(pp("{#info info message}"));
    EXPECT_EQ("<unknown location>:1.0: info: PP93: 'info message'", message());
}

TEST_F(MessageDirectiveTest, SevereCode) {
    // Fact: severe/error throw; warning/info do not throw
    EXPECT_THROW(pp("{#severe severe message}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::SEVERE_MESSAGE, code());
    EXPECT_TRUE(empty());
}

TEST_F(MessageDirectiveTest, ErrorCode) {
    // Fact: severe/error throw; warning/info do not throw
    EXPECT_THROW(pp("{#error error message}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ERROR_MESSAGE, code());
    EXPECT_TRUE(empty());
}

TEST_F(MessageDirectiveTest, WarningCode) {
    EXPECT_NO_THROW(pp("{#warning warning message}"));
    EXPECT_EQ(Issue::Code::WARNING_MESSAGE, code());
    EXPECT_TRUE(empty());
}

TEST_F(MessageDirectiveTest, InfoCode) {
    EXPECT_NO_THROW(pp("{#info info message}"));
    EXPECT_EQ(Issue::Code::INFO_MESSAGE, code());
    EXPECT_TRUE(empty());
}

TEST_F(MessageDirectiveTest, SevereCodename) {
    // Fact: severe/error throw; warning/info do not throw
    EXPECT_THROW(pp("{#severe severe message}"), Issue::Exception);
    EXPECT_EQ("SEVERE_MESSAGE", codename());
}

TEST_F(MessageDirectiveTest, ErrorCodename) {
    // Fact: severe/error throw; warning/info do not throw
    EXPECT_THROW(pp("{#error error message}"), Issue::Exception);
    EXPECT_EQ("ERROR_MESSAGE", codename());
}

TEST_F(MessageDirectiveTest, WarningCodename) {
    EXPECT_NO_THROW(pp("{#warning warning message}"));
    EXPECT_EQ("WARNING_MESSAGE", codename());
}

TEST_F(MessageDirectiveTest, InfoCodename) {
    EXPECT_NO_THROW(pp("{#info info message}"));
    EXPECT_EQ("INFO_MESSAGE", codename());
}

TEST_F(MessageDirectiveTest, Regular) {
    // Fact: error inside active #if fires; inside inactive #if does not
    EXPECT_NO_THROW(pp("\n{#if false}\n{#error message}\n{#endif}"));
    EXPECT_TRUE(empty());

    EXPECT_THROW(pp("\n{#if true}\n{#error message}\n{#endif}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ERROR_MESSAGE, code());
}

TEST_F(MessageDirectiveTest, ExactMultipleCases) {
    EXPECT_NO_THROW(pp(
        "\n{#info message1}\n{#info message1}\n"
        "{#info message2}\n{#info message2}\n"
        "{#info message3}\n{#info message3}\n"));
    const auto recs = messages();
    ASSERT_EQ(6u, recs.size());
    EXPECT_EQ("<unknown location>:2.0: info: PP93: 'message1'", recs[0]);
    EXPECT_EQ("<unknown location>:3.0: info: PP93: 'message1'", recs[1]);
    EXPECT_EQ("<unknown location>:4.0: info: PP93: 'message2'", recs[2]);
    EXPECT_EQ("<unknown location>:5.0: info: PP93: 'message2'", recs[3]);
    EXPECT_EQ("<unknown location>:6.0: info: PP93: 'message3'", recs[4]);
    EXPECT_EQ("<unknown location>:7.0: info: PP93: 'message3'", recs[5]);
}

