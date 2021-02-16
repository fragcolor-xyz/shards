(def Root (Node))

(def test
  (Chain
   "test"
   :Looped
                                        ; params
   1 (ToString)
   (Set .params "postId")
                                        ; GET
   .params
   (Http.Get "https://jsonplaceholder.typicode.com/comments")
   (FromJson)
   (ExpectSeq)
   (Take 0)
   (ExpectTable)
   (Take "name")
   (Assert.Is "id labore ex et quam laborum" true)
   (Log)

   (Maybe
    (->
     nil
     (Http.Get "https://httpstat.us/200?sleep=5000" :Timeout 1)
     (Log)))
                                        ; POST
   .params
   (Http.Post "https://jsonplaceholder.typicode.com/posts") &> .json
   (FromJson)
   (ExpectTable)
   (Take "id")
   (Assert.Is 101 true)
   (Log)

   .json
   (Http.Post "https://postman-echo.com/post")
   (Log)))

(schedule Root test)
(run Root 0.1 50)

; test reusing sockets/streams
(schedule Root test)
(run Root 0.1 50)

(schedule Root
          (Chain
           "Download"
           nil (Http.Get "https://ipfs.io/ipfs/QmSsba3SLnAEVGFaEcnpUeRuAb2vrJE2wpLpmRonf6aRrm" :Bytes true) = .avocado
           "avocado.glb" (FS.Write .avocado :Overwrite true)))
(run Root 0.1)

;; (def server-handler
;;   (Chain
;;    "server-handler"
;;    :Looped
;;    (Msg "handled")
;;    (Http.Read)
;;    (Log)
;;    1 >> .r
;;    2 >> .r
;;    3 >> .r
;;    .r (ToJson) (Http.Response)))

;; (def test-server
;;   (Chain
;;    "test"
;;    :Looped
;;    (Http.Server :Handler server-handler)))

;; (schedule Root test-server)
;; (run Root 0.1)