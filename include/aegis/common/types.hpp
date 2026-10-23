#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace aegis {

enum class StatusCode {
    Ok = 0,
    NotFound,
    KeyExpired,
    InvalidArgument,
    TypeMismatch,
    IoError,
    SyntaxError,
    InternalError
};

class Status {
public:
    Status() : code_(StatusCode::Ok) {}
    explicit Status(StatusCode code, std::string message = "")
        : code_(code), message_(std::move(message)) {}

    [[nodiscard]] bool is_ok() const noexcept { return code_ == StatusCode::Ok; }
    [[nodiscard]] StatusCode code() const noexcept { return code_; }
    [[nodiscard]] const std::string& message() const noexcept { return message_; }

    static Status ok() { return Status(StatusCode::Ok); }
    static Status not_found(std::string msg = "Key not found") {
        return Status(StatusCode::NotFound, std::move(msg));
    }
    static Status key_expired(std::string msg = "Key expired") {
        return Status(StatusCode::KeyExpired, std::move(msg));
    }
    static Status invalid_argument(std::string msg) {
        return Status(StatusCode::InvalidArgument, std::move(msg));
    }
    static Status syntax_error(std::string msg = "Syntax error") {
        return Status(StatusCode::SyntaxError, std::move(msg));
    }
    static Status io_error(std::string msg) {
        return Status(StatusCode::IoError, std::move(msg));
    }
    static Status internal_error(std::string msg) {
        return Status(StatusCode::InternalError, std::move(msg));
    }

private:
    StatusCode code_;
    std::string message_;
};

template <typename T>
class Result {
public:
    Result(T val) : storage_(std::move(val)) {}
    Result(Status status) : storage_(std::move(status)) {}

    [[nodiscard]] bool is_ok() const noexcept {
        return std::holds_alternative<T>(storage_);
    }

    [[nodiscard]] explicit operator bool() const noexcept { return is_ok(); }

    [[nodiscard]] const T& value() const & {
        return std::get<T>(storage_);
    }

    [[nodiscard]] T& value() & {
        return std::get<T>(storage_);
    }

    [[nodiscard]] T&& value() && {
        return std::get<T>(std::move(storage_));
    }

    [[nodiscard]] const Status& status() const noexcept {
        if (is_ok()) {
            static const Status ok_status = Status::ok();
            return ok_status;
        }
        return std::get<Status>(storage_);
    }

private:
    std::variant<T, Status> storage_;
};

} // namespace aegis
