#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace aegis {

class RespSerializer {
public:
    static std::string ok();
    static std::string pong();
    static std::string null_bulk();
    static std::string null_array();

    static std::string simple_string(std::string_view str);
    static std::string error(std::string_view err_msg);
    static std::string integer(int64_t val);
    static std::string bulk_string(std::string_view str);
    static std::string array(std::span<const std::string> items);
    static std::string array(const std::vector<std::string>& items);
    static std::string empty_array();
};

} // namespace aegis
