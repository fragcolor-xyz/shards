(def Root (Node))

(def test
  (Chain
   "test"
                                        ; params
   1 (ToString)
   (Set .params "postId")
                                        ; GET
   (Get .params)
   (Http.Get "jsonplaceholder.typicode.com" "/comments")
   (FromJson)
   (ExpectSeq)
   (Take 0)
   (ExpectTable)
   (Take "name")
   (Assert.Is "id labore ex et quam laborum" true)
   (Log)
                                        ; POST
   (Get .params)
   (Http.Post "jsonplaceholder.typicode.com" "/posts")
   (FromJson)
   (ExpectTable)
   (Take "id")
   (Assert.Is 101 true)
   (Log)
   
  ;;  nil (Http.Get "localhost" "/" :Port 8000 :Secure false)
  ;;  (Log)
   ))

(schedule Root test)
(run Root 0.1)

; test reusing sockets/streams
(schedule Root test)
(run Root 0.1)
