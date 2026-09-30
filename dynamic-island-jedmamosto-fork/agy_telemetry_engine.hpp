#pragma once

#ifndef AGY_TELEMETRY_ENGINE_HPP
#define AGY_TELEMETRY_ENGINE_HPP

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <tlhelp32.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <string>
#include <vector>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <optional>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <utility>
#include <string_view>
#include <cstdlib>

namespace agy {

using Microsoft::WRL::ComPtr;

// Application layout message posted when telemetry snapshots change
constexpr UINT WM_APP_LAYOUT_CHANGED = WM_APP + 0x442;
constexpr wchar_t kIslandWindowClass[] = L"Windhawk.DynamicIslandForWindows";
constexpr wchar_t kDefaultPipeName[] = L"\\\\.\\pipe\\antigravity_island_telemetry";

// ============================================================================
// String Conversion Utilities (UTF-8 <-> UTF-16)
// ============================================================================

inline std::wstring Utf8ToWide(std::string_view utf8Str) {
    if (utf8Str.empty()) return L"";
    int req = MultiByteToWideChar(CP_UTF8, 0, utf8Str.data(), static_cast<int>(utf8Str.size()), nullptr, 0);
    if (req <= 0) return L"";
    std::wstring result(static_cast<size_t>(req), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8Str.data(), static_cast<int>(utf8Str.size()), &result[0], req);
    return result;
}

inline std::string WideToUtf8(std::wstring_view wideStr) {
    if (wideStr.empty()) return "";
    int req = WideCharToMultiByte(CP_UTF8, 0, wideStr.data(), static_cast<int>(wideStr.size()), nullptr, 0, nullptr, nullptr);
    if (req <= 0) return "";
    std::string result(static_cast<size_t>(req), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wideStr.data(), static_cast<int>(wideStr.size()), &result[0], req, nullptr, nullptr);
    return result;
}

// ============================================================================
// Zero-Dependency Modern JSON Parser
// ============================================================================

namespace json {

enum class Type { Null, Bool, Number, String, Array, Object };

struct Value {
    Type type = Type::Null;
    bool boolVal = false;
    double numVal = 0.0;
    std::string strVal;
    std::vector<Value> arrVal;
    std::vector<std::pair<std::string, Value>> objVal;

    bool IsNull() const { return type == Type::Null; }
    bool IsBool() const { return type == Type::Bool; }
    bool IsNumber() const { return type == Type::Number; }
    bool IsString() const { return type == Type::String; }
    bool IsArray() const { return type == Type::Array; }
    bool IsObject() const { return type == Type::Object; }

    bool AsBool(bool def = false) const { return IsBool() ? boolVal : def; }
    double AsDouble(double def = 0.0) const { return IsNumber() ? numVal : def; }
    uint64_t AsUInt64(uint64_t def = 0) const { return IsNumber() ? static_cast<uint64_t>(numVal) : def; }
    int64_t AsInt64(int64_t def = 0) const { return IsNumber() ? static_cast<int64_t>(numVal) : def; }
    int AsInt(int def = 0) const { return IsNumber() ? static_cast<int>(numVal) : def; }
    std::string AsString(const std::string& def = "") const { return IsString() ? strVal : def; }
    std::wstring AsWString(const std::wstring& def = L"") const {
        return IsString() ? Utf8ToWide(strVal) : def;
    }

    size_t Size() const {
        if (IsArray()) return arrVal.size();
        if (IsObject()) return objVal.size();
        return 0;
    }

    const Value& operator[](size_t idx) const {
        static const Value kNullVal;
        if (IsArray() && idx < arrVal.size()) return arrVal[idx];
        return kNullVal;
    }

    const Value& operator[](const std::string& key) const {
        static const Value kNullVal;
        if (IsObject()) {
            for (const auto& kv : objVal) {
                if (kv.first == key) return kv.second;
            }
        }
        return kNullVal;
    }

    bool HasKey(const std::string& key) const {
        if (IsObject()) {
            for (const auto& kv : objVal) {
                if (kv.first == key) return true;
            }
        }
        return false;
    }
};

class Parser {
public:
    static bool Parse(std::string_view src, Value& outRoot) {
        Parser p(src);
        p.SkipWhitespace();
        if (!p.ParseValue(outRoot)) return false;
        p.SkipWhitespace();
        return true;
    }

private:
    std::string_view text_;
    size_t pos_ = 0;

    explicit Parser(std::string_view src) : text_(src), pos_(0) {}

    bool Eof() const { return pos_ >= text_.size(); }
    char Peek() const { return Eof() ? '\0' : text_[pos_]; }
    char Get() { return Eof() ? '\0' : text_[pos_++]; }

    void SkipWhitespace() {
        while (!Eof()) {
            char c = text_[pos_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                pos_++;
            } else if (c == '/' && pos_ + 1 < text_.size() && text_[pos_ + 1] == '/') {
                pos_ += 2;
                while (!Eof() && text_[pos_] != '\n' && text_[pos_] != '\r') pos_++;
            } else {
                break;
            }
        }
    }

    bool ParseValue(Value& v) {
        SkipWhitespace();
        if (Eof()) return false;
        char c = Peek();
        if (c == 'n') return ParseNull(v);
        if (c == 't' || c == 'f') return ParseBool(v);
        if (c == '"') return ParseString(v);
        if (c == '[') return ParseArray(v);
        if (c == '{') return ParseObject(v);
        if (c == '-' || (c >= '0' && c <= '9')) return ParseNumber(v);
        return false;
    }

    bool ParseNull(Value& v) {
        if (pos_ + 4 <= text_.size() && text_.substr(pos_, 4) == "null") {
            pos_ += 4;
            v.type = Type::Null;
            return true;
        }
        return false;
    }

    bool ParseBool(Value& v) {
        if (pos_ + 4 <= text_.size() && text_.substr(pos_, 4) == "true") {
            pos_ += 4;
            v.type = Type::Bool;
            v.boolVal = true;
            return true;
        }
        if (pos_ + 5 <= text_.size() && text_.substr(pos_, 5) == "false") {
            pos_ += 5;
            v.type = Type::Bool;
            v.boolVal = false;
            return true;
        }
        return false;
    }

    bool ParseNumber(Value& v) {
        size_t start = pos_;
        if (Peek() == '-') Get();
        while (!Eof() && ((Peek() >= '0' && Peek() <= '9') || Peek() == '.' || Peek() == 'e' || Peek() == 'E' || Peek() == '+' || Peek() == '-')) {
            Get();
        }
        std::string numStr(text_.substr(start, pos_ - start));
        char* endPtr = nullptr;
        double val = std::strtod(numStr.c_str(), &endPtr);
        if (endPtr == numStr.c_str()) return false;
        v.type = Type::Number;
        v.numVal = val;
        return true;
    }

    bool ParseString(Value& v) {
        std::string s;
        if (Get() != '"') return false;
        while (!Eof()) {
            char c = Get();
            if (c == '"') {
                v.type = Type::String;
                v.strVal = std::move(s);
                return true;
            }
            if (c == '\\') {
                if (Eof()) return false;
                char esc = Get();
                switch (esc) {
                    case '"': s.push_back('"'); break;
                    case '\\': s.push_back('\\'); break;
                    case '/': s.push_back('/'); break;
                    case 'b': s.push_back('\b'); break;
                    case 'f': s.push_back('\f'); break;
                    case 'n': s.push_back('\n'); break;
                    case 'r': s.push_back('\r'); break;
                    case 't': s.push_back('\t'); break;
                    case 'u': {
                        if (pos_ + 4 > text_.size()) return false;
                        unsigned int code = 0;
                        for (int i = 0; i < 4; ++i) {
                            char h = Get();
                            code <<= 4;
                            if (h >= '0' && h <= '9') code |= (h - '0');
                            else if (h >= 'a' && h <= 'f') code |= (h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') code |= (h - 'A' + 10);
                            else return false;
                        }
                        if (code <= 0x7F) {
                            s.push_back(static_cast<char>(code));
                        } else if (code <= 0x7FF) {
                            s.push_back(static_cast<char>(0xC0 | (code >> 6)));
                            s.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                        } else {
                            s.push_back(static_cast<char>(0xE0 | (code >> 12)));
                            s.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                            s.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                        }
                        break;
                    }
                    default: s.push_back(esc); break;
                }
            } else {
                s.push_back(c);
            }
        }
        return false;
    }

    bool ParseArray(Value& v) {
        if (Get() != '[') return false;
        v.type = Type::Array;
        v.arrVal.clear();
        SkipWhitespace();
        if (Peek() == ']') {
            Get();
            return true;
        }
        while (!Eof()) {
            Value item;
            if (!ParseValue(item)) return false;
            v.arrVal.push_back(std::move(item));
            SkipWhitespace();
            char c = Peek();
            if (c == ']') {
                Get();
                return true;
            }
            if (c == ',') {
                Get();
                SkipWhitespace();
            } else {
                return false;
            }
        }
        return false;
    }

    bool ParseObject(Value& v) {
        if (Get() != '{') return false;
        v.type = Type::Object;
        v.objVal.clear();
        SkipWhitespace();
        if (Peek() == '}') {
            Get();
            return true;
        }
        while (!Eof()) {
            SkipWhitespace();
            Value keyVal;
            if (!ParseString(keyVal)) return false;
            SkipWhitespace();
            if (Get() != ':') return false;
            SkipWhitespace();
            Value item;
            if (!ParseValue(item)) return false;
            v.objVal.emplace_back(std::move(keyVal.strVal), std::move(item));
            SkipWhitespace();
            char c = Peek();
            if (c == '}') {
                Get();
                return true;
            }
            if (c == ',') {
                Get();
                SkipWhitespace();
            } else {
                return false;
            }
        }
        return false;
    }
};

inline bool Parse(std::string_view jsonStr, Value& outRoot) {
    return Parser::Parse(jsonStr, outRoot);
}

} // namespace json

// ============================================================================
// AGY Telemetry Domain Models
// ============================================================================

struct Subagent {
    std::wstring name;
    std::wstring role;
    std::wstring state; // "running", "idle", "waiting_for_input", "errored"

    bool IsRunning() const {
        return state == L"running" || state == L"active" || state == L"working";
    }
    bool IsErrored() const {
        return state == L"errored" || state == L"failed";
    }
};

struct Task {
    std::wstring id;
    std::wstring summary;
    std::wstring status; // "running", "completed", "failed", "cancelled"

    bool IsRunning() const {
        return status == L"running" || status == L"in_progress";
    }
};

struct Workspace {
    std::wstring folder;
    std::wstring branch;
    int changedFiles = 0;
};

// A session with running subagents/tasks without an update for > 3 minutes is considered stalled/abandoned
constexpr uint64_t kRunningStaleThresholdSec = 180; // 3 minutes
// A session without any updates for > 15 minutes is considered dormant
constexpr uint64_t kSessionDormantThresholdSec = 900; // 15 minutes
// An inactive session older than 1 hour is pruned if newer sessions exist
constexpr uint64_t kSessionPruneThresholdSec = 3600; // 1 hour

struct SessionTelemetry {
    std::wstring sourceFilePath;
    FILETIME lastWriteTime = {};
    uint64_t mtimeKey = 0;

    std::wstring activeConversationId;
    std::wstring sessionName;
    uint64_t contextTokens = 0;
    uint64_t maxTokens = 1048576;
    float percentUtilized = 0.0f; // 0.0 to 100.0
    std::vector<Subagent> activeSubagents;
    std::vector<Task> activeTasks;
    Workspace workspace;
    std::wstring lastCompletedEvent;
    std::wstring updatedAt;
    uint64_t updatedAtEpoch = 0;
    bool isValid = false;

    uint64_t GetAgeSeconds() const {
        FILETIME ftNow;
        GetSystemTimeAsFileTime(&ftNow);
        ULARGE_INTEGER uNow, uLast;
        uNow.LowPart = ftNow.dwLowDateTime;
        uNow.HighPart = ftNow.dwHighDateTime;
        uLast.LowPart = lastWriteTime.dwLowDateTime;
        uLast.HighPart = lastWriteTime.dwHighDateTime;

        if (updatedAtEpoch > 0) {
            // Convert FILETIME (100-ns intervals since Jan 1, 1601) to Unix timestamp (Jan 1, 1970)
            uint64_t unixNow = (uNow.QuadPart >= 116444736000000000ULL)
                ? (uNow.QuadPart - 116444736000000000ULL) / 10000000ULL
                : 0;
            if (unixNow >= updatedAtEpoch) {
                return unixNow - updatedAtEpoch;
            }
        }

        if (uNow.QuadPart >= uLast.QuadPart) {
            return (uNow.QuadPart - uLast.QuadPart) / 10000000ULL;
        }
        return 0;
    }

    bool IsStale(uint64_t maxAgeSec = kSessionDormantThresholdSec) const {
        return GetAgeSeconds() > maxAgeSec;
    }

    float GetRatio() const {
        if (percentUtilized > 0.0f) {
            float r = percentUtilized / 100.0f;
            return (r > 1.0f) ? 1.0f : ((r < 0.0f) ? 0.0f : r);
        }
        if (maxTokens == 0) return 0.0f;
        float r = static_cast<float>(contextTokens) / static_cast<float>(maxTokens);
        return (r > 1.0f) ? 1.0f : ((r < 0.0f) ? 0.0f : r);
    }

    size_t GetRunningSubagentsCount() const {
        // If the session hasn't received updates in 3 minutes, running subagents are considered dead
        if (GetAgeSeconds() > kRunningStaleThresholdSec) {
            return 0;
        }
        size_t c = 0;
        for (const auto& sa : activeSubagents) {
            if (sa.IsRunning()) c++;
        }
        return c;
    }

    size_t GetRunningTasksCount() const {
        // If the session hasn't received updates in 3 minutes, running tasks are considered dead
        if (GetAgeSeconds() > kRunningStaleThresholdSec) {
            return 0;
        }
        size_t c = 0;
        for (const auto& t : activeTasks) {
            if (t.IsRunning()) c++;
        }
        return c;
    }
};

inline bool ParseSessionJson(const std::string& jsonStr, SessionTelemetry& outTelemetry) {
    json::Value root;
    if (!json::Parse(jsonStr, root) || !root.IsObject()) {
        return false;
    }

    outTelemetry.activeConversationId = root["activeConversationId"].AsWString();
    outTelemetry.sessionName = root["sessionName"].AsWString();
    outTelemetry.contextTokens = root["contextTokens"].AsUInt64(0);
    outTelemetry.maxTokens = root["maxTokens"].AsUInt64(1048576);
    outTelemetry.percentUtilized = static_cast<float>(root["percentUtilized"].AsDouble(0.0));

    if (outTelemetry.percentUtilized <= 0.0f && outTelemetry.maxTokens > 0 && outTelemetry.contextTokens > 0) {
        outTelemetry.percentUtilized = static_cast<float>(
            (static_cast<double>(outTelemetry.contextTokens) / static_cast<double>(outTelemetry.maxTokens)) * 100.0
        );
    }

    outTelemetry.activeSubagents.clear();
    const auto& subagentsArr = root["activeSubagents"];
    if (subagentsArr.IsArray()) {
        for (size_t i = 0; i < subagentsArr.Size(); ++i) {
            const auto& item = subagentsArr[i];
            if (item.IsObject()) {
                Subagent sa;
                sa.name = item["name"].AsWString();
                sa.role = item["role"].AsWString();
                sa.state = item["state"].AsWString();
                outTelemetry.activeSubagents.push_back(sa);
            }
        }
    }

    outTelemetry.activeTasks.clear();
    const auto& tasksArr = root["activeTasks"];
    if (tasksArr.IsArray()) {
        for (size_t i = 0; i < tasksArr.Size(); ++i) {
            const auto& item = tasksArr[i];
            if (item.IsObject()) {
                Task t;
                t.id = item["id"].AsWString();
                t.summary = item["summary"].AsWString();
                t.status = item["status"].AsWString();
                outTelemetry.activeTasks.push_back(t);
            }
        }
    }

    const auto& wsObj = root["workspace"];
    if (wsObj.IsObject()) {
        outTelemetry.workspace.folder = wsObj["folder"].AsWString();
        outTelemetry.workspace.branch = wsObj["branch"].AsWString();
        outTelemetry.workspace.changedFiles = wsObj["changedFiles"].AsInt(0);
    }

    outTelemetry.lastCompletedEvent = root["lastCompletedEvent"].AsWString();
    if (root["updatedAt"].IsNumber()) {
        outTelemetry.updatedAtEpoch = root["updatedAt"].AsUInt64(0);
        wchar_t buf[32];
        swprintf_s(buf, L"%llu", static_cast<unsigned long long>(outTelemetry.updatedAtEpoch));
        outTelemetry.updatedAt = buf;
    } else {
        outTelemetry.updatedAt = root["updatedAt"].AsWString();
        if (!outTelemetry.updatedAt.empty()) {
            try {
                outTelemetry.updatedAtEpoch = std::stoull(outTelemetry.updatedAt);
            } catch (...) {}
        }
    }
    outTelemetry.isValid = true;
    return true;
}

// Token Gauge Formatting Helpers
inline std::wstring FormatTokenCount(uint64_t count) {
    wchar_t buf[32];
    if (count >= 1000000) {
        swprintf_s(buf, L"%.1fM", static_cast<double>(count) / 1000000.0);
    } else if (count >= 1000) {
        swprintf_s(buf, L"%.1fk", static_cast<double>(count) / 1000.0);
    } else {
        swprintf_s(buf, L"%llu", static_cast<unsigned long long>(count));
    }
    return buf;
}

enum class TokenPressureTier {
    Nominal,   // 0% - 60%  (Apple systemGreen)
    Elevated,  // 60% - 80% (Apple systemOrange)
    Critical   // 80%+      (Apple systemRed with live pulse)
};

inline TokenPressureTier GetTokenPressureTier(float ratio) {
    if (ratio >= 0.80f) return TokenPressureTier::Critical;
    if (ratio >= 0.60f) return TokenPressureTier::Elevated;
    return TokenPressureTier::Nominal;
}

inline D2D1_COLOR_F GetTokenGaugeColor(float ratio, double now) {
    TokenPressureTier tier = GetTokenPressureTier(ratio);
    if (tier == TokenPressureTier::Critical) {
        float pulse = 0.65f + 0.35f * std::sin(static_cast<float>(now) * 6.28318f);
        return D2D1::ColorF(1.0f, 0.271f, 0.227f, pulse);
    } else if (tier == TokenPressureTier::Elevated) {
        return D2D1::ColorF(1.0f, 0.624f, 0.039f, 0.95f);
    } else {
        return D2D1::ColorF(0.204f, 0.780f, 0.349f, 0.95f);
    }
}

// Win32 Helper: Read file content with non-exclusive shared-access flags
inline std::string ReadFileShared(const std::wstring& path) {
    HANDLE hFile = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (hFile == INVALID_HANDLE_VALUE) {
        return "";
    }

    LARGE_INTEGER size;
    if (!GetFileSizeEx(hFile, &size) || size.QuadPart <= 0 || size.QuadPart > (16 * 1024 * 1024)) {
        CloseHandle(hFile);
        return "";
    }

    std::string buffer;
    buffer.resize(static_cast<size_t>(size.QuadPart));
    DWORD bytesRead = 0;
    if (!ReadFile(hFile, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, nullptr)) {
        CloseHandle(hFile);
        return "";
    }
    CloseHandle(hFile);
    buffer.resize(bytesRead);
    return buffer;
}

inline bool DirectoryExists(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));
}

inline bool FileExists(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
}

inline std::wstring GetDirectoryFromPath(const std::wstring& path) {
    size_t lastSlash = path.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        return path.substr(0, lastSlash);
    }
    return L"";
}

// ============================================================================
// AGY Telemetry Engine & Multi-Session Hybrid Resolver
// Architecture: Decoupled Worker Thread + Windows Named Pipe + ReadDirectoryChangesW
// ============================================================================

class AgyTelemetryEngine {
public:
    struct CarouselHitTargets {
        D2D1_RECT_F prevBtn = {};
        D2D1_RECT_F nextBtn = {};
        std::vector<D2D1_RECT_F> dots;
    };

    AgyTelemetryEngine() = default;

    ~AgyTelemetryEngine() {
        Shutdown();
    }

    AgyTelemetryEngine(const AgyTelemetryEngine&) = delete;
    AgyTelemetryEngine& operator=(const AgyTelemetryEngine&) = delete;

    void Initialize(const std::wstring& primaryFilePath = L"C:\\Users\\ASUS\\.gemini\\antigravity\\brain\\island_telemetry.json",
                    const std::wstring& brainScanDir = L"C:\\Users\\ASUS\\.gemini\\antigravity\\brain",
                    HWND notifyHwnd = nullptr) {
        Shutdown();

        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            primaryFilePath_ = primaryFilePath;
            brainScanDir_ = brainScanDir;
            notifyHwnd_ = notifyHwnd;
            pipeName_ = kDefaultPipeName;
            sessions_.clear();
            selectedSessionIndex_ = 0;
            autoFollowMru_ = true;
            hasUpdates_.store(false);

            // Synchronous initial load so data is populated on first frame
            ReloadFromDiskLocked();
        }

        stopEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        running_.store(true);
        workerThread_ = std::thread(&AgyTelemetryEngine::WorkerLoop, this);
    }

    void Shutdown() {
        if (running_.load()) {
            running_.store(false);
            if (stopEvent_) {
                SetEvent(stopEvent_);
            }
            HANDLE pipe = hPipe_.load();
            if (pipe != INVALID_HANDLE_VALUE) {
                CancelIoEx(pipe, nullptr);
            }
            HANDLE dir = hDir_.load();
            if (dir != INVALID_HANDLE_VALUE) {
                CancelIoEx(dir, nullptr);
            }
            if (workerThread_.joinable()) {
                workerThread_.join();
            }
        }
        if (stopEvent_) {
            CloseHandle(stopEvent_);
            stopEvent_ = NULL;
        }
    }

    void SetNotificationHwnd(HWND hwnd) {
        std::lock_guard<std::mutex> lock(stateMutex_);
        notifyHwnd_ = hwnd;
    }

    void SetCheckInterval(double seconds) {
        std::lock_guard<std::mutex> lock(stateMutex_);
        checkInterval_ = seconds;
    }

    // Zero CPU File Watcher: Fast non-blocking atomic check on render thread
    bool CheckForUpdates(double /*nowSec*/ = 0.0, bool force = false) {
        if (force) {
            std::lock_guard<std::mutex> lock(stateMutex_);
            bool changed = ReloadFromDiskLocked();
            if (changed) {
                hasUpdates_.store(false);
            }
            return changed;
        }
        // Non-blocking check for render thread (zero disk I/O)
        bool expected = true;
        return hasUpdates_.compare_exchange_strong(expected, false);
    }

    // Direct manual session loading (useful for testing or direct IPC feeds)
    bool LoadSessionFromJsonString(const std::string& jsonContent, const std::wstring& sessionTag = L"manual", uint64_t mtimeKey = 0) {
        bool result = IngestJsonPayload(jsonContent, sessionTag, mtimeKey);
        if (result) {
            NotifyLayoutChanged();
        }
        return result;
    }

    // Carousel cycling and selection
    void CycleNextSession() {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (sessions_.empty()) return;
        autoFollowMru_ = false;
        selectedSessionIndex_ = (selectedSessionIndex_ + 1) % sessions_.size();
        UpdateActiveSnapshotLocked();
    }

    void CyclePrevSession() {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (sessions_.empty()) return;
        autoFollowMru_ = false;
        selectedSessionIndex_ = (selectedSessionIndex_ + sessions_.size() - 1) % sessions_.size();
        UpdateActiveSnapshotLocked();
    }

    void SelectSession(size_t index) {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (index < sessions_.size()) {
            autoFollowMru_ = false;
            selectedSessionIndex_ = index;
            UpdateActiveSnapshotLocked();
        }
    }

    void ResetToMru() {
        std::lock_guard<std::mutex> lock(stateMutex_);
        autoFollowMru_ = true;
        selectedSessionIndex_ = 0;
        UpdateActiveSnapshotLocked();
    }

    const SessionTelemetry* GetActiveSession() const {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (!hasActiveSessionSnapshot_ || sessions_.empty()) return nullptr;
        return &activeSessionSnapshot_;
    }

    size_t GetActiveSessionIndex() const {
        std::lock_guard<std::mutex> lock(stateMutex_);
        return selectedSessionIndex_;
    }

    size_t GetSessionCount() const {
        std::lock_guard<std::mutex> lock(stateMutex_);
        return sessions_.size();
    }

    bool HasMultipleSessions() const {
        std::lock_guard<std::mutex> lock(stateMutex_);
        return sessions_.size() >= 2;
    }

    bool IsAutoFollowingMru() const {
        std::lock_guard<std::mutex> lock(stateMutex_);
        return autoFollowMru_;
    }

    const std::vector<SessionTelemetry>& GetAllSessions() const {
        std::lock_guard<std::mutex> lock(stateMutex_);
        return sessionsSnapshot_;
    }

    std::vector<SessionTelemetry> GetSessions() const {
        std::lock_guard<std::mutex> lock(stateMutex_);
        return sessions_;
    }

    static BOOL CALLBACK EnumAgyWndProc(HWND hwnd, LPARAM lParam) {
        if (!IsWindowVisible(hwnd)) return TRUE;
        wchar_t title[256] = {};
        GetWindowTextW(hwnd, title, ARRAYSIZE(title));
        if (title[0] != L'\0' && wcsstr(title, L"Antigravity") != nullptr) {
            *reinterpret_cast<HWND*>(lParam) = hwnd;
            return FALSE;
        }
        return TRUE;
    }

    static bool IsAntigravityRunningCached() {
        static ULONGLONG s_lastCheck = 0;
        static bool s_cachedRunning = false;
        ULONGLONG now = GetTickCount64();
        if (now - s_lastCheck < 2500) {
            return s_cachedRunning;
        }
        s_lastCheck = now;

        // 1. Search top-level windows for Antigravity in title
        HWND foundHwnd = nullptr;
        EnumWindows(EnumAgyWndProc, reinterpret_cast<LPARAM>(&foundHwnd));

        if (foundHwnd != nullptr) {
            s_cachedRunning = true;
            return true;
        }

        // 2. Fallback: Search processes for antigravity executable
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W pe = { sizeof(pe) };
            if (Process32FirstW(hSnap, &pe)) {
                do {
                    wchar_t lowerName[MAX_PATH] = {};
                    wcsncpy_s(lowerName, pe.szExeFile, _TRUNCATE);
                    for (size_t i = 0; i < wcslen(lowerName); ++i) {
                        lowerName[i] = towlower(lowerName[i]);
                    }
                    if (wcsstr(lowerName, L"antigravity") != nullptr) {
                        CloseHandle(hSnap);
                        s_cachedRunning = true;
                        return true;
                    }
                } while (Process32NextW(hSnap, &pe));
            }
            CloseHandle(hSnap);
        }

        s_cachedRunning = false;
        return false;
    }

    // Evaluates if telemetry is active (running subagents, running tasks, or active IDE session)
    bool IsActive() const {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (sessions_.empty()) return false;

        const bool isIdeRunning = IsAntigravityRunningCached();

        for (const auto& s : sessions_) {
            uint64_t age = s.GetAgeSeconds();

            // Active if running subagents or tasks are within fresh threshold (<= 3 mins)
            if (s.GetRunningSubagentsCount() > 0 || s.GetRunningTasksCount() > 0) {
                return true;
            }

            // Active if session was updated recently within 15 minutes
            if (age < kSessionDormantThresholdSec) {
                return true;
            }

            // If Antigravity IDE is actively running, retain recent session (up to 45 minutes)
            if (isIdeRunning && age < (45 * 60)) {
                return true;
            }
        }
        return false;
    }

    // Carousel Mouse Hit Testing
    bool HandleMouseClick(D2D1_POINT_2F pt) {
        if (!HasMultipleSessions()) return false;

        auto ptInRect = [](D2D1_POINT_2F p, D2D1_RECT_F r) {
            return p.x >= r.left && p.x <= r.right && p.y >= r.top && p.y <= r.bottom;
        };

        if (ptInRect(pt, lastCarouselHitTargets_.prevBtn)) {
            CyclePrevSession();
            return true;
        }
        if (ptInRect(pt, lastCarouselHitTargets_.nextBtn)) {
            CycleNextSession();
            return true;
        }
        for (size_t i = 0; i < lastCarouselHitTargets_.dots.size(); ++i) {
            if (ptInRect(pt, lastCarouselHitTargets_.dots[i])) {
                SelectSession(i);
                return true;
            }
        }
        return false;
    }

    // ========================================================================
    // Direct2D Dashboard Card Layout (DrawAgyDashboard)
    // ========================================================================
    void DrawAgyDashboard(ID2D1RenderTarget* target,
                          ID2D1Factory* d2dFactory,
                          IDWriteTextFormat* boldTextFormat,
                          IDWriteTextFormat* textFormat,
                          IDWriteTextFormat* smallTextFormat,
                          D2D1_RECT_F rect,
                          float scale,
                          double now,
                          D2D1_COLOR_F accentColor = D2D1::ColorF(0x4cc9f0)) {
        if (!target) return;

        // Thread-safe copy of session state to render without holding stateMutex_ during D2D calls
        SessionTelemetry sessionData;
        bool hasSession = false;
        bool hasMultiple = false;
        bool autoFollow = true;
        std::vector<SessionTelemetry> currentSessions;
        size_t activeIndex = 0;

        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            if (!sessions_.empty()) {
                size_t idx = (selectedSessionIndex_ < sessions_.size()) ? selectedSessionIndex_ : 0;
                sessionData = sessions_[idx];
                hasSession = true;
            }
            hasMultiple = sessions_.size() >= 2;
            autoFollow = autoFollowMru_;
            currentSessions = sessions_;
            activeIndex = selectedSessionIndex_;
        }

        const SessionTelemetry* session = hasSession ? &sessionData : nullptr;

        // Auto-resolve factory if null
        ComPtr<ID2D1Factory> factory = d2dFactory;
        if (!factory) {
            target->GetFactory(&factory);
        }

        const float padX = 18.0f * scale;
        const float padY = 12.0f * scale;

        // Reset hit targets
        lastCarouselHitTargets_ = CarouselHitTargets{};

        // Surface Brushes
        ComPtr<ID2D1SolidColorBrush> cardBrush;
        ComPtr<ID2D1SolidColorBrush> cardBorderBrush;
        ComPtr<ID2D1SolidColorBrush> textWhite;
        ComPtr<ID2D1SolidColorBrush> textMuted;
        ComPtr<ID2D1SolidColorBrush> accentBrush;

        target->CreateSolidColorBrush(D2D1::ColorF(0.08f, 0.08f, 0.09f, 0.85f), &cardBrush);
        target->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.08f), &cardBorderBrush);
        target->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.95f), &textWhite);
        target->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.55f), &textMuted);
        target->CreateSolidColorBrush(accentColor, &accentBrush);

        // --------------------------------------------------------------------
        // 1. Top Header Row: Title & Multi-Session Carousel Navigation
        // --------------------------------------------------------------------
        const float headerTop = rect.top + padY;
        const float headerBottom = headerTop + 24.0f * scale;

        // Main Title
        if (boldTextFormat && textWhite) {
            boldTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            boldTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            const wchar_t kTitle[] = L"ANTIGRAVITY TELEMETRY";
            target->DrawText(kTitle, static_cast<UINT32>(wcslen(kTitle)), boldTextFormat,
                             D2D1::RectF(rect.left + padX, headerTop, rect.right - padX, headerBottom),
                             textWhite.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }

        // Carousel Navigation / MRU Status
        if (hasMultiple) {
            DrawCarouselControls(target, smallTextFormat, textWhite.Get(), textMuted.Get(),
                                 accentBrush.Get(), rect, headerTop, headerBottom, scale,
                                 activeIndex, currentSessions.size());
        } else if (session && smallTextFormat && textMuted) {
            // Single session: Show Live MRU tag
            smallTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
            smallTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            std::wstring mruTag = autoFollow ? L"LIVE MRU \u25CF" : L"MANUAL";
            target->DrawText(mruTag.c_str(), static_cast<UINT32>(mruTag.size()), smallTextFormat,
                             D2D1::RectF(rect.left + padX, headerTop, rect.right - padX, headerBottom),
                             accentBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
            smallTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }

        // --------------------------------------------------------------------
        // 2. Main Content Split: Left Hero Gauge | Right Roster & Workspace
        // --------------------------------------------------------------------
        const float bodyTop = headerBottom + 6.0f * scale;
        const float bodyBottom = rect.bottom - padY;
        const float bodyWidth = (rect.right - rect.left) - padX * 2.0f;
        const float colGap = 10.0f * scale;
        const float leftColWidth = 120.0f * scale;
        const float rightColWidth = bodyWidth - leftColWidth - colGap;

        const D2D1_RECT_F heroCardRect = D2D1::RectF(
            rect.left + padX,
            bodyTop,
            rect.left + padX + leftColWidth,
            bodyBottom
        );

        const float rightColLeft = heroCardRect.right + colGap;
        const float rightColRight = rightColLeft + rightColWidth;

        // Draw Left Card: Hero Token Gauge
        DrawHeroTokenGauge(target, factory.Get(), boldTextFormat, smallTextFormat,
                           heroCardRect, session, scale, now, cardBrush.Get(), cardBorderBrush.Get(),
                           textWhite.Get(), textMuted.Get());

        // Draw Right Cards: Subagent Fleet Roster & Task/Workspace Tracker
        const float halfRightH = (bodyBottom - bodyTop - 6.0f * scale) * 0.5f;

        const D2D1_RECT_F rosterCardRect = D2D1::RectF(
            rightColLeft,
            bodyTop,
            rightColRight,
            bodyTop + halfRightH
        );

        const D2D1_RECT_F taskCardRect = D2D1::RectF(
            rightColLeft,
            bodyTop + halfRightH + 6.0f * scale,
            rightColRight,
            bodyBottom
        );

        DrawSubagentRoster(target, boldTextFormat, smallTextFormat, rosterCardRect, session,
                           scale, now, cardBrush.Get(), cardBorderBrush.Get(), textWhite.Get(), textMuted.Get());

        DrawWorkspaceAndTasks(target, textFormat, smallTextFormat, taskCardRect, session,
                              scale, now, cardBrush.Get(), cardBorderBrush.Get(), textWhite.Get(), textMuted.Get());
    }

private:
    mutable std::mutex stateMutex_;
    std::wstring primaryFilePath_;
    std::wstring brainScanDir_;
    std::wstring pipeName_ = kDefaultPipeName;
    HWND notifyHwnd_ = nullptr;

    std::vector<SessionTelemetry> sessions_;
    mutable std::vector<SessionTelemetry> sessionsSnapshot_;
    mutable SessionTelemetry activeSessionSnapshot_;
    mutable bool hasActiveSessionSnapshot_ = false;

    size_t selectedSessionIndex_ = 0;
    bool autoFollowMru_ = true;
    double checkInterval_ = 0.5;

    std::atomic<bool> hasUpdates_{false};
    std::atomic<bool> running_{false};
    std::atomic<HANDLE> hPipe_{INVALID_HANDLE_VALUE};
    std::atomic<HANDLE> hDir_{INVALID_HANDLE_VALUE};
    HANDLE stopEvent_ = NULL;
    std::thread workerThread_;

    CarouselHitTargets lastCarouselHitTargets_;

    // Notifies the island window that telemetry data has been updated
    void NotifyLayoutChanged() {
        HWND target = notifyHwnd_;
        if (!target || !IsWindow(target)) {
            target = FindWindowW(kIslandWindowClass, nullptr);
        }
        if (target && IsWindow(target)) {
            PostMessageW(target, WM_APP_LAYOUT_CHANGED, 0, 0);
        }
    }

    void UpdateActiveSnapshotLocked() {
        if (sessions_.empty()) {
            hasActiveSessionSnapshot_ = false;
            activeSessionSnapshot_ = SessionTelemetry{};
        } else {
            size_t idx = (selectedSessionIndex_ < sessions_.size()) ? selectedSessionIndex_ : 0;
            activeSessionSnapshot_ = sessions_[idx];
            hasActiveSessionSnapshot_ = true;
        }
        sessionsSnapshot_ = sessions_;
    }

    // Ingests a JSON snapshot string into the multi-session resolver
    bool IngestJsonPayload(const std::string& jsonContent, const std::wstring& sessionTag = L"pipe", uint64_t mtimeKey = 0) {
        SessionTelemetry telem;
        telem.sourceFilePath = sessionTag;
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        telem.lastWriteTime = ft;
        static uint64_t s_pipeSeq = 0;
        telem.mtimeKey = (mtimeKey != 0) ? mtimeKey : ((static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime | (++s_pipeSeq));

        if (!ParseSessionJson(jsonContent, telem)) {
            return false;
        }

        std::lock_guard<std::mutex> lock(stateMutex_);
        auto it = std::find_if(sessions_.begin(), sessions_.end(),
            [&sessionTag, &telem](const SessionTelemetry& s) {
                if (!telem.activeConversationId.empty() && !s.activeConversationId.empty()) {
                    return s.activeConversationId == telem.activeConversationId;
                }
                return s.sourceFilePath == sessionTag;
            });
        if (it != sessions_.end()) {
            *it = std::move(telem);
        } else {
            sessions_.push_back(std::move(telem));
        }

        std::sort(sessions_.begin(), sessions_.end(),
            [](const SessionTelemetry& a, const SessionTelemetry& b) {
                return a.mtimeKey > b.mtimeKey;
            });

        // Prune stale sessions older than 1 hour if multiple exist
        if (sessions_.size() > 1) {
            sessions_.erase(
                std::remove_if(sessions_.begin() + 1, sessions_.end(),
                    [](const SessionTelemetry& s) {
                        return s.IsStale(kSessionPruneThresholdSec);
                    }),
                sessions_.end()
            );
        }

        if (autoFollowMru_ || selectedSessionIndex_ >= sessions_.size()) {
            selectedSessionIndex_ = 0;
        }

        UpdateActiveSnapshotLocked();
        hasUpdates_.store(true);
        return true;
    }

    // Scans brain candidate files across active conversations
    void ScanBrainCandidatesLocked(std::vector<std::wstring>& outPaths) {
        if (brainScanDir_.empty()) return;

        std::wstring directPrimary = brainScanDir_ + L"\\island_telemetry.json";
        if (std::find(outPaths.begin(), outPaths.end(), directPrimary) == outPaths.end()) {
            if (FileExists(directPrimary)) outPaths.push_back(directPrimary);
        }

        std::wstring searchPattern = brainScanDir_ + L"\\*";
        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &fd);
        if (hFind == INVALID_HANDLE_VALUE) return;

        int scannedFolders = 0;
        do {
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0) {
                
                std::wstring subPath = brainScanDir_ + L"\\" + fd.cFileName;
                std::wstring cand1 = subPath + L"\\island_telemetry.json";
                std::wstring cand2 = subPath + L"\\scratch\\island_telemetry.json";
                std::wstring cand3 = subPath + L"\\agy_telemetry.json";

                if (FileExists(cand1) && std::find(outPaths.begin(), outPaths.end(), cand1) == outPaths.end()) outPaths.push_back(cand1);
                if (FileExists(cand2) && std::find(outPaths.begin(), outPaths.end(), cand2) == outPaths.end()) outPaths.push_back(cand2);
                if (FileExists(cand3) && std::find(outPaths.begin(), outPaths.end(), cand3) == outPaths.end()) outPaths.push_back(cand3);

                scannedFolders++;
                if (scannedFolders > 100) break;
            }
        } while (FindNextFileW(hFind, &fd));

        FindClose(hFind);
    }

    // Reloads session files from disk under lock with non-exclusive sharing
    bool ReloadFromDiskLocked() {
        bool changed = false;
        std::vector<std::wstring> candidatePaths;
        if (!primaryFilePath_.empty()) {
            candidatePaths.push_back(primaryFilePath_);
        }

        ScanBrainCandidatesLocked(candidatePaths);

        std::vector<SessionTelemetry> updatedSessions;

        for (const auto& path : candidatePaths) {
            WIN32_FILE_ATTRIBUTE_DATA fad = {};
            if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad)) {
                continue;
            }

            uint64_t mtime = (static_cast<uint64_t>(fad.ftLastWriteTime.dwHighDateTime) << 32) |
                              fad.ftLastWriteTime.dwLowDateTime;

            auto existingIt = std::find_if(sessions_.begin(), sessions_.end(),
                [&path](const SessionTelemetry& s) { return s.sourceFilePath == path; });

            if (existingIt != sessions_.end() && existingIt->mtimeKey == mtime && existingIt->isValid) {
                auto dupIt = std::find_if(updatedSessions.begin(), updatedSessions.end(),
                    [&](const SessionTelemetry& u) {
                        return !u.activeConversationId.empty() && u.activeConversationId == existingIt->activeConversationId;
                    });
                if (dupIt == updatedSessions.end()) {
                    updatedSessions.push_back(*existingIt);
                } else if (existingIt->mtimeKey > dupIt->mtimeKey) {
                    *dupIt = *existingIt;
                }
                continue;
            }

            // Read file with non-exclusive shared read/write/delete access
            std::string content = ReadFileShared(path);
            if (content.empty()) continue;

            SessionTelemetry telem;
            telem.sourceFilePath = path;
            telem.lastWriteTime = fad.ftLastWriteTime;
            telem.mtimeKey = mtime;

            if (ParseSessionJson(content, telem)) {
                auto dupIt = std::find_if(updatedSessions.begin(), updatedSessions.end(),
                    [&](const SessionTelemetry& u) {
                        return !u.activeConversationId.empty() && u.activeConversationId == telem.activeConversationId;
                    });
                if (dupIt == updatedSessions.end()) {
                    updatedSessions.push_back(std::move(telem));
                    changed = true;
                } else if (telem.mtimeKey > dupIt->mtimeKey) {
                    *dupIt = std::move(telem);
                    changed = true;
                }
            }
        }

        // Preserve active in-memory pipe sessions that were received recently
        for (const auto& existing : sessions_) {
            if (existing.sourceFilePath == L"pipe" && !existing.IsStale(kSessionPruneThresholdSec)) {
                auto matchIt = std::find_if(updatedSessions.begin(), updatedSessions.end(),
                    [&](const SessionTelemetry& u) {
                        return !u.activeConversationId.empty() && u.activeConversationId == existing.activeConversationId;
                    });
                if (matchIt == updatedSessions.end()) {
                    updatedSessions.push_back(existing);
                    changed = true;
                } else if (existing.mtimeKey > matchIt->mtimeKey) {
                    *matchIt = existing;
                    changed = true;
                }
            }
        }

        if (updatedSessions.size() != sessions_.size()) {
            changed = true;
        }

        if (changed || !updatedSessions.empty()) {
            std::sort(updatedSessions.begin(), updatedSessions.end(),
                [](const SessionTelemetry& a, const SessionTelemetry& b) {
                    return a.mtimeKey > b.mtimeKey;
                });

            // Prune stale sessions older than 1 hour if multiple exist
            if (updatedSessions.size() > 1) {
                updatedSessions.erase(
                    std::remove_if(updatedSessions.begin() + 1, updatedSessions.end(),
                        [](const SessionTelemetry& s) {
                            return s.IsStale(kSessionPruneThresholdSec);
                        }),
                    updatedSessions.end()
                );
            }

            // Cap at 8 active sessions
            if (updatedSessions.size() > 8) {
                updatedSessions.resize(8);
            }

            sessions_ = std::move(updatedSessions);

            if (autoFollowMru_ || selectedSessionIndex_ >= sessions_.size()) {
                selectedSessionIndex_ = 0;
            }

            UpdateActiveSnapshotLocked();
        }

        return changed;
    }

    // Dedicated background worker: Listens on Named Pipe & ReadDirectoryChangesW with zero polling
    void WorkerLoop() {
        std::wstring pipeName = pipeName_;

        SECURITY_DESCRIPTOR sd = {};
        InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
        SetSecurityDescriptorDacl(&sd, TRUE, nullptr, FALSE);
        SECURITY_ATTRIBUTES sa = {};
        sa.nLength = sizeof(sa);
        sa.lpSecurityDescriptor = &sd;
        sa.bInheritHandle = FALSE;

        HANDLE hPipe = CreateNamedPipeW(
            pipeName.c_str(),
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            65536,
            65536,
            1000,
            &sa
        );
        hPipe_.store(hPipe);

        HANDLE hPipeEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        OVERLAPPED pipeOverlapped = {};
        pipeOverlapped.hEvent = hPipeEvent;

        if (hPipe != INVALID_HANDLE_VALUE) {
            BOOL connRes = ConnectNamedPipe(hPipe, &pipeOverlapped);
            if (connRes) {
                SetEvent(hPipeEvent);
            } else {
                DWORD err = GetLastError();
                if (err == ERROR_PIPE_CONNECTED) {
                    SetEvent(hPipeEvent);
                }
            }
        }

        std::wstring watchDir;
        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            watchDir = !brainScanDir_.empty() ? brainScanDir_ : GetDirectoryFromPath(primaryFilePath_);
        }

        HANDLE hDir = INVALID_HANDLE_VALUE;
        HANDLE hDirEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        OVERLAPPED dirOverlapped = {};
        dirOverlapped.hEvent = hDirEvent;
        std::vector<BYTE> dirBuffer(8192);

        if (!watchDir.empty() && DirectoryExists(watchDir)) {
            hDir = CreateFileW(
                watchDir.c_str(),
                FILE_LIST_DIRECTORY,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                nullptr,
                OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                nullptr
            );
            hDir_.store(hDir);
            if (hDir != INVALID_HANDLE_VALUE) {
                ReadDirectoryChangesW(
                    hDir,
                    dirBuffer.data(),
                    static_cast<DWORD>(dirBuffer.size()),
                    TRUE,
                    FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE,
                    nullptr,
                    &dirOverlapped,
                    nullptr
                );
            }
        }

        while (running_.load()) {
            HANDLE waitHandles[3];
            DWORD waitCount = 0;

            waitHandles[waitCount++] = stopEvent_;
            if (hPipe != INVALID_HANDLE_VALUE && hPipeEvent != NULL) {
                waitHandles[waitCount++] = hPipeEvent;
            }
            if (hDir != INVALID_HANDLE_VALUE && hDirEvent != NULL) {
                waitHandles[waitCount++] = hDirEvent;
            }

            DWORD timeout = (hDir == INVALID_HANDLE_VALUE && !watchDir.empty()) ? 2000 : INFINITE;
            DWORD waitRes = WaitForMultipleObjects(waitCount, waitHandles, FALSE, timeout);

            if (!running_.load() || waitRes == WAIT_OBJECT_0) {
                break; // Stop requested
            }

            if (waitRes == WAIT_TIMEOUT) {
                // If watch directory was absent on start, attempt to attach now
                if (hDir == INVALID_HANDLE_VALUE && !watchDir.empty() && DirectoryExists(watchDir)) {
                    hDir = CreateFileW(
                        watchDir.c_str(),
                        FILE_LIST_DIRECTORY,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        nullptr,
                        OPEN_EXISTING,
                        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                        nullptr
                    );
                    hDir_.store(hDir);
                    if (hDir != INVALID_HANDLE_VALUE) {
                        ResetEvent(hDirEvent);
                        memset(&dirOverlapped, 0, sizeof(dirOverlapped));
                        dirOverlapped.hEvent = hDirEvent;
                        ReadDirectoryChangesW(
                            hDir,
                            dirBuffer.data(),
                            static_cast<DWORD>(dirBuffer.size()),
                            TRUE,
                            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE,
                            nullptr,
                            &dirOverlapped,
                            nullptr
                        );
                    }
                }
                continue;
            }

            // Check if Named Pipe received client message
            DWORD pipeWaitIdx = 1;
            if (hPipe != INVALID_HANDLE_VALUE && waitRes == (WAIT_OBJECT_0 + pipeWaitIdx)) {
                ResetEvent(hPipeEvent);
                DWORD bytesTransferred = 0;
                GetOverlappedResult(hPipe, &pipeOverlapped, &bytesTransferred, FALSE);

                std::string payload;
                std::vector<char> buf(65536);
                OVERLAPPED readOverlapped = {};
                readOverlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

                while (running_.load()) {
                    ResetEvent(readOverlapped.hEvent);
                    DWORD bytesRead = 0;
                    BOOL ok = ReadFile(hPipe, buf.data(), static_cast<DWORD>(buf.size()), &bytesRead, &readOverlapped);
                    if (!ok) {
                        DWORD err = GetLastError();
                        if (err == ERROR_IO_PENDING) {
                            HANDLE waitRead[2] = { stopEvent_, readOverlapped.hEvent };
                            DWORD rWait = WaitForMultipleObjects(2, waitRead, FALSE, 1000);
                            if (rWait == WAIT_OBJECT_0 + 1) {
                                if (GetOverlappedResult(hPipe, &readOverlapped, &bytesRead, FALSE)) {
                                    ok = TRUE;
                                }
                            } else {
                                CancelIoEx(hPipe, &readOverlapped);
                                break;
                            }
                        } else if (err == ERROR_BROKEN_PIPE || err == ERROR_PIPE_NOT_CONNECTED) {
                            break;
                        }
                    }

                    if (ok && bytesRead > 0) {
                        payload.append(buf.data(), bytesRead);
                        // Check if payload is completed (newline terminated or valid JSON)
                        if (!payload.empty() && (payload.back() == '\n' || payload.back() == '}')) {
                            json::Value testVal;
                            if (json::Parse(payload, testVal)) {
                                break;
                            }
                        }
                    } else {
                        break;
                    }
                }

                if (readOverlapped.hEvent) {
                    CloseHandle(readOverlapped.hEvent);
                }

                if (!payload.empty()) {
                    if (IngestJsonPayload(payload, L"pipe")) {
                        NotifyLayoutChanged();
                    }
                }

                DisconnectNamedPipe(hPipe);
                memset(&pipeOverlapped, 0, sizeof(pipeOverlapped));
                pipeOverlapped.hEvent = hPipeEvent;
                BOOL nextConn = ConnectNamedPipe(hPipe, &pipeOverlapped);
                if (nextConn) {
                    SetEvent(hPipeEvent);
                } else {
                    DWORD err = GetLastError();
                    if (err == ERROR_PIPE_CONNECTED) {
                        SetEvent(hPipeEvent);
                    }
                }
                continue;
            }

            // Check if Directory Watcher signaled a change
            DWORD dirWaitIdx = (hPipe != INVALID_HANDLE_VALUE) ? 2 : 1;
            if (hDir != INVALID_HANDLE_VALUE && waitRes == (WAIT_OBJECT_0 + dirWaitIdx)) {
                ResetEvent(hDirEvent);
                DWORD bytesTransferred = 0;
                GetOverlappedResult(hDir, &dirOverlapped, &bytesTransferred, FALSE);

                // Settle delay to avoid reading intermediate file flushes
                Sleep(40);

                bool changed = false;
                {
                    std::lock_guard<std::mutex> lock(stateMutex_);
                    changed = ReloadFromDiskLocked();
                }
                if (changed) {
                    hasUpdates_.store(true);
                    NotifyLayoutChanged();
                }

                // Re-arm directory watcher
                memset(&dirOverlapped, 0, sizeof(dirOverlapped));
                dirOverlapped.hEvent = hDirEvent;
                ReadDirectoryChangesW(
                    hDir,
                    dirBuffer.data(),
                    static_cast<DWORD>(dirBuffer.size()),
                    TRUE,
                    FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE,
                    nullptr,
                    &dirOverlapped,
                    nullptr
                );
                continue;
            }
        }

        // Clean worker termination
        if (hPipe != INVALID_HANDLE_VALUE) {
            CancelIoEx(hPipe, nullptr);
            CloseHandle(hPipe);
            hPipe_.store(INVALID_HANDLE_VALUE);
        }
        if (hDir != INVALID_HANDLE_VALUE) {
            CancelIoEx(hDir, nullptr);
            CloseHandle(hDir);
            hDir_.store(INVALID_HANDLE_VALUE);
        }
        if (hPipeEvent) CloseHandle(hPipeEvent);
        if (hDirEvent) CloseHandle(hDirEvent);
    }

    // Direct2D Helper: Draw rounded card background with subtle contour border
    void DrawCard(ID2D1RenderTarget* target, D2D1_RECT_F rect, float radius,
                  ID2D1Brush* fillBrush, ID2D1Brush* borderBrush) {
        D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(rect, radius, radius);
        if (fillBrush) target->FillRoundedRectangle(rr, fillBrush);
        if (borderBrush) target->DrawRoundedRectangle(rr, borderBrush, 1.0f);
    }

    // Direct2D Helper: Circular Token Gauge Ring
    void DrawRingArc(ID2D1RenderTarget* target, ID2D1Factory* factory,
                     D2D1_POINT_2F center, float radius, float strokeWidth, float ratio,
                     ID2D1Brush* arcBrush, ID2D1Brush* trackBrush) {
        if (trackBrush) {
            target->DrawEllipse(D2D1::Ellipse(center, radius, radius), trackBrush, strokeWidth);
        }

        ratio = std::clamp(ratio, 0.0f, 1.0f);
        if (ratio <= 0.001f || !arcBrush || !factory) return;

        ComPtr<ID2D1PathGeometry> geom;
        if (FAILED(factory->CreatePathGeometry(&geom)) || !geom) return;

        ComPtr<ID2D1GeometrySink> sink;
        if (FAILED(geom->Open(&sink)) || !sink) return;

        const float startAngle = -3.14159265f * 0.5f; // 12 o'clock
        const float sweepAngle = 2.0f * 3.14159265f * ratio;
        const int segments = std::max(4, static_cast<int>(48.0f * ratio));

        auto getPt = [&](float a) {
            return D2D1::Point2F(center.x + std::cos(a) * radius,
                                 center.y + std::sin(a) * radius);
        };

        sink->BeginFigure(getPt(startAngle), D2D1_FIGURE_BEGIN_HOLLOW);
        for (int i = 1; i <= segments; ++i) {
            float a = startAngle + sweepAngle * (static_cast<float>(i) / static_cast<float>(segments));
            sink->AddLine(getPt(a));
        }
        sink->EndFigure(D2D1_FIGURE_END_OPEN);
        sink->Close();

        target->DrawGeometry(geom.Get(), arcBrush, strokeWidth);
    }

    // Draw Hero Token Gauge (Circular Ring + 3-tier colors + context readouts)
    void DrawHeroTokenGauge(ID2D1RenderTarget* target, ID2D1Factory* factory,
                            IDWriteTextFormat* boldTextFormat, IDWriteTextFormat* smallTextFormat,
                            D2D1_RECT_F cardRect, const SessionTelemetry* session, float scale,
                            double now, ID2D1Brush* cardBrush, ID2D1Brush* borderBrush,
                            ID2D1Brush* textWhite, ID2D1Brush* textMuted) {
        DrawCard(target, cardRect, 10.0f * scale, cardBrush, borderBrush);

        const float ringRadius = 26.0f * scale;
        const float strokeWidth = 4.2f * scale;
        const D2D1_POINT_2F ringCenter = D2D1::Point2F(
            (cardRect.left + cardRect.right) * 0.5f,
            cardRect.top + 38.0f * scale
        );

        float ratio = session ? session->GetRatio() : 0.0f;
        D2D1_COLOR_F gaugeColor = GetTokenGaugeColor(ratio, now);

        ComPtr<ID2D1SolidColorBrush> arcBrush;
        ComPtr<ID2D1SolidColorBrush> trackBrush;
        target->CreateSolidColorBrush(gaugeColor, &arcBrush);
        target->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.09f), &trackBrush);

        if (ratio >= 0.80f) {
            float glowRadius = ringRadius + (3.5f + 2.5f * std::sin(static_cast<float>(now) * 5.0f)) * scale;
            float glowAlpha = 0.25f + 0.20f * std::sin(static_cast<float>(now) * 6.28318f);
            ComPtr<ID2D1SolidColorBrush> glowBrush;
            target->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0.271f, 0.227f, glowAlpha), &glowBrush);
            if (glowBrush) {
                target->DrawEllipse(D2D1::Ellipse(ringCenter, glowRadius, glowRadius), glowBrush.Get(), 1.5f * scale);
            }
        }

        DrawRingArc(target, factory, ringCenter, ringRadius, strokeWidth, ratio, arcBrush.Get(), trackBrush.Get());

        wchar_t pctBuf[16];
        swprintf_s(pctBuf, L"%.0f%%", ratio * 100.0f);
        if (boldTextFormat && textWhite) {
            boldTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            boldTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            target->DrawText(pctBuf, static_cast<UINT32>(wcslen(pctBuf)), boldTextFormat,
                             D2D1::RectF(ringCenter.x - ringRadius, ringCenter.y - 12.0f * scale,
                                         ringCenter.x + ringRadius, ringCenter.y + 12.0f * scale),
                             textWhite, D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }

        uint64_t curTokens = session ? session->contextTokens : 0;
        uint64_t maxTok = session ? session->maxTokens : 1048576;
        std::wstring tokenStr = FormatTokenCount(curTokens) + L" / " + FormatTokenCount(maxTok);

        if (smallTextFormat && textWhite && textMuted) {
            smallTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            smallTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

            target->DrawText(tokenStr.c_str(), static_cast<UINT32>(tokenStr.size()), smallTextFormat,
                             D2D1::RectF(cardRect.left + 4.0f * scale, cardRect.top + 70.0f * scale,
                                         cardRect.right - 4.0f * scale, cardRect.top + 86.0f * scale),
                             textWhite, D2D1_DRAW_TEXT_OPTIONS_CLIP);

            const wchar_t kLabel[] = L"CONTEXT LIMIT";
            target->DrawText(kLabel, static_cast<UINT32>(wcslen(kLabel)), smallTextFormat,
                             D2D1::RectF(cardRect.left + 4.0f * scale, cardRect.top + 86.0f * scale,
                                         cardRect.right - 4.0f * scale, cardRect.bottom - 2.0f * scale),
                             textMuted, D2D1_DRAW_TEXT_OPTIONS_CLIP);

            smallTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }
    }

    // Draw Subagent Fleet Roster
    void DrawSubagentRoster(ID2D1RenderTarget* target, IDWriteTextFormat* boldTextFormat,
                            IDWriteTextFormat* smallTextFormat, D2D1_RECT_F cardRect,
                            const SessionTelemetry* session, float scale, double now,
                            ID2D1Brush* cardBrush, ID2D1Brush* borderBrush,
                            ID2D1Brush* textWhite, ID2D1Brush* textMuted) {
        DrawCard(target, cardRect, 10.0f * scale, cardBrush, borderBrush);

        const float pad = 10.0f * scale;
        const float headerH = 16.0f * scale;

        if (boldTextFormat && textWhite && textMuted) {
            boldTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            boldTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

            size_t totalAgents = session ? session->activeSubagents.size() : 0;
            size_t runningAgents = session ? session->GetRunningSubagentsCount() : 0;
            wchar_t titleBuf[64];
            swprintf_s(titleBuf, L"SUBAGENTS (%zu RUNNING / %zu)", runningAgents, totalAgents);

            target->DrawText(titleBuf, static_cast<UINT32>(wcslen(titleBuf)), smallTextFormat,
                             D2D1::RectF(cardRect.left + pad, cardRect.top + 4.0f * scale,
                                         cardRect.right - pad, cardRect.top + 4.0f * scale + headerH),
                             textMuted, D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }

        float chipTop = cardRect.top + headerH + 6.0f * scale;
        const float chipH = 18.0f * scale;
        const float chipGap = 4.0f * scale;

        if (!session || session->activeSubagents.empty()) {
            if (smallTextFormat && textMuted) {
                const wchar_t kIdleMsg[] = L"Single Agent (No active subagent workers)";
                target->DrawText(kIdleMsg, static_cast<UINT32>(wcslen(kIdleMsg)), smallTextFormat,
                                 D2D1::RectF(cardRect.left + pad, chipTop, cardRect.right - pad, chipTop + chipH),
                                 textMuted, D2D1_DRAW_TEXT_OPTIONS_CLIP);
            }
            return;
        }

        size_t displayCount = std::min<size_t>(session->activeSubagents.size(), 2);
        for (size_t i = 0; i < displayCount; ++i) {
            const auto& sa = session->activeSubagents[i];
            const D2D1_RECT_F chipRect = D2D1::RectF(
                cardRect.left + pad,
                chipTop,
                cardRect.right - pad,
                chipTop + chipH
            );

            ComPtr<ID2D1SolidColorBrush> chipBg;
            target->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.05f), &chipBg);
            target->FillRoundedRectangle(D2D1::RoundedRect(chipRect, 4.0f * scale, 4.0f * scale), chipBg.Get());

            const bool isFresh = session && (session->GetAgeSeconds() <= kRunningStaleThresholdSec);
            D2D1_COLOR_F dotColor = D2D1::ColorF(1.0f, 0.624f, 0.039f, 0.9f);
            if (sa.IsRunning() && isFresh) {
                float pulse = 0.70f + 0.30f * std::sin(static_cast<float>(now) * 4.0f);
                dotColor = D2D1::ColorF(0.204f, 0.780f, 0.349f, pulse);
            } else if (sa.IsErrored()) {
                dotColor = D2D1::ColorF(1.0f, 0.271f, 0.227f, 1.0f);
            } else if (sa.IsRunning() && !isFresh) {
                dotColor = D2D1::ColorF(0.6f, 0.6f, 0.6f, 0.6f);
            }

            ComPtr<ID2D1SolidColorBrush> dotBrush;
            target->CreateSolidColorBrush(dotColor, &dotBrush);
            const D2D1_POINT_2F dotCenter = D2D1::Point2F(chipRect.left + 8.0f * scale, chipRect.top + chipH * 0.5f);
            target->FillEllipse(D2D1::Ellipse(dotCenter, 3.2f * scale, 3.2f * scale), dotBrush.Get());

            std::wstring label = sa.role.empty() ? sa.name : sa.role;
            if (smallTextFormat && textWhite) {
                smallTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                target->DrawText(label.c_str(), static_cast<UINT32>(label.size()), smallTextFormat,
                                 D2D1::RectF(chipRect.left + 16.0f * scale, chipRect.top,
                                             chipRect.right - 54.0f * scale, chipRect.bottom),
                                 textWhite, D2D1_DRAW_TEXT_OPTIONS_CLIP);

                smallTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
                std::wstring displayState = (sa.IsRunning() && !isFresh) ? L"stalled" : sa.state;
                target->DrawText(displayState.c_str(), static_cast<UINT32>(displayState.size()), smallTextFormat,
                                 D2D1::RectF(chipRect.right - 52.0f * scale, chipRect.top,
                                             chipRect.right - 6.0f * scale, chipRect.bottom),
                                 dotBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
                smallTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            }

            chipTop += chipH + chipGap;
        }

        if (session->activeSubagents.size() > 2 && smallTextFormat && textMuted) {
            wchar_t moreBuf[24];
            swprintf_s(moreBuf, L"+%zu more", session->activeSubagents.size() - 2);
            smallTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
            target->DrawText(moreBuf, static_cast<UINT32>(wcslen(moreBuf)), smallTextFormat,
                             D2D1::RectF(cardRect.left + pad, cardRect.bottom - 14.0f * scale,
                                         cardRect.right - pad, cardRect.bottom - 2.0f * scale),
                             textMuted, D2D1_DRAW_TEXT_OPTIONS_CLIP);
            smallTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }
    }

    // Draw Workspace & Background Task Tracker
    void DrawWorkspaceAndTasks(ID2D1RenderTarget* target, IDWriteTextFormat* textFormat,
                               IDWriteTextFormat* smallTextFormat, D2D1_RECT_F cardRect,
                               const SessionTelemetry* session, float scale, double now,
                               ID2D1Brush* cardBrush, ID2D1Brush* borderBrush,
                               ID2D1Brush* textWhite, ID2D1Brush* textMuted) {
        DrawCard(target, cardRect, 10.0f * scale, cardBrush, borderBrush);

        const float pad = 10.0f * scale;

        std::wstring taskLine;
        if (session && !session->activeTasks.empty()) {
            const auto& t = session->activeTasks[0];
            taskLine = L"\u2699 " + (t.summary.empty() ? t.id : t.summary);
            if (!t.status.empty()) taskLine += L" (" + t.status + L")";
        } else if (session && !session->lastCompletedEvent.empty()) {
            taskLine = L"\u2713 " + session->lastCompletedEvent;
        } else {
            taskLine = L"\u2713 Idle / Ready for prompt";
        }

        if (smallTextFormat && textWhite) {
            smallTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            target->DrawText(taskLine.c_str(), static_cast<UINT32>(taskLine.size()), smallTextFormat,
                             D2D1::RectF(cardRect.left + pad, cardRect.top + 4.0f * scale,
                                         cardRect.right - pad, cardRect.top + 22.0f * scale),
                             textWhite, D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }

        std::wstring wsLine;
        if (session && (!session->workspace.branch.empty() || !session->workspace.folder.empty())) {
            wsLine = L"[Branch: " + (session->workspace.branch.empty() ? L"main" : session->workspace.branch) + L"]";
            if (!session->workspace.folder.empty()) {
                wsLine += L"  [Folder: " + session->workspace.folder + L"]";
            }
            if (session->workspace.changedFiles > 0) {
                wsLine += L"  [" + std::to_wstring(session->workspace.changedFiles) + L" modified]";
            }
        } else {
            wsLine = L"[Branch: main]  [Clean working tree]";
        }

        if (smallTextFormat && textMuted) {
            smallTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            target->DrawText(wsLine.c_str(), static_cast<UINT32>(wsLine.size()), smallTextFormat,
                             D2D1::RectF(cardRect.left + pad, cardRect.top + 22.0f * scale,
                                         cardRect.right - pad, cardRect.bottom - 4.0f * scale),
                             textMuted, D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }
    }

    // Carousel Navigation Controls (Left/Right chevrons + Session indicator + Dots)
    void DrawCarouselControls(ID2D1RenderTarget* target, IDWriteTextFormat* smallTextFormat,
                              ID2D1Brush* textWhite, ID2D1Brush* textMuted, ID2D1Brush* accentBrush,
                              D2D1_RECT_F fullRect, float top, float bottom, float scale,
                              size_t selectedIndex, size_t totalCount) {
        if (!smallTextFormat) return;

        const float padX = 18.0f * scale;
        const float navW = 85.0f * scale;
        const float navRight = fullRect.right - padX;
        const float navLeft = navRight - navW;

        const float btnW = 16.0f * scale;
        lastCarouselHitTargets_.prevBtn = D2D1::RectF(navLeft, top, navLeft + btnW, bottom);
        lastCarouselHitTargets_.nextBtn = D2D1::RectF(navRight - btnW, top, navRight, bottom);

        smallTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        smallTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        const wchar_t kPrev[] = L"\u25C0";
        target->DrawText(kPrev, 1, smallTextFormat, lastCarouselHitTargets_.prevBtn,
                         textMuted, D2D1_DRAW_TEXT_OPTIONS_NONE);

        const wchar_t kNext[] = L"\u25B6";
        target->DrawText(kNext, 1, smallTextFormat, lastCarouselHitTargets_.nextBtn,
                         textMuted, D2D1_DRAW_TEXT_OPTIONS_NONE);

        wchar_t countBuf[32];
        swprintf_s(countBuf, L"%zu / %zu", selectedIndex + 1, totalCount);
        const D2D1_RECT_F labelRect = D2D1::RectF(navLeft + btnW, top, navRight - btnW, bottom);
        target->DrawText(countBuf, static_cast<UINT32>(wcslen(countBuf)), smallTextFormat,
                         labelRect, textWhite, D2D1_DRAW_TEXT_OPTIONS_CLIP);

        lastCarouselHitTargets_.dots.clear();
        if (totalCount > 1 && totalCount <= 8) {
            float dotY = bottom + 2.0f * scale;
            float totalDotsW = static_cast<float>(totalCount - 1) * 8.0f * scale;
            float startDotX = (labelRect.left + labelRect.right) * 0.5f - totalDotsW * 0.5f;

            for (size_t i = 0; i < totalCount; ++i) {
                float dx = startDotX + static_cast<float>(i) * 8.0f * scale;
                D2D1_POINT_2F dotPt = D2D1::Point2F(dx, dotY);
                bool isSelected = (i == selectedIndex);
                float r = isSelected ? (2.8f * scale) : (1.8f * scale);
                target->FillEllipse(D2D1::Ellipse(dotPt, r, r),
                                    isSelected ? accentBrush : textMuted);

                lastCarouselHitTargets_.dots.push_back(D2D1::RectF(
                    dx - 4.0f * scale, dotY - 4.0f * scale,
                    dx + 4.0f * scale, dotY + 4.0f * scale
                ));
            }
        }

        smallTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    }
};

} // namespace agy

#endif // AGY_TELEMETRY_ENGINE_HPP
