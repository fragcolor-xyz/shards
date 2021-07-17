const test = require('../../build/test_runtime.js');
test({
  postRun: () => {
    process.kill(process.pid, "SIGTERM");
  }
});
