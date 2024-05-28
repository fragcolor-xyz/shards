const Helper = {

  // DOM code ==> SDL code. See
  // https://developer.mozilla.org/en/Document_Object_Model_%28DOM%29/KeyboardEvent
  // and SDL_keycode.h
  // For keys that don't have unicode value, we map DOM codes with the
  // corresponding scan codes + 1024 (using "| 1 << 10")
  keyCodes: {
    16: 225 | 1 << 10, // shift
    17: 224 | 1 << 10, // control (right, or left)
    18: 226 | 1 << 10, // alt
    20: 57 | 1 << 10, // caps lock

    33: 75 | 1 << 10, // pagedup
    34: 78 | 1 << 10, // pagedown
    35: 77 | 1 << 10, // end
    36: 74 | 1 << 10, // home
    37: 80 | 1 << 10, // left arrow
    38: 82 | 1 << 10, // up arrow
    39: 79 | 1 << 10, // right arrow
    40: 81 | 1 << 10, // down arrow
    44: 316, // print screen
    45: 73 | 1 << 10, // insert
    46: 127, // SDLK_DEL == '\177'

    91: 227 | 1 << 10, // windows key or super key on linux (doesn't work on Mac)
    93: 101 | 1 << 10, // application

    96: 98 | 1 << 10, // keypad 0
    97: 89 | 1 << 10, // keypad 1
    98: 90 | 1 << 10, // keypad 2
    99: 91 | 1 << 10, // keypad 3
    100: 92 | 1 << 10, // keypad 4
    101: 93 | 1 << 10, // keypad 5
    102: 94 | 1 << 10, // keypad 6
    103: 95 | 1 << 10, // keypad 7
    104: 96 | 1 << 10, // keypad 8
    105: 97 | 1 << 10, // keypad 9
    106: 85 | 1 << 10, // keypad multiply
    107: 87 | 1 << 10, // keypad plus
    109: 86 | 1 << 10, // keypad minus
    110: 99 | 1 << 10, // keypad decimal point
    111: 84 | 1 << 10, // keypad divide
    112: 58 | 1 << 10, // F1
    113: 59 | 1 << 10, // F2
    114: 60 | 1 << 10, // F3
    115: 61 | 1 << 10, // F4
    116: 62 | 1 << 10, // F5
    117: 63 | 1 << 10, // F6
    118: 64 | 1 << 10, // F7
    119: 65 | 1 << 10, // F8
    120: 66 | 1 << 10, // F9
    121: 67 | 1 << 10, // F10
    122: 68 | 1 << 10, // F11
    123: 69 | 1 << 10, // F12
    124: 104 | 1 << 10, // F13
    125: 105 | 1 << 10, // F14
    126: 106 | 1 << 10, // F15
    127: 107 | 1 << 10, // F16
    128: 108 | 1 << 10, // F17
    129: 109 | 1 << 10, // F18
    130: 110 | 1 << 10, // F19
    131: 111 | 1 << 10, // F20
    132: 112 | 1 << 10, // F21
    133: 113 | 1 << 10, // F22
    134: 114 | 1 << 10, // F23
    135: 115 | 1 << 10, // F24

    144: 83 | 1 << 10, // keypad num lock

    160: 94, // caret
    161: 33, // exclaim
    162: 34, // double quote
    163: 35, // hash
    164: 36, // dollar
    165: 37, // percent
    166: 38, // ampersand
    167: 95, // underscore
    168: 40, // open parenthesis
    169: 41, // close parenthesis
    170: 42, // asterix
    171: 43, // plus
    172: 124, // pipe
    173: 45, // minus
    174: 123, // open curly bracket
    175: 125, // close curly bracket
    176: 126, // tilde

    181: 127, // audio mute
    182: 129, // audio volume down
    183: 128, // audio volume up

    188: 44, // comma
    190: 46, // period
    191: 47, // slash (/)
    192: 96, // backtick/backquote (`)
    219: 91, // open square bracket
    220: 92, // back slash
    221: 93, // close square bracket
    222: 39, // quote
    224: 227 | 1 << 10, // meta (command/windows)
  },

  scanCodes: { // SDL keycode ==> SDL scancode. See SDL_scancode.h
    8: 42, // backspace
    9: 43, // tab
    13: 40, // return
    27: 41, // escape
    32: 44, // space
    35: 204, // hash

    39: 53, // grave

    44: 54, // comma
    46: 55, // period
    47: 56, // slash
    48: 39, // 0
    49: 30, // 1
    50: 31, // 2
    51: 32, // 3
    52: 33, // 4
    53: 34, // 5
    54: 35, // 6
    55: 36, // 7
    56: 37, // 8
    57: 38, // 9
    58: 203, // colon
    59: 51, // semicolon

    61: 46, // equals

    91: 47, // left bracket
    92: 49, // backslash
    93: 48, // right bracket

    96: 52, // apostrophe
    97: 4, // A
    98: 5, // B
    99: 6, // C
    100: 7, // D
    101: 8, // E
    102: 9, // F
    103: 10, // G
    104: 11, // H
    105: 12, // I
    106: 13, // J
    107: 14, // K
    108: 15, // L
    109: 16, // M
    110: 17, // N
    111: 18, // O
    112: 19, // P
    113: 20, // Q
    114: 21, // R
    115: 22, // S
    116: 23, // T
    117: 24, // U
    118: 25, // V
    119: 26, // W
    120: 27, // X
    121: 28, // Y
    122: 29, // Z

    127: 76, // delete

    305: 224, // ctrl

    308: 226, // alt

    316: 70, // print screen
  },
  lookupKeyCodeForEvent(event) {
    var code = event.keyCode;
    if (code >= 65 && code <= 90) {
      code += 32; // make lowercase for SDL
    } else {
      code = SDL.keyCodes[event.keyCode] || event.keyCode;
      // If this is one of the modifier keys (224 | 1<<10 - 227 | 1<<10), and the event specifies that it is
      // a right key, add 4 to get the right key SDL key code.
      if (event.location === 2 /*KeyboardEvent.DOM_KEY_LOCATION_RIGHT*/ && code >= (224 | 1 << 10) && code <= (227 | 1 << 10)) {
        code += 4;
      }
    }
    return code;
  }
};

var LibraryGFXEvents = {
  $gfxSetup__deps: ["$Browser"],
  $gfxSetup: (canvasContainer, canvas) => {
    const eh = this.getEventHandler();
    this.eventHandlerSetCanvas(eh, canvas.id, canvasContainer.id);

    canvasContainer.onmousemove = (e) => {
      e.type_ = 0;
      this.eventHandlerPostMouseEvent(eh, e);
      e.preventDefault();
    };
    const handleMouseEvent = (event) => {
      this.eventHandlerPostMouseEvent(eh, e);
      e.preventDefault();
    };
    canvasContainer.onmousedown = (e) => {
      e.type_ = 1;
      handleMouseEvent(e);
    };
    canvasContainer.onmouseup = (e) => {
      e.type_ = 2;
      handleMouseEvent(e);
    };
    window.oncontextmenu = (e) => {
      e.preventDefault();
    };

    var state = { cursorInPage: false };
    window.onmouseout = () => {
      state.cursorInPage = false;
    };
    window.onmouseover = () => {
      state.cursorInPage = true;
    };
    const trapKeyEvents = (code) => { return state.cursorInPage; };
    const handleKeyEvent = (event) => {
      var key = Helper.lookupKeyCodeForEvent(event);
      if (key >= 1024) {
        event.scan_ = key - 1024;
      } else {
        event.scan_ = Helper.scanCodes[key] || key;
      }
      event.key_ = key;
      this.eventHandlerPostKeyEvent(eh, event);
      event.preventDefault();
    };
    window.addEventListener('keydown', (event) => {
      if (trapKeyEvents(event.code)) {
        event.code_ = SDL.lookupKeyCodeForEvent(event);;
        event.type_ = 0;
        handleKeyEvent(event);
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
      console.log("Wheel", event);

      // Flip the wheel direction to translate from browser wheel direction
      // (+:down) to SDL direction (+:up)
      var deltaY = -Browser.getMouseWheelDelta(event);
      // Quantize to integer so that minimum scroll is at least +/- 1.
      deltaY = (deltaY == 0) ? 0 : (deltaY > 0 ? Math.max(deltaY, 1) : Math.min(deltaY, -1));

      this.eventHandlerPostWheelEvent(eh, { deltaY: deltaY });
      event.preventDefault();
    });

    (async function resizeCanvasLoop() {
      var lastW, lastH;
      var lastPixelRatio;
      while (true) {
        const rect = canvasContainer.getBoundingClientRect();
        const pixelRatio = window.devicePixelRatio;
        if (lastW !== rect.width || lastH !== rect.height || lastPixelRatio !== pixelRatio) {
          canvas.width = rect.width * pixelRatio;
          canvas.height = rect.height * pixelRatio;
          this.eventHandlerPostDisplayFormat(eh, rect.width, rect.height, canvas.width, canvas.height, pixelRatio);
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
