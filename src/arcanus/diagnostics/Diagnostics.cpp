#include "arcanus/diagnostics/Diagnostics.h"

#include <utility>

namespace arcanus {

void Diagnostics::Info(std::string message) {
    Push(LogLevel::Info, std::move(message));
}

void Diagnostics::Warn(std::string message) {
    Push(LogLevel::Warning, std::move(message));
}

void Diagnostics::Error(std::string message) {
    Push(LogLevel::Error, std::move(message));
}

std::vector<LogEntry> Diagnostics::Snapshot() const {
    std::scoped_lock lock(mutex_);
    return logs_;
}

std::string Diagnostics::HookStatus() const {
    std::scoped_lock lock(mutex_);
    return hook_status_;
}

void Diagnostics::SetHookStatus(std::string status) {
    std::scoped_lock lock(mutex_);
    hook_status_ = std::move(status);
}

void Diagnostics::SetScannerLatencyMs(double value) {
    std::scoped_lock lock(mutex_);
    scanner_latency_ms_ = value;
}

double Diagnostics::ScannerLatencyMs() const {
    std::scoped_lock lock(mutex_);
    return scanner_latency_ms_;
}

void Diagnostics::SetGameAssemblyBase(std::uintptr_t value) {
    std::scoped_lock lock(mutex_);
    game_assembly_base_ = value;
}

std::uintptr_t Diagnostics::GameAssemblyBase() const {
    std::scoped_lock lock(mutex_);
    return game_assembly_base_;
}

void Diagnostics::Push(LogLevel level, std::string message) {
    std::scoped_lock lock(mutex_);
    logs_.push_back({level, std::chrono::system_clock::now(), std::move(message)});
    if (logs_.size() > 300) {
        logs_.erase(logs_.begin(), logs_.begin() + 50);
    }
}

std::string ToString(LogLevel level) {
    switch (level) {
    case LogLevel::Error: return "ERROR";
    case LogLevel::Warning: return "WARN";
    default: return "INFO";
    }
}

}
