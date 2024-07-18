/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

// emscripten related utilities

var shards_core = {
  now: function () {
    return Date.now() / 1000.0;
  },
  _emscripten_get_now: function () {
    return Date.now() / 1000.0;
  }, // work around for emscripten and rust crate "instant"
  emEval__deps: ['$stringToUTF8', '$lengthBytesUTF8', '$UTF8ToString'],
  emEval: function (code) {
    try {
      const scode = UTF8ToString(code);
      var result = eval(scode);
      // if undefined just return null
      if (result === undefined) {
        return 0;
      }
      // if not undefined return a string
      if (typeof (result) !== "string") {
        result = JSON.stringify(result);
      }
      var len = lengthBytesUTF8(result) + 1;
      var buffer = _malloc(len);
      stringToUTF8(result, buffer, len);
      return buffer;
    } catch (error) {
      console.error(error);
      return -1;
    }
  },
  emBrowsePage__proxy: 'async',
  emBrowsePage: function (curl) {
    const url = UTF8ToString(curl);
    window.open(url, '_self');
  },
  // emEvalAsyncRun: function (code, index) {
  //   try {
  //     const scode = UTF8ToString(code);
  //     const promise = eval(scode);
  //     const bond = new ShardsBonder(promise);
  //     bond.run();
  //     globalThis.shards.bonds[index] = bond;
  //     return true;
  //   } catch (error) {
  //     console.error(error);
  //     return false;
  //   }
  // },
  // emEvalAsyncCheck: function (index) {
  //   try {
  //     const bond = globalThis.shards.bonds[index];
  //     if (bond.finished) {
  //       if (bond.hadErrors) {
  //         globalThis.shards.bonds[index] = undefined;
  //         return -1;
  //       } else {
  //         return 1;
  //       }
  //     } else {
  //       return 0;
  //     }
  //   } catch (error) {
  //     console.error(error);
  //     globalThis.shards.bonds[index] = undefined;
  //     return -1;
  //   }
  // },
  // emEvalAsyncGet: function (index) {
  //   try {
  //     const bond = globalThis.shards.bonds[index];
  //     var result = bond.result;
  //     globalThis.shards.bonds[index] = undefined;
  //     // if undefined just return null
  //     if (result === undefined) {
  //       return 0;
  //     }
  //     // if not undefined return a string
  //     if (typeof (result) !== "string") {
  //       result = JSON.stringify(result);
  //     }
  //     var len = lengthBytesUTF8(result) + 1;
  //     var buffer = _malloc(len);
  //     stringToUTF8(result, buffer, len);
  //     return buffer;
  //   } catch (error) {
  //     console.error(error);
  //     return -1;
  //   }
  // }
}

mergeInto(LibraryManager.library, shards_core);
