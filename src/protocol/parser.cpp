#include "aegis/protocol/parser.hpp"

#include <charconv>
#include <optional>
#include <vector>

namespace aegis {

namespace {

std::optional<size_t> find_crlf(std::string_view sv, size_t start = 0) {
    size_t pos = sv.find("\r\n", start);
    if (pos == std::string_view::npos) {
        return std::nullopt;
    }
    return pos;
}

std::optional<int64_t> parse_int(std::string_view sv) {
    int64_t val = 0;
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), val);
    if (ec != std::errc() || ptr != sv.data() + sv.size()) {
        return std::nullopt;
    }
    return val;
}

} // namespace

ParseResult RespParser::parse(std::string_view input, size_t& bytes_consumed, Command& out_cmd) {
    bytes_consumed = 0;
    if (input.empty()) {
        return ParseResult::NeedMoreData;
    }

    if (input.front() == '*') {
        return parse_resp_array(input, bytes_consumed, out_cmd);
    }

    return parse_inline(input, bytes_consumed, out_cmd);
}

ParseResult RespParser::parse_resp_array(std::string_view input, size_t& bytes_consumed, Command& out_cmd) {
    // Array format: *<count>\r\n$<len>\r\n<arg>\r\n...
    auto crlf_pos = find_crlf(input, 1);
    if (!crlf_pos.has_value()) {
        return ParseResult::NeedMoreData;
    }

    std::string_view count_sv = input.substr(1, *crlf_pos - 1);
    auto num_elements_opt = parse_int(count_sv);
    if (!num_elements_opt.has_value()) {
        return ParseResult::Error;
    }

    int64_t num_elements = *num_elements_opt;
    if (num_elements <= 0) {
        bytes_consumed = *crlf_pos + 2;
        out_cmd = Command(std::vector<std::string>{});
        return ParseResult::Success;
    }

    size_t offset = *crlf_pos + 2;
    std::vector<std::string> args;
    args.reserve(static_cast<size_t>(num_elements));

    for (int64_t i = 0; i < num_elements; ++i) {
        if (offset >= input.size()) {
            return ParseResult::NeedMoreData;
        }

        if (input[offset] != '$') {
            return ParseResult::Error;
        }

        auto bulk_crlf = find_crlf(input, offset + 1);
        if (!bulk_crlf.has_value()) {
            return ParseResult::NeedMoreData;
        }

        std::string_view len_sv = input.substr(offset + 1, *bulk_crlf - (offset + 1));
        auto len_opt = parse_int(len_sv);
        if (!len_opt.has_value()) {
            return ParseResult::Error;
        }

        int64_t bulk_len = *len_opt;
        if (bulk_len < -1) {
            return ParseResult::Error;
        }

        size_t data_start = *bulk_crlf + 2;
        if (bulk_len == -1) {
            args.emplace_back("");
            offset = data_start;
            continue;
        }

        size_t total_needed = data_start + static_cast<size_t>(bulk_len) + 2;
        if (input.size() < total_needed) {
            return ParseResult::NeedMoreData;
        }

        // Verify \r\n after bulk data
        if (input[data_start + static_cast<size_t>(bulk_len)] != '\r' ||
            input[data_start + static_cast<size_t>(bulk_len) + 1] != '\n') {
            return ParseResult::Error;
        }

        args.emplace_back(input.substr(data_start, static_cast<size_t>(bulk_len)));
        offset = total_needed;
    }

    bytes_consumed = offset;
    out_cmd = Command(std::move(args));
    return ParseResult::Success;
}

ParseResult RespParser::parse_inline(std::string_view input, size_t& bytes_consumed, Command& out_cmd) {
    auto crlf_pos = find_crlf(input);
    if (!crlf_pos.has_value()) {
        return ParseResult::NeedMoreData;
    }

    std::string_view line = input.substr(0, *crlf_pos);
    std::vector<std::string> args;

    size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
            ++i;
        }
        if (i >= line.size()) break;

        size_t start = i;
        while (i < line.size() && line[i] != ' ' && line[i] != '\t') {
            ++i;
        }
        args.emplace_back(line.substr(start, i - start));
    }

    bytes_consumed = *crlf_pos + 2;
    if (args.empty()) {
        out_cmd = Command(std::vector<std::string>{});
    } else {
        out_cmd = Command(std::move(args));
    }
    return ParseResult::Success;
}

} // namespace aegis
