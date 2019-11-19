(def! Root (Node))

(def! testChain (Chain "namedChain"  
  "Value1" (Set "tab1" "v1")
  "Value2" (Set "tab1" "v2")
  (Get "tab1" "v1")
  (Assert.Is "Value1" true)
  (Log)
  (Get "tab1" "v2")
  (Assert.IsNot "Value1" true)
  (Log)

  (Get "tab1")
  (Set "tab1-set-copy")
  (Get "tab1-set-copy" "v1")
  (Assert.Is "Value1" true)))

(schedule Root testChain)
