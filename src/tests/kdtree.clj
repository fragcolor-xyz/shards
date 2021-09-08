; SPDX-License-Identifier: BSD-3-Clause
; Copyright © 2020 Fragcolor Pte. Ltd.

;; not meant to be something to use in production
;; a (Algo.KDTree) can be implemented in the future for that
;; this is just a proof of concept and a test for the language efficiency

(def k 2) ;; 2 dims
(def build-tree
  (Chain
   "build-tree"
                                        ; organize args from stack
   (Pop) (ExpectInt) = .depth
   (Pop) = .points
                                        ; count points
   (Count .points)
   (Cond
    [(-> (IsNot 0))                     ; if empty don't bother going further
     (->
      (Math.Divide 2) = .median
                                        ; store also median+1 for later use
      (Math.Add 1) = .median+1
                                        ; find dimension
      .depth (Math.Mod k) = .dimension
                                        ; sort points
      (Sort .points :Key (-> (Take .dimension)))
                                        ; split left and right, push points
      (Slice :To .median)
                                        ; left points arg
      (Push)
                                        ; depth arg
      .depth (Math.Add 1)
      (Push)
                                        ; recurse to build deeper tree
      (Recur)
      (Push .left-mem :Clear false)
                                        ; pop points
      .points
      (Slice :From .median+1)
                                        ; left points arg
      (Push)
                                        ; depth arg
      .depth (Math.Add 1)
      (Push)
                                        ; recurse to build deeper tree
      (Recur)
      (Push .right-mem :Clear false)
                                        ; compose our result "tuple" seq
      (Clear .result)
                                        ; top
      .points (Take .median) >> .result
                                        ;start with left
      (Pop .left-mem) >> .result
                                        ; right
      (Pop .right-mem) >> .result)
     (-> true)
     (Clear .result)])
   .result ?? []))

(def Root (Node))
(schedule
 Root
 (Chain
  "kdtree"
                                        ; points
  [[7 2] [5 4] [9 6] [4 7] [8 1] [2 3]]
  (Push)
                                        ; depth
  0
  (Push)
  (Do build-tree)
  (Log)
  (Assert.Is [[7, 2], [[5, 4], [[2, 3], [], []], [[4, 7], [], []]], [[9, 6], [[8, 1], [], []], []]] true)
  ))
(run Root 0.1)
