#pragma once

#include <Windows.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace arcanus {

class SafeMemory {
public:
    template <typename T>
    static std::optional<T> Read(std::uintptr_t address) {
        T value{};
        if (!ReadBytes(address, &value, sizeof(T))) {
            return std::nullopt;
        }
        return value;
    }

    static std::optional<std::uintptr_t> ReadPtr(std::uintptr_t address);
    static std::optional<std::vector<std::byte>> ReadBuffer(std::uintptr_t address, std::size_t size);
    static std::optional<std::string> ReadIl2CppString(std::uintptr_t string_address, std::size_t max_chars = 4096);
    static std::optional<int> ReadIl2CppArrayLength(std::uintptr_t array_address, int max_length = 4096);
    static std::optional<int> ReadIl2CppListSize(std::uintptr_t list_address, int max_size = 4096);
    static std::optional<std::uintptr_t> ReadIl2CppListItems(std::uintptr_t list_address);
    static std::optional<std::vector<int>> ReadIl2CppIntArray(std::uintptr_t array_address, int max_items = 4096);
    static std::optional<std::vector<int>> ReadIl2CppIntList(std::uintptr_t list_address, int max_items = 64);
    static std::optional<std::vector<std::uintptr_t>> ReadIl2CppPtrList(std::uintptr_t list_address, int max_items = 256);
    static bool ReadBytes(std::uintptr_t address, void* out, std::size_t size);
    static bool IsProbablyUserPointer(std::uintptr_t address);
};

}
