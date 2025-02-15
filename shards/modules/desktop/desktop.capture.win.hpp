/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#ifndef SH_EXTRA_DESKTOP_CAPTURE_WIN
#define SH_EXTRA_DESKTOP_CAPTURE_WIN

#include <shards/shards.h>
#include <shards/core/runtime.hpp>
#include <D3Dcommon.h>
#include <Windows.h>
#include <array>
#include <cassert>
#include <d3d11.h>
#include <dxgi.h>
#include <iostream>
#include <dxgi1_2.h>
#include <dxgi1_3.h>
#include <dxgi1_4.h>
#include <dxgi1_5.h>
#include <dxgi1_6.h>
#include <directxmath.h>
#include <d3dcompiler.h>

using namespace DirectX;

namespace shards {

// Helper class for D3D error handling
class D3DError {
public:
  static std::string GetErrorMessage(HRESULT hr) {
    char errorBuffer[256];
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), errorBuffer,
                   sizeof(errorBuffer), NULL);
    return std::string(errorBuffer);
  }

  static bool Check(HRESULT hr, const char *operation, const char *file, int line) {
    if (FAILED(hr)) {
      std::string error = GetErrorMessage(hr);
      SHLOG_ERROR("D3D operation '{}' failed with error 0x{:08X} - {} at {}:{}", operation, static_cast<unsigned int>(hr), error,
                  file, line);
      return false;
    }
    return true;
  }

  static bool CheckDevice(HRESULT hr, ID3D11Device *device, const char *operation, const char *file, int line) {
    if (FAILED(hr)) {
      std::string error = GetErrorMessage(hr);

      // Get extended error info from Debug device if available
      if (device) {
        ID3D11Debug *debug = nullptr;
        if (SUCCEEDED(device->QueryInterface(__uuidof(ID3D11Debug), reinterpret_cast<void **>(&debug)))) {
          debug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
          debug->Release();
        }
      }

      SHLOG_ERROR("D3D device operation '{}' failed with error 0x{:08X} - {} at {}:{}", operation, static_cast<unsigned int>(hr),
                  error, file, line);
      return false;
    }
    return true;
  }
};

// Macro helpers for error checking
#define D3D_CHECK(hr, op) D3DError::Check(hr, op, __FILE__, __LINE__)

#define D3D_CHECK_DEVICE(hr, device, op) D3DError::CheckDevice(hr, device, op, __FILE__, __LINE__)

struct ScreenInfo {
  IDXGIAdapter *adapter;
  IDXGIOutput *output;
  HMONITOR screen;
};

class DXGIDesktopCapture {
  struct FrameData {
    ID3D11Texture2D *texture = nullptr;
    DXGI_FORMAT format = DXGI_FORMAT_B8G8R8A8_UNORM;
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

  bool _isHDR = false;
  DXGI_COLOR_SPACE_TYPE _colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;

  ID3D11ComputeShader *_tonemapCS = nullptr;
  ID3D11Buffer *_tonemapParams = nullptr;
  ID3D11UnorderedAccessView *_outputUAV = nullptr;
  ID3D11ShaderResourceView *_inputSRV = nullptr;
  bool _enableDither = true;

  // Reusable textures for HDR processing
  ID3D11Texture2D *_sdrTexture = nullptr;
  ID3D11Texture2D *_stagingTexture = nullptr;

  struct TonemapParams {
    float exposure;
    float gamma;
    float dither;
    float pad;
  };

  static constexpr DXGI_FORMAT HDR_FORMAT = DXGI_FORMAT_R16G16B16A16_FLOAT;
  static constexpr DXGI_FORMAT SDR_FORMAT = DXGI_FORMAT_B8G8R8A8_UNORM;
  static constexpr UINT THREAD_GROUP_SIZE = 8;
  static constexpr UINT FRAME_TIMEOUT_MS = 1000;

public:
  enum State { Normal, Timeout, Lost, Error };

  const int left() { return _left; }
  const int right() { return _right; }
  const int top() { return _top; }
  const int bottom() { return _bottom; }
  const int width() { return _width; }
  const int height() { return _height; }
  const uint8_t *image() { return _cpuBuffer; }
  bool isHDR() const { return _isHDR; }
  DXGI_FORMAT getCurrentFormat() const { return _buffers[_cpuIndex].format; }

  static ScreenInfo FindScreen(int x, int y, int width, int height) {
    ScreenInfo result{};
    IDXGIFactory6 *factory6 = nullptr;
    IDXGIFactory1 *factory1 = nullptr;
    HRESULT err = 0;

    // Try to create IDXGIFactory6 first
    err = CreateDXGIFactory2(0, __uuidof(IDXGIFactory6), (void **)&factory6);
    if (D3D_CHECK(err, "CreateDXGIFactory2")) {
      SHLOG_INFO("Using modern DXGI factory for screen enumeration");
      // Use modern enumeration path with IDXGIFactory6
      UINT adapterIndex = 0;
      IDXGIAdapter1 *adapter1 = nullptr;

      while (factory6->EnumAdapters1(adapterIndex, &adapter1) != DXGI_ERROR_NOT_FOUND) {
        // Try to get IDXGIAdapter4 interface
        IDXGIAdapter4 *adapter4 = nullptr;
        if (D3D_CHECK(adapter1->QueryInterface(__uuidof(IDXGIAdapter4), (void **)&adapter4), "QueryInterface IDXGIAdapter4")) {
          DXGI_ADAPTER_DESC3 adapterDesc = {};
          adapter4->GetDesc3(&adapterDesc);

          UINT outputIndex = 0;
          IDXGIOutput *output = nullptr;

          while (adapter4->EnumOutputs(outputIndex, &output) != DXGI_ERROR_NOT_FOUND) {
            // Try to get IDXGIOutput6 interface
            IDXGIOutput6 *output6 = nullptr;
            if (D3D_CHECK(output->QueryInterface(__uuidof(IDXGIOutput6), (void **)&output6), "QueryInterface IDXGIOutput6")) {
              DXGI_OUTPUT_DESC1 outputDesc = {};
              if (D3D_CHECK(output6->GetDesc1(&outputDesc), "GetDesc1")) {
                if (outputDesc.AttachedToDesktop) {
                  MONITORINFO minfo;
                  minfo.cbSize = sizeof(MONITORINFO);
                  GetMonitorInfo(outputDesc.Monitor, &minfo);

                  if (x >= minfo.rcMonitor.left && x < minfo.rcMonitor.right && y >= minfo.rcMonitor.top &&
                      y < minfo.rcMonitor.bottom) {
                    // Found our target display
                    result.adapter = adapter4;
                    result.output = output6;
                    result.screen = outputDesc.Monitor;
                    adapter1->Release();
                    factory6->Release();
                    return result;
                  }
                }
                output6->Release();
              }
            }
            output->Release();
            outputIndex++;
          }
          adapter4->Release();
        }
        adapter1->Release();
        adapterIndex++;
      }
      factory6->Release();
    } else {
      SHLOG_INFO("Using legacy DXGI factory for screen enumeration");
      // Fallback to legacy path with IDXGIFactory1
      err = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void **)&factory1);
      if (D3D_CHECK(err, "CreateDXGIFactory1")) {
        // ...existing legacy enumeration code...
        UINT aindex = 0;
        while (factory1->EnumAdapters(aindex, &result.adapter) != DXGI_ERROR_NOT_FOUND) {
          UINT oindex = 0;
          while (result.adapter->EnumOutputs(oindex, &result.output) != DXGI_ERROR_NOT_FOUND) {
            DXGI_OUTPUT_DESC desc;
            MONITORINFO minfo;

            err = result.output->GetDesc(&desc);
            D3D_CHECK(err, "GetDesc");

            result.screen = desc.Monitor;

            minfo.cbSize = sizeof(MONITORINFO);
            auto yes = GetMonitorInfo(result.screen, &minfo);
            assert(yes);

            if (x >= minfo.rcMonitor.left && x < minfo.rcMonitor.right && y >= minfo.rcMonitor.top &&
                y < minfo.rcMonitor.bottom) {
              factory1->Release();
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
        factory1->Release();
      }
    }
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
    if (!D3D_CHECK(err, "D3D11CreateDevice")) {
      return;
    }

    IDXGIOutput6 *output6 = nullptr;
    HRESULT hr = screen.output->QueryInterface(__uuidof(IDXGIOutput6), (void **)&output6);
    if (D3D_CHECK(hr, "QueryInterface IDXGIOutput6")) {
      _isHDR = CheckHDRSupport(output6);

      // Setup formats and color space for HDR
      DXGI_FORMAT supportedFormats[] = {DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_B8G8R8A8_UNORM};

      // Required for proper DPI handling
      SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

      // Use DuplicateOutput1 with proper format specification
      hr = output6->DuplicateOutput1(_device,          // D3D device
                                     0,                // Flags
                                     2,                // Number of supported formats
                                     supportedFormats, // Array of supported formats
                                     &_dup             // Output duplication interface
      );

      if (!D3D_CHECK(hr, "DuplicateOutput1")) {
        return;
      }

      if (_isHDR) {
        _colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
        for (auto &buf : _buffers) {
          buf.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        }
      }

      output6->Release();
    } else {
      // Fallback to non-HDR duplication
      IDXGIOutput1 *output1;
      err = screen.output->QueryInterface(__uuidof(output1), (void **)&output1);
      if (!D3D_CHECK(err, "QueryInterface IDXGIOutput1")) {
        return;
      }

      err = output1->DuplicateOutput(_device, &_dup);
      if (!D3D_CHECK(err, "DuplicateOutput")) {
        return;
      }
      output1->Release();
    }

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

    if (_tonemapCS)
      _tonemapCS->Release();
    if (_tonemapParams)
      _tonemapParams->Release();
    if (_outputUAV)
      _outputUAV->Release();
    if (_inputSRV)
      _inputSRV->Release();
    if (_sdrTexture)
      _sdrTexture->Release();
    if (_stagingTexture)
      _stagingTexture->Release();
  }

  bool CheckHDRSupport(IDXGIOutput6 *output6) {
    DXGI_OUTPUT_DESC1 desc1;
    HRESULT hr = output6->GetDesc1(&desc1);
    if (!D3D_CHECK(hr, "GetDesc1")) {
      return false;
    }
    return desc1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
  }

  HRESULT CreateTextureWithDesc(const D3D11_TEXTURE2D_DESC &desc, ID3D11Texture2D **ppTexture) {
    HRESULT hr = _device->CreateTexture2D(&desc, nullptr, ppTexture);
    D3D_CHECK_DEVICE(hr, _device, "CreateTexture2D");
    return hr;
  }

  void CreateBuffer(FrameData &buffer, const D3D11_TEXTURE2D_DESC &sourceDesc) {
    D3D11_TEXTURE2D_DESC desc2 = sourceDesc;
    if (_isHDR) {
      desc2.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
      // For HDR we need both DEFAULT usage and proper bind flags
      desc2.Usage = D3D11_USAGE_DEFAULT;
      desc2.BindFlags = D3D11_BIND_SHADER_RESOURCE;
      desc2.CPUAccessFlags = 0;
    } else {
      // For SDR we use staging texture
      desc2.Usage = D3D11_USAGE_STAGING;
      desc2.BindFlags = 0;
      desc2.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    }
    desc2.MiscFlags = 0;

    HRESULT err = _device->CreateTexture2D(&desc2, nullptr, &buffer.texture);
    if (!D3D_CHECK_DEVICE(err, _device, "CreateBuffer")) {
      return;
    }
    buffer.format = desc2.Format;
  }

  void CreateTonemapResources() {
    // Simplified Reinhard tonemap compute shader
    const char *cs_hlsl = R"(
      cbuffer TonemapParams : register(b0) {
        float exposure;
        float gamma;
        float dither;
        float pad;
      };

      Texture2D<float4> Input : register(t0);
      RWTexture2D<unorm float4> Output : register(u0);

      float3 Reinhard(float3 hdr) {
        float3 color = hdr * exposure;
        return color / (1.0 + color);
      }

      float Random(float2 p) {
        return frac(sin(dot(p, float2(12.9898f, 78.233f))) * 43758.5453f);
      }

      [numthreads(8, 8, 1)]
      void main(uint3 DTid : SV_DispatchThreadID) {
        float4 hdr = Input[DTid.xy];
        float3 color = Reinhard(hdr.rgb);
        
        // Apply gamma correction
        color = pow(color, 1.0/gamma);

        // Optional dithering
        if (dither > 0.0) {
          float noise = Random(DTid.xy) * 2.0 - 1.0;
          color += noise * dither / 255.0;
        }

        Output[DTid.xy] = float4(saturate(color), hdr.a);
      }
    )";

    // Create compute shader and resources
    // Note: In real implementation, compile this shader at build time
    ID3DBlob *csBlob;
    HRESULT hr = D3DCompile(cs_hlsl, strlen(cs_hlsl), nullptr, nullptr, nullptr, "main", "cs_5_0", 0, 0, &csBlob, nullptr);
    if (!D3D_CHECK(hr, "D3DCompile")) {
      return;
    }

    hr = _device->CreateComputeShader(csBlob->GetBufferPointer(), csBlob->GetBufferSize(), nullptr, &_tonemapCS);
    if (!D3D_CHECK_DEVICE(hr, _device, "CreateComputeShader")) {
      csBlob->Release();
      return;
    }
    csBlob->Release();

    // Create parameter buffer
    D3D11_BUFFER_DESC bdesc = {};
    bdesc.ByteWidth = sizeof(TonemapParams);
    bdesc.Usage = D3D11_USAGE_DYNAMIC;
    bdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    _device->CreateBuffer(&bdesc, nullptr, &_tonemapParams);
  }

  // Modify ToneMapTexture to add proper synchronization
  void ToneMapTexture(ID3D11Texture2D *hdrTexture, ID3D11Texture2D *sdrTexture) {
    if (!_tonemapCS) {
      CreateTonemapResources();
    }
    assert(_tonemapCS != nullptr && "Compute shader creation failed");
    assert(_tonemapParams != nullptr && "Tonemap params buffer creation failed");

    // Verify input texture format
    D3D11_TEXTURE2D_DESC hdrDesc;
    hdrTexture->GetDesc(&hdrDesc);
    assert(hdrDesc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT && "Input texture must be HDR format");

    // Verify output texture format
    D3D11_TEXTURE2D_DESC sdrDesc;
    sdrTexture->GetDesc(&sdrDesc);
    assert(sdrDesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM && "Output texture must be SDR format");
    assert(sdrDesc.BindFlags & D3D11_BIND_UNORDERED_ACCESS && "Output texture must have UAV bind flag");

    // Create/Update views if needed
    if (!_inputSRV) {
      // Release previous view if it exists
      if (_inputSRV) {
        _inputSRV->Release();
        _inputSRV = nullptr;
      }

      // Get proper texture description
      D3D11_TEXTURE2D_DESC texDesc;
      hdrTexture->GetDesc(&texDesc);

      // Create proper SRV description
      D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
      srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; // Must match HDR texture format
      srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
      srvDesc.Texture2D.MostDetailedMip = 0;
      srvDesc.Texture2D.MipLevels = 1;

      // Ensure texture has proper bind flags
      if (!(texDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE)) {
        // Need to create an intermediate texture with proper bind flags
        D3D11_TEXTURE2D_DESC intermediateDesc = texDesc;
        intermediateDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        intermediateDesc.Usage = D3D11_USAGE_DEFAULT;
        intermediateDesc.CPUAccessFlags = 0;

        ID3D11Texture2D *intermediateTex = nullptr;
        HRESULT hr = _device->CreateTexture2D(&intermediateDesc, nullptr, &intermediateTex);
        if (!D3D_CHECK_DEVICE(hr, _device, "CreateIntermediateTexture")) {
          return;
        }

        // Copy content
        _ctx->CopyResource(intermediateTex, hdrTexture);

        // Create SRV from intermediate texture
        hr = _device->CreateShaderResourceView(intermediateTex, &srvDesc, &_inputSRV);
        if (!D3D_CHECK_DEVICE(hr, _device, "CreateShaderResourceView")) {
          intermediateTex->Release();
          return;
        }
        intermediateTex->Release();
      } else {
        // Create SRV directly from input texture
        HRESULT hr = _device->CreateShaderResourceView(hdrTexture, &srvDesc, &_inputSRV);
        if (!D3D_CHECK_DEVICE(hr, _device, "CreateShaderResourceView")) {
          return;
        }
      }
    }

    if (!_outputUAV) {
      D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
      uavDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
      uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
      HRESULT hr = _device->CreateUnorderedAccessView(sdrTexture, &uavDesc, &_outputUAV);
      assert(SUCCEEDED(hr) && "Failed to create output UAV");
    }

    // Update tonemap parameters
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = _ctx->Map(_tonemapParams, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (!D3D_CHECK(hr, "Map tonemap parameters")) {
      return;
    }

    TonemapParams *params = (TonemapParams *)mapped.pData;
    params->exposure = 2.0f; // Increased exposure for testing
    params->gamma = 2.2f;
    params->dither = _enableDither ? 0.5f : 0.0f;
    _ctx->Unmap(_tonemapParams, 0);

    // Clear previous bindings
    ID3D11UnorderedAccessView *nullUAV = nullptr;
    ID3D11ShaderResourceView *nullSRV = nullptr;
    _ctx->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
    _ctx->CSSetShaderResources(0, 1, &nullSRV);

    // Set resources and dispatch
    _ctx->CSSetShader(_tonemapCS, nullptr, 0);
    _ctx->CSSetConstantBuffers(0, 1, &_tonemapParams);
    _ctx->CSSetShaderResources(0, 1, &_inputSRV);
    _ctx->CSSetUnorderedAccessViews(0, 1, &_outputUAV, nullptr);

    UINT dispatchX = (_width + 7) / 8;
    UINT dispatchY = (_height + 7) / 8;
    _ctx->Dispatch(dispatchX, dispatchY, 1);

    // Ensure compute shader completion
    ID3D11Query *query = nullptr;
    D3D11_QUERY_DESC queryDesc = {};
    queryDesc.Query = D3D11_QUERY_EVENT;
    hr = _device->CreateQuery(&queryDesc, &query);
    assert(SUCCEEDED(hr) && "Failed to create query");

    _ctx->End(query);

    // Wait with timeout to prevent infinite loop
    const UINT timeoutMs = 1000;
    UINT startTime = GetTickCount();
    while (_ctx->GetData(query, nullptr, 0, 0) == S_FALSE) {
      if (GetTickCount() - startTime > timeoutMs) {
        assert(false && "Compute shader timeout");
        break;
      }
      Sleep(1);
    }
    query->Release();

    // Copy to staging texture
    _ctx->CopyResource(_stagingTexture, sdrTexture);

    // Clear bindings
    _ctx->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
    _ctx->CSSetShaderResources(0, 1, &nullSRV);
  }

  State capture() {
    DXGI_OUTDUPL_FRAME_INFO info;
    IDXGIResource *resource;
    HRESULT hr = _dup->AcquireNextFrame(0, &info, &resource);

    if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
      return Timeout;
    } else if (hr == DXGI_ERROR_ACCESS_LOST) {
      return Lost;
    } else if (!D3D_CHECK(hr, "AcquireNextFrame")) {
      return Error;
    }

    ID3D11Texture2D *texture;
    hr = resource->QueryInterface(__uuidof(texture), (void **)&texture);
    if (!D3D_CHECK(hr, "QueryInterface texture")) {
      resource->Release();
      return Error;
    }
    resource->Release();

    D3D11_TEXTURE2D_DESC desc, desc2;
    texture->GetDesc(&desc);

    auto &buffer = _buffers[_bufferIndex];
    buffer.valid = true;
    _bufferIndex = (_bufferIndex + 1) % _nbuffers; // advance for next frame
    _cpuIndex = (_cpuIndex + 1) % _nbuffers;       // advance for next frame
    if (unlikely(buffer.texture == nullptr)) {
      CreateBuffer(buffer, desc);
    } else {
      buffer.texture->GetDesc(&desc2);
      if (unlikely(desc.Width != desc2.Width || desc.Height != desc2.Height)) {
        buffer.texture->Release();
        CreateBuffer(buffer, desc);
      }
    }

    _ctx->CopyResource(buffer.texture, texture);

    texture->Release();
    hr = _dup->ReleaseFrame();
    if (!D3D_CHECK(hr, "ReleaseFrame")) {
      return Error;
    }

    return Normal;
  }

  void CopyTextureData(const void *src, size_t srcStride, void *dst, size_t dstStride, size_t rowSize, size_t height) {
    if (srcStride == dstStride) {
      memcpy(dst, src, srcStride * height);
    } else {
      const uint8_t *srcPtr = static_cast<const uint8_t *>(src);
      uint8_t *dstPtr = static_cast<uint8_t *>(dst);
      for (size_t y = 0; y < height; ++y) {
        memcpy(dstPtr, srcPtr, rowSize);
        srcPtr += srcStride;
        dstPtr += dstStride;
      }
    }
  }

  bool EnsureHDRResources() {
    if (!_isHDR || _sdrTexture) {
      return true;
    }

    D3D11_TEXTURE2D_DESC desc;
    _buffers[0].texture->GetDesc(&desc);

    // Create SDR texture for tone mapping output
    desc.Format = SDR_FORMAT;
    desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.CPUAccessFlags = 0;

    HRESULT hr = CreateTextureWithDesc(desc, &_sdrTexture);
    if (FAILED(hr))
      return false;

    // Create staging texture for CPU read
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    hr = CreateTextureWithDesc(desc, &_stagingTexture);
    if (FAILED(hr)) {
      _sdrTexture->Release();
      _sdrTexture = nullptr;
      return false;
    }

    return true;
  }

  void CalculateDispatchDimensions(UINT width, UINT height, UINT &dispatchX, UINT &dispatchY) {
    dispatchX = (width + THREAD_GROUP_SIZE - 1) / THREAD_GROUP_SIZE;
    dispatchY = (height + THREAD_GROUP_SIZE - 1) / THREAD_GROUP_SIZE;
  }

  void update() {
    auto &buffer = _buffers[_cpuIndex];
    if (!buffer.texture || !buffer.valid) {
      return;
    }

    if (_isHDR) {
      if (!EnsureHDRResources()) {
        return;
      }

      // Process HDR content
      ToneMapTexture(buffer.texture, _sdrTexture);

      // Read final SDR result
      D3D11_MAPPED_SUBRESOURCE resource;
      if (D3D_CHECK(_ctx->Map(_stagingTexture, 0, D3D11_MAP_READ, 0, &resource), "Map staging texture")) {
        CopyTextureData(resource.pData, resource.RowPitch, _cpuBuffer, _width * 4, _width * 4, _height);
        _ctx->Unmap(_stagingTexture, 0);
      }
    } else {
      D3D11_MAPPED_SUBRESOURCE resource;
      if (SUCCEEDED(_ctx->Map(buffer.texture, 0, D3D11_MAP_READ, 0, &resource))) {
        CopyTextureData(resource.pData, resource.RowPitch, _cpuBuffer, _width * 4, _width * 4, _height);
        _ctx->Unmap(buffer.texture, 0);
      }
    }
  }

  void setDithering(bool enable) { _enableDither = enable; }
};
}; // namespace shards

#endif // SH_EXTRA_DESKTOP_CAPTURE_WIN
