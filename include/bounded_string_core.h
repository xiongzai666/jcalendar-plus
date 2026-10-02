#pragma once
#include <cstddef>
#include <cstring>
#include <memory>
#include <new>

// Preferences' String overload allocates a variable-sized array on the task
// stack. Its buffer overload lets configuration size affect heap use instead.
template<class Reader, class Use>
bool readBoundedString(size_t maximumBytes, const Reader& reader, const Use& use) {
    if (!maximumBytes || maximumBytes == size_t(-1)) return false;
    const size_t capacity = maximumBytes + 1;
    std::unique_ptr<char[]> buffer(new(std::nothrow) char[capacity]);
    if (!buffer) return false;
    const size_t length = reader(buffer.get(), capacity); // Includes terminating NUL.
    if (!length || length > capacity || buffer[length-1] != '\0' ||
        memchr(buffer.get(), '\0', length-1)) return false;
    use(buffer.get(), length-1);
    return true;
}
