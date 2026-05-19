#include <Windows.h>
#include <Shellapi.h>
#include <TlHelp32.h>
#include <dwmapi.h>
#include <windowsx.h>

#pragma comment(lib, "dwmapi.lib")

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using NtCreateThreadExFn = LONG(NTAPI*)(
    PHANDLE ThreadHandle,
    ACCESS_MASK DesiredAccess,
    PVOID ObjectAttributes,
    HANDLE ProcessHandle,
    PVOID StartRoutine,
    PVOID Argument,
    ULONG CreateFlags,
    SIZE_T ZeroBits,
    SIZE_T StackSize,
    SIZE_T MaximumStackSize,
    PVOID AttributeList);

static constexpr ULONG kNtHideFromDebugger = 0x4;

namespace {

constexpr wchar_t kWindowClass[] = L"ArcanusLoaderWindow";
constexpr wchar_t kTargetProcess[] = L"HeroesOldenEra.exe";
constexpr wchar_t kOverlayDll[] = L"arcanus_overlay.dll";
constexpr wchar_t kSteamRunGameUri[] = L"steam://rungameid/3105440";
constexpr UINT kStatusMessage = WM_APP + 10;
constexpr COLORREF kBg = RGB(10, 10, 10);
constexpr COLORREF kPanel = RGB(26, 26, 26);
constexpr COLORREF kPanelDeep = RGB(18, 18, 18);
constexpr COLORREF kOrange = RGB(255, 140, 0);
constexpr COLORREF kOrangeDim = RGB(170, 92, 18);
constexpr COLORREF kText = RGB(230, 230, 230);
constexpr COLORREF kMuted = RGB(150, 150, 150);
constexpr int kHeaderHeight = 92;
constexpr int kChromeButtonW = 46;
constexpr int kChromeButtonH = 30;
constexpr int kMinWindowWidth = 900;
constexpr int kMinWindowHeight = 620;

enum class LoaderAction {
    StartGame,
    Inject,
    StartAndInject,
    OpenFolder,
    Refresh,
};

struct UiButton {
    RECT rect{};
    std::wstring text;
    LoaderAction action = LoaderAction::Refresh;
};

struct SharedState {
    std::mutex mutex;
    std::wstring status = L"Ready.";
    std::wstring process = L"Game process: not running";
    std::wstring path = L"Game path: searching...";
    std::wstring operation;
    std::wstring patch_notes;
    std::filesystem::path game_path;
    bool busy = false;
    int progress = -1;
};

SharedState g_state;
HFONT g_title_font = nullptr;
HFONT g_header_font = nullptr;
HFONT g_body_font = nullptr;
HFONT g_small_font = nullptr;
int g_hover_button = -1;
int g_hover_chrome = -1; // 0 = minimize, 1 = close
bool g_mouse_tracking = false;
RECT g_btn_min{};
RECT g_btn_close{};

std::filesystem::path ExeDir() {
    std::vector<wchar_t> buffer(MAX_PATH);
    DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
}

std::wstring FormatError(DWORD error) {
    if (error == 0) {
        return L"OK";
    }

    wchar_t* message = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD size = FormatMessageW(flags, nullptr, error, 0, reinterpret_cast<LPWSTR>(&message), 0, nullptr);
    std::wstring result = size > 0 && message != nullptr ? std::wstring(message, size) : L"unknown error";
    if (message != nullptr) {
        LocalFree(message);
    }
    while (!result.empty() && (result.back() == L'\r' || result.back() == L'\n' || result.back() == L' ')) {
        result.pop_back();
    }
    return result;
}

void SetStatus(HWND hwnd, std::wstring status, bool busy, int progress = -1, std::wstring operation = {}) {
    {
        std::lock_guard lock(g_state.mutex);
        g_state.status = std::move(status);
        g_state.busy = busy;
        g_state.progress = progress;
        g_state.operation = std::move(operation);
    }
    PostMessageW(hwnd, kStatusMessage, 0, 0);
}

std::optional<std::filesystem::path> QuerySteamInstallFromRegistry(HKEY root, REGSAM view) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, L"SOFTWARE\\Valve\\Steam", 0, KEY_READ | view, &key) != ERROR_SUCCESS &&
        RegOpenKeyExW(root, L"SOFTWARE\\WOW6432Node\\Valve\\Steam", 0, KEY_READ | view, &key) != ERROR_SUCCESS) {
        return std::nullopt;
    }

    wchar_t value[MAX_PATH]{};
    DWORD type = 0;
    DWORD size = sizeof(value);
    const auto result = RegQueryValueExW(key, L"InstallPath", nullptr, &type, reinterpret_cast<LPBYTE>(value), &size);
    RegCloseKey(key);
    if (result != ERROR_SUCCESS || type != REG_SZ || value[0] == L'\0') {
        return std::nullopt;
    }
    return std::filesystem::path(value);
}

std::wstring UnescapeVdfPath(std::wstring value) {
    std::wstring out;
    out.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == L'\\' && i + 1 < value.size() && value[i + 1] == L'\\') {
            out.push_back(L'\\');
            ++i;
        } else {
            out.push_back(value[i]);
        }
    }
    return out;
}

std::vector<std::filesystem::path> ReadSteamLibraries(const std::filesystem::path& steam_root) {
    std::vector<std::filesystem::path> libraries;
    if (!steam_root.empty()) {
        libraries.push_back(steam_root);
    }

    const auto vdf = steam_root / L"steamapps" / L"libraryfolders.vdf";
    std::wifstream file(vdf);
    if (!file) {
        return libraries;
    }

    std::wstring line;
    while (std::getline(file, line)) {
        if (line.find(L"\"path\"") == std::wstring::npos) {
            continue;
        }

        std::vector<std::wstring> quoted;
        std::size_t pos = 0;
        while ((pos = line.find(L'"', pos)) != std::wstring::npos) {
            const auto end = line.find(L'"', pos + 1);
            if (end == std::wstring::npos) {
                break;
            }
            quoted.push_back(line.substr(pos + 1, end - pos - 1));
            pos = end + 1;
        }

        if (quoted.size() >= 2) {
            libraries.emplace_back(UnescapeVdfPath(quoted[1]));
        }
    }
    return libraries;
}

std::optional<std::filesystem::path> FindGameExe() {
    wchar_t env[MAX_PATH]{};
    const DWORD env_len = GetEnvironmentVariableW(L"ARCANUS_GAME_EXE", env, static_cast<DWORD>(std::size(env)));
    if (env_len > 0 && env_len < std::size(env) && std::filesystem::exists(env)) {
        return std::filesystem::path(env);
    }

    const std::vector<std::filesystem::path> direct_candidates = {
        L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Heroes of Might and Magic Olden Era\\HeroesOldenEra.exe",
        L"C:\\Program Files\\Steam\\steamapps\\common\\Heroes of Might and Magic Olden Era\\HeroesOldenEra.exe",
    };
    for (const auto& candidate : direct_candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    std::vector<std::filesystem::path> steam_roots;
    if (auto root = QuerySteamInstallFromRegistry(HKEY_CURRENT_USER, 0)) {
        steam_roots.push_back(*root);
    }
    if (auto root = QuerySteamInstallFromRegistry(HKEY_CURRENT_USER, KEY_WOW64_32KEY)) {
        steam_roots.push_back(*root);
    }
    if (auto root = QuerySteamInstallFromRegistry(HKEY_LOCAL_MACHINE, KEY_WOW64_32KEY)) {
        steam_roots.push_back(*root);
    }
    if (auto root = QuerySteamInstallFromRegistry(HKEY_LOCAL_MACHINE, 0)) {
        steam_roots.push_back(*root);
    }

    for (const auto& steam_root : steam_roots) {
        for (const auto& library : ReadSteamLibraries(steam_root)) {
            const auto common = library / L"steamapps" / L"common";
            const auto expected = common / L"Heroes of Might and Magic Olden Era" / kTargetProcess;
            if (std::filesystem::exists(expected)) {
                return expected;
            }

            std::error_code ec;
            if (!std::filesystem::exists(common, ec)) {
                continue;
            }
            for (const auto& entry : std::filesystem::directory_iterator(common, ec)) {
                if (ec || !entry.is_directory(ec)) {
                    continue;
                }
                const auto exe = entry.path() / kTargetProcess;
                if (std::filesystem::exists(exe, ec)) {
                    return exe;
                }
            }
        }
    }

    return std::nullopt;
}

DWORD FindProcessId(const wchar_t* process_name) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (!Process32FirstW(snapshot, &entry)) {
        CloseHandle(snapshot);
        return 0;
    }

    do {
        if (_wcsicmp(entry.szExeFile, process_name) == 0) {
            CloseHandle(snapshot);
            return entry.th32ProcessID;
        }
    } while (Process32NextW(snapshot, &entry));

    CloseHandle(snapshot);
    return 0;
}

bool IsOverlayLoaded(DWORD pid) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (!Module32FirstW(snapshot, &entry)) {
        CloseHandle(snapshot);
        return false;
    }

    do {
        if (_wcsicmp(entry.szModule, kOverlayDll) == 0) {
            CloseHandle(snapshot);
            return true;
        }
    } while (Module32NextW(snapshot, &entry));

    CloseHandle(snapshot);
    return false;
}

bool InjectLoadLibrary(DWORD pid, const std::filesystem::path& dll_path, std::wstring& error) {
    const std::wstring dll = dll_path.wstring();
    const auto bytes = (dll.size() + 1) * sizeof(wchar_t);

    // Request minimal permissions — avoid PROCESS_VM_READ and PROCESS_QUERY_INFORMATION
    // which are suspicious flags not needed for injection.
    HANDLE process = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE,
        FALSE, pid);
    if (process == nullptr) {
        error = L"OpenProcess failed: " + FormatError(GetLastError());
        return false;
    }

    void* remote_path = VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (remote_path == nullptr) {
        error = L"VirtualAllocEx failed: " + FormatError(GetLastError());
        CloseHandle(process);
        return false;
    }

    if (WriteProcessMemory(process, remote_path, dll.c_str(), bytes, nullptr) == FALSE) {
        error = L"WriteProcessMemory failed: " + FormatError(GetLastError());
        VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
        CloseHandle(process);
        return false;
    }

    const auto load_library = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));

    HANDLE thread = nullptr;

    // Prefer NtCreateThreadEx with HIDE_FROM_DEBUGGER to avoid hooks on CreateRemoteThread
    const auto nt_create_thread = reinterpret_cast<NtCreateThreadExFn>(
        GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtCreateThreadEx"));
    if (nt_create_thread != nullptr) {
        const LONG status = nt_create_thread(
            &thread, THREAD_ALL_ACCESS, nullptr, process,
            load_library, remote_path,
            kNtHideFromDebugger, 0, 0, 0, nullptr);
        if (status < 0) {
            thread = nullptr;
        }
    }

    if (thread == nullptr) {
        thread = CreateRemoteThread(process, nullptr, 0, load_library, remote_path, 0, nullptr);
    }

    if (thread == nullptr) {
        error = L"Thread creation failed: " + FormatError(GetLastError());
        VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
        CloseHandle(process);
        return false;
    }

    const DWORD wait = WaitForSingleObject(thread, 15000);
    DWORD exit_code = 0;
    GetExitCodeThread(thread, &exit_code);
    CloseHandle(thread);

    // Zero remote memory before freeing — avoids leaving DLL path visible in target process
    std::vector<BYTE> zeros(bytes, 0);
    WriteProcessMemory(process, remote_path, zeros.data(), bytes, nullptr);
    VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
    CloseHandle(process);

    if (wait != WAIT_OBJECT_0) {
        error = L"LoadLibrary timed out";
        return false;
    }
    if (exit_code == 0) {
        error = L"LoadLibraryW returned null. Check DLL dependencies.";
        return false;
    }

    return true;
}

bool LaunchGameFromPath(const std::filesystem::path& game_path, std::wstring& error) {
    std::wstring command = L"\"" + game_path.wstring() + L"\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    const std::wstring cwd = game_path.parent_path().wstring();
    if (CreateProcessW(
            game_path.c_str(),
            command.data(),
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            cwd.c_str(),
            &startup,
            &process) == FALSE) {
        error = L"CreateProcess failed: " + FormatError(GetLastError());
        return false;
    }

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
}

bool LaunchGame(std::wstring& error) {
    const auto existing = FindProcessId(kTargetProcess);
    if (existing != 0) {
        return true;
    }

    std::filesystem::path game_path;
    {
        std::lock_guard lock(g_state.mutex);
        game_path = g_state.game_path;
    }

    if (!game_path.empty() && std::filesystem::exists(game_path)) {
        return LaunchGameFromPath(game_path, error);
    }

    const auto shell = ShellExecuteW(nullptr, L"open", kSteamRunGameUri, nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(shell) <= 32) {
        error = L"Could not start via Steam URI";
        return false;
    }
    return true;
}

bool InjectOverlay(std::wstring& error) {
    const auto dll_path = ExeDir() / kOverlayDll;
    if (!std::filesystem::exists(dll_path)) {
        error = L"Cannot find " + dll_path.wstring();
        return false;
    }

    const DWORD pid = FindProcessId(kTargetProcess);
    if (pid == 0) {
        error = L"Game process is not running";
        return false;
    }
    if (IsOverlayLoaded(pid)) {
        return true;
    }
    return InjectLoadLibrary(pid, dll_path, error);
}

void RefreshState(HWND hwnd) {
    const DWORD pid = FindProcessId(kTargetProcess);
    auto game_path = FindGameExe();
    {
        std::lock_guard lock(g_state.mutex);
        if (game_path) {
            g_state.game_path = *game_path;
            g_state.path = L"Game path: " + game_path->wstring();
        } else {
            g_state.game_path.clear();
            g_state.path = L"Game path: not found, Steam URI fallback enabled";
        }

        if (pid != 0) {
            g_state.process = L"Game process: PID " + std::to_wstring(pid) +
                (IsOverlayLoaded(pid) ? L" | overlay loaded" : L" | overlay not loaded");
        } else {
            g_state.process = L"Game process: not running";
        }
    }
    InvalidateRect(hwnd, nullptr, FALSE);
}

void RunAction(HWND hwnd, LoaderAction action) {
    SetStatus(hwnd, L"Working...", true, 5, L"Preparing action...");

    std::thread([hwnd, action]() {
        std::wstring error;
        bool ok = false;

        switch (action) {
        case LoaderAction::StartGame:
            SetStatus(hwnd, L"Starting game...", true, 20, L"Launching HeroesOldenEra.exe");
            ok = LaunchGame(error);
            SetStatus(hwnd, ok ? L"Game start requested." : L"Start failed: " + error, false, ok ? 100 : 0, ok ? L"Launch request sent" : L"Launch failed");
            break;
        case LoaderAction::Inject:
            SetStatus(hwnd, L"Injecting overlay...", true, 35, L"Attaching read-only overlay DLL");
            ok = InjectOverlay(error);
            SetStatus(hwnd, ok ? L"Overlay injected. Use F1 / ~ / End hide in game." : L"Inject failed: " + error, false, ok ? 100 : 0, ok ? L"Overlay ready" : L"Injection failed");
            break;
        case LoaderAction::StartAndInject:
            SetStatus(hwnd, L"Starting game...", true, 15, L"Launching game before injection");
            ok = LaunchGame(error);
            if (!ok) {
                SetStatus(hwnd, L"Start failed: " + error, false, 0, L"Launch failed");
                break;
            }
            SetStatus(hwnd, L"Waiting for game process...", true, 35, L"Waiting for HeroesOldenEra.exe");
            for (int i = 0; i < 120 && FindProcessId(kTargetProcess) == 0; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                if (i % 4 == 0) {
                    SetStatus(hwnd, L"Waiting for game process...", true, 35 + std::min(40, i / 3), L"Waiting for HeroesOldenEra.exe");
                }
            }
            if (FindProcessId(kTargetProcess) == 0) {
                SetStatus(hwnd, L"Game did not appear within 60 seconds.", false, 0, L"Timed out");
                break;
            }
            SetStatus(hwnd, L"Game found. Injecting overlay...", true, 82, L"Attaching read-only overlay DLL");
            std::this_thread::sleep_for(std::chrono::seconds(3));
            ok = InjectOverlay(error);
            SetStatus(hwnd, ok ? L"Game running, overlay injected." : L"Inject failed: " + error, false, ok ? 100 : 0, ok ? L"Overlay ready" : L"Injection failed");
            break;
        case LoaderAction::OpenFolder: {
            std::filesystem::path folder;
            {
                std::lock_guard lock(g_state.mutex);
                folder = g_state.game_path.empty() ? ExeDir() : g_state.game_path.parent_path();
            }
            ShellExecuteW(nullptr, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            SetStatus(hwnd, L"Opened folder.", false, 100, L"Explorer opened");
            break;
        }
        case LoaderAction::Refresh:
            SetStatus(hwnd, L"Refreshed.", false, 100, L"Status refreshed");
            break;
        }

        RefreshState(hwnd);
    }).detach();
}

std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    const int required = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (required <= 0) {
        return {};
    }
    std::wstring wide(static_cast<std::size_t>(required), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), required);
    return wide;
}

std::wstring LoadPatchNotes() {
    const auto path = ExeDir() / L"PATCH_NOTES.md";
    std::ifstream file(path, std::ios::binary);
    if (file) {
        std::ostringstream buffer;
        buffer << file.rdbuf();
        if (auto notes = Utf8ToWide(buffer.str()); !notes.empty()) {
            return notes;
        }
    }

    return
        L"ARCANUS Loader\n"
        L"- Start game directly or through Steam URI fallback.\n"
        L"- Inject read-only DX11 ImGui overlay into a running game.\n"
        L"- ESP labels now support Clean and Debug modes.\n"
        L"- Route arrows are disabled by default until real pathfinding lands.\n";
}

void DrawFilledRect(HDC hdc, RECT rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
}

void DrawBorder(HDC hdc, RECT rect, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ old_pen = SelectObject(hdc, pen);
    HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
}

void DrawBorderWide(HDC hdc, RECT rect, COLORREF color, int width) {
    HPEN pen = CreatePen(PS_SOLID, width, color);
    HGDIOBJ old_pen = SelectObject(hdc, pen);
    HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
}

void DrawTriangleEyeIcon(HDC hdc, int x, int y, int size) {
    HPEN pen = CreatePen(PS_SOLID, 2, kOrange);
    HBRUSH brush = static_cast<HBRUSH>(GetStockObject(HOLLOW_BRUSH));
    HGDIOBJ old_pen = SelectObject(hdc, pen);
    HGDIOBJ old_brush = SelectObject(hdc, brush);
    POINT triangle[3] = {
        {x + size / 2, y},
        {x, y + size},
        {x + size, y + size},
    };
    Polygon(hdc, triangle, 3);
    Ellipse(hdc, x + size / 4, y + size / 2 - 5, x + size * 3 / 4, y + size / 2 + 5);
    HBRUSH dot = CreateSolidBrush(kOrange);
    SelectObject(hdc, dot);
    Ellipse(hdc, x + size / 2 - 3, y + size / 2 - 3, x + size / 2 + 3, y + size / 2 + 3);
    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    DeleteObject(dot);
    DeleteObject(pen);
}

void DrawTextBlock(HDC hdc, const std::wstring& text, RECT rect, HFONT font, COLORREF color, UINT flags) {
    HGDIOBJ old_font = SelectObject(hdc, font);
    SetTextColor(hdc, color);
    SetBkMode(hdc, TRANSPARENT);
    DrawTextW(hdc, text.c_str(), -1, &rect, flags);
    SelectObject(hdc, old_font);
}

// Buttons start at absolute y=311 (left_panel.top=116, offset=195)
// This positions them below the status info area which ends at ~116+183=299
std::vector<UiButton> BuildButtons(const RECT& client) {
    const int left = 36;
    const int top = 311;
    const int client_width = static_cast<int>(client.right - client.left);
    const int width = std::min(360, client_width / 2 - 70);
    const int height = 42;
    const int gap = 12;
    return {
        {{left, top, left + width, top + height}, L"Start Game", LoaderAction::StartGame},
        {{left, top + (height + gap), left + width, top + (height + gap) + height}, L"Inject Overlay", LoaderAction::Inject},
        {{left, top + 2 * (height + gap), left + width, top + 2 * (height + gap) + height}, L"Start + Inject", LoaderAction::StartAndInject},
        {{left, top + 3 * (height + gap), left + width, top + 3 * (height + gap) + height}, L"Open Game Folder", LoaderAction::OpenFolder},
        {{left, top + 4 * (height + gap), left + width, top + 4 * (height + gap) + height}, L"Refresh", LoaderAction::Refresh},
    };
}

bool PointInRect(const RECT& rect, int x, int y) {
    return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
}

std::wstring ShortPathForDisplay(const std::wstring& value) {
    constexpr std::size_t max_len = 78;
    if (value.size() <= max_len) {
        return value;
    }
    const std::size_t tail = std::min<std::size_t>(value.size(), max_len - 3);
    return L"..." + value.substr(value.size() - tail);
}

void DrawProgressBar(HDC hdc, RECT rect, int progress) {
    progress = std::clamp(progress, 0, 100);
    DrawFilledRect(hdc, rect, kPanelDeep);
    DrawBorderWide(hdc, rect, kOrange, 2);
    RECT fill = rect;
    fill.right = fill.left + MulDiv(rect.right - rect.left, progress, 100);
    if (fill.right > fill.left) {
        DrawFilledRect(hdc, fill, kOrange);
    }

    const std::wstring percent = std::to_wstring(progress) + L"%";
    DrawTextBlock(hdc, percent, rect, g_small_font, progress >= 45 ? kBg : kOrange, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
}

void PaintButton(HDC hdc, const UiButton& button, bool disabled, bool hovered) {
    COLORREF fill, border, text_color;
    if (disabled) {
        fill = RGB(31, 31, 31);
        border = RGB(88, 72, 48);
        text_color = RGB(130, 130, 130);
    } else if (hovered) {
        fill = kOrange;
        border = kOrange;
        text_color = kBg;
    } else {
        fill = RGB(42, 42, 42);
        border = kOrange;
        text_color = kOrange;
    }
    DrawFilledRect(hdc, button.rect, fill);
    DrawBorderWide(hdc, button.rect, border, 2);
    RECT text_rect = button.rect;
    text_rect.left += 14;
    DrawTextBlock(hdc, button.text, text_rect, g_header_font, text_color, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
}

void PaintWindow(HWND hwnd, HDC hdc) {
    RECT client{};
    GetClientRect(hwnd, &client);

    HDC mem_dc = CreateCompatibleDC(hdc);
    HBITMAP mem_bitmap = CreateCompatibleBitmap(hdc, client.right, client.bottom);
    HGDIOBJ old_bitmap = SelectObject(mem_dc, mem_bitmap);

    DrawFilledRect(mem_dc, client, kBg);

    // Header bar
    RECT header{0, 0, client.right, kHeaderHeight};
    DrawFilledRect(mem_dc, header, kBg);
    RECT accent{0, kHeaderHeight - 4, client.right, kHeaderHeight};
    DrawFilledRect(mem_dc, accent, kOrange);

    DrawTriangleEyeIcon(mem_dc, 32, 18, 48);
    RECT title{96, 16, client.right - (kChromeButtonW * 2 + 20), 56};
    DrawTextBlock(mem_dc, L"ARCANUS LOADER", title, g_title_font, kOrange, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
    RECT subtitle{98, 56, client.right - (kChromeButtonW * 2 + 20), 84};
    DrawTextBlock(mem_dc, L"Read-only tactical overlay for Heroes of Might and Magic: Olden Era", subtitle, g_body_font, kMuted, DT_SINGLELINE | DT_LEFT | DT_VCENTER);

    // Custom chrome buttons — stored so WM_NCHITTEST and WM_LBUTTONDOWN can use them
    g_btn_close = {client.right - kChromeButtonW - 8, 8, client.right - 8, 8 + kChromeButtonH};
    g_btn_min   = {g_btn_close.left - kChromeButtonW - 4, 8, g_btn_close.left - 4, 8 + kChromeButtonH};

    {
        const bool hov = g_hover_chrome == 0;
        DrawFilledRect(mem_dc, g_btn_min, hov ? kOrange : RGB(42, 42, 42));
        DrawBorder(mem_dc, g_btn_min, kOrange);
        DrawTextBlock(mem_dc, L"−", g_btn_min, g_body_font, hov ? kBg : kOrange, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    }
    {
        const bool hov = g_hover_chrome == 1;
        const COLORREF close_fill   = hov ? RGB(192, 32, 32) : RGB(42, 42, 42);
        const COLORREF close_border = hov ? RGB(220, 60, 60) : kOrange;
        DrawFilledRect(mem_dc, g_btn_close, close_fill);
        DrawBorder(mem_dc, g_btn_close, close_border);
        DrawTextBlock(mem_dc, L"×", g_btn_close, g_body_font, hov ? kText : kOrange, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    }

    // Two-column panels
    RECT left_panel{24, 116, std::max<LONG>(430, client.right / 2 - 16), client.bottom - 24};
    RECT right_panel{left_panel.right + 18, 116, client.right - 24, client.bottom - 24};
    DrawFilledRect(mem_dc, left_panel, kPanel);
    DrawBorderWide(mem_dc, left_panel, kOrange, 2);
    DrawFilledRect(mem_dc, right_panel, kPanel);
    DrawBorderWide(mem_dc, right_panel, kOrange, 2);

    std::wstring status;
    std::wstring process;
    std::wstring path;
    std::wstring operation;
    std::wstring notes;
    bool busy = false;
    int progress = -1;
    {
        std::lock_guard lock(g_state.mutex);
        status    = g_state.status;
        process   = g_state.process;
        path      = g_state.path;
        operation = g_state.operation;
        notes     = g_state.patch_notes;
        busy      = g_state.busy;
        progress  = g_state.progress;
    }

    // Left panel: STATUS section
    RECT status_header{left_panel.left + 16, left_panel.top + 14, left_panel.right - 16, left_panel.top + 42};
    DrawTextBlock(mem_dc, L"STATUS", status_header, g_header_font, kOrange, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
    RECT status_line{left_panel.left + 16, left_panel.top + 44, left_panel.right - 16, left_panel.top + 46};
    DrawFilledRect(mem_dc, status_line, kOrange);

    // Status message (2-line area)
    RECT status_rect{left_panel.left + 16, left_panel.top + 50, left_panel.right - 16, left_panel.top + 92};
    DrawTextBlock(mem_dc, status, status_rect, g_body_font, busy ? RGB(255, 197, 92) : RGB(108, 235, 152), DT_WORDBREAK | DT_LEFT);

    // Process line
    RECT proc_rect{left_panel.left + 16, left_panel.top + 95, left_panel.right - 16, left_panel.top + 115};
    DrawTextBlock(mem_dc, process, proc_rect, g_small_font, kText, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);

    // Path line
    RECT path_rect{left_panel.left + 16, left_panel.top + 117, left_panel.right - 16, left_panel.top + 137};
    DrawTextBlock(mem_dc, ShortPathForDisplay(path), path_rect, g_small_font, kMuted, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);

    // Thin separator before operation/progress
    RECT sep{left_panel.left + 16, left_panel.top + 141, left_panel.right - 16, left_panel.top + 143};
    DrawFilledRect(mem_dc, sep, kOrangeDim);

    // Operation label
    RECT operation_rect{left_panel.left + 16, left_panel.top + 147, left_panel.right - 16, left_panel.top + 165};
    DrawTextBlock(mem_dc, operation.empty() ? L"No active operation" : operation, operation_rect, g_small_font, busy ? kOrange : kMuted, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);

    // Progress bar  (ends at top+189, buttons start at top+195)
    RECT progress_rect{left_panel.left + 16, left_panel.top + 167, left_panel.right - 16, left_panel.top + 191};
    DrawProgressBar(mem_dc, progress_rect, progress >= 0 ? progress : 0);

    // Action buttons
    const auto buttons = BuildButtons(client);
    for (int i = 0; i < static_cast<int>(buttons.size()); ++i) {
        const bool disabled = busy && buttons[i].action != LoaderAction::Refresh;
        PaintButton(mem_dc, buttons[i], disabled, !disabled && g_hover_button == i);
    }

    // Hotkeys hint below buttons
    // buttons end at top + 311+5*42+4*12 - left_panel.top = 569; hint starts at 569+14=583
    constexpr int kBtnSectionEnd = 311 + 5 * 42 + 4 * 12; // absolute y = 569
    RECT hint_line{left_panel.left + 16, kBtnSectionEnd + 10, left_panel.right - 16, kBtnSectionEnd + 12};
    DrawFilledRect(mem_dc, hint_line, kOrangeDim);
    RECT hint{left_panel.left + 16, kBtnSectionEnd + 16, left_panel.right - 16, left_panel.bottom - 16};
    DrawTextBlock(mem_dc, L"Hotkeys: F1 HUD, ~ Dev Panel, End hides overlay safely. Loader never edits game values.", hint, g_small_font, kMuted, DT_WORDBREAK | DT_LEFT);

    // Right panel: PATCH NOTES
    RECT notes_header{right_panel.left + 18, right_panel.top + 14, right_panel.right - 18, right_panel.top + 42};
    DrawTextBlock(mem_dc, L"PATCH NOTES", notes_header, g_header_font, kOrange, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
    RECT notes_line{right_panel.left + 18, right_panel.top + 44, right_panel.right - 18, right_panel.top + 46};
    DrawFilledRect(mem_dc, notes_line, kOrange);
    RECT notes_rect{right_panel.left + 18, right_panel.top + 52, right_panel.right - 18, right_panel.bottom - 18};
    DrawTextBlock(mem_dc, notes, notes_rect, g_body_font, kText, DT_WORDBREAK | DT_LEFT | DT_NOPREFIX);

    BitBlt(hdc, 0, 0, client.right, client.bottom, mem_dc, 0, 0, SRCCOPY);
    SelectObject(mem_dc, old_bitmap);
    DeleteObject(mem_bitmap);
    DeleteDC(mem_dc);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE:
        g_title_font  = CreateFontW(-30, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Consolas");
        g_header_font = CreateFontW(-18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Consolas");
        g_body_font   = CreateFontW(-16, 0, 0, 0, FW_NORMAL,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Consolas");
        g_small_font  = CreateFontW(-14, 0, 0, 0, FW_NORMAL,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Consolas");
        {
            std::lock_guard lock(g_state.mutex);
            g_state.patch_notes = LoadPatchNotes();
        }
        // Extend DWM frame to preserve drop shadow with borderless chrome
        {
            MARGINS margins{0, 0, 0, 1};
            DwmExtendFrameIntoClientArea(hwnd, &margins);
        }
        RefreshState(hwnd);
        SetTimer(hwnd, 1, 2000, nullptr);
        return 0;
    case WM_TIMER:
        RefreshState(hwnd);
        return 0;
    case WM_NCCALCSIZE:
        // Returning 0 when wParam=1 makes the client area fill the entire window rect,
        // removing the visible title bar and frame while keeping resize/DWM shadow.
        if (wparam) return 0;
        return DefWindowProcW(hwnd, message, wparam, lparam);
    case WM_NCHITTEST: {
        // Let DefWindowProc handle sizing borders at the edges first
        const LRESULT hit = DefWindowProcW(hwnd, message, wparam, lparam);
        if (hit == HTCLIENT) {
            POINT pt{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            ScreenToClient(hwnd, &pt);
            // Chrome buttons → keep as HTCLIENT so WM_LBUTTONDOWN fires
            if (PointInRect(g_btn_close, pt.x, pt.y) || PointInRect(g_btn_min, pt.x, pt.y)) {
                return HTCLIENT;
            }
            // Header area → draggable
            if (pt.y >= 0 && pt.y < kHeaderHeight) {
                return HTCAPTION;
            }
        }
        return hit;
    }
    case WM_SIZE:
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
        info->ptMinTrackSize.x = kMinWindowWidth;
        info->ptMinTrackSize.y = kMinWindowHeight;
        // WS_POPUP doesn't clamp maximize to work area automatically
        HMONITOR hmon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{sizeof(mi)};
        if (GetMonitorInfoW(hmon, &mi)) {
            info->ptMaxPosition.x = mi.rcWork.left;
            info->ptMaxPosition.y = mi.rcWork.top;
            info->ptMaxSize.x = mi.rcWork.right - mi.rcWork.left;
            info->ptMaxSize.y = mi.rcWork.bottom - mi.rcWork.top;
        }
        return 0;
    }
    case WM_ERASEBKGND:
        return TRUE; // WM_PAINT double-buffers the full client — skip GDI erase
    case WM_MOUSEMOVE: {
        const int mx = GET_X_LPARAM(lparam);
        const int my = GET_Y_LPARAM(lparam);
        RECT client{};
        GetClientRect(hwnd, &client);

        // Chrome button hover
        int new_chrome = -1;
        if (PointInRect(g_btn_min, mx, my))   new_chrome = 0;
        if (PointInRect(g_btn_close, mx, my)) new_chrome = 1;
        if (new_chrome != g_hover_chrome) {
            g_hover_chrome = new_chrome;
            InvalidateRect(hwnd, nullptr, FALSE);
        }

        // Action button hover
        const auto buttons = BuildButtons(client);
        int new_hover = -1;
        for (int i = 0; i < static_cast<int>(buttons.size()); ++i) {
            if (PointInRect(buttons[i].rect, mx, my)) {
                new_hover = i;
                break;
            }
        }
        if (new_hover != g_hover_button) {
            g_hover_button = new_hover;
            InvalidateRect(hwnd, nullptr, FALSE);
        }

        if (!g_mouse_tracking) {
            TRACKMOUSEEVENT tme{};
            tme.cbSize  = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
            g_mouse_tracking = true;
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        g_hover_button = -1;
        g_hover_chrome = -1;
        g_mouse_tracking = false;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(lparam) == HTCLIENT && (g_hover_button >= 0 || g_hover_chrome >= 0)) {
            SetCursor(LoadCursorW(nullptr, MAKEINTRESOURCEW(32649))); // IDC_HAND
            return TRUE;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    case WM_LBUTTONDOWN: {
        const int x = GET_X_LPARAM(lparam);
        const int y = GET_Y_LPARAM(lparam);

        // Chrome buttons work regardless of busy state
        if (PointInRect(g_btn_close, x, y)) {
            DestroyWindow(hwnd);
            return 0;
        }
        if (PointInRect(g_btn_min, x, y)) {
            ShowWindow(hwnd, SW_MINIMIZE);
            return 0;
        }

        {
            std::lock_guard lock(g_state.mutex);
            if (g_state.busy) {
                return 0;
            }
        }
        RECT client{};
        GetClientRect(hwnd, &client);
        for (const auto& button : BuildButtons(client)) {
            if (PointInRect(button.rect, x, y)) {
                RunAction(hwnd, button.action);
                return 0;
            }
        }
        return 0;
    }
    case kStatusMessage:
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        PaintWindow(hwnd, hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        if (g_title_font)  DeleteObject(g_title_font);
        if (g_header_font) DeleteObject(g_header_font);
        if (g_body_font)   DeleteObject(g_body_font);
        if (g_small_font)  DeleteObject(g_small_font);
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    WNDCLASSW wc{};
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = instance;
    wc.lpszClassName = kWindowClass;
    wc.hCursor       = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.hIcon         = LoadIconW(nullptr, MAKEINTRESOURCEW(32512));
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        kWindowClass,
        L"ARCANUS Loader",
        WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1040,
        680,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (hwnd == nullptr) {
        return 1;
    }

    ShowWindow(hwnd, show_command);
    UpdateWindow(hwnd);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return 0;
}
