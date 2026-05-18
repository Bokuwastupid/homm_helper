#pragma once

#include <filesystem>

namespace arcanus {

struct RuntimePaths {
    std::filesystem::path process_exe;
    std::filesystem::path process_dir;
    std::filesystem::path module_path;
    std::filesystem::path module_dir;
    std::filesystem::path game_data_dir;
    std::filesystem::path streaming_assets_dir;
    std::filesystem::path core_zip;
    std::filesystem::path local_data_dir;
    std::filesystem::path cache_dir;
    std::filesystem::path logs_dir;
};

RuntimePaths ResolveRuntimePaths();
std::filesystem::path CurrentModulePath();

}

