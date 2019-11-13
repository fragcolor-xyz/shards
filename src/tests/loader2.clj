(def n (Node))
(schedule n (Chain "n" :Looped
  (ChainLoader "loadme.clj")
))

(run n 0.1)
