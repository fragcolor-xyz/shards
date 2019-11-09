(def n (Node))
(schedule n (Chain "n" :Looped
  (ChainLoader "../../chainblocks/src/tests/loadme.clj")
))

(run n 0.1)
