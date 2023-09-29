#pragma once

#include <iostream>
#include <memory>
#include <utility>
#include <algorithm>
#include <functional>
#include <chrono>
#include <comdef.h>
#include <fstream>
#include <cstdint>
#include <cassert>

#include <float.h>
#include <cmath>

#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>

#include "directx/d3dx12.h"

#include <Windows.h>
#include <WindowsX.h>
#include <wrl.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgidebug.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

template <typename T>
using Ref = std::shared_ptr<T>;

template <typename T, typename... Args>
inline Ref<T> make_ref(Args &&...args)
{
    return std::make_shared<T>(std::forward<Args>(args)...);
}

#include "core/Log.h"

// undefine min/max macros from the windows.h
#if defined(max)
#undef max
#endif

#if defined(min)
#undef min
#endif

#define ASSERT(x, ...)                                       \
    {                                                        \
        if (!(x))                                            \
        {                                                    \
            LOG_ERROR("Assertion Failed: {0}", __VA_ARGS__); \
            __debugbreak();                                  \
        }                                                    \
    }

#ifndef ThrowIfFailed
#define ThrowIfFailed(x)                          \
    {                                             \
        HRESULT hr__ = (x);                       \
        if (FAILED(hr__))                         \
        {                                         \
            _com_error err(hr__);                 \
            LOG_ERROR("{0}", err.ErrorMessage()); \
            ASSERT(false, #x);                    \
        }                                         \
    }
#endif