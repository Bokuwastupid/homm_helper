#include "arcanus/memory/SafeMemory.h"

#include "arcanus/core/Utf.h"
#include "arcanus/memory/KnownOffsets.h"

#include <algorithm>

namespace arcanus {

std::optional<std::uintptr_t> SafeMemory::ReadPtr(std::uintptr_t address) {
    return Read<std::uintptr_t>(address);
}

std::optional<std::vector<std::byte>> SafeMemory::ReadBuffer(std::uintptr_t address, std::size_t size) {
    if (size == 0 || size > 1024 * 1024) {
        return std::nullopt;
    }

    std::vector<std::byte> buffer(size);
    if (!ReadBytes(address, buffer.data(), buffer.size())) {
        return std::nullopt;
    }
    return buffer;
}

std::optional<std::string> SafeMemory::ReadIl2CppString(std::uintptr_t string_address, std::size_t max_chars) {
    if (!IsProbablyUserPointer(string_address)) {
        return std::nullopt;
    }

    const auto length = Read<std::int32_t>(string_address + offsets::il2cpp::StringLength);
    if (!length.has_value() || *length < 0 || static_cast<std::size_t>(*length) > max_chars) {
        return std::nullopt;
    }

    if (*length == 0) {
        return std::string{};
    }

    std::wstring chars(static_cast<std::size_t>(*length), L'\0');
    if (!ReadBytes(string_address + offsets::il2cpp::StringChars, chars.data(), chars.size() * sizeof(wchar_t))) {
        return std::nullopt;
    }

    return WideToUtf8(chars);
}

std::optional<int> SafeMemory::ReadIl2CppArrayLength(std::uintptr_t array_address, int max_length) {
    if (!IsProbablyUserPointer(array_address)) {
        return std::nullopt;
    }

    const auto length = Read<std::int32_t>(array_address + offsets::il2cpp::ArrayLength);
    if (!length.has_value() || *length < 0 || *length > max_length) {
        return std::nullopt;
    }
    return *length;
}

std::optional<int> SafeMemory::ReadIl2CppListSize(std::uintptr_t list_address, int max_size) {
    if (!IsProbablyUserPointer(list_address)) {
        return std::nullopt;
    }

    const auto size = Read<std::int32_t>(list_address + offsets::il2cpp::ListSize);
    if (!size.has_value() || *size < 0 || *size > max_size) {
        return std::nullopt;
    }
    return *size;
}

std::optional<std::uintptr_t> SafeMemory::ReadIl2CppListItems(std::uintptr_t list_address) {
    if (!IsProbablyUserPointer(list_address)) {
        return std::nullopt;
    }

    const auto items = ReadPtr(list_address + offsets::il2cpp::ListItems);
    if (!items.has_value() || !IsProbablyUserPointer(*items)) {
        return std::nullopt;
    }
    return *items;
}

std::optional<std::vector<int>> SafeMemory::ReadIl2CppIntArray(std::uintptr_t array_address, int max_items) {
    const auto length = ReadIl2CppArrayLength(array_address, max_items);
    if (!length.has_value()) {
        return std::nullopt;
    }

    std::vector<int> values;
    values.reserve(static_cast<std::size_t>(*length));
    for (int index = 0; index < *length; ++index) {
        const auto value = Read<std::int32_t>(array_address + offsets::il2cpp::ArrayItems + static_cast<std::uintptr_t>(index) * sizeof(std::int32_t));
        if (!value.has_value()) {
            return std::nullopt;
        }
        values.push_back(*value);
    }
    return values;
}

std::optional<std::vector<int>> SafeMemory::ReadIl2CppIntList(std::uintptr_t list_address, int max_items) {
    const auto size = ReadIl2CppListSize(list_address, max_items);
    const auto items = ReadIl2CppListItems(list_address);
    if (!size.has_value() || !items.has_value()) {
        return std::nullopt;
    }

    const auto array_length = ReadIl2CppArrayLength(*items, max_items);
    if (!array_length.has_value() || *size > *array_length) {
        return std::nullopt;
    }

    std::vector<int> values;
    values.reserve(static_cast<std::size_t>(*size));
    for (int index = 0; index < *size; ++index) {
        const auto value = Read<std::int32_t>(*items + offsets::il2cpp::ArrayItems + static_cast<std::uintptr_t>(index) * sizeof(std::int32_t));
        if (!value.has_value()) {
            return std::nullopt;
        }
        values.push_back(*value);
    }
    return values;
}

std::optional<std::vector<std::uintptr_t>> SafeMemory::ReadIl2CppPtrList(std::uintptr_t list_address, int max_items) {
    const auto size = ReadIl2CppListSize(list_address, max_items);
    const auto items = ReadIl2CppListItems(list_address);
    if (!size.has_value() || !items.has_value()) {
        return std::nullopt;
    }

    const auto array_length = ReadIl2CppArrayLength(*items, max_items);
    if (!array_length.has_value() || *size > *array_length) {
        return std::nullopt;
    }

    std::vector<std::uintptr_t> values;
    values.reserve(static_cast<std::size_t>(*size));
    for (int index = 0; index < *size; ++index) {
        const auto value = Read<std::uintptr_t>(*items + offsets::il2cpp::ArrayItems + static_cast<std::uintptr_t>(index) * sizeof(std::uintptr_t));
        if (!value.has_value()) {
            return std::nullopt;
        }
        values.push_back(*value);
    }
    return values;
}

bool SafeMemory::ReadBytes(std::uintptr_t address, void* out, std::size_t size) {
    if (address == 0 || out == nullptr || size == 0) {
        return false;
    }

    SIZE_T read = 0;
    if (ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<LPCVOID>(address), out, size, &read) != FALSE && read == size) {
        return true;
    }

    __try {
        std::memcpy(out, reinterpret_cast<const void*>(address), size);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool SafeMemory::IsProbablyUserPointer(std::uintptr_t address) {
    return address >= 0x10000ull && address < 0x0000800000000000ull;
}

}
