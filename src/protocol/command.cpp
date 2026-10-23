#include "aegis/protocol/command.hpp"

#include <algorithm>
#include <cctype>

namespace aegis {

namespace {

std::string to_upper(std::string_view str) {
    std::string result;
    result.reserve(str.size());
    for (char c : str) {
        result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }
    return result;
}

} // namespace

Command::Command(std::vector<std::string> args)
    : args_(std::move(args)) {
    if (!args_.empty()) {
        name_ = to_upper(args_[0]);
        type_ = string_to_type(name_);
    } else {
        type_ = CommandType::Unknown;
    }
}

CommandType Command::string_to_type(std::string_view name) noexcept {
    std::string upper = to_upper(name);

    if (upper == "PING") return CommandType::Ping;
    if (upper == "ECHO") return CommandType::Echo;
    if (upper == "SET") return CommandType::Set;
    if (upper == "GET") return CommandType::Get;
    if (upper == "DEL") return CommandType::Del;
    if (upper == "EXISTS") return CommandType::Exists;
    if (upper == "EXPIRE") return CommandType::Expire;
    if (upper == "PEXPIRE") return CommandType::Pexpire;
    if (upper == "TTL") return CommandType::Ttl;
    if (upper == "PTTL") return CommandType::Pttl;
    if (upper == "PERSIST") return CommandType::Persist;
    if (upper == "DBSIZE") return CommandType::DbSize;
    if (upper == "FLUSHALL" || upper == "FLUSHDB") return CommandType::FlushAll;
    if (upper == "KEYS") return CommandType::Keys;
    if (upper == "SAVE") return CommandType::Save;
    if (upper == "BGSAVE") return CommandType::BgSave;
    if (upper == "INFO") return CommandType::Info;
    if (upper == "COMMAND") return CommandType::Command;
    if (upper == "QUIT") return CommandType::Quit;
    if (upper == "SHUTDOWN") return CommandType::Shutdown;

    return CommandType::Unknown;
}

std::string_view Command::type_to_string(CommandType type) noexcept {
    switch (type) {
    case CommandType::Ping: return "PING";
    case CommandType::Echo: return "ECHO";
    case CommandType::Set: return "SET";
    case CommandType::Get: return "GET";
    case CommandType::Del: return "DEL";
    case CommandType::Exists: return "EXISTS";
    case CommandType::Expire: return "EXPIRE";
    case CommandType::Pexpire: return "PEXPIRE";
    case CommandType::Ttl: return "TTL";
    case CommandType::Pttl: return "PTTL";
    case CommandType::Persist: return "PERSIST";
    case CommandType::DbSize: return "DBSIZE";
    case CommandType::FlushAll: return "FLUSHALL";
    case CommandType::Keys: return "KEYS";
    case CommandType::Save: return "SAVE";
    case CommandType::BgSave: return "BGSAVE";
    case CommandType::Info: return "INFO";
    case CommandType::Command: return "COMMAND";
    case CommandType::Quit: return "QUIT";
    case CommandType::Shutdown: return "SHUTDOWN";
    default: return "UNKNOWN";
    }
}

} // namespace aegis
