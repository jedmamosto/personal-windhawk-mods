#pragma once

#ifndef DYNAMIC_ISLAND_PCH_HPP
#define DYNAMIC_ISLAND_PCH_HPP

#ifndef UNICODE
#define UNICODE
#endif

#ifndef _UNICODE
#define _UNICODE
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

// Win32 APIs & Platform Headers
#include <windows.h>
#include <windowsx.h>
#include <winioctl.h>
#include <poclass.h>
#include <dwmapi.h>
#include <shcore.h>
#include <shellapi.h>
#include <setupapi.h>
#include <devpropdef.h>
#include <mmsystem.h>
#include <powrprof.h>
#include <pdh.h>
#include <winhttp.h>
#include <locationapi.h>
#include <wrl/client.h>

#ifdef InsetRect
#undef InsetRect
#endif

// Direct2D, DirectWrite, WIC
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wincodec.h>

// C++ Standard Library
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <fstream>
#include <functional>
#include <iomanip>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

// Windhawk API Header (when compiling inside Windhawk mod context)
#if defined(WH_MOD) && __has_include(<windhawk_api.h>)
#include <windhawk_api.h>
#endif

#endif // DYNAMIC_ISLAND_PCH_HPP
