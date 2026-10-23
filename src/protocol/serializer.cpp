#include "aegis/protocol/serializer.hpp"

namespace aegis {

std::string RespSerializer::ok() {
    return "+OK\r\n";
}

std::string RespSerializer::pong() {
    return "+PONG\r\n";
}

std::string RespSerializer::null_bulk() {
    return "$-1\r\n";
}

std::string RespSerializer::null_array() {
    return "*-1\r\n";
}

std::string RespSerializer::empty_array() {
    return "*0\r\n";
}

std::string RespSerializer::simple_string(std::string_view str) {
    std::string out;
    out.reserve(str.size() + 3);
    out.push_back('+');
    out.append(str);
    out.append("\r\n");
    return out;
}

std::string RespSerializer::error(std::string_view err_msg) {
    std::string out;
    out.reserve(err_msg.size() + 8);
    out.append("-ERR ");
    out.append(err_msg);
    out.append("\r\n");
    return out;
}

std::string RespSerializer::integer(int64_t val) {
    return ":" + std::to_string(val) + "\r\n";
}

std::string RespSerializer::bulk_string(std::string_view str) {
    std::string out;
    std::string len_str = std::to_string(str.size());
    out.reserve(1 + len_str.size() + 2 + str.size() + 2);
    out.push_back('$');
    out.append(len_str);
    out.append("\r\n");
    out.append(str);
    out.append("\r\n");
    return out;
}

std::string RespSerializer::array(std::span<const std::string> items) {
    std::string out;
    out.push_back('*');
    out.append(std::to_string(items.size()));
    out.append("\r\n");

    for (const auto& item : items) {
        out.append(bulk_string(item));
    }
    return out;
}

std::string RespSerializer::array(const std::vector<std::string>& items) {
    return array(std::span<const std::string>(items.data(), items.size()));
}

} // namespace aegis
