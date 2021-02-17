const cbl = require('../../build/cbl.js');
cbl({
  arguments: ["-e", '(do (def Root (Node)) (schedule Root (Chain "x" (Pause 2.0) (Msg "Hello") 0 (Exit))) (run Root 0.1))']
});
