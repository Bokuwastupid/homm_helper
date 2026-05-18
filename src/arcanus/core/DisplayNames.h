#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace arcanus::display {

std::string ShortTokenName(std::string_view token, std::size_t max_len = 18);
std::string ShortObjectName(std::string_view kind, std::string_view type, std::size_t max_len = 18);
std::string ShortRewardName(std::string_view token, std::size_t max_len = 22);
std::string ResourceName(std::string_view token);
bool IsResourceToken(std::string_view token);

}
