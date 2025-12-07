var LibraryGFXWebGPU = {
  // Custom function to read a mapped buffer directly into WASM memory
  // This is faster than the default implementation that copies into a temporary buffer
  gfxWgpuBufferReadInto__deps: ['$WebGPU'],
  gfxWgpuBufferReadInto: (bufferPtr, dst, offset, size) => {
    var buffer = WebGPU.getJsObject(bufferPtr);
#if ASSERTIONS
    assert(buffer, 'buffer not found for ptr ' + bufferPtr);
#endif

    var mapped;
    try {
      mapped = buffer.getMappedRange(offset, size);
    } catch (ex) {
#if ASSERTIONS
      err(`gfxWgpuBufferReadInto(${dst}, ${offset}, ${size}) failed: ${ex}`);
#endif
      return 0;
    }

    // Copy directly into the heap at the given into pointer
    HEAPU8.set(new Uint8Array(mapped), dst);
  },
};

addToLibrary(LibraryGFXWebGPU);
