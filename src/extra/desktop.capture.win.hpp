/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "chainblocks.h"
#include <D3Dcommon.h>
#include <Windows.h>
#include <array>
#include <cassert>
#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <iostream>

namespace chainblocks {
struct ScreenInfo {
  IDXGIAdapter *adapter;
  IDXGIOutput *output;
  HMONITOR screen;
};

class DXGIDesktopCapture {
  struct FrameData {
    ID3D11Texture2D *texture = nullptr;
    bool valid = false;
  };

  ID3D11Device *_device = nullptr;
  ID3D11DeviceContext *_ctx = nullptr;
  IDXGIOutputDuplication *_dup = nullptr;

  static constexpr int _nbuffers = 2;
  std::array<FrameData, _nbuffers> _buffers;
  int _bufferIndex = 0;
  int _cpuIndex = 0;
  uint8_t *_cpuBuffer;

  int _left, _right, _top, _bottom, _width, _height;

public:
  enum State { Normal, Timeout, Lost, Error };

  const int left() { return _left; }
  const int right() { return _right; }
  const int top() { return _top; }
  const int bottom() { return _bottom; }
  const int width() { return _width; }
  const int height() { return _height; }
  const uint8_t *image() { return _cpuBuffer; }

  static ScreenInfo FindScreen(int x, int y, int width, int height) {
    ScreenInfo result{};
    IDXGIFactory1 *factory;
    HRESULT err = 0;
    UINT aindex = 0;

    err = CreateDXGIFactory1(__uuidof(factory), (void **)&factory);
    assert(err == 0);

    while (factory->EnumAdapters(aindex, &result.adapter) != DXGI_ERROR_NOT_FOUND) {
      UINT oindex = 0;
      while (result.adapter->EnumOutputs(oindex, &result.output) != DXGI_ERROR_NOT_FOUND) {
        DXGI_OUTPUT_DESC desc;
        MONITORINFO minfo;

        err = result.output->GetDesc(&desc);
        assert(err == 0);

        result.screen = desc.Monitor;

        minfo.cbSize = sizeof(MONITORINFO);
        auto yes = GetMonitorInfo(result.screen, &minfo);
        assert(yes);

        if (x >= minfo.rcMonitor.left && x < minfo.rcMonitor.right && y >= minfo.rcMonitor.top && y < minfo.rcMonitor.bottom) {
          factory->Release();
          return result;
        } else {
          result.output->Release();
          result.output = nullptr;
          result.screen = nullptr;
        }

        oindex++;
      }

      result.adapter->Release();
      result.adapter = nullptr;

      aindex++;
    }

    factory->Release();
    return result;
  }

  DXGIDesktopCapture(ScreenInfo &screen) {
    for (auto &buf : _buffers) {
      buf.texture = nullptr;
    }

#ifndef NDEBUG
    UINT initFlags = D3D11_CREATE_DEVICE_DEBUG;
#else
    UINT initFlags = 0;
#endif
    HRESULT err;
    D3D_FEATURE_LEVEL features;
    D3D_FEATURE_LEVEL inFeats = {D3D_FEATURE_LEVEL_11_0};

    err = D3D11CreateDevice(screen.adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, initFlags, &inFeats, 1, D3D11_SDK_VERSION, &_device,
                            &features, &_ctx);
    // if you crash here, don't panic! you are just missing DX debug layers most
    // likely!
    // https://docs.microsoft.com/en-us/windows/uwp/gaming/use-the-directx-runtime-and-visual-studio-graphics-diagnostic-features
    //
    // Go to Settings, select Apps, and then click Manage optional features.
    // Click Add a feature
    // In the Optional features list, select Graphics Tools and then click
    // Install.
    assert(err == 0);

    IDXGIOutput1 *output1;
    err = screen.output->QueryInterface(__uuidof(output1), (void **)&output1);
    assert(err == 0);

    err = output1->DuplicateOutput(_device, &_dup);
    assert(err == 0);
    output1->Release();

    MONITORINFO minfo;
    minfo.cbSize = sizeof(MONITORINFO);
    auto yes = GetMonitorInfo(screen.screen, &minfo);
    assert(yes);

    _left = minfo.rcMonitor.left;
    _right = minfo.rcMonitor.right;
    _top = minfo.rcMonitor.top;
    _bottom = minfo.rcMonitor.bottom;
    _width = _right - _left;
    _height = _bottom - _top;

    _cpuBuffer = new uint8_t[4 * _width * _height];
  }

  ~DXGIDesktopCapture() {
    if (_dup) {
      _dup->Release();
    }

    for (auto &buf : _buffers) {
      if (buf.texture) {
        buf.texture->Release();
      }
    }

    if (_ctx) {
      _ctx->Release();
    }

    if (_device) {
      _device->Release();
    }

    delete[] _cpuBuffer;
  }

  State capture() {
    DXGI_OUTDUPL_FRAME_INFO info;
    IDXGIResource *resource;
    HRESULT err;
    err = _dup->AcquireNextFrame(0, &info, &resource);

    if (err == DXGI_ERROR_WAIT_TIMEOUT) {
      auto &buffer = _buffers[_bufferIndex];
      buffer.valid = false;
      _bufferIndex = (_bufferIndex + 1) % _nbuffers; // advance for next frame
      _cpuIndex = (_cpuIndex + 1) % _nbuffers;       // advance for next frame
      return Timeout;
    } else if (unlikely(err == DXGI_ERROR_ACCESS_LOST)) {
      return Lost;
    } else if (unlikely(err != S_OK)) {
      return Error;
    }

    ID3D11Texture2D *texture;
    err = resource->QueryInterface(__uuidof(texture), (void **)&texture);
    assert(err == 0);
    resource->Release();

    D3D11_TEXTURE2D_DESC desc, desc2;
    texture->GetDesc(&desc);

#define CREATE_BUFFER                                                 \
  {                                                                   \
    desc2 = desc;                                                     \
    desc2.Usage = D3D11_USAGE_STAGING;                                \
    desc2.CPUAccessFlags = D3D11_CPU_ACCESS_READ;                     \
    desc2.MiscFlags = 0;                                              \
    desc2.BindFlags = 0;                                              \
    err = _device->CreateTexture2D(&desc2, nullptr, &buffer.texture); \
    assert(err == 0);                                                 \
  }

    auto &buffer = _buffers[_bufferIndex];
    buffer.valid = true;
    _bufferIndex = (_bufferIndex + 1) % _nbuffers; // advance for next frame
    _cpuIndex = (_cpuIndex + 1) % _nbuffers;       // advance for next frame
    if (unlikely(buffer.texture == nullptr)) {
      CREATE_BUFFER;
    } else {
      buffer.texture->GetDesc(&desc2);
      if (unlikely(desc.Width != desc2.Width || desc.Height != desc2.Height)) {
        buffer.texture->Release();
        CREATE_BUFFER;
      }
    }

    _ctx->CopyResource(buffer.texture, texture);

    texture->Release();
    err = _dup->ReleaseFrame();
    assert(err == 0);

    return Normal;
  }

  void update() {
    auto &buffer = _buffers[_cpuIndex];
    if (buffer.texture != nullptr && buffer.valid) {
      D3D11_MAPPED_SUBRESOURCE resource;
      HRESULT err = _ctx->Map(buffer.texture, 0, D3D11_MAP_READ, 0, &resource);
      assert(err == 0);
      memcpy(_cpuBuffer, resource.pData, 4 * _width * _height);
      _ctx->Unmap(buffer.texture, 0);
    }
  }
};
}; // namespace chainblocks
