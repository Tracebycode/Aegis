#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace aegis {

enum class CommandType {
    Unknown = 0,
    Ping,
    Echo,
    Set,
    Get,
    Del,
    Exists,
    Expire,
    Pexpire,
    Ttl,
    Pttl,
    Persist,
    DbSize,
    FlushAll,
    Keys,
    Save,
    BgSave,
    Info,
    Command,
    Quit,
    Shutdown
};

class Command {
public:
    Command() : type_(CommandType::Unknown) {}
    explicit Command(std::vector<std::string> args);

    [[nodiscard]] CommandType type() const noexcept { return type_; }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] const std::vector<std::string>& args() const noexcept { return args_; }

    [[nodiscard]] size_t arg_count() const noexcept {
        return args_.empty() ? 0 : args_.size() - 1;
    }

    [[nodiscard]] const std::string& arg(size_t index) const {
        return args_.at(index + 1);
    }

    [[nodiscard]] bool empty() const noexcept { return args_.empty(); }

    static CommandType string_to_type(std::string_view name) noexcept;
    static std::string_view type_to_string(CommandType type) noexcept;

private:
    std::string name_;
    CommandType type_;
    std::vector<std::string> args_;
};

} // namespace aegis
