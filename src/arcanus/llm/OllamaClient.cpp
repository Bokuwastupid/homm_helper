#include "arcanus/llm/OllamaClient.h"

#include "arcanus/core/Utf.h"

#include <Windows.h>

#include <chrono>
#include <filesystem>
#include <thread>

#ifdef ARCANUS_WITH_OLLAMA
#include <httplib.h>
#include <nlohmann/json.hpp>
#endif

namespace arcanus {
namespace {

std::filesystem::path FindOllamaExe() {
    wchar_t* local_app_data = nullptr;
    std::filesystem::path result;
    if (_wdupenv_s(&local_app_data, nullptr, L"LOCALAPPDATA") == 0 && local_app_data != nullptr) {
        result = std::filesystem::path(local_app_data) / L"Programs" / L"Ollama" / L"ollama.exe";
        free(local_app_data);
        if (std::filesystem::exists(result)) {
            return result;
        }
    }
    return {};
}

bool IsOllamaAlive() {
#ifndef ARCANUS_WITH_OLLAMA
    return false;
#else
    httplib::Client client("127.0.0.1", 11434);
    client.set_connection_timeout(1, 0);
    client.set_read_timeout(2, 0);
    auto result = client.Get("/api/version");
    return result && result->status >= 200 && result->status < 300;
#endif
}

bool StartOllamaServer() {
    const auto exe = FindOllamaExe();
    if (exe.empty()) {
        return false;
    }

    std::wstring command = L"\"" + exe.wstring() + L"\" serve";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION process{};
    std::wstring mutable_command = command;
    const BOOL ok = CreateProcessW(
        nullptr,
        mutable_command.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        exe.parent_path().c_str(),
        &startup,
        &process);

    if (ok == FALSE) {
        return false;
    }

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);

    for (int i = 0; i < 20; ++i) {
        if (IsOllamaAlive()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    return false;
}

}

OllamaClient::OllamaClient(Diagnostics& diagnostics)
    : diagnostics_(diagnostics) {}

OllamaClient::~OllamaClient() {
    Stop();
}

void OllamaClient::Start() {
    Stop();
    {
        std::scoped_lock lock(mutex_);
        stop_requested_ = false;
    }
    worker_ = std::thread(&OllamaClient::Worker, this);
}

void OllamaClient::Stop() {
    {
        std::scoped_lock lock(mutex_);
        stop_requested_ = true;
        has_request_ = false;
    }
    cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

bool OllamaClient::Request(std::string prompt, std::string model) {
#if !defined(ARCANUS_INPROCESS_OLLAMA)
    (void)prompt;
    (void)model;
    OllamaStatus status;
    status.state = AIState::Error;
    status.model = "external-bridge-required";
    status.message = "Ollama отключена внутри DLL: нужен внешний arcanus_ai_bridge.exe";
    Publish(status);
    diagnostics_.Warn(status.message);
    return false;
#else
    {
        std::scoped_lock lock(mutex_);
        if (has_request_) {
            return false;
        }
        queued_prompt_ = std::move(prompt);
        queued_model_ = std::move(model);
        has_request_ = true;
    }
    cv_.notify_one();
    return true;
#endif
}

void OllamaClient::Reset() {
    OllamaStatus status;
    status.state = AIState::Idle;
    status.message = "AI idle";
    Publish(status);
}

std::shared_ptr<const OllamaStatus> OllamaClient::Snapshot() const {
    return status_.Load();
}

void OllamaClient::Worker() {
    while (true) {
        std::string prompt;
        std::string model;
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [&] { return stop_requested_ || has_request_; });
            if (stop_requested_) {
                return;
            }
            prompt = std::move(queued_prompt_);
            model = std::move(queued_model_);
            has_request_ = false;
        }

        OllamaStatus status;
        status.state = AIState::Requesting;
        status.model = model;
        status.message = "Ollama: запрос отправлен";
        status.last_prompt = prompt;
        Publish(status);

        std::string response;
        std::string error;
        status.state = AIState::Receiving;
        status.message = "Ollama: модель думает...";
        Publish(status);

        if (PerformRequest(prompt, model, response, error)) {
            status.state = AIState::Success;
            status.message = "Ollama: ответ готов";
            status.last_response = response;
            Publish(status);
        } else {
            status.state = error == "timeout" ? AIState::ErrorTimeout : AIState::Error;
            status.message = "Ollama: " + error;
            Publish(status);
            diagnostics_.Warn(status.message);
        }
    }
}

void OllamaClient::Publish(OllamaStatus status) {
    status_.Publish(std::move(status));
}

bool OllamaClient::PerformRequest(const std::string& prompt, const std::string& model, std::string& response, std::string& error) {
#if !defined(ARCANUS_WITH_OLLAMA) || !defined(ARCANUS_INPROCESS_OLLAMA)
    (void)prompt;
    (void)model;
    (void)response;
    error = "disabled at build time";
    return false;
#else
    try {
        if (!IsOllamaAlive()) {
            if (!StartOllamaServer()) {
                error = "Ollama не запущена: не удалось поднять localhost:11434";
                return false;
            }
        }

        httplib::Client client("127.0.0.1", 11434);
        client.set_connection_timeout(2, 0);
        client.set_read_timeout(120, 0);
        client.set_write_timeout(5, 0);

        nlohmann::json body = {
            {"model", model},
            {"stream", false},
            {"max_tokens", 700},
            {"temperature", 0.2},
            {"messages", nlohmann::json::array({
                {
                    {"role", "system"},
                    {"content", "Ты ARCANUS, краткий тактический помощник Heroes of Might and Magic: Olden Era. Отвечай по-русски, без воды, только полезные действия."}
                },
                {
                    {"role", "user"},
                    {"content", prompt}
                }
            })}
        };

        auto result = client.Post("/v1/chat/completions", body.dump(), "application/json");
        if (!result) {
            error = "нет соединения с Ollama или истёк timeout";
            return false;
        }

        if (result->status < 200 || result->status >= 300) {
            error = "HTTP " + std::to_string(result->status);
            return false;
        }

        auto json = nlohmann::json::parse(result->body, nullptr, false);
        if (json.is_discarded()) {
            error = "bad JSON response";
            return false;
        }

        const auto& choices = json["choices"];
        if (!choices.is_array() || choices.empty()) {
            error = "empty choices";
            return false;
        }
        response = choices[0].value("message", nlohmann::json::object()).value("content", "");
        if (response.empty()) {
            error = "empty response";
            return false;
        }
        return true;
    } catch (...) {
        error = "request exception";
        return false;
    }
#endif
}

std::string ToString(AIState state) {
    switch (state) {
    case AIState::Requesting: return "REQUESTING";
    case AIState::Receiving: return "RECEIVING";
    case AIState::Success: return "SUCCESS";
    case AIState::ErrorTimeout: return "ERROR_TIMEOUT";
    case AIState::Error: return "ERROR";
    default: return "IDLE";
    }
}

}
