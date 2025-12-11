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
    const eh = this.getEventHandler();
    this.eventHandlerSetCanvas(eh, canvas.id, canvasContainer.id);

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
      this.eventHandlerPostDisplayFormat(eh, rect.width, rect.height, canvasWidth, canvasHeight, pixelRatio);
    }

    canvasContainer.onmousemove = (e) => {
      e.type_ = 0;
      callNonAsync(() => this.eventHandlerPostMouseEvent(eh, e));
    };
    const handleMouseEvent = (e) => {
      callNonAsync(() => this.eventHandlerPostMouseEvent(eh, e));
    };
    canvasContainer.onmousedown = (e) => {
      e.type_ = 1;
      handleMouseEvent(e);
    };
    canvasContainer.onmouseup = (e) => {
      e.type_ = 2;
      handleMouseEvent(e);
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
    const handleKeyEvent = (event) => {
      event.domKey_ = event.keyCode;
      if (event.key.length == 1) {
        event.key_ = event.key.codePointAt(0);
      } else {
        event.key_ = 0;
      }
      callNonAsync(() => this.eventHandlerPostKeyEvent(eh, event));
    };
    window.addEventListener('keydown', (event) => {
      if (trapKeyEvents(event.code)) {
        event.type_ = 0;
        handleKeyEvent(event);
        event.preventDefault();
      }
    }, true);
    window.addEventListener('keyup', (event) => {
      event.type_ = 1;
      handleKeyEvent(event);
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

      callNonAsync(() => this.eventHandlerPostWheelEvent(eh, { deltaY: deltaY }));
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
          callNonAsync(() => this.eventHandlerPostDisplayFormat(eh, rect.width, rect.height, canvasWidth, canvasHeight, pixelRatio));
          lastW = rect.width;
          lastH = rect.height;
          lastPixelRatio = pixelRatio;
        }
        await new Promise(done => setTimeout(done, 1));
      }
    }.bind(this))();
  }
};

addToLibrary(LibraryGFXEvents);
