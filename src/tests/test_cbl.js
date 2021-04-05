const cbl = require('../../build/' + process.argv[2]);
cbl({
  arguments: ["-e", '(do (def Root (Node)) (schedule Root (Chain "x" (Pause 2.0) (Msg "Hello") 0 (Exit))) (run Root 0.1))']
});
