#pragma once

#include "arcanus/core/GameState.h"
#include "arcanus/diagnostics/Diagnostics.h"

#include <Windows.h>

#include <cstdint>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace arcanus {

struct AobByte {
    std::optional<std::uint8_t> value;
};

struct PointerCache {
    DWORD process_id = 0;
    std::uintptr_t game_assembly_base = 0;
    std::uintptr_t data = 0;
    std::uintptr_t map_data = 0;
};

class ReadOnlyScanner {
public:
    explicit ReadOnlyScanner(Diagnostics& diagnostics);

    GameState Capture();
    std::optional<std::uintptr_t> FindPattern(HMODULE module, const std::string& pattern) const;

private:
    std::optional<GameState> CaptureFromData(std::uintptr_t data_ptr) const;
    std::optional<std::uintptr_t> ResolveGameAssemblyBase() const;
    std::optional<std::uintptr_t> FindDataByHeapScan() const;
    std::optional<std::uintptr_t> FindMapDataByHeapScan() const;
    std::optional<PointerCache> LoadPointerCache() const;
    void SavePointerCache() const;
    bool ValidateDataCandidate(std::uintptr_t data_ptr) const;
    bool ValidateStableDataCandidate(std::uintptr_t data_ptr) const;
    bool ValidateMapDataCandidate(std::uintptr_t map_data_ptr) const;
    bool ValidateReadableRegion(const MEMORY_BASIC_INFORMATION& mbi) const;
    GameState CaptureMockState() const;
    bool Reject(std::string reason) const;
    static std::vector<AobByte> ParsePattern(const std::string& pattern);
    static std::optional<std::pair<std::uintptr_t, std::size_t>> ModuleRange(HMODULE module);

    Diagnostics& diagnostics_;
    mutable std::uintptr_t game_assembly_base_ = 0;
    mutable std::uintptr_t data_ptr_ = 0;
    mutable std::uintptr_t map_data_ptr_ = 0;
    mutable std::chrono::steady_clock::time_point last_heap_scan_{};
    mutable std::chrono::steady_clock::time_point last_map_heap_scan_{};
    mutable ScannerDebugState debug_{};
    mutable std::string data_source_ = "none";
    mutable std::string last_reject_reason_ = "not scanned";
};

}
