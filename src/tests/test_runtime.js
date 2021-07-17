const test = require('../../build-wasm/test_runtime.js');
test({
  postRun: () => {
    process.kill(process.pid, "SIGTERM");
  }
});
