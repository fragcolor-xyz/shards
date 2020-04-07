(def Root (Node))

(def test
  (Chain
   "test"
   :Looped

   1 (ToString)
   (Set .params "postId")
   (Get .params)
   (Http.Get "jsonplaceholder.typicode.com" "/comments")
   (FromJson)
   (ExpectSeq)
   (Take 0)
   (ExpectTable)
   (Take "name")
   (Assert.Is "id labore ex et quam laborum" true)
   (Log)

   ))

(schedule Root test)
(run Root 0.1 10)
