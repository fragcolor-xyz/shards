var LibraryGFXWebGPU = {
  gfxWgpuDeviceCreateSwapChain__deps: ['$WebGPU'],
  gfxWgpuDeviceCreateSwapChain: (deviceId, surfaceId, descriptor) => {
    {{{ gpu.makeCheckDescriptor('descriptor') }}}
    var device = WebGPU.mgrDevice.get(deviceId);
    var context = WebGPU.mgrSurface.get(surfaceId);

#if ASSERTIONS
    assert({{{ gpu.PresentMode.Fifo }}} ===
      {{{ gpu.makeGetU32('descriptor', C_STRUCTS.WGPUSwapChainDescriptor.presentMode) }}});
#endif

    var canvasSize = [
      {{{ gpu.makeGetU32('descriptor', C_STRUCTS.WGPUSwapChainDescriptor.width) }}},
      {{{ gpu.makeGetU32('descriptor', C_STRUCTS.WGPUSwapChainDescriptor.height) }}}
    ];

    if (canvasSize[0] !== 0) {
      context["canvas"]["width"] = canvasSize[0];
    }

    if (canvasSize[1] !== 0) {
      context["canvas"]["height"] = canvasSize[1];
    }

    var format = WebGPU.TextureFormat[{{{ gpu.makeGetU32('descriptor', C_STRUCTS.WGPUSwapChainDescriptor.format) }}}];
    var viewFormats = [];
    if(format == "rgba8unorm" || format == "bgra8unorm") {
      viewFormats.push(`${format}-srgb`);
    }

    var configuration = {
      "device": device,
      "format": format,
      "usage": {{{ gpu.makeGetU32('descriptor', C_STRUCTS.WGPUSwapChainDescriptor.usage) }}},
      "alphaMode": "opaque",
      "viewFormats": viewFormats,
    };
    context.configure(configuration);

    return WebGPU.mgrSwapChain.create(context);
  },
};

addToLibrary(LibraryGFXWebGPU);
