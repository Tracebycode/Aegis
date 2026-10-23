#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>
#include <vector>

namespace aegis {

class ByteBuffer {
public:
    explicit ByteBuffer(size_t initial_capacity = 4096)
        : buffer_(initial_capacity), read_pos_(0), write_pos_(0) {}

    [[nodiscard]] size_t readable_bytes() const noexcept {
        return write_pos_ - read_pos_;
    }

    [[nodiscard]] size_t writable_bytes() const noexcept {
        return buffer_.size() - write_pos_;
    }

    [[nodiscard]] size_t capacity() const noexcept {
        return buffer_.size();
    }

    [[nodiscard]] bool empty() const noexcept {
        return read_pos_ == write_pos_;
    }

    [[nodiscard]] const char* read_ptr() const noexcept {
        return reinterpret_cast<const char*>(buffer_.data() + read_pos_);
    }

    [[nodiscard]] char* write_ptr() noexcept {
        return reinterpret_cast<char*>(buffer_.data() + write_pos_);
    }

    [[nodiscard]] std::string_view read_view() const noexcept {
        return std::string_view(read_ptr(), readable_bytes());
    }

    void advance_read(size_t bytes) noexcept {
        read_pos_ = std::min(read_pos_ + bytes, write_pos_);
        if (read_pos_ == write_pos_) {
            reset();
        }
    }

    void advance_write(size_t bytes) noexcept {
        write_pos_ = std::min(write_pos_ + bytes, buffer_.size());
    }

    void ensure_writable(size_t bytes) {
        if (writable_bytes() >= bytes) {
            return;
        }

        // Try compacting first
        if (read_pos_ > 0) {
            compact();
            if (writable_bytes() >= bytes) {
                return;
            }
        }

        // Expand buffer
        size_t new_cap = std::max(buffer_.size() * 2, write_pos_ + bytes);
        buffer_.resize(new_cap);
    }

    void write(const void* data, size_t bytes) {
        if (bytes == 0) return;
        ensure_writable(bytes);
        std::memcpy(buffer_.data() + write_pos_, data, bytes);
        write_pos_ += bytes;
    }

    void write(std::string_view str) {
        write(str.data(), str.size());
    }

    void reset() noexcept {
        read_pos_ = 0;
        write_pos_ = 0;
    }

    void compact() noexcept {
        if (read_pos_ == 0) return;
        size_t bytes = readable_bytes();
        if (bytes > 0) {
            std::memmove(buffer_.data(), buffer_.data() + read_pos_, bytes);
        }
        read_pos_ = 0;
        write_pos_ = bytes;
    }

private:
    std::vector<uint8_t> buffer_;
    size_t read_pos_{0};
    size_t write_pos_{0};
};

} // namespace aegis
