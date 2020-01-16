;; not meant to be something to use in production
;; a (Algo.KDTree) can be implemented in the future for that
;; this is just a proof of concept and a test for the language efficiency

(def k 2) ;; 2 dims
(def build-tree
  (Chain
   "build-tree"
                                        ; organize args from stack
   (Pop)
   (Set "depth")
   (Log "depth")
   (Pop)
   (Set "points")
   (Log "points")
                                        ; count points
   (Count "points")
                                        ; if empty don't bother going further
   (Set "point-count")
                                        ; if no points stop
   (IsNot 0)
   (And)
                                        ; find median
   (Get "point-count")
   (Math.Divide 2)
   (Set "median")
                                        ; store also median+1 for later use
   (Math.Add 1)
   (Set "median+1")
                                        ; find dimension
   (Get "depth")
   (Math.Mod k)
   (Set "dimension")
                                        ; sort points
   (Get "points")
   (Sort :Key (--> (Take (# "dimension"))))
                                        ; split left and right, push points
   (Push "points-mem" :Clear false)
   (Slice :From 0 :To (# "median"))
                                        ; left points arg
   (Push)
                                        ; depth arg
   (Get "depth")
   (Math.Add 1)
   (Push)
                                        ; recurse to build deeper tree
   (Do "build-tree")
   (Push "left-mem" :Clear false)
                                        ; pop args
   (Pop)
   (Pop)
                                        ; pop points
   (Pop "points-mem")
   (Slice :From (# "median+1"))
                                        ; left points arg
   (Push)
                                        ; depth arg
   (Get "depth")
   (Math.Add 1)
   (Push)
                                        ; recurse to build deeper tree
   (Do "build-tree")
   (Push "right-mem" :Clear false)
                                        ; pop args
   (Pop)
   (Pop)
                                        ; compose our result "tuple" seq
   (Clear "result")
                                        ;start with left
   (Pop "left-mem")
   (Push "result")
                                        ; right
   (Pop "right-mem")
   (Push "result")
                                        ; set result as final chain output
   (Get "result")
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
