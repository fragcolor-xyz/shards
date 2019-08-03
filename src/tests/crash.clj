(def! root (Node))
(schedule root (Chain "crash" 10 (Assert.Is 1 true)))