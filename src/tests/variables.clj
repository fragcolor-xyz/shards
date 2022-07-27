; SPDX-License-Identifier: BSD-3-Clause
; Copyright © 2019 Fragcolor Pte. Ltd.

(def! mesh (Mesh))

(schedule
 mesh
 (Wire
  "variables" :Looped
  10
  (Set "x")
  (Assert.Is 10 true)
  20
  (Assert.Is 20 true)
  (Get "x")
  (Assert.Is 10 true)

  (Set "y" "key1")
  (Assert.Is 10 true)
  (Get "y")
  (Assert.IsNot 10 true)
  (Log)
  (Get "y" "key1")
  (Assert.Is 10 true)
  (Log)
  77
  (Set "y" "key2")
  (Assert.Is 77 true)
  20
  (Assert.Is 20 true)
  (Get "y" "key1")
  (Assert.Is 10 true)
  (Get "y" "key2")
  (Assert.Is 77 true)

  9
  (Set "a")
  18
  (Set "b")
  (Get "a")
  (Assert.Is 9 true)
  (Get "b")
  (Assert.Is 18 true)
  (Swap "a" "b")
  (Get "a")
  (Assert.Is 18 true)
  (Get "b")
  (Assert.Is 9 true)
  (ToString)
  (Set "as")
  (Assert.Is "9" true)
  (Assert.IsNot 9 true)

  (Sequence "e" :Clear false)
  1
  (Push "e")
  2
  (Push "e")
  3
  (Push "e")
  4
  (Push "e")
  5
  (Push "e")
  (Get "e")
  (Assert.Is [1 2 3 4 5] true)
  (Log)

  1
  (Push "e")
  2
  (Push "e")
  3
  (Push "e")
  4
  (Push "e")
  5
  (Push "e")
  (Get "e")
  (Assert.Is [1 2 3 4 5 1 2 3 4 5] true)
  (Log)

  (Remove .e :Predicate (IsMore 2) :Unordered false)
  (Assert.Is [1 2 1 2] true)
  (Log)

  2
  (Push .e)
  (Get .e)
  (Assert.Is [1 2 1 2 2] true)
  (Log)

  (Remove .e :Predicate (IsLessEqual 2) :Unordered false)
  (Assert.Is [] true)
  (Log)

  2
  (Push .e)
  (Get .e)
  (Assert.Is [2] true)
  (Log)

  (Remove .e :Predicate (IsLessEqual 2) :Unordered false)
  (Assert.Is [] true)
  (Log)

  (Const [6 7 8 9])
  (Set "c")
  (Log)
  (Get "c")
  (Assert.Is [6 7 8 9] true)
  (Take [0 1])
  (Assert.Is [6 7] true)
  (Log)
  (Take 0)
  (Assert.Is 6 true)
  (Log)

  (Const ["a" "b"])
  (Set "d" "k1")
  "c"
  (Push "d" "k1")
  (Get "d" "k1")
  (Assert.Is ["a" "b" "c"] true)

  (Sequence .nested :Clear false)
  (Get .d)
  (Push .nested)

  (Sequence .d "k2")
  1111
  (Push .d "k2")
  (Get .d)
  (Push .nested)

  1112
  (Push .d "k2")
  (Get .d)
  (Push .nested)

  (Get .nested)
  (Log "nested")
  (Remove .nested :Predicate (Const true) :Unordered true)
  (Assert.Is [] true)
  (Log)

  (Sequence .n1)
  (Const [1 2 3])
  (Push .n1)
  (Const [2 3 4])
  (Push .n1)
  (Const [3 4 5])
  (Push .n1)
  (Get .n1)
  (Log "before U remove")
  (Remove .n1 :Predicate (-> (Take 0) (IsLess 3)) :Unordered true)
  (Log "after U remove")
  (Assert.Is [[3 4 5]] true)
  (Count .n1)
  (Log "count U")

  (Sequence .n2)
  (Const [1 2 3])
  (Push .n2)
  (Const [2 3 4])
  (Push .n2)
  (Const [3 4 5])
  (Push .n2)
  (Get .n2)
  (Log "before O remove")
  (Remove .n2 :Predicate (-> (Take 0) (IsLess 3)) :Unordered false)
  (Log "after O remove")
  (Assert.Is [[3 4 5]] true)
  (Count .n2)
  (Log "count O")

  "global1"
  (Set .global1 :Global true)
  (Get .global1)
  (Assert.Is "global1" true)

  ; Seq of Floats
  (Sequence .sf :Types Type.Float)
  ; Seq of Floats Seqs and so on
  (Sequence .sff :Types [Type.Float])

  (Repeat
   (->
    0.1 >> .sf
    .sf >> .sff)
   :Times 3)

  .sf (Assert.Is [0.1 0.1 0.1] true) (Log)
  .sff (Assert.Is [[0.1] [0.1 0.1] [0.1 0.1 0.1]] true) (Log)

  (Sequence .smany :Types [[Type.Float] Type.Bool])

  true >> .smany
  .sff >> .smany
  ;; this is how push works... it will add Int type to the seq OK types basically
  10 >> .smany
  .smany (Assert.Is [true, [[0.1], [0.1, 0.1], [0.1, 0.1, 0.1]], 10] true) (Log)

  ; without Table shard the rest would not work cos Table won't be exposed by When
  (Table .table-decl-1 :Types Type.Float)
  true (When (Is true) (-> 11.0 (Set .table-decl-1 "A")))
  (Get .table-decl-1 "A") (Math.Add 2.0) (Assert.Is 13.0 true)
  (Get .table-decl-1 "B" :Default 10.0) (Assert.Is 10.0 true)

  ; without Table shard the rest would not work cos Table won't be exposed by When
  (Table .table-decl-2)
  true (When (Is true) (-> 11.0 (Set .table-decl-2 "A")))
  (Get .table-decl-2 "A") (ExpectFloat) (Math.Add 2.0) (Assert.Is 13.0 true)


  [1 2 3] = .assoc-test-seq
  [1 22] (Assoc .assoc-test-seq)
  .assoc-test-seq (Assert.Is [1 22 3] true)

  .assoc-test-seq (Reverse) (Assert.Is [3 22 1] true)
  (Log "Reversed")
  "Hello World" (Reverse) (Assert.Is "dlroW olleH" true)
  (Log "Reversed")

  "Hello World" (StringToBytes) (Reverse) (BytesToString)
  (Assert.Is "dlroW olleH" true)
  (Log "Reversed")

  {"x" 1 "y" 2 "z" 3} = .assoc-test-table
  ["z" 33] (Assoc .assoc-test-table)
  .assoc-test-table (Assert.Is {"x" 1 "y" 2 "z" 33} true)

  (Msg "Done!")))

(if (tick mesh) nil (throw "failure"))
(if (tick mesh) nil (throw "failure"))
(if (tick mesh) nil (throw "failure"))
(if (tick mesh) nil (throw "failure"))
(def mesh nil)

(defmesh main)
(defwire external-var-test
  .external-variable-1 (Assert.Is "Hello" true) (Log "EXTVAR") (PrependTo .external-variable-2)
  .external-variable-2 (Assert.Is "Hello World" true) (Log))

(do
  ; do this inside a do to test wire ownership of the variable
  (set-var external-var-test "external-variable-1" "Hello"))

(def mutating-ext-var (set-var external-var-test "external-variable-2" " World"))

(schedule main external-var-test)
(run main)

; another property is that we can read back those variables!
(println (read-var mutating-ext-var))
(if (= "Hello World" (read-var mutating-ext-var)) nil (throw "external variable not updated!"))

(prn "Done!")
