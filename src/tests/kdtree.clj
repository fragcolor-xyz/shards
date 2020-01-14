(def k 2) ;; 2 dims
(def build-tree
  (Chain
   "build-tree"
                                        ; organize args from stack
   (Pop)
   (Set "depth")
   (Pop)
   (Set "points")
                                        ; count points
   (Count "points")
   (Set "point-count")
                                        ; find median re-using count
   (Math.Divide 2)
   (Set "median")
                                        ; find dimension
   (Get "depth")
   (Math.Mod k)
   (Set "dimension")
                                        ; sort points
   (Get "points")
   (Sort :Key
         (--> (Take (# "dimension"))))
   (Log)
   ))

(def Root (Node))
(schedule
 Root
 (Chain
  "kdtree"
                                        ; points
  (Const [[7 2] [5 4] [9 6] [4 7] [8 1] [2 3]])
  (Push)
                                        ; depth
  0
  (Push)
  (Do build-tree)
  ))
(run Root 0.1)
