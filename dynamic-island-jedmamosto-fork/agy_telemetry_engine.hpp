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
    std::wstring conversationTitle;
    int currentTurn = 0;
    int currentStep = 0;
    std::wstring lastMessageSender = L"AGENT"; // e.g. L"USER" or L"AGENT"
    std::wstring lastMessageSnippet;
    float gemini5HourRemainingFraction = 0.80f; // 0.0 to 1.0
    std::wstring gemini5HourResetCountdown = L"Resets in 4h 27m";
    float geminiWeeklyRemainingFraction = 0.48f; // 0.0 to 1.0
    std::wstring geminiWeeklyResetCountdown = L"Resets in 5d 23h";
    float compactionThresholdFraction = 0.80f; // 0.0 to 1.0

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

    bool IsCompactionWarning() const {
        return GetRatio() >= compactionThresholdFraction;
    }

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

    // 1. conversationTitle with fallback to sessionName or activeConversationId
    if (root.HasKey("conversationTitle") && !root["conversationTitle"].AsWString().empty()) {
        outTelemetry.conversationTitle = root["conversationTitle"].AsWString();
    } else if (root.HasKey("title") && !root["title"].AsWString().empty()) {
        outTelemetry.conversationTitle = root["title"].AsWString();
    } else if (!outTelemetry.sessionName.empty()) {
        outTelemetry.conversationTitle = outTelemetry.sessionName;
    } else {
        outTelemetry.conversationTitle = outTelemetry.activeConversationId;
    }

    outTelemetry.contextTokens = root["contextTokens"].AsUInt64(0);
    outTelemetry.maxTokens = root["maxTokens"].AsUInt64(1048576);
    outTelemetry.percentUtilized = static_cast<float>(root["percentUtilized"].AsDouble(0.0));

    if (outTelemetry.percentUtilized <= 0.0f && outTelemetry.maxTokens > 0 && outTelemetry.contextTokens > 0) {
        outTelemetry.percentUtilized = static_cast<float>(
            (static_cast<double>(outTelemetry.contextTokens) / static_cast<double>(outTelemetry.maxTokens)) * 100.0
        );
    }

    // 2. Gemini 5-hour limit
    if (root.HasKey("gemini5HourRemaining")) {
        double val = root["gemini5HourRemaining"].AsDouble(0.80);
        outTelemetry.gemini5HourRemainingFraction = static_cast<float>((val > 1.0) ? (val / 100.0) : val);
    } else if (root.HasKey("gemini5HourRemainingFraction")) {
        double val = root["gemini5HourRemainingFraction"].AsDouble(0.80);
        outTelemetry.gemini5HourRemainingFraction = static_cast<float>((val > 1.0) ? (val / 100.0) : val);
    } else {
        outTelemetry.gemini5HourRemainingFraction = 0.80f;
    }

    if (root.HasKey("gemini5HourReset")) {
        outTelemetry.gemini5HourResetCountdown = root["gemini5HourReset"].AsWString();
    } else if (root.HasKey("gemini5HourResetCountdown")) {
        outTelemetry.gemini5HourResetCountdown = root["gemini5HourResetCountdown"].AsWString();
    } else {
        outTelemetry.gemini5HourResetCountdown = L"Resets in 4h 27m";
    }

    // 3. Gemini weekly limit
    if (root.HasKey("geminiWeeklyRemaining")) {
        double val = root["geminiWeeklyRemaining"].AsDouble(0.48);
        outTelemetry.geminiWeeklyRemainingFraction = static_cast<float>((val > 1.0) ? (val / 100.0) : val);
    } else if (root.HasKey("geminiWeeklyRemainingFraction")) {
        double val = root["geminiWeeklyRemainingFraction"].AsDouble(0.48);
        outTelemetry.geminiWeeklyRemainingFraction = static_cast<float>((val > 1.0) ? (val / 100.0) : val);
    } else {
        outTelemetry.geminiWeeklyRemainingFraction = 0.48f;
    }

    if (root.HasKey("geminiWeeklyReset")) {
        outTelemetry.geminiWeeklyResetCountdown = root["geminiWeeklyReset"].AsWString();
    } else if (root.HasKey("geminiWeeklyResetCountdown")) {
        outTelemetry.geminiWeeklyResetCountdown = root["geminiWeeklyResetCountdown"].AsWString();
    } else {
        outTelemetry.geminiWeeklyResetCountdown = L"Resets in 5d 23h";
    }

    // 4. Compaction threshold (default 0.80f)
    if (root.HasKey("compactionThreshold")) {
        double val = root["compactionThreshold"].AsDouble(0.80);
        outTelemetry.compactionThresholdFraction = static_cast<float>((val > 1.0) ? (val / 100.0) : val);
    } else if (root.HasKey("compactionThresholdFraction")) {
        double val = root["compactionThresholdFraction"].AsDouble(0.80);
        outTelemetry.compactionThresholdFraction = static_cast<float>((val > 1.0) ? (val / 100.0) : val);
    } else {
        outTelemetry.compactionThresholdFraction = 0.80f;
    }

    // 5. Active Subagents
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

    // 6. Active Tasks
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

    // 7. Workspace
    const auto& wsObj = root["workspace"];
    if (wsObj.IsObject()) {
        outTelemetry.workspace.folder = wsObj["folder"].AsWString();
        outTelemetry.workspace.branch = wsObj["branch"].AsWString();
        outTelemetry.workspace.changedFiles = wsObj["changedFiles"].AsInt(0);
    }

    // 8. lastCompletedEvent
    outTelemetry.lastCompletedEvent = root["lastCompletedEvent"].AsWString();

    // 9. currentTurn and currentStep (with fallbacks if activeTasks/events present)
    outTelemetry.currentTurn = root["currentTurn"].AsInt(root["turn"].AsInt(0));
    outTelemetry.currentStep = root["currentStep"].AsInt(root["step"].AsInt(0));

    if (outTelemetry.currentStep == 0) {
        for (const auto& t : outTelemetry.activeTasks) {
            int parsedStep = 0;
            if (swscanf_s(t.id.c_str(), L"step-%d", &parsedStep) == 1 ||
                swscanf_s(t.id.c_str(), L"task-%d", &parsedStep) == 1) {
                if (parsedStep > outTelemetry.currentStep) {
                    outTelemetry.currentStep = parsedStep;
                }
            }
        }
        if (outTelemetry.currentStep == 0 && !outTelemetry.lastCompletedEvent.empty()) {
            int parsedStep = 0;
            if (swscanf_s(outTelemetry.lastCompletedEvent.c_str(), L"step-%d", &parsedStep) == 1 ||
                swscanf_s(outTelemetry.lastCompletedEvent.c_str(), L"Step %d", &parsedStep) == 1) {
                outTelemetry.currentStep = parsedStep;
            }
        }
    }
    if (outTelemetry.currentTurn == 0) {
        if (outTelemetry.currentStep > 0) {
            outTelemetry.currentTurn = std::max(1, (outTelemetry.currentStep + 3) / 4);
        } else if (!outTelemetry.activeTasks.empty() || !outTelemetry.lastCompletedEvent.empty()) {
            outTelemetry.currentTurn = 1;
        }
    }

    // 10. lastMessageSender and lastMessageSnippet
    outTelemetry.lastMessageSender = root["lastMessageSender"].AsWString();
    if (outTelemetry.lastMessageSender.empty()) {
        outTelemetry.lastMessageSender = root["sender"].AsWString();
    }
    outTelemetry.lastMessageSnippet = root["lastMessageSnippet"].AsWString();
    if (outTelemetry.lastMessageSnippet.empty()) {
        outTelemetry.lastMessageSnippet = root["snippet"].AsWString();
    }
    if (outTelemetry.lastMessageSnippet.empty()) {
        outTelemetry.lastMessageSnippet = root["lastMessage"].AsWString();
    }

    if (outTelemetry.lastMessageSnippet.empty()) {
        if (!outTelemetry.lastCompletedEvent.empty()) {
            outTelemetry.lastMessageSnippet = outTelemetry.lastCompletedEvent;
        } else if (!outTelemetry.activeTasks.empty() && !outTelemetry.activeTasks[0].summary.empty()) {
            outTelemetry.lastMessageSnippet = outTelemetry.activeTasks[0].summary;
        }
    }

    if (outTelemetry.lastMessageSender.empty()) {
        if (!outTelemetry.lastMessageSnippet.empty()) {
            std::wstring snippetLower = outTelemetry.lastMessageSnippet;
            for (auto& c : snippetLower) c = towlower(c);
            if (snippetLower.find(L"user:") != std::wstring::npos ||
                snippetLower.find(L"prompt:") != std::wstring::npos) {
                outTelemetry.lastMessageSender = L"USER";
            } else {
                outTelemetry.lastMessageSender = L"AGENT";
            }
        } else {
            outTelemetry.lastMessageSender = L"AGENT";
        }
    } else {
        std::wstring sUpper = outTelemetry.lastMessageSender;
        for (auto& c : sUpper) c = towupper(c);
        if (sUpper.find(L"USER") != std::wstring::npos) {
            outTelemetry.lastMessageSender = L"USER";
        } else {
            outTelemetry.lastMessageSender = L"AGENT";
        }
    }

    // 11. updatedAt
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

        // Surface Brushes & Semantic Colors
        const D2D1_COLOR_F kCardBg       = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.055f);
        const D2D1_COLOR_F kCardBorder   = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.085f);
        const D2D1_COLOR_F kTrackBg      = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.090f);
        const D2D1_COLOR_F kTextPrimary  = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.96f);
        const D2D1_COLOR_F kTextMuted    = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.60f);
        const D2D1_COLOR_F kTextTertiary = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.38f);
        const D2D1_COLOR_F kAppleGreen   = D2D1::ColorF(0.204f, 0.780f, 0.349f, 1.0f); // #34C759
        const D2D1_COLOR_F kAppleAmber   = D2D1::ColorF(1.000f, 0.690f, 0.129f, 1.0f); // #FFB021
        const D2D1_COLOR_F kAppleRed     = D2D1::ColorF(1.000f, 0.349f, 0.322f, 1.0f); // #FF5952
        const D2D1_COLOR_F kGeminiCyan   = D2D1::ColorF(0.298f, 0.788f, 0.941f, 1.0f); // #4CC9F0
        const D2D1_COLOR_F kGeminiPurple = D2D1::ColorF(0.608f, 0.447f, 0.796f, 1.0f); // #9B72CB

        ComPtr<ID2D1SolidColorBrush> bentoBgBrush;
        ComPtr<ID2D1SolidColorBrush> bentoBorderBrush;
        ComPtr<ID2D1SolidColorBrush> textWhiteBrush;
        ComPtr<ID2D1SolidColorBrush> textMutedBrush;
        ComPtr<ID2D1SolidColorBrush> textTertiaryBrush;
        ComPtr<ID2D1SolidColorBrush> cyanBrush;
        ComPtr<ID2D1SolidColorBrush> purpleBrush;
        ComPtr<ID2D1SolidColorBrush> greenBrush;
        ComPtr<ID2D1SolidColorBrush> amberBrush;
        ComPtr<ID2D1SolidColorBrush> redBrush;
        ComPtr<ID2D1SolidColorBrush> trackBgBrush;

        target->CreateSolidColorBrush(kCardBg, &bentoBgBrush);
        target->CreateSolidColorBrush(kCardBorder, &bentoBorderBrush);
        target->CreateSolidColorBrush(kTextPrimary, &textWhiteBrush);
        target->CreateSolidColorBrush(kTextMuted, &textMutedBrush);
        target->CreateSolidColorBrush(kTextTertiary, &textTertiaryBrush);
        target->CreateSolidColorBrush(kGeminiCyan, &cyanBrush);
        target->CreateSolidColorBrush(kGeminiPurple, &purpleBrush);
        target->CreateSolidColorBrush(kAppleGreen, &greenBrush);
        target->CreateSolidColorBrush(kAppleAmber, &amberBrush);
        target->CreateSolidColorBrush(kAppleRed, &redBrush);
        target->CreateSolidColorBrush(kTrackBg, &trackBgBrush);

        static ComPtr<IDWriteFactory> s_dwriteFactory;
        if (!s_dwriteFactory) {
            DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(s_dwriteFactory.GetAddressOf()));
        }

        // Micro format for 9.5px crisp labels & uppercase tags
        ComPtr<IDWriteTextFormat> microFormat;
        if (s_dwriteFactory) {
            s_dwriteFactory->CreateTextFormat(
                L"Segoe UI", nullptr,
                DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                9.5f * scale, L"", &microFormat
            );
        }
        if (!microFormat) {
            microFormat = smallTextFormat;
        }

        const float totalHeight = (rect.bottom - rect.top) - padY * 2.0f;
        const float vFit = std::clamp(totalHeight / 190.0f, 0.85f, 1.15f);

        // --------------------------------------------------------------------
        // ZONE A: Header Row (Sparkle Icon, Conversation Title, Turn/Step Pill)
        // --------------------------------------------------------------------
        const float headerTop = rect.top + padY;
        const float headerHeight = 22.0f * scale * vFit;
        const float headerBottom = headerTop + headerHeight;

        // Turn & Step Pill Geometry
        const float turnPillW = (hasMultiple ? 154.0f : 126.0f) * scale;
        const float turnPillH = 20.0f * scale;
        const D2D1_RECT_F turnPillRect = D2D1::RectF(
            rect.right - padX - turnPillW,
            headerTop + (headerHeight - turnPillH) * 0.5f,
            rect.right - padX,
            headerTop + (headerHeight - turnPillH) * 0.5f + turnPillH
        );

        // A1. Antigravity Sparkle Glyph ("✦")
        if (boldTextFormat && cyanBrush) {
            D2D1_RECT_F iconRect = D2D1::RectF(rect.left + padX, headerTop, rect.left + padX + 16.0f * scale, headerBottom);
            boldTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            boldTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            const wchar_t kSparkle[] = L"\u2726"; // ✦
            target->DrawText(kSparkle, 1, boldTextFormat, iconRect, cyanBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }

        // A2. Conversation Title
        if (textWhiteBrush) {
            std::wstring convTitle = (session && !session->conversationTitle.empty())
                ? session->conversationTitle
                : L"Personal Windhawk Mods";
            D2D1_RECT_F titleRect = D2D1::RectF(
                rect.left + padX + 18.0f * scale,
                headerTop,
                turnPillRect.left - 8.0f * scale,
                headerBottom
            );
            IDWriteTextFormat* tf = boldTextFormat ? boldTextFormat : textFormat;
            if (tf) {
                tf->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                tf->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                target->DrawText(convTitle.c_str(), static_cast<UINT32>(convTitle.size()),
                                 tf, titleRect, textWhiteBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
            }
        }

        // A3. Turn / Step Capsule Pill & Carousel Controls
        DrawCard(target, turnPillRect, 10.0f * scale, bentoBgBrush.Get(), bentoBorderBrush.Get());

        float turnContentLeft = turnPillRect.left + 8.0f * scale;

        // If multiple sessions, render clickable chevrons for carousel
        if (hasMultiple && smallTextFormat && textMutedBrush) {
            const float btnW = 14.0f * scale;
            lastCarouselHitTargets_.prevBtn = D2D1::RectF(turnPillRect.left + 2.0f * scale, turnPillRect.top,
                                                         turnPillRect.left + 2.0f * scale + btnW, turnPillRect.bottom);
            lastCarouselHitTargets_.nextBtn = D2D1::RectF(turnPillRect.right - 2.0f * scale - btnW, turnPillRect.top,
                                                         turnPillRect.right - 2.0f * scale, turnPillRect.bottom);

            smallTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            smallTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            target->DrawText(L"\u25C0", 1, smallTextFormat, lastCarouselHitTargets_.prevBtn, textMutedBrush.Get());
            target->DrawText(L"\u25B6", 1, smallTextFormat, lastCarouselHitTargets_.nextBtn, textMutedBrush.Get());

            turnContentLeft = lastCarouselHitTargets_.prevBtn.right + 4.0f * scale;
        }

        // Active Status Dot inside Turn Pill
        const D2D1_POINT_2F dotCenter = D2D1::Point2F(turnContentLeft + 4.0f * scale, (turnPillRect.top + turnPillRect.bottom) * 0.5f);
        float dotPulse = (session && session->GetRunningSubagentsCount() > 0)
            ? (0.70f + 0.30f * std::sin(static_cast<float>(now) * 4.5f))
            : 0.90f;
        ComPtr<ID2D1SolidColorBrush> liveDotBrush;
        target->CreateSolidColorBrush(D2D1::ColorF(0.204f, 0.780f, 0.349f, dotPulse), &liveDotBrush);
        target->FillEllipse(D2D1::Ellipse(dotCenter, 3.2f * scale, 3.2f * scale), liveDotBrush.Get());

        // Turn & Step Text
        if (microFormat && textWhiteBrush) {
            wchar_t turnBuf[64];
            int turn = session ? session->currentTurn : 0;
            int step = session ? session->currentStep : 0;
            if (turn > 0 && step > 0) {
                swprintf_s(turnBuf, L"Turn %d \u2022 Step %d", turn, step);
            } else if (turn > 0) {
                swprintf_s(turnBuf, L"Turn %d", turn);
            } else {
                swprintf_s(turnBuf, L"AGY Ready");
            }

            const float textRight = hasMultiple ? (lastCarouselHitTargets_.nextBtn.left - 4.0f * scale) : (turnPillRect.right - 6.0f * scale);
            D2D1_RECT_F turnTextRect = D2D1::RectF(dotCenter.x + 6.0f * scale, turnPillRect.top, textRight, turnPillRect.bottom);
            microFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            microFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            target->DrawText(turnBuf, static_cast<UINT32>(wcslen(turnBuf)), microFormat.Get(),
                             turnTextRect, textWhiteBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }

        // --------------------------------------------------------------------
        // ZONE B: Latest Message Snippet Box with Role Attribution Badge
        // --------------------------------------------------------------------
        const float msgGap = 7.0f * scale * vFit;
        const float msgTop = headerBottom + msgGap;
        const float msgHeight = 44.0f * scale * vFit;
        const float msgBottom = msgTop + msgHeight;
        const D2D1_RECT_F msgBoxRect = D2D1::RectF(rect.left + padX, msgTop, rect.right - padX, msgBottom);

        DrawCard(target, msgBoxRect, 8.0f * scale, bentoBgBrush.Get(), bentoBorderBrush.Get());

        // B1. Role Attribution Badge
        const bool isUser = session && (session->lastMessageSender == L"USER");
        const wchar_t* roleText = isUser ? L"USER" : L"AGENT";
        const D2D1_COLOR_F roleColor   = isUser ? D2D1::ColorF(0.506f, 0.549f, 0.973f, 1.0f) : kAppleGreen;
        const D2D1_COLOR_F roleBgColor = isUser ? D2D1::ColorF(0.506f, 0.549f, 0.973f, 0.15f) : D2D1::ColorF(0.204f, 0.780f, 0.349f, 0.15f);
        const D2D1_COLOR_F roleBrdCol  = isUser ? D2D1::ColorF(0.506f, 0.549f, 0.973f, 0.35f) : D2D1::ColorF(0.204f, 0.780f, 0.349f, 0.35f);

        ComPtr<ID2D1SolidColorBrush> roleTextBrush;
        ComPtr<ID2D1SolidColorBrush> roleBgBrush;
        ComPtr<ID2D1SolidColorBrush> roleBrdBrush;
        target->CreateSolidColorBrush(roleColor, &roleTextBrush);
        target->CreateSolidColorBrush(roleBgColor, &roleBgBrush);
        target->CreateSolidColorBrush(roleBrdCol, &roleBrdBrush);

        const float badgeW = 42.0f * scale;
        const float badgeH = 14.0f * scale;
        const D2D1_RECT_F badgeRect = D2D1::RectF(
            msgBoxRect.left + 9.0f * scale,
            msgBoxRect.top + 6.0f * scale,
            msgBoxRect.left + 9.0f * scale + badgeW,
            msgBoxRect.top + 6.0f * scale + badgeH
        );
        DrawCard(target, badgeRect, 3.5f * scale, roleBgBrush.Get(), roleBrdBrush.Get());

        if (microFormat && roleTextBrush) {
            microFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            microFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            target->DrawText(roleText, static_cast<UINT32>(wcslen(roleText)), microFormat.Get(),
                             badgeRect, roleTextBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }

        // B2. Relative Timestamp
        std::wstring timeStr = L"Just now";
        if (session) {
            uint64_t age = session->GetAgeSeconds();
            if (age >= 3600) {
                timeStr = std::to_wstring(age / 3600) + L"h ago";
            } else if (age >= 60) {
                timeStr = std::to_wstring(age / 60) + L"m ago";
            }
        }
        if (microFormat && textTertiaryBrush) {
            D2D1_RECT_F timeRect = D2D1::RectF(
                badgeRect.right + 7.0f * scale,
                badgeRect.top,
                msgBoxRect.right - 9.0f * scale,
                badgeRect.bottom
            );
            microFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            microFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            target->DrawText(timeStr.c_str(), static_cast<UINT32>(timeStr.size()), microFormat.Get(),
                             timeRect, textTertiaryBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }

        // B3. Message Snippet Content
        std::wstring snippet = (session && !session->lastMessageSnippet.empty())
            ? session->lastMessageSnippet
            : L"Session active and ready for instructions.";
        if (smallTextFormat && textWhiteBrush) {
            D2D1_RECT_F snippetRect = D2D1::RectF(
                msgBoxRect.left + 9.0f * scale,
                msgBoxRect.top + 22.0f * scale,
                msgBoxRect.right - 9.0f * scale,
                msgBoxRect.bottom - 4.0f * scale
            );
            smallTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            smallTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
            target->DrawText(snippet.c_str(), static_cast<UINT32>(snippet.size()), smallTextFormat,
                             snippetRect, textWhiteBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }

        // --------------------------------------------------------------------
        // ZONE C: Gemini Models Usage Limits (Dual Bento Cards with Progress Rings)
        // --------------------------------------------------------------------
        const float quotaGap = 7.0f * scale * vFit;
        const float quotaTop = msgBottom + quotaGap;
        const float quotaHeight = 54.0f * scale * vFit;
        const float quotaBottom = quotaTop + quotaHeight;
        const float quotaTotalWidth = (rect.right - rect.left) - padX * 2.0f;
        const float colGap = 7.0f * scale;
        const float cardWidth = (quotaTotalWidth - colGap) * 0.5f;

        // C1. 5-Hour Limit Card (Left)
        const D2D1_RECT_F fiveHrRect = D2D1::RectF(rect.left + padX, quotaTop, rect.left + padX + cardWidth, quotaBottom);
        DrawCard(target, fiveHrRect, 8.0f * scale, bentoBgBrush.Get(), bentoBorderBrush.Get());

        const float fiveHrFraction = session ? session->gemini5HourRemainingFraction : 0.80f;
        const D2D1_POINT_2F ring5hCenter = D2D1::Point2F(fiveHrRect.left + 24.0f * scale, (fiveHrRect.top + fiveHrRect.bottom) * 0.5f);
        const float ringRadius = 15.0f * scale;
        const float strokeW = 2.8f * scale;

        DrawCircularProgressRing(target, factory.Get(), ring5hCenter, ringRadius, strokeW,
                                 fiveHrFraction, cyanBrush.Get(), trackBgBrush.Get());

        // 5-Hour Limit Text Content
        wchar_t buf5h[16];
        swprintf_s(buf5h, L"%.0f%%", fiveHrFraction * 100.0f);
        if (microFormat && textWhiteBrush) {
            D2D1_RECT_F pct5hRect = D2D1::RectF(ring5hCenter.x - ringRadius, ring5hCenter.y - ringRadius,
                                                ring5hCenter.x + ringRadius, ring5hCenter.y + ringRadius);
            microFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            microFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            target->DrawText(buf5h, static_cast<UINT32>(wcslen(buf5h)), microFormat.Get(),
                             pct5hRect, textWhiteBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }

        const float text5hLeft = ring5hCenter.x + 20.0f * scale;
        if (microFormat && textMutedBrush && textWhiteBrush && textTertiaryBrush) {
            // Label
            D2D1_RECT_F lbl5hRect = D2D1::RectF(text5hLeft, fiveHrRect.top + 6.0f * scale, fiveHrRect.right - 6.0f * scale, fiveHrRect.top + 18.0f * scale);
            microFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            microFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            target->DrawText(L"5-HOUR LIMIT", 12, microFormat.Get(), lbl5hRect, textMutedBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);

            // Value Remaining
            wchar_t val5hBuf[32];
            swprintf_s(val5hBuf, L"%.0f%% remaining", fiveHrFraction * 100.0f);
            D2D1_RECT_F val5hRect = D2D1::RectF(text5hLeft, fiveHrRect.top + 18.0f * scale, fiveHrRect.right - 6.0f * scale, fiveHrRect.top + 34.0f * scale);
            IDWriteTextFormat* boldFmt = boldTextFormat ? boldTextFormat : smallTextFormat;
            if (boldFmt) {
                boldFmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                boldFmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                target->DrawText(val5hBuf, static_cast<UINT32>(wcslen(val5hBuf)), boldFmt, val5hRect, textWhiteBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
            }

            // Reset Countdown
            std::wstring rst5h = (session && !session->gemini5HourResetCountdown.empty())
                ? session->gemini5HourResetCountdown
                : L"Resets in 4h 27m";
            D2D1_RECT_F rst5hRect = D2D1::RectF(text5hLeft, fiveHrRect.top + 34.0f * scale, fiveHrRect.right - 6.0f * scale, fiveHrRect.bottom - 4.0f * scale);
            microFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            microFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            target->DrawText(rst5h.c_str(), static_cast<UINT32>(rst5h.size()), microFormat.Get(), rst5hRect, textTertiaryBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }

        // C2. Weekly Limit Card (Right)
        const D2D1_RECT_F weeklyRect = D2D1::RectF(fiveHrRect.right + colGap, quotaTop, rect.right - padX, quotaBottom);
        DrawCard(target, weeklyRect, 8.0f * scale, bentoBgBrush.Get(), bentoBorderBrush.Get());

        const float weeklyFraction = session ? session->geminiWeeklyRemainingFraction : 0.48f;
        const D2D1_POINT_2F ringWkCenter = D2D1::Point2F(weeklyRect.left + 24.0f * scale, (weeklyRect.top + weeklyRect.bottom) * 0.5f);

        DrawCircularProgressRing(target, factory.Get(), ringWkCenter, ringRadius, strokeW,
                                 weeklyFraction, purpleBrush.Get(), trackBgBrush.Get());

        wchar_t bufWk[16];
        swprintf_s(bufWk, L"%.0f%%", weeklyFraction * 100.0f);
        if (microFormat && textWhiteBrush) {
            D2D1_RECT_F pctWkRect = D2D1::RectF(ringWkCenter.x - ringRadius, ringWkCenter.y - ringRadius,
                                                ringWkCenter.x + ringRadius, ringWkCenter.y + ringRadius);
            microFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            microFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            target->DrawText(bufWk, static_cast<UINT32>(wcslen(bufWk)), microFormat.Get(),
                             pctWkRect, textWhiteBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }

        const float textWkLeft = ringWkCenter.x + 20.0f * scale;
        if (microFormat && textMutedBrush && textWhiteBrush && textTertiaryBrush) {
            // Label
            D2D1_RECT_F lblWkRect = D2D1::RectF(textWkLeft, weeklyRect.top + 6.0f * scale, weeklyRect.right - 6.0f * scale, weeklyRect.top + 18.0f * scale);
            microFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            microFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            target->DrawText(L"WEEKLY LIMIT", 12, microFormat.Get(), lblWkRect, textMutedBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);

            // Value Remaining
            wchar_t valWkBuf[32];
            swprintf_s(valWkBuf, L"%.0f%% remaining", weeklyFraction * 100.0f);
            D2D1_RECT_F valWkRect = D2D1::RectF(textWkLeft, weeklyRect.top + 18.0f * scale, weeklyRect.right - 6.0f * scale, weeklyRect.top + 34.0f * scale);
            IDWriteTextFormat* boldFmt = boldTextFormat ? boldTextFormat : smallTextFormat;
            if (boldFmt) {
                boldFmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                boldFmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                target->DrawText(valWkBuf, static_cast<UINT32>(wcslen(valWkBuf)), boldFmt, valWkRect, textWhiteBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
            }

            // Reset Countdown
            std::wstring rstWk = (session && !session->geminiWeeklyResetCountdown.empty())
                ? session->geminiWeeklyResetCountdown
                : L"Resets in 5d 23h";
            D2D1_RECT_F rstWkRect = D2D1::RectF(textWkLeft, weeklyRect.top + 34.0f * scale, weeklyRect.right - 6.0f * scale, weeklyRect.bottom - 4.0f * scale);
            microFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            microFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            target->DrawText(rstWk.c_str(), static_cast<UINT32>(rstWk.size()), microFormat.Get(), rstWkRect, textTertiaryBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }

        // --------------------------------------------------------------------
        // ZONE D: Context Compaction Warning & Token Budget Track
        // --------------------------------------------------------------------
        const float ctxGap = 7.0f * scale * vFit;
        const float ctxTop = quotaBottom + ctxGap;
        const float ctxHeight = 32.0f * scale * vFit;
        const float ctxBottom = ctxTop + ctxHeight;
        const D2D1_RECT_F contextRect = D2D1::RectF(rect.left + padX, ctxTop, rect.right - padX, ctxBottom);

        DrawCard(target, contextRect, 8.0f * scale, bentoBgBrush.Get(), bentoBorderBrush.Get());

        const uint64_t curTokens = session ? session->contextTokens : 0;
        const uint64_t maxTokens = session ? session->maxTokens : 1048576;
        const float ratio = session ? session->GetRatio() : 0.0f;
        const float threshold = session ? session->compactionThresholdFraction : 0.80f;
        const bool isCompactionWarn = session ? session->IsCompactionWarning() : (ratio >= 0.80f);

        // D1. Labels
        if (microFormat) {
            D2D1_RECT_F ctxLblRect = D2D1::RectF(contextRect.left + 9.0f * scale, contextRect.top + 4.0f * scale,
                                                contextRect.left + 140.0f * scale, contextRect.top + 16.0f * scale);
            microFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            microFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            target->DrawText(L"CONTEXT TOKENS", 14, microFormat.Get(), ctxLblRect, textMutedBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);

            wchar_t ctxValBuf[96];
            swprintf_s(ctxValBuf, L"%s / %s (%.0f%%) \u2022 Compaction at %.0f%%",
                       FormatTokenCount(curTokens).c_str(), FormatTokenCount(maxTokens).c_str(),
                       ratio * 100.0f, threshold * 100.0f);

            D2D1_RECT_F ctxValRect = D2D1::RectF(contextRect.left + 140.0f * scale, contextRect.top + 4.0f * scale,
                                                contextRect.right - 9.0f * scale, contextRect.top + 16.0f * scale);
            microFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
            microFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

            ID2D1Brush* valBrush = (ratio >= 0.90f) ? redBrush.Get()
                : (isCompactionWarn ? amberBrush.Get() : textMutedBrush.Get());
            target->DrawText(ctxValBuf, static_cast<UINT32>(wcslen(ctxValBuf)), microFormat.Get(),
                             ctxValRect, valBrush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }

        // D2. Progress Bar Track & 80% Threshold Hairline
        const float barLeft = contextRect.left + 9.0f * scale;
        const float barRight = contextRect.right - 9.0f * scale;
        const float barWidth = barRight - barLeft;
        const float barTop = contextRect.top + 18.0f * scale;
        const float barBottom = barTop + 5.0f * scale;
        const float barRadius = 2.5f * scale;

        const D2D1_RECT_F trackRect = D2D1::RectF(barLeft, barTop, barRight, barBottom);
        target->FillRoundedRectangle(D2D1::RoundedRect(trackRect, barRadius, barRadius), trackBgBrush.Get());

        const float fillW = barWidth * std::clamp(ratio, 0.0f, 1.0f);
        if (fillW > 1.0f) {
            const D2D1_RECT_F fillRect = D2D1::RectF(barLeft, barTop, barLeft + fillW, barBottom);
            ID2D1Brush* fillBrush = (ratio >= 0.90f) ? redBrush.Get()
                : (isCompactionWarn ? amberBrush.Get() : greenBrush.Get());
            target->FillRoundedRectangle(D2D1::RoundedRect(fillRect, barRadius, barRadius), fillBrush);
        }

        // 80% Threshold Hairline Indicator
        const float thresholdX = barLeft + barWidth * std::clamp(threshold, 0.1f, 1.0f);
        target->DrawLine(
            D2D1::Point2F(thresholdX, barTop - 2.0f * scale),
            D2D1::Point2F(thresholdX, barBottom + 2.0f * scale),
            redBrush.Get(),
            1.5f * scale
        );
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
        EnrichSessionFromBrainLocked(telem);

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

    // Populates conversationTitle, currentStep, currentTurn, lastMessageSender, and lastMessageSnippet from brain files/transcripts
    void EnrichSessionFromBrainLocked(SessionTelemetry& telem) {
        std::wstring convFolder;
        if (!telem.activeConversationId.empty() && !brainScanDir_.empty()) {
            std::wstring cand = brainScanDir_ + L"\\" + telem.activeConversationId;
            if (DirectoryExists(cand)) {
                convFolder = cand;
            }
        }
        if (convFolder.empty() && !telem.sourceFilePath.empty() && telem.sourceFilePath != L"pipe" && telem.sourceFilePath != L"manual") {
            std::wstring dir = GetDirectoryFromPath(telem.sourceFilePath);
            if (dir.find(L"scratch") != std::wstring::npos) {
                dir = GetDirectoryFromPath(dir);
            }
            if (DirectoryExists(dir)) {
                convFolder = dir;
            }
        }

        // 1. Scan .system_generated\steps to find latest step number if needed
        if (!convFolder.empty()) {
            std::wstring stepsDir = convFolder + L"\\.system_generated\\steps";
            if (DirectoryExists(stepsDir)) {
                WIN32_FIND_DATAW sfd;
                HANDLE hFindSteps = FindFirstFileW((stepsDir + L"\\*").c_str(), &sfd);
                if (hFindSteps != INVALID_HANDLE_VALUE) {
                    do {
                        if ((sfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                            wcscmp(sfd.cFileName, L".") != 0 && wcscmp(sfd.cFileName, L"..") != 0) {
                            int stepNum = _wtoi(sfd.cFileName);
                            if (stepNum > telem.currentStep) {
                                telem.currentStep = stepNum;
                            }
                        }
                    } while (FindNextFileW(hFindSteps, &sfd));
                    FindClose(hFindSteps);
                }
            }

            // 2. Scan .system_generated\messages to get latest message sender & snippet if needed
            std::wstring messagesDir = convFolder + L"\\.system_generated\\messages";
            if (DirectoryExists(messagesDir)) {
                WIN32_FIND_DATAW mfd;
                HANDLE hFindMsg = FindFirstFileW((messagesDir + L"\\*.json").c_str(), &mfd);
                if (hFindMsg != INVALID_HANDLE_VALUE) {
                    FILETIME latestFt = {};
                    std::wstring latestMsgFile;
                    do {
                        if (!(mfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                            wcscmp(mfd.cFileName, L"read.json") != 0) {
                            ULARGE_INTEGER uCur, uLat;
                            uCur.LowPart = mfd.ftLastWriteTime.dwLowDateTime;
                            uCur.HighPart = mfd.ftLastWriteTime.dwHighDateTime;
                            uLat.LowPart = latestFt.dwLowDateTime;
                            uLat.HighPart = latestFt.dwHighDateTime;
                            if (uCur.QuadPart >= uLat.QuadPart) {
                                latestFt = mfd.ftLastWriteTime;
                                latestMsgFile = messagesDir + L"\\" + mfd.cFileName;
                            }
                        }
                    } while (FindNextFileW(hFindMsg, &mfd));
                    FindClose(hFindMsg);

                    if (!latestMsgFile.empty()) {
                        std::string msgContent = ReadFileShared(latestMsgFile);
                        if (!msgContent.empty()) {
                            json::Value msgRoot;
                            if (json::Parse(msgContent, msgRoot) && msgRoot.IsObject()) {
                                std::wstring sender = msgRoot["sender"].AsWString();
                                std::wstring msgTitle = msgRoot["renderDetails"]["messageTitle"].AsWString();
                                if (msgTitle.find(L"User") != std::wstring::npos ||
                                    sender == L"user" || sender == L"USER") {
                                    telem.lastMessageSender = L"USER";
                                } else {
                                    telem.lastMessageSender = L"AGENT";
                                }

                                if (telem.lastMessageSnippet.empty()) {
                                    std::wstring content = msgRoot["content"].AsWString();
                                    if (!content.empty()) {
                                        size_t start = 0;
                                        while (start < content.size() && (content[start] == L'\r' || content[start] == L'\n' || content[start] == L' ')) start++;
                                        size_t end = content.find_first_of(L"\r\n", start);
                                        if (end == std::wstring::npos) end = content.size();
                                        std::wstring line = content.substr(start, end - start);
                                        if (line.size() > 80) line = line.substr(0, 77) + L"...";
                                        telem.lastMessageSnippet = line;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // 3. Fallbacks and defaults
        if (telem.currentTurn == 0) {
            if (telem.currentStep > 0) {
                telem.currentTurn = std::max(1, (telem.currentStep + 3) / 4);
            } else {
                telem.currentTurn = 1;
            }
        }
        if (telem.currentStep == 0) {
            telem.currentStep = 1;
        }

        if (telem.conversationTitle.empty() || telem.conversationTitle == telem.activeConversationId) {
            if (!telem.sessionName.empty()) {
                telem.conversationTitle = telem.sessionName;
            } else if (!telem.activeConversationId.empty()) {
                telem.conversationTitle = telem.activeConversationId;
            } else {
                telem.conversationTitle = L"Active Session";
            }
        }

        if (telem.lastMessageSender.empty()) {
            telem.lastMessageSender = L"AGENT";
        }
        if (telem.lastMessageSnippet.empty()) {
            if (!telem.lastCompletedEvent.empty()) {
                telem.lastMessageSnippet = telem.lastCompletedEvent;
            } else {
                telem.lastMessageSnippet = L"Ready for prompt";
            }
        }
    }

    // Scans brain candidate files across active conversations
    void ScanBrainDirectoryLocked(std::vector<std::wstring>& outPaths) {
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

    // Backward-compatibility alias
    void ScanBrainCandidatesLocked(std::vector<std::wstring>& outPaths) {
        ScanBrainDirectoryLocked(outPaths);
    }

    // Reloads session files from disk under lock with non-exclusive sharing
    bool ReloadFromDiskLocked() {
        bool changed = false;
        std::vector<std::wstring> candidatePaths;
        if (!primaryFilePath_.empty()) {
            candidatePaths.push_back(primaryFilePath_);
        }

        ScanBrainDirectoryLocked(candidatePaths);

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
                EnrichSessionFromBrainLocked(telem);
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

    // Direct2D Helper: Circular Progress Ring with Rounded Caps
    void DrawCircularProgressRing(
        ID2D1RenderTarget* rt,
        ID2D1Factory* factory,
        D2D1_POINT_2F center,
        float ringRadius,
        float strokeWidth,
        float percentage,
        ID2D1Brush* arcBrush,
        ID2D1Brush* trackBrush
    ) {
        if (trackBrush) {
            rt->DrawEllipse(D2D1::Ellipse(center, ringRadius, ringRadius), trackBrush, strokeWidth);
        }

        float clampedPct = std::clamp(percentage, 0.001f, 1.0f);
        if (!arcBrush || !factory) return;

        ComPtr<ID2D1PathGeometry> arcGeom;
        if (FAILED(factory->CreatePathGeometry(&arcGeom)) || !arcGeom) return;

        ComPtr<ID2D1GeometrySink> sink;
        if (FAILED(arcGeom->Open(&sink)) || !sink) return;

        const float startAngle = -3.14159265f * 0.5f; // 12 o'clock (-90 degrees)
        const float sweepAngle = 2.0f * 3.14159265f * clampedPct;
        const int segments = std::max(4, static_cast<int>(36.0f * clampedPct));

        sink->BeginFigure(
            D2D1::Point2F(center.x + std::cos(startAngle) * ringRadius,
                         center.y + std::sin(startAngle) * ringRadius),
            D2D1_FIGURE_BEGIN_HOLLOW);
        for (int i = 1; i <= segments; ++i) {
            float a = startAngle + sweepAngle * (static_cast<float>(i) / static_cast<float>(segments));
            sink->AddLine(D2D1::Point2F(center.x + std::cos(a) * ringRadius,
                                        center.y + std::sin(a) * ringRadius));
        }
        sink->EndFigure(D2D1_FIGURE_END_OPEN);
        sink->Close();

        ComPtr<ID2D1StrokeStyle> strokeStyle;
        D2D1_STROKE_STYLE_PROPERTIES strokeProps = D2D1::StrokeStyleProperties(
            D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND,
            D2D1_LINE_JOIN_ROUND, 10.0f, D2D1_DASH_STYLE_SOLID, 0.0f
        );
        factory->CreateStrokeStyle(&strokeProps, nullptr, 0, &strokeStyle);

        rt->DrawGeometry(arcGeom.Get(), arcBrush, strokeWidth, strokeStyle.Get());
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
