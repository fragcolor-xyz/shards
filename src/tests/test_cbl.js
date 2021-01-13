const cbl = require('../../build/cbl.js');
cbl({
  arguments: ["-e", '(do (def Root (Node)) (schedule Root (Chain "x" (Pause 2.0) (Msg "Hello") (Dummy) 0 (Exit))) (run Root 0.1))']
});
