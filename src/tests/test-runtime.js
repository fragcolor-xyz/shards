/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

const test = require('../../build/test-runtime.js');
test({
  postRun: () => {
    process.kill(process.pid, "SIGTERM");
  }
});
