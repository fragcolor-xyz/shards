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
  gfxWgpuBufferMapAsync__deps: ['$WebGPU', '$callUserCallback'],
  gfxWgpuBufferMapAsync: (bufferId, mode, offset, size, callback, userdata) => {
    var bufferWrapper = WebGPU.mgrBuffer.objects[bufferId];
    {{{ gpu.makeCheckDefined('bufferWrapper') }}}
    bufferWrapper.mapMode = mode;
    bufferWrapper.onUnmap = [];
    var buffer = bufferWrapper.object;

    {{{ gpu.convertSentinelToUndefined('size') }}}

    // `callback` takes (WGPUBufferMapAsyncStatus status, void * userdata)

    {{{ runtimeKeepalivePush() }}}
    buffer.mapAsync(mode, offset, size).then(() => {
      {{{ runtimeKeepalivePop() }}}
      callUserCallback(() => {
        {{{ makeDynCall('vip', 'callback') }}}({{{ gpu.BufferMapAsyncStatus.Success }}}, userdata);
      });
    }, () => {
      {{{ runtimeKeepalivePop() }}}
      callUserCallback(() => {
        // TODO(kainino0x): Figure out how to pick other error status values.
        {{{ makeDynCall('vip', 'callback') }}}({{{ gpu.BufferMapAsyncStatus.ValidationError }}}, userdata);
      });
    });
  },
  
  gfxWgpuBufferReadInto__deps: ['$WebGPU'],
  gfxWgpuBufferReadInto: (bufferId, dst, offset, size) => {
    var bufferWrapper = WebGPU.mgrBuffer.objects[bufferId];
    {{{ gpu.makeCheckDefined('bufferWrapper') }}}
    var buffer = bufferWrapper.object;

    var mapped;
    try {
      mapped = bufferWrapper.object.getMappedRange(offset, size);
    } catch (ex) {
#if ASSERTIONS
      err(`gfxWgpuReadMappedBuffer(${dst}, ${offset}, ${size}) failed: ${ex}`);
#endif
      return 0;
    }

    // Copy directly into the heap at the given into pointer
    HEAPU8.set(new Uint8Array(mapped), dst);
  },
};

addToLibrary(LibraryGFXWebGPU);
