#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace arcanus {

enum class LogLevel {
    Info,
    Warning,
    Error
};

struct LogEntry {
    LogLevel level = LogLevel::Info;
    std::chrono::system_clock::time_point timestamp;
    std::string message;
};

class Diagnostics {
public:
    void Info(std::string message);
    void Warn(std::string message);
    void Error(std::string message);

    std::vector<LogEntry> Snapshot() const;
std::string HookStatus() const;
void SetHookStatus(std::string status);
void SetScannerLatencyMs(double value);
double ScannerLatencyMs() const;
void SetGameAssemblyBase(std::uintptr_t value);
std::uintptr_t GameAssemblyBase() const;

private:
    void Push(LogLevel level, std::string message);

    mutable std::mutex mutex_;
    std::vector<LogEntry> logs_;
    std::string hook_status_ = "inactive";
    double scanner_latency_ms_ = 0.0;
    std::uintptr_t game_assembly_base_ = 0;
};

std::string ToString(LogLevel level);

}
