#pragma once

#ifndef WEATHER_LOCATION_ENGINE_HPP
#define WEATHER_LOCATION_ENGINE_HPP

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
#include <winhttp.h>
#include <locationapi.h>
#include <wrl/client.h>

#include <string>
#include <vector>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cwctype>

#if __has_include(<winrt/base.h>) && __has_include(<winrt/Windows.Foundation.h>)
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#endif

#ifndef DYNAMIC_ISLAND_HAS_GEOLOCATION
#if __has_include(<winrt/Windows.Devices.Geolocation.h>)
#define DYNAMIC_ISLAND_HAS_GEOLOCATION 1
#include <winrt/Windows.Devices.Geolocation.h>
#else
#define DYNAMIC_ISLAND_HAS_GEOLOCATION 0
#endif
#endif

#include "island_common.hpp"

// Forward declarations for external globals declared in the main module
extern HANDLE g_stopEvent;
extern HANDLE g_settingsChangedEvent;

// --- Weather Fetching Helpers ---
inline std::string HttpGet(const wchar_t* host, const wchar_t* path, bool https = true) {
    std::string response;
    HINTERNET hSession = WinHttpOpen(L"DynamicIsland/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        Wh_Log(L"Weather HttpGet: WinHttpOpen failed with error %lu", GetLastError());
        return response;
    }
    WinHttpSetTimeouts(hSession, 5000, 5000, 5000, 5000);

    HINTERNET hConnect = WinHttpConnect(hSession, host, https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);
    if (hConnect) {
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, https ? WINHTTP_FLAG_SECURE : 0);
        if (hRequest) {
            if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                WinHttpReceiveResponse(hRequest, nullptr)) {
                DWORD size = 0;
                DWORD downloaded = 0;
                do {
                    if (WinHttpQueryDataAvailable(hRequest, &size) && size > 0) {
                        std::vector<char> buffer(size + 1);
                        if (WinHttpReadData(hRequest, buffer.data(), size, &downloaded)) {
                            buffer[downloaded] = '\0';
                            response.append(buffer.data());
                        } else {
                            Wh_Log(L"Weather HttpGet: WinHttpReadData failed with error %lu", GetLastError());
                        }
                    }
                } while (size > 0);
            } else {
                Wh_Log(L"Weather HttpGet: WinHttpSendRequest/ReceiveResponse failed with error %lu", GetLastError());
            }
            WinHttpCloseHandle(hRequest);
        } else {
            Wh_Log(L"Weather HttpGet: WinHttpOpenRequest failed with error %lu", GetLastError());
        }
        WinHttpCloseHandle(hConnect);
    } else {
        Wh_Log(L"Weather HttpGet: WinHttpConnect failed with error %lu", GetLastError());
    }
    WinHttpCloseHandle(hSession);
    return response;
}

// Percent-encodes a city name for use as a wttr.in path segment.
inline std::wstring PercentEncodeUtf8(const std::wstring& text) {
    auto looksPreEncoded = [](const std::wstring& value) {
        const size_t pos = value.find(L'%');
        if (pos == std::wstring::npos || pos + 2 >= value.size()) {
            return false;
        }
        return iswxdigit(value[pos + 1]) && iswxdigit(value[pos + 2]);
    };
    if (looksPreEncoded(text)) {
        return text;
    }

    const int bytes = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (bytes <= 1) {
        return std::wstring();
    }

    std::string utf8(static_cast<size_t>(bytes - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, utf8.data(), bytes, nullptr, nullptr);

    static constexpr wchar_t kHexDigits[] = L"0123456789ABCDEF";
    std::wstring out;
    out.reserve(utf8.size() * 3);
    for (const unsigned char c : utf8) {
        const bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                                (c >= '0' && c <= '9') ||
                                c == '-' || c == '_' || c == '.' || c == '~';
        if (unreserved) {
            out.push_back(static_cast<wchar_t>(c));
        } else {
            out.push_back(L'%');
            out.push_back(kHexDigits[(c >> 4) & 0x0f]);
            out.push_back(kHexDigits[c & 0x0f]);
        }
    }
    return out;
}

// Attempts to retrieve geographic coordinates using Windows Location APIs:
// 1. Windows 10/11 WinRT Geolocator (hardware sensor & Wi-Fi BSSID triangulation via Windows Location)
// 2. Legacy Windows Location COM API (CLSID_Location fallback)
// Fully non-blocking and defensive: fails fast (<2ms) if location services are disabled or denied.
inline bool TryGetWindowsCoordinates(double& outLat, double& outLon) {
    outLat = 0.0;
    outLon = 0.0;

#if DYNAMIC_ISLAND_HAS_GEOLOCATION
    // 1. Try modern WinRT Geolocation (Windows 10/11)
    try {
        winrt::Windows::Devices::Geolocation::Geolocator locator;
        auto status = locator.LocationStatus();
        if (status != winrt::Windows::Devices::Geolocation::PositionStatus::Disabled &&
            status != winrt::Windows::Devices::Geolocation::PositionStatus::NotAvailable) {
            auto asyncOp = locator.GetGeopositionAsync();
            auto asyncStatus = asyncOp.wait_for(std::chrono::seconds(2));
            if (asyncStatus == winrt::Windows::Foundation::AsyncStatus::Completed) {
                auto pos = asyncOp.GetResults();
                if (pos && pos.Coordinate()) {
                    auto point = pos.Coordinate().Point();
                    if (point) {
                        auto p = point.Position();
                        if (std::fabs(p.Latitude) > 0.0001 || std::fabs(p.Longitude) > 0.0001) {
                            outLat = p.Latitude;
                            outLon = p.Longitude;
                            return true;
                        }
                    }
                }
            }
        }
    } catch (...) {
        // Silently handled: Service disabled or access denied
    }
#endif

    // 2. Fallback to COM Location API (Windows 7/8/legacy fallback)
    HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool success = false;
    Microsoft::WRL::ComPtr<ILocation> location;
    HRESULT hr = CoCreateInstance(CLSID_Location, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&location));
    if (SUCCEEDED(hr) && location) {
        LOCATION_REPORT_STATUS status = REPORT_NOT_SUPPORTED;
        hr = location->GetReportStatus(IID_ILatLongReport, &status);
        if (SUCCEEDED(hr) && (status == REPORT_RUNNING || status == REPORT_INITIALIZING)) {
            Microsoft::WRL::ComPtr<ILocationReport> report;
            hr = location->GetReport(IID_ILatLongReport, &report);
            if (SUCCEEDED(hr) && report) {
                Microsoft::WRL::ComPtr<ILatLongReport> latLongReport;
                if (SUCCEEDED(report.As(&latLongReport)) && latLongReport) {
                    DOUBLE lat = 0.0, lon = 0.0;
                    if (SUCCEEDED(latLongReport->GetLatitude(&lat)) &&
                        SUCCEEDED(latLongReport->GetLongitude(&lon))) {
                        if (std::fabs(lat) > 0.0001 || std::fabs(lon) > 0.0001) {
                            outLat = lat;
                            outLon = lon;
                            success = true;
                        }
                    }
                }
            }
        }
    }

    if (hrCo == S_OK || hrCo == S_FALSE) {
        CoUninitialize();
    }
    return success;
}

inline DWORD WINAPI WeatherThreadProc(void*) {
    // Initial delay to avoid slowing down startup
    WaitForSingleObject(g_stopEvent, 3000);

    while (WaitForSingleObject(g_stopEvent, 0) == WAIT_TIMEOUT) {
        std::wstring cityOverride;
        bool isFahrenheit = false;
        bool weatherEnabled = true;
        {
            std::lock_guard lock(g_settingsMutex);
            weatherEnabled = g_settings.weather;
            cityOverride = g_settings.weatherCity;
            isFahrenheit = g_settings.weatherFahrenheit;
        }

        if (!weatherEnabled) {
            WaitForSingleObject(g_stopEvent, 2000);
            continue;
        }

        std::wstring url = L"/?format=j1";
        if (!cityOverride.empty()) {
            const std::wstring encodedCity = PercentEncodeUtf8(cityOverride);
            if (!encodedCity.empty()) {
                url = L"/" + encodedCity + L"?format=j1";
            }
        } else {
            // Adaptive auto-detection when custom location is empty:
            // 1. Try Windows Location API (Wi-Fi / sensor triangulation)
            double lat = 0.0, lon = 0.0;
            if (TryGetWindowsCoordinates(lat, lon)) {
                wchar_t coordBuf[64] = {};
                swprintf_s(coordBuf, L"/%.4f,%.4f?format=j1", lat, lon);
                url = coordBuf;
                Wh_Log(L"Weather: Auto-detected Windows coordinates: %.4f, %.4f", lat, lon);
            } else {
                // 2. Try fast IP-geolocation fallback for more reliable local coordinates
                std::string ipGeoRes = HttpGet(L"ip-api.com", L"/json", false);
                bool gotIpCoords = false;
                if (!ipGeoRes.empty() && ipGeoRes.find("\"status\":\"success\"") != std::string::npos) {
                    const char* latStr = strstr(ipGeoRes.c_str(), "\"lat\":");
                    const char* lonStr = strstr(ipGeoRes.c_str(), "\"lon\":");
                    if (latStr && lonStr) {
                        float ipLat = 0.0f, ipLon = 0.0f;
                        if (sscanf(latStr + 6, "%f", &ipLat) == 1 &&
                            sscanf(lonStr + 6, "%f", &ipLon) == 1) {
                            wchar_t coordBuf[64] = {};
                            swprintf_s(coordBuf, L"/%.4f,%.4f?format=j1", ipLat, ipLon);
                            url = coordBuf;
                            gotIpCoords = true;
                            Wh_Log(L"Weather: Auto-detected IP-API coordinates: %.4f, %.4f", ipLat, ipLon);
                        }
                    }
                }
                if (!gotIpCoords) {
                    // 3. Fallback to wttr.in default server-side GeoIP lookup
                    url = L"/?format=j1";
                    Wh_Log(L"Weather: Using wttr.in default GeoIP auto-detection");
                }
            }
        }

        Wh_Log(L"Weather: Requesting weather from wttr.in/host: wttr.in, path: %s", url.c_str());
        std::string wRes = HttpGet(L"wttr.in", url.c_str(), true);
        if (wRes.empty()) {
            Wh_Log(L"Weather: HTTPS request failed, retrying over plain HTTP...");
            wRes = HttpGet(L"wttr.in", url.c_str(), false);
        }

        bool weatherCommitted = false;

        if (!wRes.empty()) {
            Wh_Log(L"Weather: Received response from wttr.in (size: %zu bytes)", wRes.size());
            float temp = 0.0f;
            int code = 0;
            std::wstring desc = L"";
            std::wstring windSpeed = L"";
            std::wstring windDir = L"";
            std::wstring humidity = L"";
            std::wstring feelsLike = L"";
            std::wstring cityLabel = L"Local Weather";

            const char* areaStr = strstr(wRes.c_str(), "\"areaName\":");
            if (areaStr) {
                const char* valStr = strstr(areaStr, "\"value\":");
                if (valStr) {
                    valStr += 8;
                    while (*valStr == ' ' || *valStr == '\"') valStr++;
                    const char* end = strchr(valStr, '\"');
                    if (end) {
                        std::string cityA(valStr, end - valStr);
                        int wchars_num = MultiByteToWideChar(CP_UTF8, 0, cityA.c_str(), -1, NULL, 0);
                        if (wchars_num > 0) {
                            std::vector<wchar_t> wstr(wchars_num);
                            MultiByteToWideChar(CP_UTF8, 0, cityA.c_str(), -1, &wstr[0], wchars_num);
                            cityLabel = wstr.data();
                        }
                    }
                }
            }

            const char* currentStr = strstr(wRes.c_str(), "\"current_condition\":");
            if (currentStr) {
                auto ParseStringField = [&](const char* key, std::wstring& out) {
                    const char* kStr = strstr(currentStr, key);
                    if (kStr) {
                        kStr += strlen(key);
                        while (*kStr == ' ' || *kStr == '\"' || *kStr == ':') kStr++;
                        const char* end = strchr(kStr, '\"');
                        if (end) {
                            std::string valA(kStr, end - kStr);
                            int wchars_num = MultiByteToWideChar(CP_UTF8, 0, valA.c_str(), -1, NULL, 0);
                            if (wchars_num > 0) {
                                std::vector<wchar_t> wstr(wchars_num);
                                MultiByteToWideChar(CP_UTF8, 0, valA.c_str(), -1, &wstr[0], wchars_num);
                                out = wstr.data();
                            }
                        }
                    }
                };

                const char* tempStr = strstr(currentStr, isFahrenheit ? "\"temp_F\":" : "\"temp_C\":");
                if (tempStr) {
                    tempStr += 9;
                    while (*tempStr == ' ' || *tempStr == '\"') tempStr++;
                    sscanf(tempStr, "%f", &temp);
                }
                const char* codeStr = strstr(currentStr, "\"weatherCode\":");
                if (codeStr) {
                    codeStr += 14;
                    while (*codeStr == ' ' || *codeStr == '\"') codeStr++;
                    sscanf(codeStr, "%d", &code);
                }

                const char* descStr = strstr(currentStr, "\"weatherDesc\":");
                if (descStr) {
                    const char* valStr = strstr(descStr, "\"value\":");
                    if (valStr) {
                        valStr += 8;
                        while (*valStr == ' ' || *valStr == '\"') valStr++;
                        const char* end = strchr(valStr, '\"');
                        if (end) {
                            std::string valA(valStr, end - valStr);
                            int wchars_num = MultiByteToWideChar(CP_UTF8, 0, valA.c_str(), -1, NULL, 0);
                            if (wchars_num > 0) {
                                std::vector<wchar_t> wstr(wchars_num);
                                MultiByteToWideChar(CP_UTF8, 0, valA.c_str(), -1, &wstr[0], wchars_num);
                                desc = wstr.data();
                                while(!desc.empty() && desc.back() == L' ') desc.pop_back();
                            }
                        }
                    }
                }

                ParseStringField(isFahrenheit ? "\"windspeedMiles\"" : "\"windspeedKmph\"", windSpeed);
                ParseStringField("\"winddir16Point\"", windDir);
                ParseStringField("\"humidity\"", humidity);
                ParseStringField(isFahrenheit ? "\"FeelsLikeF\"" : "\"FeelsLikeC\"", feelsLike);

                weatherCommitted = (code != 0) || !desc.empty();

                std::wstring finalCity = cityOverride.empty() ? cityLabel : cityOverride;
                if (weatherCommitted) {
                    Wh_Log(L"Weather parsed success: city=%s, temp=%.1f, feelsLike=%s, humidity=%s%%, desc=%s",
                           finalCity.c_str(), temp, feelsLike.c_str(), humidity.c_str(), desc.c_str());
                } else {
                    Wh_Log(L"Weather: response had no usable reading, keeping previous data.");
                }
            } else {
                Wh_Log(L"Weather: Failed to find \"current_condition\" in response.");
            }

            if (weatherCommitted) {
                std::lock_guard lock(g_stateMutex);
                g_state.weather.hasData = true;
                g_state.weather.temperature = temp;
                g_state.weather.weatherCode = code;
                if (!cityOverride.empty()) {
                    g_state.weather.city = (cityLabel != L"Local Weather" && !cityLabel.empty()) ? cityLabel : cityOverride;
                } else {
                    g_state.weather.city = cityLabel;
                }
                g_state.weather.weatherDesc = desc;
                g_state.weather.windSpeed = windSpeed;
                g_state.weather.windDir = windDir;
                g_state.weather.humidity = humidity;
                g_state.weather.feelsLike = feelsLike;
                g_state.weather.lastUpdated = NowSeconds();
            }
        } else {
            Wh_Log(L"Weather: HttpGet returned empty response.");
        }

        static const DWORD kBackoffMs[] = {60 * 1000, 2 * 60 * 1000, 5 * 60 * 1000, 15 * 60 * 1000};
        static size_t backoffIndex = 0;

        DWORD waitMs;
        if (weatherCommitted) {
            backoffIndex = 0;
            waitMs = 15 * 60 * 1000;
        } else {
            waitMs = kBackoffMs[backoffIndex];
            if (backoffIndex + 1 < ARRAYSIZE(kBackoffMs)) {
                ++backoffIndex;
            }
        }

        HANDLE events[] = {g_stopEvent, g_settingsChangedEvent};
        DWORD waitResult = WaitForMultipleObjects(2, events, FALSE, waitMs);
        if (waitResult == WAIT_OBJECT_0) {
            break;
        }
        if (waitResult == WAIT_OBJECT_0 + 1) {
            backoffIndex = 0;
        }
    }
    return 0;
}

#endif // WEATHER_LOCATION_ENGINE_HPP
