(def! Root (Node))

(def! inner1 (Chain "inner"
  (Assert.Is "My input" true)
  "My input 2"
))

(def! testChain (Chain "namedChain"
  (Msg "Running tests!")

  3.14
  (AsInt64)
  (Log)
  (AsFloat64)
  (Log)
  (Assert.Is 3.14 true)

  1078523331
  (AsFloat32)
  (Log)

  1286304790
  (AsFloat32)
  (Log)
  
  true
  (Cond [
    (--> (Is true)) (--> (Msg "Cond was true!!") false)
    (--> (Is false)) (--> (Msg "Cond was false!") true)] :Passthrough false)
  (Assert.Is false true)

  true
  (Cond [
    (--> (Is false) (Or) (Is true)) (--> (Msg "Cond was true!!") false)
    (--> (Is false)) (--> (Msg "Cond was false!") true)] :Passthrough false)
  (Assert.Is false true)

  true
  (Cond [
    (--> (Is false) (And) (Is true)) (--> (Assert.Is nil true))
    (--> (Is true)) (--> (Msg "Cond was false!") true)] :Passthrough false)
  (Assert.Is true true)

  false
  (Cond [
    (--> (Is true)) (--> (Msg "Cond was true!!") false)
    (--> (Is false)) (--> (Msg "Cond was false!") true)] :Passthrough false)
  (Assert.Is true true)

  false
  (Cond [
    (--> (Is true)) (--> (Msg "Cond was true!!") false)
    (--> (Is false) (And) (IsNot true)) (--> (Msg "Cond was false!") true)] :Passthrough false)
  (Assert.Is true true)

  false
  (Cond [
    (--> (Is false) (And) (IsNot false)) (--> (Assert.Is nil true))
    (--> (Is false)) (--> (Msg "Cond was true!!") false)] :Passthrough false)
  (Assert.Is false true)

  10
  (Cond [
    (--> true)    (--> (Msg "Cond was true!!") false)
    (--> (Is 10)) (--> (Msg "Cond was false!") true)
  ] :Threading true :Passthrough false)
  (Assert.Is true true)

  10
  (Cond [
    (--> true)    (--> (Msg "Cond was true!!") false)
    (--> (Is 10)) (--> (Msg "Cond was false!") true)
  ] :Threading false :Passthrough false)
  (Assert.Is false true)

  "Hello"
  (Assert.Is "Hello" true)
  (Log)

  77
  (Assert.Is 77 true)
  (Log)

  10
  (Math.Add 10)
  (Assert.Is (+ 10 10) true)
  (Log)

  11
  (Math.Subtract 10)
  (Assert.Is (- 11 10) true)
  (Log)

  (Float4 10.3 3.6 2.1 1.1)
  (Math.Multiply (Float4 2 2 2 2))
  (Assert.Is (Float4 (* 10.3 2) (* 3.6 2) (* 2.1 2) (* 1.1 2)) true)
  (Log)

  (Float3 10.3 2.1 1.1)
  (Math.Multiply (Float3 2 2 2))
  (Assert.Is (Float3 (* 10.3 2) (* 2.1 2) (* 1.1 2)) true)
  (Log)

  10
  (Push "list1")
  20
  (Push "list1")
  30
  (Push "list1")
  (Get "list1")
  (Log)
  (Take 0)
  (Assert.Is 10 true)
  (Get "list1")
  (Take 1)
  (Assert.Is 20 true)
  (Get "list1")
  (Take 2)
  (Assert.Is 30 true)
  (Log)

  (Repeat (-->
    10 (Push "list1")
    20 (Push "list1")
    30 (Push "list1")
    (Get "list1")
    (Log)
    (Take 0)
    (Assert.Is 10 true)
    (Get "list1")
    (Take 1)
    (Assert.Is 20 true)
    (Get "list1")
    (Take 2)
    (Assert.Is 30 true)
    (Log)
  ) :Times 5)

  0
  (Set "counter")
  (Repeat (-->
    (Get "counter")
    (Math.Add 1)
    (Update "counter")
   ) :Times 5)
  (Get "counter")
  (Assert.Is 5 true)
  (Log)

  20
  (Set "a")
  30
  (Set "b")
  (Swap "a" "b")
  (Get "a")
  (Assert.Is 30)
  (Get "b")
  (Assert.Is 20)
  (Log)

  "Value1" (Set "tab1" "v1")
  "Value2" (Set "tab1" "v2")
  (Get "tab1" "v1")
  (Assert.Is "Value1" true)
  (Log)
  (Get "tab1" "v2")
  (Assert.IsNot "Value1" true)
  (Log)

  ; "chain:initChain[1]"
  ; (ReplaceText "[^abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\\-\\.\\_]+" "_")
  ; (Log)

  "My input"
  (Dispatch inner1)
  (Assert.Is "My input" true)

  "My input"
  (Do inner1)
  (Assert.Is "My input 2" true)

  ; b0r
  0
  (Math.And 0xFF)
  (Math.LShift 8)
  (Set "x")
  ; b1r
  0
  (Math.And 0xFF)
  (Math.Or (# "x"))
  (Math.LShift 8)
  (Update "x")
  ; b2r
  59
  (Math.And 0xFF)
  (Math.Or (# "x"))
  (Math.LShift 8)
  (Update "x")
  ; b3r
  156
  (Math.And 0xFF)
  (Math.Or (# "x"))
  (Update "x")
  ; result
  (Get "x")
  (Log)

  (Color 2 2 2 255)
  (Log)

  (Const [(Float 1) (Float 2) (Float 3) (Float 4)])
  (Math.Multiply (Float 2.0))
  (Log)
  (Assert.Is [(Float 2) (Float 4) (Float 6) (Float 8)] true)

  (Const [(Float 1) (Float 2) (Float 3) (Float 4)])
  (Math.Multiply [(Float 2.0) (Float 1.0) (Float 1.0) (Float 2.0)])
  (Log)
  (Assert.Is [(Float 2) (Float 2) (Float 3) (Float 8)] true)

  5 (ToFloat) (Math.Divide (Float 10))
  (Assert.Is 0.5 true)

  (Int 10) (ToFloat) (Set "fx")
  (Get "fx") (Assert.Is (Float 10) true)
  (Get "fx") (Assert.IsNot (Int 10) true)
  (Float 5) (Math.Divide (# "fx")) (Assert.Is 0.5 true)

  10 (Push "myseq")
  20 (Push "myseq")
  30 (Push "myseq")
  (Get "myseq")
  (Take 0)
  (Math.Add 1)
  (Assert.Is 11 true)
  (Log)

  (Clear "myseq")

  12 (Push "myseq")
  22 (Push "myseq")
  32 (Push "myseq")
  (Get "myseq")
  (Take 0)
  (Math.Add 1)
  (Assert.Is 13 true)
  (Log)
  
  10 (Push "tab1" "myseq")
  20 (Push "tab1" "myseq")
  30 (Push "tab1" "myseq")
  (Get "tab1" "myseq")
  (Take 0)
  (Math.Add 1)
  (Assert.Is 11 true)
  (Log)

  (Clear "tab1" "myseq")

  12 (Push "tab1" "myseq")
  22 (Push "tab1" "myseq")
  32 (Push "tab1" "myseq")
  (Get "tab1" "myseq")
  (Take 0)
  (Math.Add 1)
  (Assert.Is 13 true)
  (Log)

  10 (Push "tab1new" "myseq")
  20 (Push "tab1new" "myseq")
  30 (Push "tab1new" "myseq")
  (Get "tab1new" "myseq")
  (Take 0)
  (Math.Add 1)
  (Assert.Is 11 true)
  (Log)

  (Clear "tab1new" "myseq")

  12 (Push "tab1new" "myseq")
  22 (Push "tab1new" "myseq")
  32 (Push "tab1new" "myseq")
  (Get "tab1new" "myseq")
  (Take 0)
  (Math.Add 1)
  (Assert.Is 13 true)
  (Log)

  (Int2 10 11) (Push "myseq2")
  (Int2 20 21) (Push "myseq2")
  (Int2 30 31) (Push "myseq2")
  (Get "myseq2")
  (Flatten)
  (Set "myseq2flat")
  (Get "myseq2flat")
  (Take 0)
  (Math.Add 1)
  (Assert.Is 11 true)
  (Log)

  (Repeat (-->
    (Pop "myseq2flat")
    (Log)
  ) 6)

  (Repeat (-->
    (Get "index" :Default 0)
    (Math.Add 1)
    (Set "index")
  ) 6)
  (Get "index")
  (Assert.Is 6 true)

  (Repeat (-->
    (Get "index2" :Default 0)
    (Math.Add 1)
    (Set "index2")
    (Cond [(--> (Is 6)) (--> (Return))])
  ) :Forever true)
  (Get "index2")
  (Assert.Is 6 true)

  (Repeat (-->
    10 (Set "tableInList" "x")
    20 (Set "tableInList" "y")
    30 (Set "tableInList" "z")
    (Get "tableInList") (Push "newListInRepeat")
  ) 5)
  (Get "newListInRepeat") (Log)

  2 (Push "unsortedList")
  4 (Push "unsortedList")
  1 (Push "unsortedList")
  0 (Push "unsortedList")
  1 (Push "unsortedList")
  5 (Push "unsortedList")
  (Get "unsortedList") (Sort)
  (Log)
  (Assert.Is [0 1 1 2 4 5] true)

  2 (Push "unsortedList2")
  4 (Push "unsortedList2")
  1 (Push "unsortedList2")
  0 (Push "unsortedList2")
  1 (Push "unsortedList2")
  5 (Push "unsortedList2")

  2 (Push "unsortedList3")
  4 (Push "unsortedList3")
  1 (Push "unsortedList3")
  0 (Push "unsortedList3")
  1 (Push "unsortedList3")
  5 (Push "unsortedList3")
  
  (Get "unsortedList2") (Sort [(# "unsortedList3")])
  (Log)
  (Assert.Is [0 1 1 2 4 5] true)
  (Get "unsortedList2") (Sort (# "unsortedList3"))
  (Log)
  (Assert.Is [0 1 1 2 4 5] true)
  (Get "unsortedList3") 
  (Log)
  (Assert.Is [0 1 1 2 4 5] true)

  (Clear "unsortedList")
  2 (Push "unsortedList")
  4 (Push "unsortedList")
  1 (Push "unsortedList")
  0 (Push "unsortedList")
  1 (Push "unsortedList")
  5 (Push "unsortedList")
  (Get "unsortedList") (Sort :Desc true)
  (Log)
  (Assert.Is [5 4 2 1 1 0] true)
  (Count "unsortedList")
  (Assert.Is 6 true)

  (Get "unsortedList")
  (IndexOf 2)
  (Assert.Is 2 true)

  (Get "unsortedList")
  (IndexOf 5)
  (Assert.Is 0 true)

  (Get "unsortedList")
  (IndexOf [1 1])
  (Assert.Is 3 true)

  (Get "unsortedList")
  (IndexOf [1 1 0])
  (Assert.Is 3 true)

  (Get "unsortedList")
  (IndexOf 10)
  (Assert.Is -1 true)

  (Get "unsortedList")
  (IndexOf [1 1 0 1])
  (Assert.IsNot 3 true)

  (Const [1 1 0])
  (Set "toFindVar")
  (Get "unsortedList")
  (IndexOf (# "toFindVar"))
  (Assert.Is 3 true)

  (Get "unsortedList")
  (Remove :Predicate (-->(IsMore 3)))
  (Sort :Desc true)
  (Assert.Is [2 1 1 0] true)
  (Count "unsortedList")
  (Assert.Is 4 true)
  (Get "unsortedList")
  (Log)
  (Assert.Is [2 1 1 0] true)

  (Get "unsortedList2")
  (Set "unsortedList2Copy")
  (Get "unsortedList2Copy")
  (Remove [(# "unsortedList3")] (-->(IsMore 3)))
  (Sort [(# "unsortedList3")])
  (Log)
  (Assert.Is [0 1 1 2] true)
  (Get "unsortedList3")
  (Log)
  (Assert.Is [0 1 1 2] true)
  (Count "unsortedList2")
  (Assert.Is 6 true)

  1.0 (Push "meanTest")
  2.0 (Push "meanTest")
  0.0 (Push "meanTest")
  3.0 (Push "meanTest")
  (Get "meanTest") (Math.Mean) (Log)
  (Assert.Is 1.5 true)

  1 (Set "indexAsVar")
  (Get "meanTest")
  (Take (# "indexAsVar"))
  (Assert.Is 2.0 true)
  (Const [1 2]) (Set "indexAsVar2")
  (Get "meanTest")
  (Take (# "indexAsVar2"))
  (Assert.Is [2.0 0.0] true)

  (Repeat (-->
    (Pop "meanTest")
    (Math.Add 1.0)
    (Log)
  ) 4)

  (Get "unsortedList") (Limit 2) (Set "limitTest") (Count "limitTest")
  (Assert.Is 2 true)

  10 (Set "repeatsn")
  (Repeat (-->
    (Get "repeatsCount" :Default 0)
    (Math.Add 1)
    (Set "repeatsCount")
  ) (# "repeatsn"))
  (Get "repeatsCount")
  (Assert.Is 10 true)

  0 (Math.Add 10) (Assert.Is 10 true)

  0x0000000000070090
  (BitSwap64)
  (ToHex) (Log)
  (Assert.Is "0x9000070000000000" true)

  0x00070090
  (BitSwap32)
  (Assert.Is 0x90000700 true)
  (ToHex) (Log)

  "Hello file append..."
  (WriteFile "test.bin")
  "Hello file append..."
  (WriteFile "test.bin")

  "Hello Pandas"
  (ToBytes)
  (BytesToInt8)
  (Assert.Is [72 101 108 108 111 32 80 97 110 100 97 115 0] true)
  (Log)
  (Math.Xor [77 78 77 11 16])
  (Assert.Is [5 43 33 103 127 109 30 44 101 116 44 61 77] true)
  (Log)
  (Math.Xor [77 78 77 11 16])
  (Assert.Is [72 101 108 108 111 32 80 97 110 100 97 115 0] true)
  (Log)

  ; show induced mutability with Ref
  "Hello reference" ; Const
  (Ref "ref1") ; no copy will happen!
  (Get "ref1")
  (Assert.Is "Hello reference" true)

  (Const [1 2 3 4 5])
  (Set "pretest")
  (ToBytes)
  (BytesToInt64)
  (Assert.Is [1 2 3 4 5] true)
  
  0 (PrependTo (# "pretest"))
  (Get "pretest")
  (Assert.Is [0 1 2 3 4 5] true)
  (Log)

  "." (FS.Iterate :Recursive false) (Log)
  (Take 4) (FS.Extension) (Log)

  "." (FS.Iterate :Recursive false) (Log)
  (Take 4) (FS.Filename :NoExtension true) (Log)

  "The result is: "   (Set "text1")
  "Hello world, "     (AppendTo (# "text1"))
  "this is a string"  (AppendTo (# "text1"))
  (Get "text1") (Log)
  (Assert.Is "The result is: Hello world, this is a string" true)

  "The result is: "   (Set "text2")
  "Hello world, "     (AppendTo (# "text2"))
  "this is a string again"  (AppendTo (# "text2"))
  (Get "text2") (Log)
  (Assert.IsNot "The result is: Hello world, this is a string" true)

  "## " (PrependTo (# "text2"))
  (Get "text2") (Log)
  (Assert.Is "## The result is: Hello world, this is a string again" true)

  "test.txt"
  (FS.Write (# "text2") :Overwrite true)
  (FS.Read)
  (Assert.Is "## The result is: Hello world, this is a string again" true)
  (Log)

  (Get "text1")
  (ToJson)
  (Log)
  (Assert.Is "{\"type\":20,\"value\":\"The result is: Hello world, this is a string\"}" true)
  (FromJson)
  (ExpectString)
  (Assert.Is "The result is: Hello world, this is a string" true)

  (Get "unsortedList2Copy")
  (ToJson)
  (Log)
  (Assert.Is "{\"type\":23,\"values\":[{\"type\":5,\"value\":0},{\"type\":5,\"value\":1},{\"type\":5,\"value\":1},{\"type\":5,\"value\":2}]}" true)

  (Get "tab1new")
  (ToJson)
  (Log)
  (Assert.Is "{\"type\":24,\"values\":[{\"key\":\"myseq\",\"value\":{\"type\":23,\"values\":[{\"type\":5,\"value\":12},{\"type\":5,\"value\":22},{\"type\":5,\"value\":32}]}}]}" true)

  (Float4 1 2 3 4)
  (Take 0)
  (Assert.Is 1.0 true)

  (Float4 1 2 3 4)
  (Take [2 3])
  (Assert.Is (Float2 3 4) true)

  (Msg "All looking good!")))

(schedule Root testChain)
(if (tick Root) nil (throw "Root tick failed"))

(def Root (Node))
(def testChain nil)

(def loopedChain (Chain "LoopedChain" :Looped
  10 (Push "loopVar")
  11 (Push "loopVar")
  12 (Push "loopVar")
  (Count "loopVar")
  (Assert.Is 3 true)))

 (def loopedChain2 (Chain "LoopedChain" :Looped
  10 (Push "loopVar" :Clear false)
  11 (Push "loopVar")
  12 (Push "loopVar")
  (Count "loopVar")))

(prepare loopedChain)
(start loopedChain)
(tick loopedChain)
(tick loopedChain)

(prepare loopedChain2)
(start loopedChain2)
(tick loopedChain2)
(tick loopedChain2)
(if (not (= (stop loopedChain2) (Int 9))) (throw "Seq :Clear test failed"))

(def fileReader (Chain "readFile"
  (Repeat (-->
    (ReadFile "test.bin")
    (ExpectString)
    (Log)
    (Assert.Is "Hello file append..." true))
    2)
))

(prepare fileReader)
(start fileReader)
(stop fileReader)
