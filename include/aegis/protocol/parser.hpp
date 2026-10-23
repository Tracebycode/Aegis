#pragma once

#include "aegis/protocol/command.hpp"
#include "aegis/protocol/resp_types.hpp"

#include <cstddef>
#include <string_view>

namespace aegis {

class RespParser {
public:
    RespParser() = default;

    // Parses a single command from input.
    // If a full command is parsed: returns Success, sets bytes_consumed to the number of bytes read.
    // If the input is incomplete: returns NeedMoreData, bytes_consumed is 0.
    // If syntax is malformed: returns Error.
    ParseResult parse(std::string_view input, size_t& bytes_consumed, Command& out_cmd);

private:
    ParseResult parse_resp_array(std::string_view input, size_t& bytes_consumed, Command& out_cmd);
    ParseResult parse_inline(std::string_view input, size_t& bytes_consumed, Command& out_cmd);
};

} // namespace aegis
