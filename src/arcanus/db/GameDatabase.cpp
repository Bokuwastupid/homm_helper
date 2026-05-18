#include "arcanus/db/GameDatabase.h"

#include "arcanus/core/Utf.h"

#include <algorithm>
#include <filesystem>
#include <map>

#ifdef ARCANUS_WITH_GAME_DB
#include <miniz.h>
#include <nlohmann/json.hpp>
#endif

namespace arcanus {
namespace {

std::string PathToUtf8(const std::filesystem::path& path) {
    return WideToUtf8(path.wstring());
}

std::string TopLevelTableName(const std::string& name) {
    constexpr std::string_view prefix = "DB/";
    if (!name.starts_with(prefix)) {
        return "other";
    }
    const auto rest = name.substr(prefix.size());
    const auto slash = rest.find('/');
    if (slash == std::string::npos) {
        const auto dot = rest.find('.');
        return dot == std::string::npos ? rest : rest.substr(0, dot);
    }
    return rest.substr(0, slash);
}

bool IsInterestingJson(const std::string& name) {
    return name.starts_with("DB/fractions/") ||
           name.starts_with("DB/fractions_laws/") ||
           name.starts_with("DB/magics/") ||
           name.starts_with("DB/buffs/") ||
           name.starts_with("DB/buildings_") ||
           name.starts_with("DB/heroes") ||
           name.starts_with("DB/ai_battle/") ||
           name.starts_with("DB/arenas/");
}

}

GameDatabase::GameDatabase(Diagnostics& diagnostics)
    : diagnostics_(diagnostics) {}

GameDatabase::~GameDatabase() {
    Stop();
}

void GameDatabase::Start(const RuntimePaths& paths) {
    Stop();
    stop_requested_ = false;
    worker_ = std::thread(&GameDatabase::Worker, this, paths);
}

void GameDatabase::Stop() {
    stop_requested_ = true;
    if (worker_.joinable()) {
        worker_.join();
    }
}

std::shared_ptr<const DatabaseStatus> GameDatabase::Snapshot() const {
    return status_.Load();
}

void GameDatabase::Worker(RuntimePaths paths) {
    DatabaseStatus status;
    status.state = DatabaseState::Indexing;
    status.core_zip_path = PathToUtf8(paths.core_zip);
    status.message = "[Database] Индексация игровых файлов...";
    Publish(status);

#ifndef ARCANUS_WITH_GAME_DB
    status.state = DatabaseState::Error;
    status.message = "Game database disabled at build time";
    Publish(status);
    return;
#else
    if (!std::filesystem::exists(paths.core_zip)) {
        status.state = DatabaseState::Error;
        status.message = "Core.zip not found";
        Publish(status);
        diagnostics_.Warn("Core.zip not found: " + status.core_zip_path);
        return;
    }

    mz_zip_archive zip{};
    const auto zip_path = PathToUtf8(paths.core_zip);
    if (!mz_zip_reader_init_file(&zip, zip_path.c_str(), 0)) {
        status.state = DatabaseState::Error;
        status.message = "Cannot open Core.zip";
        Publish(status);
        diagnostics_.Error("Cannot open Core.zip");
        return;
    }

    std::map<std::string, int> table_counts;
    status.total_entries = static_cast<int>(mz_zip_reader_get_num_files(&zip));

    for (mz_uint i = 0; i < mz_zip_reader_get_num_files(&zip) && !stop_requested_; ++i) {
        mz_zip_archive_file_stat file_stat{};
        if (!mz_zip_reader_file_stat(&zip, i, &file_stat)) {
            continue;
        }

        std::string name = file_stat.m_filename ? file_stat.m_filename : "";
        if (!name.ends_with(".json")) {
            continue;
        }

        ++status.json_entries;
        ++table_counts[TopLevelTableName(name)];

        if (IsInterestingJson(name)) {
            size_t size = 0;
            void* data = mz_zip_reader_extract_to_heap(&zip, i, &size, 0);
            if (data != nullptr) {
                try {
                    const std::string content(static_cast<const char*>(data), size);
                    (void)nlohmann::json::parse(content, nullptr, false);
                    ++status.indexed_entries;
                } catch (...) {
                    diagnostics_.Warn("JSON parse exception in " + name);
                }
                mz_free(data);
            }
        }

        if ((status.json_entries % 50) == 0) {
            status.message = "[Database] Индексация: " + std::to_string(status.json_entries) + " JSON";
            Publish(status);
        }
    }

    mz_zip_reader_end(&zip);

    status.tables.clear();
    for (const auto& [name, files] : table_counts) {
        status.tables.push_back({name, files});
    }
    std::ranges::sort(status.tables, [](const auto& a, const auto& b) {
        return a.files > b.files;
    });

    status.state = stop_requested_ ? DatabaseState::Idle : DatabaseState::Ready;
    status.message = stop_requested_ ? "Database indexing stopped" : "[Database] Индекс готов";
    Publish(status);
    diagnostics_.Info(status.message);
#endif
}

void GameDatabase::Publish(DatabaseStatus status) {
    status_.Publish(std::move(status));
}

std::string ToString(DatabaseState state) {
    switch (state) {
    case DatabaseState::Indexing: return "INDEXING";
    case DatabaseState::Ready: return "READY";
    case DatabaseState::Error: return "ERROR";
    default: return "IDLE";
    }
}

}

