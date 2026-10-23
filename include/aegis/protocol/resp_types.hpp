#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace aegis {

enum class RespType : char {
    SimpleString = '+',
    Error = '-',
    Integer = ':',
    BulkString = '$',
    Array = '*',
    Null = '_'
};

enum class ParseResult {
    Success,
    NeedMoreData,
    Error
};

} // namespace aegis
