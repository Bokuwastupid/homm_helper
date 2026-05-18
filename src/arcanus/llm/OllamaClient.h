#pragma once

#include "arcanus/core/SnapshotStore.h"
#include "arcanus/diagnostics/Diagnostics.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

namespace arcanus {

enum class AIState {
    Idle,
    Requesting,
    Receiving,
    Success,
    ErrorTimeout,
    Error
};

struct OllamaStatus {
    AIState state = AIState::Idle;
    std::string model = "qwen2.5:7b";
    std::string message = "AI idle";
    std::string last_prompt;
    std::string last_response;
};

class OllamaClient {
public:
    explicit OllamaClient(Diagnostics& diagnostics);
    ~OllamaClient();

    void Start();
    void Stop();
    bool Request(std::string prompt, std::string model = "qwen2.5:7b");
    void Reset();
    std::shared_ptr<const OllamaStatus> Snapshot() const;

private:
    void Worker();
    void Publish(OllamaStatus status);
    bool PerformRequest(const std::string& prompt, const std::string& model, std::string& response, std::string& error);

    Diagnostics& diagnostics_;
    SnapshotStore<OllamaStatus> status_;
    std::thread worker_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_requested_ = false;
    bool has_request_ = false;
    std::string queued_prompt_;
    std::string queued_model_ = "qwen2.5:7b";
};

std::string ToString(AIState state);

}
