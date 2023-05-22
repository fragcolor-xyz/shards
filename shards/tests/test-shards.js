/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

const shards = require('../../build/' + process.argv[2]);
shards({
  arguments: ["-e", '(do (def Root (Mesh)) (schedule Root (Wire "x" (Pause 2.0) (Msg "Hello") "process.kill(process.pid, \\\"SIGTERM\\\")" (_Emscripten.Eval))) (run Root 0.1))']
});
