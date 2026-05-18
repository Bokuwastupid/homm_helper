#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace arcanus {

std::string WideToUtf8(std::wstring_view value);
std::wstring Utf8ToWide(std::string_view value);
std::string WidePtrToUtf8(const wchar_t* value, std::size_t length);

}

