#pragma once

#include "arcanus/core/RuntimePaths.h"
#include "arcanus/core/SnapshotStore.h"
#include "arcanus/diagnostics/Diagnostics.h"

#include <atomic>
#include <string>
#include <thread>
#include <vector>

namespace arcanus {

enum class DatabaseState {
    Idle,
    Indexing,
    Ready,
    Error
};

struct DatabaseTableSummary {
    std::string name;
    int files = 0;
};

struct DatabaseStatus {
    DatabaseState state = DatabaseState::Idle;
    std::string message = "database idle";
    std::string core_zip_path;
    int total_entries = 0;
    int json_entries = 0;
    int indexed_entries = 0;
    std::vector<DatabaseTableSummary> tables;
};

class GameDatabase {
public:
    explicit GameDatabase(Diagnostics& diagnostics);
    ~GameDatabase();

    void Start(const RuntimePaths& paths);
    void Stop();
    std::shared_ptr<const DatabaseStatus> Snapshot() const;

private:
    void Worker(RuntimePaths paths);
    void Publish(DatabaseStatus status);

    Diagnostics& diagnostics_;
    SnapshotStore<DatabaseStatus> status_;
    std::thread worker_;
    std::atomic_bool stop_requested_{false};
};

std::string ToString(DatabaseState state);

}

