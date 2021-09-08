/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

const cbl = require('../../build/' + process.argv[2]);
cbl({
  arguments: ["-e", '(do (def Root (Node)) (schedule Root (Chain "x" (Pause 2.0) (Msg "Hello") "process.kill(process.pid, \\\"SIGTERM\\\")" (_Emscripten.Eval))) (run Root 0.1))']
});
