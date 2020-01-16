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
   (Cond
    [(--> (IsNot 0))
     (-->
                                        ; if empty don't bother going further
      (Set "point-count")
                                        ; find median
      (Get "point-count")
      (Math.Divide 2)
      (Set "median")
      (Push "median-mem" :Clear false)
                                        ; store also median+1 for later use
      (Math.Add 1)
      (Push "median+1-mem" :Clear false)
                                        ; find dimension
      (Get "depth")
      (Math.Mod k)
      (Set "dimension")
                                        ; sort points
      (Get "points")
      (Sort :Key (--> (Take (# "dimension"))))
                                        ; split left and right, push points
      (Push "points-mem" :Clear false)
      (Push "points-mem" :Clear false)
      (Slice :To (# "median"))
                                        ; left points arg
      (Push)
                                        ; depth arg
      (Get "depth")
      (Math.Add 1)
      (Push)
      (Push "depth-mem" :Clear false)
      
      ;; -->
                                        ; recurse to build deeper tree
      (Do "build-tree")
      ;; <--
      
      (Push "left-mem" :Clear false)
      (Log "left")
                                        ; pop args
      (Pop)
      (Pop)
                                        ; pop points
      (Pop "median+1-mem")
      (Set "median")
      (Pop "points-mem")
      (Slice :From (# "median"))
                                        ; left points arg
      (Push)
                                        ; depth arg
      (Pop "depth-mem")
      (Push)
      
      ;; -->
                                        ; recurse to build deeper tree
      (Do "build-tree")
      ;; <--
      
      (Push "right-mem" :Clear false)
      (Log "right")
                                        ; pop args
      (Pop)
      (Pop)
                                        ; compose our result "tuple" seq
      (Clear "result")
                                        ; top
      (Pop "median-mem")
      (Set "median")
      (Pop "points-mem")
      (Take (# "median"))
      (Push "result")
                                        ;start with left
      (Pop "left-mem")
      (Push "result")
                                        ; right
      (Pop "right-mem")
      (Push "result")
                                        ; set result as final chain output
      (Get "result")
      (Log "result"))
     (--> true)
     (Clear "result")
     ])
   (Get "result" :Default [])
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
