#include "aegis/protocol/parser.hpp"
#include "aegis/protocol/serializer.hpp"

#include <gtest/gtest.h>

TEST(ProtocolTest, ParseRespArray) {
    aegis::RespParser parser;
    std::string input = "*3\r\n$3\r\nSET\r\n$4\r\nuser\r\n$5\r\nalice\r\n";

    size_t consumed = 0;
    aegis::Command cmd;
    auto res = parser.parse(input, consumed, cmd);

    EXPECT_EQ(res, aegis::ParseResult::Success);
    EXPECT_EQ(consumed, input.size());
    EXPECT_EQ(cmd.type(), aegis::CommandType::Set);
    EXPECT_EQ(cmd.arg_count(), 2);
    EXPECT_EQ(cmd.arg(0), "user");
    EXPECT_EQ(cmd.arg(1), "alice");
}

TEST(ProtocolTest, ParseFragmentedPackets) {
    aegis::RespParser parser;
    std::string chunk1 = "*3\r\n$3\r\nSET";
    std::string chunk2 = "\r\n$3\r\nfoo\r\n$3\r\nbar\r\n";

    size_t consumed = 0;
    aegis::Command cmd;

    // First chunk incomplete
    auto res = parser.parse(chunk1, consumed, cmd);
    EXPECT_EQ(res, aegis::ParseResult::NeedMoreData);
    EXPECT_EQ(consumed, 0);

    // Combine chunks
    std::string full = chunk1 + chunk2;
    res = parser.parse(full, consumed, cmd);
    EXPECT_EQ(res, aegis::ParseResult::Success);
    EXPECT_EQ(consumed, full.size());
    EXPECT_EQ(cmd.type(), aegis::CommandType::Set);
    EXPECT_EQ(cmd.arg(0), "foo");
    EXPECT_EQ(cmd.arg(1), "bar");
}

TEST(ProtocolTest, ParseInlineCommand) {
    aegis::RespParser parser;
    std::string input = "PING hello\r\n";

    size_t consumed = 0;
    aegis::Command cmd;
    auto res = parser.parse(input, consumed, cmd);

    EXPECT_EQ(res, aegis::ParseResult::Success);
    EXPECT_EQ(consumed, input.size());
    EXPECT_EQ(cmd.type(), aegis::CommandType::Ping);
    EXPECT_EQ(cmd.arg_count(), 1);
    EXPECT_EQ(cmd.arg(0), "hello");
}

TEST(ProtocolTest, ParsePipelinedCommands) {
    aegis::RespParser parser;
    std::string pipe = "*1\r\n$4\r\nPING\r\n*2\r\n$4\r\nECHO\r\n$2\r\nhi\r\n";

    size_t c1 = 0;
    aegis::Command cmd1;
    auto res1 = parser.parse(pipe, c1, cmd1);
    EXPECT_EQ(res1, aegis::ParseResult::Success);
    EXPECT_EQ(cmd1.type(), aegis::CommandType::Ping);

    std::string_view remaining(pipe.data() + c1, pipe.size() - c1);
    size_t c2 = 0;
    aegis::Command cmd2;
    auto res2 = parser.parse(remaining, c2, cmd2);
    EXPECT_EQ(res2, aegis::ParseResult::Success);
    EXPECT_EQ(cmd2.type(), aegis::CommandType::Echo);
    EXPECT_EQ(cmd2.arg(0), "hi");
    EXPECT_EQ(c1 + c2, pipe.size());
}

TEST(ProtocolTest, SerializerOutputs) {
    EXPECT_EQ(aegis::RespSerializer::ok(), "+OK\r\n");
    EXPECT_EQ(aegis::RespSerializer::pong(), "+PONG\r\n");
    EXPECT_EQ(aegis::RespSerializer::null_bulk(), "$-1\r\n");
    EXPECT_EQ(aegis::RespSerializer::null_array(), "*-1\r\n");
    EXPECT_EQ(aegis::RespSerializer::integer(42), ":42\r\n");
    EXPECT_EQ(aegis::RespSerializer::bulk_string("val"), "$3\r\nval\r\n");
    EXPECT_EQ(aegis::RespSerializer::error("some err"), "-ERR some err\r\n");

    std::vector<std::string> arr = {"apple", "banana"};
    EXPECT_EQ(aegis::RespSerializer::array(arr), "*2\r\n$5\r\napple\r\n$6\r\nbanana\r\n");
}
