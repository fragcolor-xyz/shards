var LibraryGFXEvents = {
  sleep: async function (ms) {
    await new Promise(done => setTimeout(done, ms));
  },

  // Helper to call a function in non-async context
  // With Asyncify: temporarily disable async state to prevent unwinding
  // With JSPI: just call directly (JSPI only suspends at marked imports)
  $callNonAsync__deps: [],
  $callNonAsync: function(fn) {
#if ASYNCIFY
    let t = Asyncify.currData;
    Asyncify.currData = 0;
    fn();
    Asyncify.currData = t;
#else
    // JSPI or no async - just call directly
    fn();
#endif
  },
  gfxClipboardGet__proxy: 'async',
  gfxClipboardGet: function (dataPtrPtr, readyPtr) {
    console.log("Reading data from clipboard");
    (async () => {
      while (true) {
        try {
          let text = await navigator.clipboard.readText();
          Atomics.store(HEAP32, dataPtrPtr >> 2, stringToNewUTF8(text));
          Atomics.store(HEAP32, readyPtr >> 2, 0x01000000);
          console.log('Read data from clipboard: ', text);
          break;
        } catch (e) {
          console.warn("Failed to get clipboard", e);
          // Might fail sometimes if document is not focused
          await sleep(100);
        }
      }
    })();
  },
  gfxClipboardSet__proxy: 'async',
  gfxClipboardSet: function (dataCopy) {
    (async () => {
      try {
        let text = UTF8ToString(dataCopy);
        console.log('Copying data to clipboard: ', text);
        await navigator.clipboard.writeText(text);
        console.log('Data copied to clipboard');
        _free(dataCopy);
      } catch (e) {
        console.warn("Failed to set clipboard", e);
      }
    })();
  },
  $gfxSetup__deps: ["$Browser", "$callNonAsync"],
  $gfxSetup(canvasContainer, canvas) {
    // Use direct C function calls instead of embind
    const eh = Module._gfxGetEventHandler();

    // For strings, use ccall which handles string conversion
    Module.ccall('gfxEventHandlerSetCanvas', null, ['number', 'string', 'string'], [eh, canvas.id, canvasContainer.id]);

    // Helper functions that call C directly with individual parameters
    const postKeyEvent = (type, domKey, key, ctrlKey, altKey, shiftKey, repeat) => {
      Module._gfxEventHandlerPostKeyEvent(eh, type, domKey, key, ctrlKey ? 1 : 0, altKey ? 1 : 0, shiftKey ? 1 : 0, repeat ? 1 : 0);
    };
    const postMouseEvent = (type, x, y, button, movementX, movementY) => {
      Module._gfxEventHandlerPostMouseEvent(eh, type, x, y, button, movementX, movementY);
    };
    const postWheelEvent = (deltaY) => {
      Module._gfxEventHandlerPostWheelEvent(eh, deltaY);
    };
    const postDisplayFormat = (width, height, cwidth, cheight, pixelRatio) => {
      Module._gfxEventHandlerPostDisplayFormat(eh, width, height, cwidth, cheight, pixelRatio);
    };

    // Send initial display format synchronously before graphics starts
    // IMPORTANT: Must set canvas.width/height (intrinsic size) for WebGPU surface
    // The CSS size (getBoundingClientRect) is only for layout; WebGPU uses intrinsic size
    {
      const rect = canvasContainer.getBoundingClientRect();
      const pixelRatio = window.devicePixelRatio;
      let canvasWidth = rect.width * pixelRatio;
      let canvasHeight = rect.height * pixelRatio;
      canvas.width = canvasWidth;
      canvas.height = canvasHeight;
      postDisplayFormat(rect.width, rect.height, canvasWidth, canvasHeight, pixelRatio);
    }

    canvasContainer.onmousemove = (e) => {
      callNonAsync(() => postMouseEvent(0, e.x, e.y, e.button, e.movementX, e.movementY));
    };
    const handleMouseEvent = (e, type) => {
      callNonAsync(() => postMouseEvent(type, e.x, e.y, e.button, e.movementX, e.movementY));
    };
    canvasContainer.onmousedown = (e) => {
      handleMouseEvent(e, 1);
    };
    canvasContainer.onmouseup = (e) => {
      handleMouseEvent(e, 2);
    };
    canvasContainer.oncontextmenu = (e) => {
      e.preventDefault();
    };

    var state = { cursorInPage: false };
    canvasContainer.onmouseout = () => {
      state.cursorInPage = false;
    };
    canvasContainer.onmouseover = () => {
      state.cursorInPage = true;
    };
    const trapKeyEvents = (code) => { return state.cursorInPage; };
    const handleKeyEvent = (event, type) => {
      const domKey = event.keyCode;
      const key = (event.key.length == 1) ? event.key.codePointAt(0) : 0;
      callNonAsync(() => postKeyEvent(type, domKey, key, event.ctrlKey, event.altKey, event.shiftKey, event.repeat));
    };
    window.addEventListener('keydown', (event) => {
      if (trapKeyEvents(event.code)) {
        handleKeyEvent(event, 0);
        event.preventDefault();
      }
    }, true);
    window.addEventListener('keyup', (event) => {
      handleKeyEvent(event, 1);
      if (trapKeyEvents(event.code)) {
        event.preventDefault();
      }
    }, true);
    window.addEventListener('wheel', (event) => {
      // console.log("Wheel", event);

      // Flip the wheel direction to translate from browser wheel direction
      // (+:down) to SDL direction (+:up)
      var deltaY = -Browser.getMouseWheelDelta(event);
      // Quantize to integer so that minimum scroll is at least +/- 1.
      deltaY = (deltaY == 0) ? 0 : (deltaY > 0 ? Math.max(deltaY, 1) : Math.min(deltaY, -1));

      callNonAsync(() => postWheelEvent(deltaY));
    });

    (async function resizeCanvasLoop() {
      var lastW, lastH;
      var lastPixelRatio;
      while (true) {
        const rect = canvasContainer.getBoundingClientRect();
        const pixelRatio = window.devicePixelRatio;
        if (lastW !== rect.width || lastH !== rect.height || lastPixelRatio !== pixelRatio) {
          let canvasWidth = rect.width * pixelRatio;
          let canvasHeight = rect.height * pixelRatio;
          // Update canvas intrinsic size for WebGPU surface
          canvas.width = canvasWidth;
          canvas.height = canvasHeight;
          callNonAsync(() => postDisplayFormat(rect.width, rect.height, canvasWidth, canvasHeight, pixelRatio));
          lastW = rect.width;
          lastH = rect.height;
          lastPixelRatio = pixelRatio;
        }
        await new Promise(done => setTimeout(done, 1));
      }
    })();
  }
};

addToLibrary(LibraryGFXEvents);
