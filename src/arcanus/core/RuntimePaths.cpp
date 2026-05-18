#include "arcanus/core/RuntimePaths.h"

#include <Windows.h>

#include <array>

namespace arcanus {
namespace {

std::filesystem::path ModulePathFromAddress() {
    HMODULE module = nullptr;
    const auto flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
    if (GetModuleHandleExW(flags, reinterpret_cast<LPCWSTR>(&ModulePathFromAddress), &module) == FALSE) {
        return {};
    }

    std::array<wchar_t, MAX_PATH> path{};
    const auto length = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0) {
        return {};
    }
    return std::filesystem::path(std::wstring(path.data(), length));
}

std::filesystem::path ProcessPath() {
    std::array<wchar_t, MAX_PATH> path{};
    const auto length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0) {
        return {};
    }
    return std::filesystem::path(std::wstring(path.data(), length));
}

}

RuntimePaths ResolveRuntimePaths() {
    RuntimePaths paths;
    paths.process_exe = ProcessPath();
    paths.process_dir = paths.process_exe.parent_path();
    paths.module_path = ModulePathFromAddress();
    paths.module_dir = paths.module_path.parent_path();
    paths.game_data_dir = paths.process_dir / L"HeroesOldenEra_Data";
    paths.streaming_assets_dir = paths.game_data_dir / L"StreamingAssets";
    paths.core_zip = paths.streaming_assets_dir / L"Core.zip";
    paths.local_data_dir = paths.module_dir / L"arcanus_data";
    paths.cache_dir = paths.local_data_dir / L"cache";
    paths.logs_dir = paths.local_data_dir / L"logs";
    return paths;
}

std::filesystem::path CurrentModulePath() {
    return ModulePathFromAddress();
}

}

